#ifndef _efsa_OMALLOC_H
#define _efsa_OMALLOC_H

#include <stdlib.h>

void *o_malloc(size_t s);
void *o_malloc_m(size_t, char *);
void o_free(void *);
void o_free_m(void *, char *);

#endif
