#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_NAME_LEN 32
#define MAX_CONSUME 512
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

void m_trunc(char *dest, const char *src, size_t n) {
	size_t p;
	for (p = 0; src[p] && p + 1 < n; p++) {
		dest[p] = src[p];
	}
	dest[p] = '\0';
}

char consume_buf[MAX_CONSUME];

int consume_end(FILE *inp) {
	int peek;
	do {
		peek = fgetc(inp);
	} while (m_whitespace(peek) && peek != EOF);
	if (peek == EOF)
		return 1;
	int c = 0;
	while (1) {
		if (c >= MAX_CONSUME - 1) {
			consume_buf[MAX_CONSUME - 1] = '\0';
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

int consume_expr_end(FILE *inp) {
	int peek;
	do {
		peek = fgetc(inp);
	} while (m_whitespace(peek) && peek != EOF);
	if (peek == EOF)
		return 1;
	int c = 0, b = 0;
	while (1) {
		if (c >= MAX_CONSUME - 1) {
			consume_buf[MAX_CONSUME - 1] = '\0';
			fprintf(stderr, "Word too long: `%s'\n", consume_buf);
			exit(1);
		}
		consume_buf[c++] = peek;
		if (peek == '(')
			b++;
		else if (peek == ')')
			b--;
		peek = fgetc(inp);
		if (peek == EOF) {
			if (b) {
				fprintf(stderr, "Unmatched bracket, EOF\n");
				exit(1);
			} else {
				break;
			}
		}
		if (m_whitespace(peek) && !b)
			break;
	}
	consume_buf[c] = '\0';
	return 0;
}

void consume_expr(FILE *inp) {
	if (consume_expr_end(inp))
		die("Unexpected EOF\n");
}

#define EXTEND(A, CAP, N, BY) { \
		N += (BY); \
		while (CAP < N) { \
			CAP *= 2; \
			if (!CAP) CAP = 1; \
		} \
		A = realloc(A, sizeof(*A) * CAP); \
		if (A == NULL) die("Bad realloc\n"); \
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
	struct decl_field_t *fields;
	struct decl_field_t *last_field;
} decl_t;

decl_t *decls = NULL;
size_t decl_cap = 0, decl_n = 0;

typedef struct common_field_t {
	char name[MAX_NAME_LEN];
	char *value;
	struct common_field_t *next;
} common_field_t;

typedef struct common_effect_t {
	char name[MAX_NAME_LEN];
	unsigned writable:1;
	struct common_field_t *fields;
	struct common_field_t *last_field;
	struct common_effect_t *next;
} common_effect_t;

typedef struct common_t {
	char name[MAX_NAME_LEN];
	int common_size;
	struct common_effect_t *effects;
	struct common_effect_t *last_effect;
} common_t;

common_t *block_defs = NULL;
size_t block_defs_cap = 0, block_defs_n = 0;

common_t *common_defs = NULL;
size_t common_defs_cap = 0, common_defs_n = 0;

void add_decl_field(decl_t *d, int type_tag, const char *type_name, const char *name) {
	decl_field_t *x = m_malloc(sizeof(decl_field_t));
	x->type_tag = type_tag;
	m_trunc(x->type_name, type_name, MAX_NAME_LEN - 1);
	m_trunc(x->name, name, MAX_NAME_LEN - 1);
	x->next = NULL;
	if (d == NULL)
		die("Fuck\n");
	if (d->fields == NULL)
		d->fields = x;
	if (d->last_field != NULL)
		d->last_field->next = x;
	d->last_field = x;
}

void blockdef_add_field(common_t *d, const char *name, const char *value) {
	common_field_t *x = m_malloc(sizeof(common_field_t));
	m_trunc(x->name, name, MAX_NAME_LEN);
	x->value = strdup(value);
	x->next = NULL;
	if (d->last_effect == NULL)
		die("Impossible\n");
	if (value[0] == '<')
		d->last_effect->writable = 1;
	if (d->last_effect->fields == NULL)
		d->last_effect->fields = x;
	if (d->last_effect->last_field != NULL)
		d->last_effect->last_field->next = x;
	d->last_effect->last_field = x;
}

void blockdef_add_effect(common_t *d, const char *name) {
	common_effect_t *x = m_malloc(sizeof(common_effect_t));
	m_trunc(x->name, name, MAX_NAME_LEN);
	x->writable = 0;
	x->next = NULL;
	if (d->effects == NULL)
		d->effects = x;
	if (d->last_effect != NULL)
		d->last_effect->next = x;
	d->last_effect = x;
	d->last_effect->last_field = NULL;
	d->last_effect->fields = NULL;
}

int int_field(common_field_t *f, int *r) {
	static const char *pref = "<int:";
	int p;
	*r = 0;
	for (p = 0; pref[p]; p++) {
		if (f->value[p] != pref[p])
			return 0;
	}
	while (f->value[p] >= '0' && f->value[p] <= '9') {
		*r = 10 * (*r) + f->value[p] - '0';
		p++;
	}
	if (f->value[p++] != '>')
		return 0;
	if (f->value[p++] != '\0')
		return 0;
	return 1;
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
	EXTEND(decls, decl_cap, decl_n, 1);
	m_trunc(decls[decl_n - 1].name, consume_buf, MAX_NAME_LEN);
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
				m_trunc(type_name, consume_buf, MAX_NAME_LEN);
				name_w = 1;
			} else {
				fprintf(stderr, "Unmatching word `%s'\n", consume_buf);
				exit(1);
			}
		}
	}
}

int test_decl_block(FILE *inp) {
	if (strcmp(consume_buf, "decl-block"))
		return 0;
	consume(inp);
	if (!m_name(consume_buf)) {
		fprintf(stderr, "Bad name `%s'\n", consume_buf);
		exit(1);
	}
	EXTEND(block_defs, block_defs_cap, block_defs_n, 1);
	m_trunc(block_defs[block_defs_n - 1].name, consume_buf, MAX_NAME_LEN);
	// blockdefs have a fixed size of 4
	block_defs[block_defs_n - 1].common_size = -1;
	block_defs[block_defs_n - 1].effects = NULL;
	block_defs[block_defs_n - 1].last_effect = NULL;
	int effect_w = 1, field_w = 0;
	char effect_name[MAX_NAME_LEN], field_name[MAX_NAME_LEN];
	while (1) {
		if (!effect_w && !field_w) {
			consume_expr(inp);
		} else {
			consume(inp);
		}
		if (test_comment(inp)) {
			;
		} else if (!effect_w && !field_w) {
			blockdef_add_field(&block_defs[block_defs_n - 1], field_name, consume_buf);
			field_w = 1;
		} else if (!effect_w && field_w && !strcmp(".", consume_buf)) {
			field_w = 0;
			effect_w = 1;
		} else if (!effect_w && field_w && !strcmp("<coordinates>", consume_buf)) {
			blockdef_add_field(&block_defs[block_defs_n - 1], "x", "<x>");
			blockdef_add_field(&block_defs[block_defs_n - 1], "y", "<y>");
			blockdef_add_field(&block_defs[block_defs_n - 1], "z", "<z>");
		} else if (!effect_w && field_w && m_name(consume_buf)) {
			m_trunc(field_name, consume_buf, MAX_NAME_LEN);
			field_w = 0;
		} else if (effect_w && !strcmp(":", consume_buf)) {
			return 1;
		} else if (effect_w && m_name(consume_buf)) {
			// TODO check for effect name
			m_trunc(effect_name, consume_buf, MAX_NAME_LEN);
			blockdef_add_effect(&block_defs[block_defs_n - 1], effect_name);
			effect_w = 0;
			field_w = 1;
		} else {
			fprintf(stderr, "Unmatching word `%s'\n", consume_buf);
			exit(1);
		}
	}
}

int test_decl_common(FILE *inp) {
	if (strcmp(consume_buf, "decl-common"))
		return 0;
	consume(inp);
	if (!m_name(consume_buf)) {
		fprintf(stderr, "Bad name `%s'\n", consume_buf);
		exit(1);
	}
	EXTEND(common_defs, common_defs_cap, common_defs_n, 1);
	m_trunc(common_defs[common_defs_n - 1].name, consume_buf, MAX_NAME_LEN);
	common_defs[block_defs_n - 1].common_size = 0;
	common_defs[common_defs_n - 1].effects = NULL;
	common_defs[common_defs_n - 1].last_effect = NULL;
	int effect_w = 1, field_w = 0;
	char effect_name[MAX_NAME_LEN], field_name[MAX_NAME_LEN];
	while (1) {
		if (!effect_w && !field_w) {
			consume_expr(inp);
		} else {
			consume(inp);
		}
		if (test_comment(inp)) {
			;
		} else if (!effect_w && !field_w) {
			blockdef_add_field(&common_defs[common_defs_n - 1], field_name, consume_buf);
			int p;
			if (int_field(common_defs[common_defs_n - 1].last_effect->last_field, &p)) {
				if (p + 1 > common_defs[common_defs_n - 1].common_size)
					common_defs[common_defs_n - 1].common_size = p + 1;
			}
			field_w = 1;
		} else if (!effect_w && field_w && !strcmp(".", consume_buf)) {
			field_w = 0;
			effect_w = 1;
		} else if (!effect_w && field_w && m_name(consume_buf)) {
			m_trunc(field_name, consume_buf, MAX_NAME_LEN);
			field_w = 0;
		} else if (effect_w && !strcmp(":", consume_buf)) {
			return 1;
		} else if (effect_w && m_name(consume_buf)) {
			// TODO check for effect name
			m_trunc(effect_name, consume_buf, MAX_NAME_LEN);
			blockdef_add_effect(&common_defs[common_defs_n - 1], effect_name);
			effect_w = 0;
			field_w = 1;
		} else {
			fprintf(stderr, "Unmatching word `%s'\n", consume_buf);
			exit(1);
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
	if (test_decl_block(inp))
		return 1;
	if (test_decl_common(inp))
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
		"void effect_scan_%s(effect_s *e, int n_ent, entity_s **a_ent, int n_sec, sector_s **a_sec, FILE *stream) {\n"
		"\t(void)n_ent; (void)a_ent; (void)n_sec; (void)a_sec;\n"
		"\teffect_%s_data *d = (void*)e->data;\n",
		d->name,
		d->name
	);
	decl_field_t *p = d->fields;
	while (p != NULL) {
		if (p->type_tag == ENT_R_TAG || p->type_tag == ENT_N_TAG) {
			fprintf(
				to,
				"\t{ int t; fread(&t, sizeof(int), 1, stream); if (t != -1 && t < n_ent) {d->%s = ent_sptr(a_ent[t]);}\n"
				"\telse if (t != -1 && (t & STORED_CPTR_BIT)) { unsigned sec_nr = (t ^ STORED_CPTR_BIT) >> 9, co = t & 0x1FF;\n"
				"\t\tif (sec_nr <= (unsigned)n_sec) d->%s = ent_cptr(a_sec[sec_nr], co >> 6, (co >> 3) & 7, co & 7);\n"
				"\t\telse d->%s = ENT_NULL; }\n"
				"\telse {d->%s = ENT_NULL;} }\n",
				p->name,
				p->name,
				p->name,
				p->name
			);
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
				"\telse { sector_s *sec; int x, y, z; if ((sec = ent_acptr(d->%s, &x, &y, &z)) != NULL) {t = STORED_CPTR_BIT | (sec->stored_id << 9) | (x << 6) | (y << 3) | z;}\n"
				"\telse {fprintf(stderr, \"bad bad bad\\n\"); t = -1;} }\n"
				"\tfwrite(&t, sizeof(int), 1, stream); }\n",
				p->name,
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
			fprintf(to, "%s%s%s", data_pre, decls[i].name, data_after);
		}
		fprintf(to, ",\n");
	}
	fprintf(to, "};\n");
}

int main(int argc, char **argv) {
	const char *struct_name = NULL, *func_name = NULL, *inp_name = NULL;
	int log_functions = 0, log_structs = 0;
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
		} else if (!strcmp(argv[i], ".log-f")) {
			log_functions = 1;
		} else if (!strcmp(argv[i], ".log-s")) {
			log_structs = 1;
		} else {
			fprintf(stderr, "Unknown parameter `%s'\n", argv[i]);
			return 1;
		}
	}
	if (struct_name == NULL && func_name == NULL) {
		fprintf(stderr, "Zero output files specified, aborting\n");
		return 0;
	}
	FILE *inp = inp_name != NULL ? m_open(inp_name, "r") : stdin;
	while (parse_base(inp))
		;
	FILE *struct_file = struct_name != NULL ? m_open(struct_name, "w") : NULL;
	FILE *func_file = func_name != NULL ? m_open(func_name, "w") : NULL;
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
				if (log_structs)
					printf("!! struct %s\n", decls[i].name);
				put_struct(struct_file, &decls[i]);
			}
			if (func_file != NULL) {
				/*
				 * Load/Dump functions are generated. They aren't if the data is empty.
				 * `exter' flag signifies they are written somewhere else.
				 */
				if (!decls[i].empty && !decls[i].exter) {
					if (log_functions)
						printf("!! loader %s\n", decls[i].name);
					put_loader(func_file, &decls[i]);
					if (log_functions)
						printf("!! dumper %s\n", decls[i].name);
					put_dumper(func_file, &decls[i]);
				}
				/*
				 * Remover is generated. It isn't if it would be empty.
				 * `exter_rem' flag signifies it's written somewhere else.
				 */
				if (!decls[i].exter_rem && has_remover(&decls[i])) {
					if (log_functions)
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
	if (struct_file != NULL) {
		size_t i, j;
		fprintf(struct_file, "typedef enum common_type_t {\n\tCT_NONE = 0,\n");
		for (i = 0; i < block_defs_n; i++) {
			fprintf(struct_file, "\tCT_B_");
			for (j = 0; block_defs[i].name[j]; j++)
				fputc(toupper(block_defs[i].name[j]), struct_file);
			fprintf(struct_file, ",\n");
		}
		for (i = 0; i < common_defs_n; i++) {
			fprintf(struct_file, "\tCT_");
			for (j = 0; common_defs[i].name[j]; j++)
				fputc(toupper(common_defs[i].name[j]), struct_file);
			fprintf(struct_file, ",\n");
		}
		fprintf(struct_file, "} common_type_t;\n");
		fprintf(struct_file, "typedef enum block_type {\nBLK_EMPTY = 0,\n");
		for (i = 0; i < block_defs_n; i++) {
			fprintf(struct_file, "\tBLK_");
			for (j = 0; block_defs[i].name[j]; j++)
				fputc(toupper(block_defs[i].name[j]), struct_file);
			fprintf(struct_file, ",\n");
		}
		fprintf(struct_file, "} block_type;\n");
	}
	if (func_file != NULL) {
		put_table(func_file, "int", "effect_data_size", "sizeof(effect_", "_data)", TABLE_EMPTY_ZERO);
		put_table(func_file, "effect_dump_t", "effect_dump_functions", "effect_dump_", "", TABLE_EMPTY_NULL);
		put_table(func_file, "effect_scan_t", "effect_scan_functions", "effect_scan_", "", TABLE_EMPTY_NULL);
		put_table(func_file, "effect_rem_t", "effect_rem_functions", "effect_rem_", "", TABLE_SKIP_NON_REM);
	}
	if (func_file != NULL) {
		size_t i, j;
		fprintf(
			func_file,
			"int entity_block_load_effect(sector_s *sec, int x, int y, int z, effect_type t, void *d) {\n"
			"\tblock_s blk = sec->block_blocks[x][y][z];\n\tswitch (blk.type) {\n"
		);
		for (i = 0; i < block_defs_n; i++) {
			fprintf(func_file, "\tcase BLK_");
			for (j = 0; block_defs[i].name[j]; j++)
				fputc(toupper(block_defs[i].name[j]), func_file);
			fprintf(
				func_file,
				": {\n"
				"\t\tswitch (t) {\n"
			);
			for (common_effect_t *ef = block_defs[i].effects; ef != NULL; ef = ef->next) {
				fprintf(func_file, "\t\tcase EF_");
				for (j = 0; ef->name[j]; j++)
					fputc(toupper(ef->name[j]), func_file);
				fprintf(
					func_file,
					": {\n"
					"\t\t\teffect_%s_data *rd = d;\n",
					ef->name
				);
				for (common_field_t *f = ef->fields; f != NULL; f = f->next) {
					if (!strcmp("<durability>", f->value)) {
						fprintf(func_file, "\t\t\trd->%s = blk.dur;\n", f->name);
					} else if (!strcmp("<x>", f->value)) {
						fprintf(func_file, "\t\t\trd->%s = x + sec->x * G_SECTOR_SIZE;\n", f->name);
					} else if (!strcmp("<y>", f->value)) {
						fprintf(func_file, "\t\t\trd->%s = y + sec->y * G_SECTOR_SIZE;\n", f->name);
					} else if (!strcmp("<z>", f->value)) {
						fprintf(func_file, "\t\t\trd->%s = z + sec->z * G_SECTOR_SIZE;\n", f->name);
					} else {
						fprintf(func_file, "\t\t\trd->%s = %s;\n", f->name, f->value);
					}
				}
				fprintf(func_file, "\t\t} return 1;\n");
			}
			fprintf(
				func_file,
				"\t\tdefault: return 0;\n"
				"\t\t}\n"
				"\t} break;\n"
			);
		}
		fprintf(
			func_file,
			"\tdefault: return 0;\n"
			"\t}\n"
			"}\n"
		);
		fprintf(
			func_file,
			"int entity_block_has_effect(sector_s *sec, int x, int y, int z, effect_type t) {\n"
			"\tblock_s blk = sec->block_blocks[x][y][z];\n"
			"\tswitch (blk.type) {\n"
		);
		for (i = 0; i < block_defs_n; i++) {
			fprintf(func_file, "\tcase BLK_");
			for (j = 0; block_defs[i].name[j]; j++)
				fputc(toupper(block_defs[i].name[j]), func_file);
			fprintf(
				func_file,
				": {\n"
				"\tswitch (t) {\n"
			);
			for (common_effect_t *ef = block_defs[i].effects; ef != NULL; ef = ef->next) {
				fprintf(func_file, "\t\tcase EF_");
				for (j = 0; ef->name[j]; j++)
					fputc(toupper(ef->name[j]), func_file);
				fprintf(
					func_file,
					":\n"
				);
			}
			fprintf(
				func_file,
				"\t\t\treturn 1;\n"
				"\t\tdefault: return 0;\n"
				"\t\t}\n"
				"\t} break;\n"
			);
		}
		fprintf(func_file, "\tdefault: return 0;\n\t}\n}\n");
		fprintf(
			func_file,
			"int entity_common_has_effect(entity_s *s, effect_type t) {\n"
			"\tswitch (s->common_type) {\n"
		);
		for (i = 0; i < block_defs_n; i++) {
			fprintf(func_file, "\tcase CT_B_");
			for (j = 0; block_defs[i].name[j]; j++)
				fputc(toupper(block_defs[i].name[j]), func_file);
			fprintf(
				func_file,
				": {\n"
				"\tswitch (t) {\n"
			);
			for (common_effect_t *ef = block_defs[i].effects; ef != NULL; ef = ef->next) {
				fprintf(func_file, "\t\tcase EF_");
				for (j = 0; ef->name[j]; j++)
					fputc(toupper(ef->name[j]), func_file);
				fprintf(func_file, ":\n");
			}
			fprintf(
				func_file,
				"\t\t\treturn 1;\n"
				"\t\tdefault: return 0;\n"
				"\t\t}\n"
				"\t} break;\n"
			);
		}
		for (i = 0; i < common_defs_n; i++) {
			fprintf(func_file, "\tcase CT_");
			for (j = 0; common_defs[i].name[j]; j++)
				fputc(toupper(common_defs[i].name[j]), func_file);
			fprintf(
				func_file,
				": {\n"
				"\tswitch (t) {\n"
			);
			for (common_effect_t *ef = common_defs[i].effects; ef != NULL; ef = ef->next) {
				fprintf(func_file, "\t\tcase EF_");
				for (j = 0; ef->name[j]; j++)
					fputc(toupper(ef->name[j]), func_file);
				fprintf(func_file, ":\n");
			}
			fprintf(
				func_file,
				"\t\t\treturn 1;\n"
				"\t\tdefault: return 0;\n"
				"\t\t}\n"
				"\t} break;\n"
			);
		}
		fprintf(func_file, "\tdefault: return 0;\n\t}\n}\n");
		fprintf(
			func_file,
			"int entity_common_load_effect(entity_s *s, effect_type t, void *d) {\n"
			"\tswitch (s->common_type) {\n"
		);
		for (i = 0; i < block_defs_n; i++) {
			fprintf(func_file, "\tcase CT_B_");
			for (j = 0; block_defs[i].name[j]; j++)
				fputc(toupper(block_defs[i].name[j]), func_file);
			fprintf(func_file, ": {\n\t\tswitch (t) {\n");
			for (common_effect_t *ef = block_defs[i].effects; ef != NULL; ef = ef->next) {
				fprintf(func_file, "\t\tcase EF_");
				for (j = 0; ef->name[j]; j++)
					fputc(toupper(ef->name[j]), func_file);
				fprintf(
					func_file,
					": {\n"
					"\t\t\teffect_%s_data *rd = d;\n",
					ef->name
				);
				for (common_field_t *f = ef->fields; f != NULL; f = f->next) {
					if (!strcmp("<durability>", f->value)) {
						fprintf(func_file, "\t\t\trd->%s = ((int*)s->common_data)[3];\n", f->name);
					} else if (!strcmp("<x>", f->value)) {
						fprintf(func_file, "\t\t\trd->%s = ((int*)s->common_data)[0];\n", f->name);
					} else if (!strcmp("<y>", f->value)) {
						fprintf(func_file, "\t\t\trd->%s = ((int*)s->common_data)[1];\n", f->name);
					} else if (!strcmp("<z>", f->value)) {
						fprintf(func_file, "\t\t\trd->%s = ((int*)s->common_data)[2];\n", f->name);
					} else {
						fprintf(func_file, "\t\t\trd->%s = %s;\n", f->name, f->value);
					}
				}
				fprintf(func_file, "\t\t} return 1;\n");
			}
			fprintf(func_file, "\t\tdefault: return 0;\n\t\t}\n\t} break;\n");
		}
		for (i = 0; i < common_defs_n; i++) {
			fprintf(func_file, "\tcase CT_");
			for (j = 0; common_defs[i].name[j]; j++)
				fputc(toupper(common_defs[i].name[j]), func_file);
			fprintf(func_file, ": {\n\t\tswitch (t) {\n");
			for (common_effect_t *ef = common_defs[i].effects; ef != NULL; ef = ef->next) {
				fprintf(func_file, "\t\tcase EF_");
				for (j = 0; ef->name[j]; j++)
					fputc(toupper(ef->name[j]), func_file);
				fprintf(
					func_file,
					": {\n"
					"\t\t\teffect_%s_data *rd = d;\n",
					ef->name
				);
				for (common_field_t *f = ef->fields; f != NULL; f = f->next) {
					int p;
					if (int_field(f, &p)) {
						fprintf(func_file, "\t\t\trd->%s = ((int*)s->common_data)[%d];\n", f->name, p);
					} else {
						fprintf(func_file, "\t\t\trd->%s = %s;\n", f->name, f->value);
					}
				}
				fprintf(func_file, "\t\t} return 1;\n");
			}
			fprintf(func_file, "\t\tdefault: return 0;\n\t\t}\n\t} break;\n");
		}
		fprintf(
			func_file,
			"\tdefault: return 0;\n"
			"\t}\n}\n"
		);
		fprintf(
			func_file,
			"int entity_common_store_effect(entity_s *s, effect_type t, void *d) {\n"
			"\tswitch (s->common_type) {\n"
		);
		for (i = 0; i < block_defs_n; i++) {
			fprintf(func_file, "\tcase CT_B_");
			for (j = 0; block_defs[i].name[j]; j++)
				fputc(toupper(block_defs[i].name[j]), func_file);
			fprintf(func_file, ": {\n\t\tswitch (t) {\n");
			for (common_effect_t *ef = block_defs[i].effects; ef != NULL; ef = ef->next) {
				if (!ef->writable)
					continue;
				fprintf(func_file, "\t\tcase EF_");
				for (j = 0; ef->name[j]; j++)
					fputc(toupper(ef->name[j]), func_file);
				fprintf(func_file, ": {\n\t\t\teffect_%s_data *rd = d;\n", ef->name);
				for (common_field_t *f = ef->fields; f != NULL; f = f->next) {
					if (!strcmp("<durability>", f->value)) {
						fprintf(func_file, "\t\t\t((int*)s->common_data)[3] = rd->%s;\n", f->name);
					} else if (!strcmp("<x>", f->value)) {
						fprintf(func_file, "\t\t\t((int*)s->common_data)[0] = rd->%s;\n", f->name);
					} else if (!strcmp("<y>", f->value)) {
						fprintf(func_file, "\t\t\t((int*)s->common_data)[1] = rd->%s;\n", f->name);
					} else if (!strcmp("<z>", f->value)) {
						fprintf(func_file, "\t\t\t((int*)s->common_data)[2] = rd->%s;\n", f->name);
					}
				}
				fprintf(func_file, "\t\t} return 1;\n");
			}
			fprintf(func_file, "\t\tdefault: return 0;\n\t\t}\n\t}\n");
		}
		for (i = 0; i < common_defs_n; i++) {
			fprintf(func_file, "\tcase CT_");
			for (j = 0; common_defs[i].name[j]; j++)
				fputc(toupper(common_defs[i].name[j]), func_file);
			fprintf(func_file, ": {\n\t\tswitch (t) {\n");
			for (common_effect_t *ef = common_defs[i].effects; ef != NULL; ef = ef->next) {
				if (!ef->writable)
					continue;
				fprintf(func_file, "\t\tcase EF_");
				for (j = 0; ef->name[j]; j++)
					fputc(toupper(ef->name[j]), func_file);
				fprintf(func_file, ": {\n\t\t\teffect_%s_data *rd = d;\n", ef->name);
				for (common_field_t *f = ef->fields; f != NULL; f = f->next) {
					int p;
					if (int_field(f, &p)) {
						fprintf(func_file, "\t\t\t((int*)s->common_data)[%d] = rd->%s;\n", p, f->name);
					}
				}
				fprintf(func_file, "\t\t} return 1;\n");
			}
			fprintf(func_file, "\t\tdefault: return 0;\n\t\t}\n\t}\n");
		}
		fprintf(func_file, "\tdefault: return 0;\n\t}\n}\n");
		fprintf(func_file, "\nint common_type_size[] = {\n\t[CT_NONE] = 0,\n");
		for (i = 0; i < block_defs_n; i++) {
			fprintf(func_file, "\t[CT_B_");
			for (j = 0; block_defs[i].name[j]; j++)
				fputc(toupper(block_defs[i].name[j]), func_file);
			fprintf(func_file, "] = 4 * sizeof(int),\n");
		}
		for (i = 0; i < common_defs_n; i++) {
			fprintf(func_file, "\t[CT_");
			for (j = 0; common_defs[i].name[j]; j++)
				fputc(toupper(common_defs[i].name[j]), func_file);
			fprintf(func_file, "] = %d * sizeof(int),\n", common_defs[i].common_size);
		}
		fprintf(func_file, "};\n");
	}
}
