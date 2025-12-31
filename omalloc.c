#include "omalloc.h"
#include "entity.h"
#include <stdio.h>

#define O_BUF_S         8192
#define O_ENTITY_NR     8192
#define O_EFF_NR        8192
#define O_EFF_ZERO_NR   8192
#define O_EFF_SMALL_NR  8192
#define O_EFF_MEDIUM_NR 8192
#define O_SECTOR_NR     8192

typedef struct {
	char d[sizeof(effect_s)];
} placeholder_zero;

typedef struct {
	char d[sizeof(effect_s) + 16];
} placeholder_small;

typedef struct {
	char d[sizeof(effect_s) + 32];
} placeholder_medium;

static entity_s *ent_buf;
static int *ent_buf_queue;
int ent_free_nr;

static placeholder_zero *eff_buf_zero;
static int *eff_queue_zero;
static placeholder_small *eff_buf_small;
static int *eff_queue_small;
static placeholder_medium *eff_buf_medium;
static int *eff_queue_medium;
int eff_zero_nr, eff_small_nr, eff_medium_nr;

static sector_s *sect_buf;
static int *sect_queue;
int sect_nr;

static void *o_buf[O_BUF_S];
static size_t o_buf_cur = 0;
static void *l_buf[O_BUF_S];
static size_t l_buf_cur = 0;

void o_init_allocator(void) {
	int i;
	ent_buf = malloc(sizeof(entity_s) * O_ENTITY_NR);
	ent_buf_queue = malloc(sizeof(int) * O_ENTITY_NR);
	eff_buf_zero = malloc(sizeof(placeholder_zero) * O_EFF_ZERO_NR);
	eff_queue_zero = malloc(sizeof(int) * O_EFF_ZERO_NR);
	eff_buf_small = malloc(sizeof(placeholder_small) * O_EFF_SMALL_NR);
	eff_queue_small = malloc(sizeof(int) * O_EFF_SMALL_NR);
	eff_buf_medium = malloc(sizeof(placeholder_medium) * O_EFF_MEDIUM_NR);
	eff_queue_medium = malloc(sizeof(int) * O_EFF_MEDIUM_NR);
	sect_buf = malloc(sizeof(sector_s) * O_SECTOR_NR);
	sect_queue = malloc(sizeof(int) * O_SECTOR_NR);
	for (i = 0; i < O_ENTITY_NR; i++) {
		ent_buf_queue[i] = i;
	}
	ent_free_nr = O_ENTITY_NR;
	for (i = 0; i < O_EFF_ZERO_NR; i++) {
		eff_queue_zero[i] = i;
	}
	eff_zero_nr = O_EFF_ZERO_NR;
	for (i = 0; i < O_EFF_SMALL_NR; i++) {
		eff_queue_small[i] = i;
	}
	eff_small_nr = O_EFF_SMALL_NR;
	for (i = 0; i < O_EFF_MEDIUM_NR; i++) {
		eff_queue_medium[i] = i;
	}
	eff_medium_nr = O_EFF_MEDIUM_NR;
	for (i = 0; i < O_SECTOR_NR; i++) {
		sect_queue[i] = i;
	}
	sect_nr = O_SECTOR_NR;
}

entity_s* o_alloc_entity(void) {
	if (ent_free_nr == 0) {
		fprintf(stderr, "No free entitites\n");
		return NULL;
	}
	ent_free_nr--;
	entity_s *t = ent_buf + ent_buf_queue[ent_free_nr];
	ent_buf_queue[ent_free_nr] = -1;
	t->effects = NULL;
	t->common_type = CT_NONE;
	return t;
}

void o_free_entity(entity_s *s) {
	if (s < ent_buf || s > ent_buf + O_ENTITY_NR - 1) {
		fprintf(stderr, "Attempt to free a garbage pointer (entity) %p\n", s);
		return;
	}
	ent_buf_queue[ent_free_nr] = s - ent_buf;
	ent_free_nr++;
}

effect_s* o_alloc_effect(o_effect_size t) {
	switch (t) {
	case O_EFS_ZERO: {
		if (eff_zero_nr == 0) {
			fprintf(stderr, "No free zero sections\n");
			return NULL;
		}
		eff_zero_nr--;
		effect_s *t = (void*)(eff_buf_zero + eff_queue_zero[eff_zero_nr]);
		eff_queue_zero[eff_zero_nr] = -1;
		return t;
	} break;
	case O_EFS_SMALL: {
		if (eff_small_nr == 0) {
			fprintf(stderr, "No free small sections\n");
			return NULL;
		}
		eff_small_nr--;
		effect_s *t = (void*)(eff_buf_small + eff_queue_small[eff_small_nr]);
		eff_queue_small[eff_small_nr] = -1;
		return t;
	} break;
	case O_EFS_MEDIUM: {
		if (eff_medium_nr == 0) {
			fprintf(stderr, "No free medium sections\n");
			return NULL;
		}
		eff_medium_nr--;
		effect_s *t = (void*)(eff_buf_medium + eff_queue_medium[eff_medium_nr]);
		eff_queue_medium[eff_medium_nr] = -1;
		return t;
	} break;
	}
	return NULL;
}

effect_s* o_alloc_effect_i(size_t t) {
	o_effect_size s;
	if (t == 0) {
		s = O_EFS_ZERO;
	} else if (t <= 16) {
		s = O_EFS_SMALL;
	} else if (t <= 32) {
		s = O_EFS_MEDIUM;
	} else {
		fprintf(stderr, "Too large effect data size: %zu\n", t);
		return NULL;
	}
	return o_alloc_effect(s);
}

void o_free_effect(effect_s *e, o_effect_size t) {
#ifdef O_FREE_LOG
	if ((void*)e >= (void*)eff_buf_zero && (void*)e < (void*)(eff_buf_zero + O_EFF_ZERO_NR)) {
		fprintf(stderr, "Pointer in section zero at position %zu\n", (placeholder_zero*)e - eff_buf_zero);
	}
	if ((void*)e >= (void*)eff_buf_small && (void*)e < (void*)(eff_buf_small + O_EFF_SMALL_NR)) {
		fprintf(stderr, "Pointer in section small at position %zu\n", (placeholder_small*)e - eff_buf_small);
	}
	if ((void*)e >= (void*)eff_buf_medium && (void*)e < (void*)(eff_buf_medium + O_EFF_MEDIUM_NR)) {
		fprintf(stderr, "Pointer in section medium at position %zu\n", (placeholder_medium*)e - eff_buf_medium);
	}
#endif
	switch (t) {
	case O_EFS_ZERO: {
		placeholder_zero *s = (void*)e;
		if (s < eff_buf_zero || s >= eff_buf_zero + O_EFF_ZERO_NR) {
			fprintf(stderr, "Attempt to free a garbage pointer (zero) %p (type %d)\n", s, e->type);
			return;
		}
		eff_queue_zero[eff_zero_nr] = s - eff_buf_zero;
		eff_zero_nr++;
	} break;
	case O_EFS_SMALL: {
		placeholder_small *s = (void*)e;
		if (s < eff_buf_small || s >= eff_buf_small + O_EFF_SMALL_NR) {
			fprintf(stderr, "Attempt to free a garbage pointer (small) %p (type %d)\n", s, e->type);
			return;
		}
		eff_queue_small[eff_small_nr] = s - eff_buf_small;
		eff_small_nr++;
	} break;
	case O_EFS_MEDIUM: {
		placeholder_medium *s = (void*)e;
		if (s < eff_buf_medium || s >= eff_buf_medium + O_EFF_MEDIUM_NR) {
			fprintf(stderr, "Attempt to free a garbage pointer (medium) %p (type %d)\n", s, e->type);
			return;
		}
		eff_queue_medium[eff_medium_nr] = s - eff_buf_medium;
		eff_medium_nr++;
	} break;
	}
}

void o_free_effect_i(effect_s *e, size_t t) {
	o_effect_size s;
	if (t == 0) {
		s = O_EFS_ZERO;
	} else if (t <= 16) {
		s = O_EFS_SMALL;
	} else if (t <= 32) {
		s = O_EFS_MEDIUM;
	} else {
		fprintf(stderr, "Too large effect data size: %zu\n", t);
		return;
	}
	o_free_effect(e, s);
}

sector_s* o_alloc_sector(void) {
	if (sect_nr == 0) {
		fprintf(stderr, "No free sectors\n");
		return NULL;
	}
	sect_nr--;
	sector_s *t = sect_buf + sect_queue[sect_nr];
	sect_queue[sect_nr] = -1;
	return t;
}

void o_free_sector(sector_s *s) {
	if (s < sect_buf || s > sect_buf + O_SECTOR_NR - 1) {
		fprintf(stderr, "Attempt to free a garbage pointer (sector) %p\n", s);
		return;
	}
	sect_queue[sect_nr] = s - sect_buf;
	sect_nr++;
}

ent_ptr ent_sptr(entity_s *s) {
	return (size_t)(s - ent_buf) + 1;
}

entity_s* ent_aptr(ent_ptr s) {
	if (s == 0 || s > O_ENTITY_NR)
		return NULL;
	return ent_buf + (s - 1);
}

ent_ptr ent_cptr(sector_s *s, int x, int y, int z) {
	return ((size_t)(s - sect_buf) << 9) | (x << 6) | (y << 3) | z | ENT_CPTR_BIT;
}

sector_s* ent_acptr(ent_ptr s, int *x, int *y, int *z) {
	if (!(s & ENT_CPTR_BIT))
		return NULL;
	*x = (s >> 6) & 7;
	*y = (s >> 3) & 7;
	*z = s & 7;
	return sect_buf + ((s ^ ENT_CPTR_BIT) >> 9);
}

void* o_malloc(size_t s) {
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
	o_buf[o_buf_cur] = x;
	o_buf_cur ++;
	if (o_buf_cur == O_BUF_S) {
		o_buf_cur = 0;
	}
	free(x);
}
