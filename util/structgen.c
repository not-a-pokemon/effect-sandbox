#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define ENUM_BIT (1<<30)
#define TYPEDEF_BIT (1<<29)

_Bool m_is_white(char c) {
	return c == ' ' || c == '\t' || c == '\n';
}

_Bool m_isac(char c) {
	return
		(c >= 'a' && c <= 'z') ||
		(c >= 'A' && c <= 'Z') ||
		c == '_';
}

_Bool m_isan(char c) {
	return m_isac(c) || (c >= '0' && c <= '9');
}

_Bool m_is_c_word(char *c) {
	if (!m_isac(*c))
		return 0;
	for (int i = 1; c[i]; i++) {
		if (!m_isan(c[i]))
			return 0;
	}
	return 1;
}

enum m_get_token_err {
	m_get_token_ok = 0,
	m_get_token_overlength = -2,
	m_get_token_eof = -1,
};

int m_get_token(FILE *stream, char *buf, int buf_len) {
	int peek;
	do {
		peek = fgetc(stream);
	} while (m_is_white(peek) && peek != EOF);
	if (feof(stream)) {
		return m_get_token_eof;
	}
	int c = 0;
	do {
		buf[c++] = peek;
		peek = fgetc(stream);
		if (m_is_white(peek) || peek == EOF) break;
		if (c + 1 >= buf_len) return m_get_token_overlength;
	} while (1);
	buf[c] = '\0';
	return m_get_token_ok;
}

#define N_ENUMS 1024
static char *enum_tab[N_ENUMS];
static char *typedef_tab[N_ENUMS];
int tab_pull(char **tab, const char *s) {
	int i = 0;
	for (; i < N_ENUMS; i++) {
		if (tab[i] == NULL) {
			size_t l = strlen(s)+1;
			tab[i] = malloc(l);
			if (tab[i] == NULL)
				return -2;
			memcpy(tab[i], s, l);
			return i;
		} else if (!strcmp(tab[i], s)) {
			return i;
		}
	}
	return -1;
}

typedef struct variable_tag {
	char name[64];
	int type;
	int mod;
} variable_tag;

typedef struct struct_tag {
	char name[64];
	variable_tag *v;
	int n;
} struct_tag;

int put_struct(FILE *stream, struct_tag *s) {
	fprintf(stream, "typedef struct effect_%s_data {\n", s->name);
	for (int i = 0; i < s->n; i++) {
		const char *type_str;
		int space = 1;
		char tmp[256];
		if (s->v[i].type == 0) {
			if (s->v[i].mod == 0) {
				type_str = "int";
			} else if (s->v[i].mod & ENUM_BIT) {
				snprintf(tmp, 256, "enum %s", enum_tab[s->v[i].mod & ~ENUM_BIT]);
				type_str = tmp;
			} else if (s->v[i].mod & TYPEDEF_BIT) {
				snprintf(tmp, 256, "%s", typedef_tab[s->v[i].mod & ~TYPEDEF_BIT]);
				type_str = tmp;
			} else {
				fprintf(stream, "what the F?");
				goto T;
			}
		} else if (s->v[i].type == 1) {
			type_str = "struct entity_s *";
			space = 0;
		}
		fprintf(
			stream,
			"\t%s%s%s;\n",
			type_str, space ? " " : "",
			s->v[i].name
		);
T:
		;
	}
	fprintf(stream, "} effect_%s_data;\n", s->name);
	return 0;
}

int put_loader(FILE *stream, struct_tag *s) {
	fprintf(
		stream,
		"void effect_scan_%s(effect_s *e, int n_ent, entity_s **a_ent, int n_eff, effect_s **a_eff, FILE *stream) {\n"
		"\t(void)n_ent; (void)a_ent; (void)n_eff; (void)a_eff;\n"
		"\teffect_%s_data *d = (void*)e->data;\n",
		s->name,
		s->name
	);
	for (int i = 0; i < s->n; i++) {
		if (s->v[i].type == 0) {
			if (s->v[i].mod & TYPEDEF_BIT) {
				fprintf(stream, "\tfread(&d->%s, sizeof(%s), 1, stream);\n", s->v[i].name, typedef_tab[s->v[i].mod & ~TYPEDEF_BIT]);
			} else {
				fprintf(stream, "\tfread(&d->%s, sizeof(int), 1, stream);\n", s->v[i].name);
			}
		} else if (s->v[i].type == 1) {
			fprintf(stream, "\t{ int t; fread(&t, sizeof(int), 1, stream); if (t == -1 || t >= n_ent) d->%s = NULL; else d->%s = a_ent[t]; }\n", s->v[i].name, s->v[i].name);
		}
	}
	fprintf(stream, "}\n");
	return 0;
}

int put_dumper(FILE *stream, struct_tag *s) {
	fprintf(
		stream,
		"void effect_dump_%s(effect_s *e, FILE *stream) {\n"
		"\teffect_%s_data *d = (void*)e->data;\n",
		s->name,
		s->name
	);
	for (int i = 0; i < s->n; i++) {
		if (s->v[i].type == 0) {
			if (s->v[i].mod & TYPEDEF_BIT) {
				fprintf(stream, "\tfwrite(&d->%s, sizeof(%s), 1, stream);\n", s->v[i].name, typedef_tab[s->v[i].mod & ~TYPEDEF_BIT]);
			} else {
				fprintf(stream, "\tfwrite(&d->%s, sizeof(int), 1, stream);\n", s->v[i].name);
			}
		} else if (s->v[i].type == 1) {
			fprintf(stream, "\t{ int t; if (d->%s == NULL){t = -1;}else{t = entity_get_index(d->%s);} fwrite(&t, sizeof(int), 1, stream); }\n", s->v[i].name, s->v[i].name);
		}
	}
	fprintf(stream, "}\n");
	return 0;
}

int main(int argc, char **argv) {
	FILE *output_struct = NULL;
	FILE *output_func = NULL;
	FILE *inp = stdin;
	{
		int i = 1;
		for (; i < argc; i++) {
			if (!strcmp(argv[i], ".s")) {
				if (i == argc - 1) {
					fprintf(stderr, "expected filename\n");
					return 1;
				}
				output_struct = fopen(argv[i + 1], "w");
				if (output_struct == NULL) {
					perror("failed to open struct file");
					return 1;
				}
				i++;
			} else if (!strcmp(argv[i], ".f")) {
				if (i == argc - 1) {
					fprintf(stderr, "expected filename\n");
					return 1;
				}
				output_func = fopen(argv[i + 1], "w");
				if (output_func == NULL) {
					perror("failed to open function file");
					return 1;
				}
				i++;
			} else if (!strcmp(argv[i], ".inp")) {
				if (i == argc - 1) {
					fprintf(stderr, "expected filename\n");
					return 1;
				}
				inp = fopen(argv[i + 1], "r");
				if (inp == NULL) {
					perror("failed to open input file");
					return 1;
				}
				i++;
			}
		}
	}
	if (output_struct == NULL) {
		fprintf(stderr, "no struct file\n");
		return 1;
	}
	if (output_func == NULL) {
		fprintf(stderr, "no function file\n");
		return 1;
	}
	static char buf[1024];
	int get_r = 0;
	int bad = 0;
	int bad1 = 0;
	struct_tag st;
	variable_tag vt;
	const char *autogen_warn = "/* This file is automatically generated with util/structgen */\n";
	fprintf(output_struct, autogen_warn);
	fprintf(output_func, autogen_warn);
	while (!(get_r = m_get_token(inp, buf, 1024))) {
		if (!strcmp("#/", buf)) {
			int ch;
			while ((ch = getc(inp)) != -1)
				if (ch == '\n') break;
		} else if (!strcmp("decl-s", buf)) {
			if ((get_r = m_get_token(inp, st.name, 64))) {
				goto B;
			}
			if (!m_is_c_word(st.name)) {
				fprintf(stderr, "not c word: '%s'\n", st.name);
				bad1 = 2;
				goto B;
			}
			st.n = 0;
			st.v = NULL;
			vt.type = -1;
			while (!(get_r = m_get_token(inp, buf, 1024))) {
				if (!strcmp(":", buf)) {
					break;
				} else if (!strcmp("int", buf)) {
					if (vt.type == -1) {
						vt.type = 0;
						vt.mod = 0;
					} else {
						bad1 = 3;
						goto B;
					}
				} else if (!strcmp("ent-r", buf)) {
					if (vt.type == -1) {
						vt.type = 1;
						vt.mod = 0;
					} else {
						bad1 = 3;
						goto B;
					}
				} else if (!strcmp("ent-n", buf)) {
					if (vt.type == -1) {
						vt.type = 1;
						vt.mod = 1;
					} else {
						bad1 = 3;
						goto B;
					}
				} else if (!strcmp("'E", buf)) {
					if ((get_r = m_get_token(inp, buf, 1024))) {
						goto B;
					}
					int e = tab_pull(enum_tab, buf);
					if (e == -1) {
						bad1 = 6;
						goto B;
					} else if (e == -2) {
						bad1 = 7;
						goto B;
					} else if (e < 0) {
						fprintf(stderr, "error: unknown what?\n");
						goto B;
					}
					if (vt.type == -1) {
						vt.type = 0;
						vt.mod = ENUM_BIT | e;
					} else {
						bad1 = 3;
						goto B;
					}
				} else if (!strcmp("'T", buf)) {
					if ((get_r = m_get_token(inp, buf, 1024))) {
						goto B;
					}
					int e = tab_pull(typedef_tab, buf);
					if (e == -1) {
						bad1 = 6;
						goto B;
					} else if (e == -2) {
						bad1 = 7;
						goto B;
					} else if (e < 0) {
						fprintf(stderr, "error: unknown what?\n");
						goto B;
					}
					if (vt.type == -1) {
						vt.type = 0;
						vt.mod = TYPEDEF_BIT | e;
					} else {
						bad1 = 3;
						goto B;
					}
				} else if (m_is_c_word(buf)) {
					if (strlen(buf) >= 64) {
						bad1 = 5;
						goto B;
					}
					strcpy(vt.name, buf);
					if (!(st.n & 15)) {
						st.v = realloc(st.v, st.n + 16 * sizeof(variable_tag));
					}
					st.v[st.n++] = vt;
					vt.type = -1;
					} else {
						bad1 = 2;
					goto B;
				}
			}
			if (get_r) {
				bad = 1;
				goto B;
			}
			if (vt.type != -1) {
				bad = 4;
				goto B;
			}
			printf("!! struct %s\n", st.name);
			put_struct(output_struct, &st);
			printf("!! loader %s\n", st.name);
			put_loader(output_func, &st);
			printf("!! dumper %s\n", st.name);
			put_dumper(output_func, &st);
		} else {
			bad1 = 1;
			goto B;
		}
	}
B:
	if (bad1 == 1) {
		fprintf(stderr, "error: invalid token in root level\n");
	} else if (bad1 == 2) {
		fprintf(stderr, "error: expected a C word\n");
	} else if (bad1 == 3) {
		fprintf(stderr, "error: why two types?\n");
	} else if (bad1 == 4) {
		fprintf(stderr, "error: trailing type\n");
	} else if (bad1 == 5) {
		fprintf(stderr, "error: too long name\n");
	} else if (bad1 == 6) {
		fprintf(stderr, "error: enum limit\n");
	} else if (bad1 == 7) {
		fprintf(stderr, "errror: malloc\n");
	}
	if (get_r == m_get_token_overlength) {
		fprintf(stderr, "error: token too long\n");
	}
	if (bad && get_r == m_get_token_eof) {
		fprintf(stderr, "error: unexpected EOF\n");
	}
	return 0;
}
