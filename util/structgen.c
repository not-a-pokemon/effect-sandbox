#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_NAME_LEN 32
#define ENUM_TAG 1
#define ENT_R_TAG 2
#define ENT_N_TAG 3

__attribute__((noreturn)) void die(const char *msg) {
	fputs(msg, stderr);
	exit(1);
}

FILE* m_open(const char *name, const char *attr) {
	FILE *t = fopen(name, attr);
	if (t == NULL) {
		perror("m_open");
		exit(1);
	}
	return t;
}

int m_whitespace(char c) {
	return c == ' ' || c == '\t' || c == '\n';
}

int m_isalpha(char c) {
	return
		(c >= 'a' && c <= 'z') ||
		(c >= 'A' && c <= 'Z') ||
		c == '_';
}

int m_alnum(char c) {
	return m_isalpha(c) || (c >= '0' && c <= '9');
}

int m_name(const char *c) {
	int i;
	if (!m_isalpha(*c))
		return 0;
	for (i = 1; c[i]; i++) {
		if (!m_alnum(c[i]))
			return 0;
	}
	return 1;
}

void* m_malloc(size_t size) {
	void *t = malloc(size);
	if (t == NULL) {
		fprintf(stderr, "Failed malloc\n");
		exit(1);
	}
	return t;
}

char consume_buf[MAX_NAME_LEN];

int consume_end(FILE *inp) {
	int peek;
	do {
		peek = fgetc(inp);
	} while (m_whitespace(peek) && peek != EOF);
	if (peek == EOF)
		return 1;
	int c = 0;
	while (1) {
		if (c >= MAX_NAME_LEN - 1) {
			consume_buf[MAX_NAME_LEN - 1] = '\0';
			fprintf(stderr, "Word too long: `%s'\n", consume_buf);
			exit(1);
		}
		consume_buf[c++] = peek;
		peek = fgetc(inp);
		if (m_whitespace(peek) || peek == EOF)
			break;
	}
	consume_buf[c] = '\0';
	return 0;
}

void consume(FILE *inp) {
	if (consume_end(inp))
		die("Unexpected EOF\n");
}

typedef struct decl_field_t {
	int type_tag;
	char type_name[MAX_NAME_LEN];
	char name[MAX_NAME_LEN];
	struct decl_field_t *next;
} decl_field_t;

typedef struct decl_t {
	char name[MAX_NAME_LEN];
	unsigned empty:1;
	unsigned exter:1;
	unsigned exter_rem:1;
	decl_field_t *fields;
	decl_field_t *last_field;
} decl_t;

decl_t *decls = NULL;
size_t decl_cap = 0, decl_n = 0;

void add_decl_field(decl_t *d, int type_tag, const char *type_name, const char *name) {
	decl_field_t *x = m_malloc(sizeof(decl_field_t));
	x->type_tag = type_tag;
	strncpy(x->type_name, type_name, MAX_NAME_LEN);
	strncpy(x->name, name, MAX_NAME_LEN);
	x->next = NULL;
	if (d == NULL)
		die("Fuck\n");
	if (d->fields == NULL)
		d->fields = x;
	if (d->last_field != NULL)
		d->last_field->next = x;
	d->last_field = x;
}

int test_comment(FILE *inp) {
	if (strcmp(consume_buf, "#/"))
		return 0;
	int ch;
	while ((ch = fgetc(inp)) != -1) {
		if (ch == '\n')
			break;
	}
	return 1;
}

int test_decl(FILE *inp) {
	if (strcmp(consume_buf, "decl-s"))
		return 0;
	consume(inp);
	if (!m_name(consume_buf)) {
		fprintf(stderr, "Bad name `%s'", consume_buf);
		exit(1);
	}
	decl_n++;
	if (decl_n > decl_cap) {
		size_t decl_cap_new = decl_cap * 2;
		if (decl_cap_new == 0)
			decl_cap_new = 1;
		decls = realloc(decls, decl_cap_new * sizeof(*decls));
		decl_cap = decl_cap_new;
		if (decls == NULL)
			die("Bad realloc\n");
	}
	strncpy(decls[decl_n - 1].name, consume_buf, MAX_NAME_LEN);
	decls[decl_n - 1].fields = NULL;
	decls[decl_n - 1].last_field = NULL;
	decls[decl_n - 1].empty = 0;
	decls[decl_n - 1].exter = 0;
	decls[decl_n - 1].exter_rem = 0;
	static char type_name[MAX_NAME_LEN];
	int type_tag = 0, name_w = 0;
	while (1) {
		consume(inp);
		if (!strcmp(":", consume_buf))
			return 1;
		if (test_comment(inp)) {
			;
		} else if (name_w) {
			if (!m_name(consume_buf)) {
				fprintf(stderr, "Bad name `%s'\n", consume_buf);
				exit(1);
			}
			add_decl_field(&decls[decl_n - 1], type_tag, type_name, consume_buf);
			name_w = 0;
			type_tag = 0;
		} else {
			if (!strcmp("'E", consume_buf)) {
				type_tag = ENUM_TAG;
			} else if (!strcmp("'T", consume_buf)) {
				;
			} else if (!strcmp("'empty", consume_buf)) {
				decls[decl_n - 1].empty = 1;
			} else if (!strcmp("'external", consume_buf)) {
				decls[decl_n - 1].exter = 1;
			} else if (!strcmp("'external-remover", consume_buf)) {
				decls[decl_n - 1].exter_rem = 1;
			} else if (!strcmp("ent-r", consume_buf)) {
				type_tag = ENT_R_TAG;
				type_name[0] = '\0';
				name_w = 1;
			} else if (!strcmp("ent-n", consume_buf)) {
				type_tag = ENT_N_TAG;
				type_name[0] = '\0';
				name_w = 1;
			} else if (m_name(consume_buf)) {
				strncpy(type_name, consume_buf, MAX_NAME_LEN);
				name_w = 1;
			} else {
				fprintf(stderr, "Unmatching word `%s'\n", consume_buf);
				exit(1);
			}
		}
	}
}

int parse_base(FILE *inp) {
	if (consume_end(inp))
		return 0;
	if (test_comment(inp))
		return 1;
	if (test_decl(inp))
		return 1;
	fprintf(stderr, "Unrecognised top-level word: `%s'\n", consume_buf);
	exit(1);
}

void put_struct(FILE *to, decl_t *d) {
	fprintf(to, "typedef struct effect_%s_data {\n", d->name);
	decl_field_t *p = d->fields;
	while (p != NULL) {
		const char *type_str;
		static char tmp[256];
		if (p->type_tag == ENT_R_TAG || p->type_tag == ENT_N_TAG) {
			type_str = "ent_ptr";
		} else if (p->type_tag == ENUM_TAG) {
			snprintf(tmp, 256, "enum %s", p->type_name);
			type_str = tmp;
		} else {
			type_str = p->type_name;
		}
		fprintf(
			to,
			"\t%s %s;\n",
			type_str,
			p->name
		);
		p = p->next;
	}
	fprintf(to, "} effect_%s_data;\n", d->name);
}

void put_loader(FILE *to, decl_t *d) {
	fprintf(
		to,
		"void effect_scan_%s(effect_s *e, int n_ent, entity_s **a_ent, FILE *stream) {\n"
		"\t(void)n_ent; (void)a_ent;\n"
		"\teffect_%s_data *d = (void*)e->data;\n",
		d->name,
		d->name
	);
	decl_field_t *p = d->fields;
	while (p != NULL) {
		if (p->type_tag == ENT_R_TAG || p->type_tag == ENT_N_TAG) {
			fprintf(to, "\t{ int t; fread(&t, sizeof(int), 1, stream); if (t == -1 || t >= n_ent) d->%s = ENT_NULL; else d->%s = ent_sptr(a_ent[t]); }\n", p->name, p->name);
		} else if (p->type_tag == ENUM_TAG) {
			fprintf(to, "\tfread(&d->%s, sizeof(int), 1, stream);\n", p->name);
		} else {
			fprintf(to, "\tfread(&d->%s, sizeof(%s), 1, stream);\n", p->name, p->type_name);
		}
		p = p->next;
	}
	fprintf(to, "}\n");
}

void put_dumper(FILE *to, decl_t *d) {
	fprintf(
		to,
		"void effect_dump_%s(effect_s *e, FILE *stream) {\n"
		"\teffect_%s_data *d = (void*)e->data;\n",
		d->name,
		d->name
	);
	decl_field_t *p = d->fields;
	while (p != NULL) {
		if (p->type_tag == ENT_R_TAG || p->type_tag == ENT_N_TAG) {
			fprintf(
				to,
				"\t{ int t; if (d->%s == ENT_NULL) {t = -1;}\n"
				"\telse if (ent_aptr(d->%s) != NULL) {t = entity_get_index(ent_aptr(d->%s));}\n"
				"\telse {fprintf(stderr, \"bad bad bad\\n\"); t = -1;}\n"
				"\tfwrite(&t, sizeof(int), 1, stream); }\n",
				p->name,
				p->name,
				p->name
			);
		} else if (p->type_tag == ENUM_TAG) {
			fprintf(to, "\tfwrite(&d->%s, sizeof(int), 1, stream);\n", p->name);
		} else {
			fprintf(to, "\tfwrite(&d->%s, sizeof(%s), 1, stream);\n", p->name, p->type_name);
		}
		p = p->next;
	}
	fprintf(to, "}\n");
}

int has_remover(decl_t *d) {
	if (d->exter_rem)
		return 1;
	decl_field_t *p = d->fields;
	while (p != NULL) {
		if (p->type_tag == ENT_R_TAG || p->type_tag == ENT_N_TAG)
			return 1;
		p = p->next;
	}
	return 0;
}

void put_remover(FILE *to, decl_t *d) {
	fprintf(
		to,
		"int effect_rem_%s(struct entity_s *s, effect_s *e) {\n"
		"\t(void)s; (void)e;\n"
		"\teffect_%s_data *d = (void*)e->data;\n",
		d->name, d->name
	);
	decl_field_t *p = d->fields;
	while (p != NULL) {
		if (p->type_tag == ENT_R_TAG) {
			fprintf(
				to,
				"\tif (d->%s == ENT_NULL || entity_has_effect(d->%s, EF_B_NONEXISTENT)) return 1;\n",
				p->name, p->name
			);
		} else if (p->type_tag == ENT_N_TAG) {
			fprintf(
				to,
				"\tif (d->%s != ENT_NULL && entity_has_effect(d->%s, EF_B_NONEXISTENT)) d->%s = ENT_NULL;\n",
				p->name, p->name, p->name
			);
		}
		p = p->next;
	}
	fprintf(to, "\treturn 0;\n}\n");
}

#define TABLE_EMPTY_NULL 1
#define TABLE_EMPTY_ZERO 2
#define TABLE_SKIP_NON_REM 4

void put_table(FILE *to, const char *type_name, const char *table_name, const char *data_pre, const char *data_after, unsigned mask) {
	size_t i, j;
	fprintf(to, "\n%s %s[] = {\n", type_name, table_name);
	for (i = 0; i < decl_n; i++) {
		fprintf(to, "\t[EF_");
		for (j = 0; decls[i].name[j]; j++) {
			fputc(toupper(decls[i].name[j]), to);
		}
		fprintf(to, "] = ");
		if ((mask & TABLE_EMPTY_NULL) && decls[i].empty) {
			fprintf(to, "NULL");
		} else if ((mask & TABLE_EMPTY_ZERO) && decls[i].empty) {
			fprintf(to, "0");
		} else if ((mask & TABLE_SKIP_NON_REM) && !has_remover(&decls[i])) {
			fprintf(to, "NULL");
		} else {
			fprintf(to, "%s", data_pre);
			for (j = 0; decls[i].name[j]; j++) {
				fputc(decls[i].name[j], to);
			}
			fprintf(to, "%s", data_after);
		}
		fprintf(to, ",\n");
	}
	fprintf(to, "};\n");
}

int main(int argc, char **argv) {
	const char *struct_name = NULL, *func_name = NULL, *inp_name = NULL;
	int i;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], ".s")) {
			if (struct_name != NULL)
				die("Duplicate .s\n");
			if (i + 1 >= argc)
				die("Expected filename after .s\n");
			struct_name = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], ".f")) {
			if (func_name != NULL)
				die("Duplicate .f\n");
			if (i + 1 >= argc)
				die("Expected filename after .f\n");
			func_name = argv[i + 1];
			i++;
		} else if (!strcmp(argv[i], ".inp")) {
			if (inp_name != NULL)
				die("Duplicate .inp\n");
			if (i + 1 >= argc)
				die("Expected filename after .inp\n");
			inp_name = argv[i + 1];
			i++;
		} else {
			fprintf(stderr, "Unknown parameter `%s'\n", argv[i]);
			return 1;
		}
	}
	FILE *struct_file = struct_name != NULL ? m_open(struct_name, "w") : NULL;
	FILE *func_file = func_name != NULL ? m_open(func_name, "w") : NULL;
	if (struct_file == NULL && func_file == NULL) {
		fprintf(stderr, "Zero output files specified, aborting\n");
		return 0;
	}
	FILE *inp = inp_name != NULL ? m_open(inp_name, "r") : stdin;
	while (parse_base(inp))
		;
	const char *autogen_warn = "/* This file is automatically generated with util/structgen */\n";
	if (struct_file != NULL)
		fprintf(struct_file, autogen_warn);
	if (func_file != NULL)
		fprintf(func_file, autogen_warn);
	{
		size_t i;
		for (i = 0; i < decl_n; i++) {
			/*
			 * A structure is generated. It shouldn't be empty.
			 * `exter' flag signifies that it's written somewhere else.
			 */
			if (struct_file != NULL && !decls[i].empty && !decls[i].exter) {
				printf("!! struct %s\n", decls[i].name);
				put_struct(struct_file, &decls[i]);
			}
			if (func_file != NULL) {
				/*
				 * Load/Dump functions are generated. They aren't if the data is empty.
				 * `exter' flag signifies they are written somewhere else.
				 */
				if (!decls[i].empty && !decls[i].exter) {
					printf("!! loader %s\n", decls[i].name);
					put_loader(func_file, &decls[i]);
					printf("!! dumper %s\n", decls[i].name);
					put_dumper(func_file, &decls[i]);
				}
				/*
				 * Remover is generated. It isn't if it would be empty.
				 * `exter_rem' flag signifies it's written somewhere else.
				 */
				if (!decls[i].exter_rem && has_remover(&decls[i])) {
					printf("!! remover %s\n", decls[i].name);
					put_remover(func_file, &decls[i]);
				}
			}
		}
	}
	if (struct_file != NULL) {
		fprintf(struct_file, "typedef enum effect_type {\n");
		size_t i, j;
		for (i = 0; i < decl_n; i++) {
			fprintf(struct_file, "\tEF_");
			for (j = 0; decls[i].name[j]; j++) {
				fputc(toupper(decls[i].name[j]), struct_file);
			}
			fprintf(struct_file, ",\n");
		}
		fprintf(struct_file, "\tEF_UNKNOWN = -1\n} effect_type;\n");
	}
	if (func_file != NULL) {
		put_table(func_file, "int", "effect_data_size", "sizeof(effect_", "_data)", TABLE_EMPTY_ZERO);
		put_table(func_file, "effect_dump_t", "effect_dump_functions", "effect_dump_", "", TABLE_EMPTY_NULL);
		put_table(func_file, "effect_scan_t", "effect_scan_functions", "effect_scan_", "", TABLE_EMPTY_NULL);
		put_table(func_file, "effect_rem_t", "effect_rem_functions", "effect_rem_", "", TABLE_SKIP_NON_REM);
	}
}
