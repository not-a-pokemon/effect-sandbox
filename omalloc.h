#ifndef _efsa_OMALLOC_H
#define _efsa_OMALLOC_H

#include "entity.h"
#include <stdlib.h>

#define ENT_CPTR_BIT (1ULL << 62)

typedef enum o_effect_size {
	O_EFS_ZERO, /* Empty data */
	O_EFS_SMALL, /* Data under 16 bytes */
	O_EFS_MEDIUM, /* Data under 32 bytes */
} o_effect_size;

extern int ent_free_nr;
extern int eff_zero_nr, eff_small_nr, eff_medium_nr;

void o_init_allocator(void);

entity_s* o_alloc_entity(void);
void o_free_entity(entity_s *);

effect_s* o_alloc_effect(o_effect_size);
effect_s* o_alloc_effect_i(size_t);
void o_free_effect(effect_s *, o_effect_size);
void o_free_effect_i(effect_s *, size_t);

sector_s* o_alloc_sector(void);
void o_free_sector(sector_s *);

ent_ptr ent_sptr(entity_s *);
entity_s* ent_aptr(ent_ptr);
ent_ptr ent_cptr(sector_s *s, int x, int y, int z);
sector_s* ent_acptr(ent_ptr s, int *x, int *y, int *z);

void* o_malloc(size_t);
void o_free(void *);

#endif
