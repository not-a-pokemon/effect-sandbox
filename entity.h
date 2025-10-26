#ifndef _efsa_ENTITY_H
#define _efsa_ENTITY_H

#include <stdio.h>
#include <stdint.h>
#include "rng.h"

#define G_SECTOR_SIZE 8
#define G_TRACER_RESOLUTION 256
#define G_TRACER_GRAVITY 128
#define G_MOVE_START_DELAY 128
#define G_PUDDLE_MAX 64

typedef enum rotation_type {
	RT_DICE,
	RT_STICK,
	RT_TABLE,
} rotation_type;

typedef enum liquid_type {
	LIQ_WATER,
} liquid_type;

struct entity_s;
struct effect_s;

typedef struct effect_ph_block_data {
	int x;
	int y;
	int z;
	unsigned floor:1;
	unsigned block_movement:1;
	unsigned floor_up:1;
	unsigned stair:1;
	unsigned slope:1;
} effect_ph_block_data;

typedef enum parent_ref_type {
	PARENT_REF_HELD,
	PARENT_REF_PLACE,
	PARENT_REF_LIMB,
	PARENT_REF_CONT,
} parent_ref_type;

typedef enum material_type {
	MAT_GHOST,
	MAT_WOOD,
	MAT_STONE,
	MAT_PLANT,
} material_type;

/* Note that this is a bit-field enum */
typedef enum material_prop_mask {
	MATP_SMALL = 1,
	MATP_SHARP = 2,
} material_prop_mask;

typedef enum damage_type {
	DMGT_BLUNT,
	DMGT_CUT,
	DMGT_PIERCE,
	DMGT_FIRE,
} damage_type;

typedef enum attack_type {
	ATK_SWING = 0,
	ATK_THRUST = 1,
	ATK_HAND_PUNCH = 2,
	ATK_KICK = 3,
	ATK_N_COUNT = 4,
} attack_type;

#include "gen-effects.h"

typedef struct effect_s {
	struct effect_s *prev;
	struct effect_s *next;
	enum effect_type type;
	char _pad[4];
	/* Must be aligned as underlying effect_data */
	char data[];
} effect_s;

typedef struct entity_s {
	struct effect_s *effects;
	struct entity_s *prev;
	struct entity_s *next;
} entity_s;

typedef struct entity_l_s {
	struct entity_l_s *prev;
	struct entity_l_s *next;
	struct entity_s *ent;
} entity_l_s;

typedef struct sector_s {
	struct sector_s *sprev;
	struct sector_s *snext;
	int x;
	int y;
	int z;
	struct entity_l_s *block_entities[G_SECTOR_SIZE][G_SECTOR_SIZE][G_SECTOR_SIZE];
} sector_s;

typedef sector_s sectors_s;

typedef struct attack_l_s {
	uint32_t limb_slot_tag;
	struct entity_s *limb_entity;
	enum attack_type type;
	struct entity_s *tool;
	uint32_t tool_mat_tag;
	struct attack_l_s *prev;
	struct attack_l_s *next;
} attack_l_s;

typedef void (*effect_dump_t)(struct effect_s *e, FILE *stream);
typedef void (*effect_scan_t)(struct effect_s *e, int n_ent, entity_s **a_ent, FILE *stream);
typedef int (*effect_rem_t)(struct entity_s *s, struct effect_s *e);

extern int effect_data_size[];
extern effect_dump_t effect_dump_functions[];
extern effect_scan_t effect_scan_functions[];
extern effect_rem_t effect_rem_functions[];
extern sectors_s *g_sectors;
extern entity_s *g_entities;
extern rng_state_s *g_dice;

extern const char *attack_type_string[];

effect_s* alloc_effect(effect_type t);
void free_effect(effect_s *s);

void coord_normalize(int *x, int *cx);

void entity_prepend(entity_s *g, entity_s *s);

sector_s *sector_get_sector(sectors_s *s, int x, int y, int z);
entity_l_s *sector_get_block_entities(sector_s *s, int x, int y, int z);
int sector_get_block_floor_up(sector_s *s, int x, int y, int z);
int sector_get_block_floor(sector_s *s, int x, int y, int z);

int block_fallable(int x, int y, int z);
int sector_get_block_blocked_movement(sector_s *s, int x, int y, int z);
int sector_get_block_stairs(sector_s *s, int x, int y, int z);
int sector_get_block_slope(sector_s *s, int x, int y, int z);

effect_s *effect_by_type(effect_s *s, effect_type t);
effect_s *effect_by_ptr(effect_s *s, effect_s *f);
effect_s* next_effect_by_type(effect_s *s, effect_type t);
effect_s* prev_effect_by_type(effect_s *s, effect_type t);

void apply_triggers(entity_s *s);
void apply_instants(entity_s *s);
void apply_reactions(entity_s *s);
void process_tick(entity_s *sl);

entity_s* clear_nonexistent(entity_s *sl);

int entity_coords(entity_s *s, int *x, int *y, int *z);
int entity_set_coords(entity_s *s, int x, int y, int z);
entity_s* entity_copy(entity_s *s);
void detach_entity(entity_s *s, int x, int y, int z);
void attach_entity(entity_s *s, int x, int y, int z);
void detach_generic_entity(entity_s *s);
void attach_generic_entity(entity_s *s);

void effect_unlink(entity_s *s, effect_s *e);
void effect_prepend(entity_s *s, effect_s *e);

effect_s* entity_limb_by_tag(entity_s *s, uint32_t tag);
effect_s* entity_limb_by_entity(entity_s *s, entity_s *t);
effect_s* entity_material_by_tag(entity_s *s, uint32_t tag);

void apply_gravity(entity_s *s);
void apply_block_move(entity_s *s);
void apply_stair_move(entity_s *s);
void apply_tracer(entity_s *s);
void apply_movement(entity_s *s);

int attack_type_possible(entity_s *s, entity_s *used_limb, attack_type type, uint32_t used_tag);
entity_s* attack_used_tool(entity_s *s, entity_s *used_limb, attack_type type);
void apply_attack(entity_s *s);

void liquid_deduplicate(entity_s *s);
void apply_liquid_movement(entity_s *s);

void apply_physics(entity_s *s);

void trigger_move(entity_s *s, int x, int y, int z);
void trigger_go_up(entity_s *s);
void trigger_go_down(entity_s *s);
void trigger_grab(entity_s *s, effect_s *h, entity_s *w, uint32_t tag);
void trigger_drop(entity_s *s, effect_s *h);
void trigger_put(entity_s *s, effect_s *h, entity_s *w);
void trigger_throw(entity_s *s, effect_s *h, int x, int y, int z, int speed);
void trigger_touch(entity_s *s, effect_s *h, entity_s *w);
void trigger_attack(entity_s *s, entity_s *e, attack_type type, entity_s *used_limb, uint32_t weapon_mat);
void trigger_aim(entity_s *s, effect_s *e, int x, int y, int z, entity_s *ent);
void trigger_fill_cont(entity_s *s, effect_s *h, entity_s *t);
void trigger_empty_cont(entity_s *s, effect_s *h);

void hand_grab(entity_s *ent, effect_s *hand, entity_s *item, uint32_t tag);
void hand_drop(entity_s *ent, effect_s *hand);
void hand_put(entity_s *ent, effect_s *hand, entity_s *w);
void hand_throw(entity_s *s, effect_s *h, int x, int y, int z, int speed);
void hand_aim(entity_s *s, effect_s *h, int x, int y, int z, entity_s *ent);
void hand_fill_cont(entity_s *s, effect_s *h, entity_s *t);
void hand_empty_cont(entity_s *s, effect_s *h);

void dump_effect(effect_s *e, FILE *stream);
void dump_entity(entity_s *s, FILE *stream);
void dump_sector(sector_s *s, FILE *stream);
void entity_enumerate(entity_s *s, int *ent_id);
void dump_sector_list(sector_s *s, FILE *stream);
void effect_dump_ph_block(effect_s *e, FILE *stream);
void unload_entity(entity_s *s);
entity_s *load_sector_list(FILE *stream);
effect_s* scan_effect(int n_ent, entity_s **a_ent, FILE *stream);
entity_s* scan_entity(int n_ent, entity_s **a_ent, FILE *stream);
void unload_sector(sector_s *s);
void effect_scan_ph_block(effect_s *e, int n_ent, entity_s **a_ent, FILE *stream);
int entity_get_index(entity_s *s);
void entity_set_index(entity_s *s, int i);
int entity_num_effects(entity_s *s);

entity_l_s* effect_enlist(effect_s *s);
entity_l_s* entity_enlist(entity_s *s);

entity_l_s* sector_get_block_entities_indirect(sector_s *s, int x, int y, int z);

entity_l_s* entity_l_s_copy(entity_l_s *eo);
void entity_l_s_free(entity_l_s *s);

int entity_reachable(entity_s *s, effect_s *limb, entity_s *e);

entity_s* tracer_check_bump(entity_s *s, int x, int y, int z);

void unparent_entity(entity_s *s);
void lift_entity(entity_s *s);

int entity_weight(entity_s *s);
int entity_size(entity_s *s);

attack_l_s* entity_list_attacks(entity_s *s, entity_s *o);

int container_get_amount(entity_s *s);
void container_add_liquid(entity_s *s, liquid_type t, int amount);

#endif
