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

#define ENT_NULL 0UL
typedef uint64_t ent_ptr;

typedef enum block_type {
	BLK_EMPTY = 0,
	BLK_FLOOR,
	BLK_WALL,
} block_type;

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

typedef enum block_prop_mask {
	PB_FLOOR = 1,
	PB_BLOCK_MOVEMENT = 1 << 1,
	PB_FLOOR_UP = 1 << 2,
	PB_STAIR = 1 << 3,
	PB_SLOPE = 1 << 4,
} block_prop_mask;

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
	MAT_GLASS,
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

typedef enum common_type_t {
	CT_NONE = 0,
	CT_WALL,
	CT_FLOOR,
} common_type_t;

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
	// zero if not of common type, otherwise a vaild common type identifier
	unsigned common_type;
	char common_data[20];
} entity_s;

typedef struct entity_l_s {
	struct entity_l_s *prev;
	struct entity_l_s *next;
	ent_ptr ent;
} entity_l_s;

// The smallest block type. It has only 8 bytes in the structure
typedef struct block_s {
	unsigned type;
	int dur;
} block_s;

typedef struct sector_s {
	// Sectors are stored in a cartesian tree
	struct sector_s *ch[2];
	int prio;
	int x;
	int y;
	int z;
	struct entity_l_s *block_entities[G_SECTOR_SIZE][G_SECTOR_SIZE][G_SECTOR_SIZE];
	struct block_s block_blocks[G_SECTOR_SIZE][G_SECTOR_SIZE][G_SECTOR_SIZE];
} sector_s;

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
extern sector_s *g_sectors;
extern entity_s *g_entities;
extern rng_state_s *g_dice;

extern const char *attack_type_string[];

effect_s* alloc_effect(effect_type t);
void free_effect(effect_s *s);

void coord_normalize(int *x, int *cx);

void entity_prepend(entity_s *g, entity_s *s);

sector_s* sector_get_sector(sector_s *s, int x, int y, int z);
void sector_split(sector_s *s, int at_x, int at_y, int at_z, sector_s **buf);
sector_s* sector_insert(sector_s *s, sector_s *n);

entity_l_s* sector_get_block_entities(sector_s *s, int x, int y, int z);
int sector_get_block_floor_up(sector_s *s, int x, int y, int z);
int sector_get_block_floor(sector_s *s, int x, int y, int z);

int block_fallable(int x, int y, int z);
int sector_get_block_blocked_movement(sector_s *s, int x, int y, int z);
int sector_get_block_stairs(sector_s *s, int x, int y, int z);
int sector_get_block_slope(sector_s *s, int x, int y, int z);

int entity_has_effect(ent_ptr s, effect_type t);
int entity_load_effect(ent_ptr s, effect_type t, void *d);
int entity_store_effect(ent_ptr s, effect_type t, void *d);

effect_s* effect_by_type(effect_s *s, effect_type t);
effect_s* effect_by_ptr(effect_s *s, effect_s *f);
effect_s* next_effect_by_type(effect_s *s, effect_type t);
effect_s* prev_effect_by_type(effect_s *s, effect_type t);

void apply_triggers(ent_ptr s);
void apply_instants(ent_ptr s);
void apply_reactions(ent_ptr s);
void process_tick(entity_s *sl);

entity_s* clear_nonexistent(entity_s *sl);

int entity_coords(ent_ptr s, int *x, int *y, int *z);
int entity_set_coords(ent_ptr s, int x, int y, int z);
entity_s* entity_copy(ent_ptr s);
void detach_entity(ent_ptr s, int x, int y, int z);
void attach_entity(ent_ptr s, int x, int y, int z);
void detach_generic_entity(ent_ptr s);
void attach_generic_entity(ent_ptr s);

void effect_unlink(entity_s *s, effect_s *e);
void effect_prepend(entity_s *s, effect_s *e);

effect_s* entity_limb_by_tag(entity_s *s, uint32_t tag);
effect_s* entity_limb_by_entity(entity_s *s, ent_ptr t);
effect_s* entity_material_by_tag(entity_s *s, uint32_t tag);

void apply_gravity(ent_ptr s);
void apply_block_move(ent_ptr s);
void apply_stair_move(ent_ptr s);
void apply_tracer(ent_ptr s);
void apply_movement(ent_ptr s);

int attack_type_possible(ent_ptr s, uint32_t limb_tag, attack_type type, uint32_t used_tag);
ent_ptr attack_used_tool(ent_ptr s, uint32_t limb_tag, attack_type type);
void apply_attack(ent_ptr s);

void liquid_deduplicate(ent_ptr s);
void apply_liquid_movement(ent_ptr s);

void apply_physics(ent_ptr s);

void trigger_move(ent_ptr s, int x, int y, int z);
void trigger_go_up(ent_ptr s);
void trigger_go_down(ent_ptr s);
void trigger_grab(ent_ptr s, effect_s *h, ent_ptr w, uint32_t tag);
void trigger_drop(ent_ptr s, effect_s *h);
void trigger_put(ent_ptr s, effect_s *h, ent_ptr w);
void trigger_throw(ent_ptr s, effect_s *h, int x, int y, int z, int speed);
void trigger_attack(ent_ptr s, ent_ptr e, attack_type type, uint32_t limb_tag, uint32_t weapon_mat);
void trigger_fill_cont(ent_ptr s, effect_s *h, ent_ptr t);
void trigger_empty_cont(ent_ptr s, effect_s *h);
void trigger_press_button(ent_ptr s, effect_s *h, ent_ptr t, effect_s *w);
void trigger_open_door(ent_ptr s, effect_s *h, ent_ptr t, int dir);

void hand_grab(ent_ptr ent, effect_s *hand, ent_ptr item, uint32_t tag);
void hand_drop(ent_ptr ent, effect_s *hand);
void hand_put(ent_ptr ent, effect_s *hand, ent_ptr w);
void hand_throw(ent_ptr s, effect_s *h, int x, int y, int z, int speed);
void hand_fill_cont(ent_ptr s, effect_s *h, ent_ptr t);
void hand_empty_cont(ent_ptr s, effect_s *h);
void hand_press_button(ent_ptr s, effect_s *h, ent_ptr t, uint32_t mat_tag);

void dump_effect(effect_s *e, FILE *stream);
void dump_entity(entity_s *s, FILE *stream);
void dump_sector(sector_s *s, FILE *stream);
void dump_sector_bslice(sector_s *s, FILE *stream);
void entity_enumerate(entity_s *s, int *ent_id);

void sector_enumerate_rec(sector_s *s, int *ent_id, int *bslice_id);
void sector_dump_bslice_rec(sector_s *s, FILE *stream);
void sector_dump_rec(sector_s *s, FILE *stream);
void dump_sector_list(sector_s *s, FILE *stream);

void unload_entity(entity_s *s);
entity_s *load_sector_list(FILE *stream);
effect_s* scan_effect(int n_ent, entity_s **a_ent, FILE *stream);
entity_s* scan_entity(int n_ent, entity_s **a_ent, FILE *stream);
void scan_bslice(FILE *stream);
void unload_sector(sector_s *s);

int effect_rem_ph_item(entity_s *s, effect_s *e);

int entity_get_index(entity_s *s);
void entity_set_index(entity_s *s, int i);
int entity_num_effects(entity_s *s);

entity_l_s* effect_enlist(effect_s *s);
entity_l_s* entity_enlist(entity_s *s);

entity_l_s* sector_get_block_entities_indirect(sector_s *s, int x, int y, int z);

entity_l_s* entity_l_s_copy(entity_l_s *eo);
void entity_l_s_free(entity_l_s *s);

int entity_reachable(ent_ptr s, effect_s *limb, ent_ptr e);

ent_ptr tracer_check_bump(ent_ptr s, int x, int y, int z);

void unparent_entity(ent_ptr s);
void unparent_attach_entity(ent_ptr s);
void lift_entity(ent_ptr s);

int entity_weight(ent_ptr s);
int entity_size(ent_ptr s);

attack_l_s* entity_list_attacks(ent_ptr s, ent_ptr o);

int container_get_amount(ent_ptr s);
void container_add_liquid(ent_ptr s, liquid_type t, int amount);

void dmg_deal(ent_ptr s, damage_type t, int v);

#endif
