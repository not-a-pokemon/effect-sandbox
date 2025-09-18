#include "omalloc.h"
#include <stdio.h>

#define O_BUF_S 1024

/* Initially all NULL */
static void *o_buf[O_BUF_S];
static size_t o_buf_cur = 0;
static void *l_buf[O_BUF_S];
static size_t l_buf_cur = 0;

void *o_malloc(size_t s) {
	void *t = s > 0 ? malloc(s) : NULL;
	for (size_t i = 0; i < O_BUF_S; i ++) {
		if (o_buf[i] == t) {
			o_buf[i] = NULL;
		}
	}
	l_buf[l_buf_cur] = t;
	l_buf_cur ++;
	if (l_buf_cur == O_BUF_S) {
		l_buf_cur = 0;
	}
#if OMALLOC_LOG
	fprintf(stderr, "[LOG] o_malloc() -> %p\n", t);
	fflush(stderr);
#endif
	return t;
}

void *o_malloc_m(size_t s, const char *msg) {
	void *t = s > 0 ? malloc(s) : NULL;
	for (size_t i = 0; i < O_BUF_S; i ++) {
		if (o_buf[i] == t) {
			o_buf[i] = NULL;
		}
	}
	l_buf[l_buf_cur] = t;
	l_buf_cur ++;
	if (l_buf_cur == O_BUF_S) {
		l_buf_cur = 0;
	}
#if OMALLOC_LOG
	fprintf(stderr, "[LOG] o_malloc() -> %p %s\n", t, msg);
	fflush(stderr);
#else
	(void)msg;
#endif
	return t;
}


void o_free(void *x) {
	if (x == NULL)
		return;
	int found = 0;
	for (size_t i = 0; i < O_BUF_S; i ++) {
		if (l_buf[i] == x) {
			l_buf[i] = NULL;
			found = 1;
		}
		if (o_buf[i] == x) {
			fprintf(stderr, "[ERR] o_free(%p) failed: double free\n", x);
			fflush(stderr);
			/* Crash badly? */
			return;
		}
	}
	if (!found) {
		fprintf(stderr, "[WARN] previously unallocated? %p\n", x);
		fflush(stderr);
	}
#if OMALLOC_LOG
	fprintf(stderr, "[LOG] o_free(%p)\n", x);
	fflush(stderr);
#endif
	o_buf[o_buf_cur] = x;
	o_buf_cur ++;
	if (o_buf_cur == O_BUF_S) {
		o_buf_cur = 0;
	}
	free(x);
#if OMALLOC_LOG
	fprintf(stderr, "[END malloc]\n");
#endif
}

void o_free_m(void *x, const char *s) {
	if (x == NULL)
		return;
	int found = 0;
	for (size_t i = 0; i < O_BUF_S; i ++) {
		if (l_buf[i] == x) {
			l_buf[i] = NULL;
			found = 1;
		}
		if (o_buf[i] == x) {
			fprintf(stderr, "[ERR] o_free(%p) failed: double free %s\n", x, s);
			fflush(stderr);
			/* Crash badly? */
			return;
		}
	}
	if (!found) {
		fprintf(stderr, "[WARN] previously unallocated? %p %s\n", x, s);
		fflush(stderr);
	}
#if OMALLOC_LOG
	fprintf(stderr, "[LOG] o_free(%p) %s\n", x, s);
	fflush(stderr);
#endif
	o_buf[o_buf_cur] = x;
	o_buf_cur ++;
	if (o_buf_cur == O_BUF_S) {
		o_buf_cur = 0;
	}
	free(x);
#if OMALLOC_LOG
	fprintf(stderr, "[END malloc]\n");
#endif
}
