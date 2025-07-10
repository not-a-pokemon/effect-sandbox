#ifndef _efsa_ENTITY_H
#define _efsa_ENTITY_H

#include <stdint.h>
#include "rng.h"

#define G_SECTOR_SIZE 8
#define G_TRACER_RESOLUTION 256
#define G_TRACER_GRAVITY 128

typedef enum effect_type {
	EF_B_NONEXISTENT,
	EF_B_INDEX,
	EF_PH_BLOCK,
	EF_PH_ITEM,
	EF_FALLING,
	EF_TRACER,
	EF_BLOCK_MOVE,
	EF_STAIR_MOVE,
	EF_RENDER,
	EF_NOPHYSICS,
	EF_LIMB_SLOT,
	EF_LIMB_HAND,
	EF_LIMB_LEG,
	EF_PUNCH,
	EF_MATERIAL,
	EF_SIZE_SCALE,
	EF_AIM,
	EF_ATTACK,
	EF_TABLE,
	EF_TABLE_ITEM,
	EF_FIRE,
	EF_S_TOUCH,
	EF_S_PUNCH,
	EF_S_BUMP,
	EF_S_DMG,
	EF_M_TOUCH,
	EF_M_GRAB,
	EF_M_DROP,
	EF_M_PUT,
	EF_M_THROW,
	EF_M_AIM_FOR,
	EF_R_TOUCH_RNG_TP,
	EF_R_TOUCH_TOGGLE_BLOCK,
	EF_R_TOUCH_SHOOT_PROJECTILE,
	EF_A_PRESSURE_PLATE,
	EF_A_CIRCLE_MOVE,
	EF_ROTATION,
	EF_UNKNOWN = -1,
} effect_type;

typedef enum rotation_type {
	RT_DICE,
	RT_STICK,
	RT_TABLE,
} rotation_type;

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
} parent_ref_type;

typedef enum material_type {
	MAT_GHOST,
	MAT_WOOD,
	MAT_STONE,
	MAT_PLANT,
} material_type;

typedef struct effect_m_grab_data {
	struct effect_s *eff;
	struct entity_s *ent;
} effect_m_grab_data;

typedef struct effect_m_drop_data {
	struct effect_s *eff;
	//struct entity_s *ent;
} effect_m_drop_data;

typedef struct effect_m_put_data {
	struct effect_s *eff;
	struct entity_s *where;
} effect_m_put_data;

typedef struct effect_m_throw_data {
	struct effect_s *eff;
	int x;
	int y;
	int z;
	int speed;
} effect_m_throw_data;

typedef struct effect_m_aim_for_data {
	struct effect_s *eff;
	int x;
	int y;
	int z;
	struct entity_s *ent;
} effect_m_aim_for_data;

typedef struct effect_m_touch_data {
	struct effect_s *eff;
	struct entity_s *ent;
} effect_m_touch_data;

typedef enum damage_type {
	DMGT_BLUNT,
	DMGT_CUT,
	DMGT_PIERCE,
	DMGT_FIRE,
} damage_type;

#include "gen-effects.h"

typedef struct effect_s {
	struct effect_s *prev;
	struct effect_s *next;
	enum effect_type type;
	/* For save/load */
	int index;
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

typedef void (*effect_dump_t)(struct effect_s *e, FILE *stream);
typedef void (*effect_scan_t)(struct effect_s *e, int n_ent, entity_s **a_ent, int n_eff, effect_s **a_eff, FILE *stream);

extern int effect_data_size[];
extern effect_dump_t effect_dump_functions[];
extern effect_scan_t effect_scan_functions[];
extern sectors_s *g_sectors;
extern entity_s *g_entities;
extern rng_state_s *g_dice;

effect_s *alloc_effect(effect_type);
void free_effect(effect_s*);

void coord_normalize(int*, int*);

void entity_prepend(entity_s*, entity_s*);

// Get a sector from list
sector_s *sector_get_sector(sectors_s *s, int x, int y, int z);
sector_s *sector_create_sector(sectors_s *s, int x, int y, int z);
sector_s *sector_use_sector(sectors_s *s, int x, int y, int z);

// Need to implement: Block physics, Item physics

// In case block_status becomes hash table or something
entity_l_s *sector_get_block_entities(sector_s *s, int x, int y, int z);
int sector_get_block_floor_up(sector_s *s, int x, int y, int z);
int sector_get_block_floor(sector_s *s, int x, int y, int z);
int sector_get_block_blocked_movement(sector_s *s, int x, int y, int z);
int sector_get_block_stairs(sector_s *s, int x, int y, int z);
int sector_get_block_slope(sector_s *s, int x, int y, int z);
int block_fallable(int x, int y, int z);

effect_s *effect_by_type(effect_s *s, effect_type t);
effect_s *effect_by_ptr(effect_s *s, effect_s *f);
// effect_s *next_effect_by_type(effect_s *s, effect_type t);

void effect_unlink(entity_s *s, effect_s *e);
void effect_prepend(entity_s *s, effect_s *e);

void apply_block_move(entity_s *s);
void apply_stair_move(entity_s *s);
void apply_tracer(entity_s *s);
void apply_punch(entity_s *s);
void apply_attack(entity_s *s);

void apply_gravity(entity_s *s);
void apply_movement(entity_s *s);
void apply_physics(entity_s *s);
void apply_triggers(entity_s *s);
void apply_instants(entity_s *s);
void process_tick(entity_s *s);
entity_s* clear_nonexistent(entity_s *s);

int entity_coords(entity_s *s, int *x, int *y, int *z);
int entity_set_coords(entity_s *s, int x, int y, int z);

void detach_entity(entity_s *s, int x, int y, int z);
void attach_entity(entity_s *s, int x, int y, int z);
void detach_generic_entity(entity_s *s);
void attach_generic_entity(entity_s *s);

void unparent_entity(entity_s *s);

void hand_grab(entity_s *ent, effect_s *hand, entity_s *item);
void hand_drop(entity_s *ent, effect_s *hand);
void hand_put(entity_s *ent, effect_s *hand, entity_s *w);
void hand_throw(entity_s *ent, effect_s *hand, int x, int y, int z, int speed);
void hand_aim(entity_s *s, effect_s *h, int x, int y, int z, entity_s *ent);

void trigger_move(entity_s *s, int start_delay, int x, int y, int z);
// Go down like staircase
void trigger_go_up(entity_s *s, int start_delay);
void trigger_go_down(entity_s *s, int start_delay);

void trigger_grab(entity_s *s, effect_s *h, entity_s *w);
void trigger_drop(entity_s *s, effect_s *h);
void trigger_put(entity_s *s, effect_s *h, entity_s *w);
void trigger_throw(entity_s *s, effect_s *h, int x, int y, int z, int speed);
void trigger_touch(entity_s *s, effect_s *h, entity_s *w);
void trigger_punch(entity_s *s, entity_s *e);
void trigger_attack(entity_s *s, entity_s *e);
void trigger_aim(entity_s *s, effect_s *h, int x, int y, int z, entity_s *ent);

void dump_effect(effect_s *e, FILE *stream);
void dump_entity(entity_s *s, FILE *stream);
void dump_sector(sector_s *s, FILE *stream);
void dump_sector_list(sector_s *s, FILE *stream);

entity_s *load_sector_list(FILE *stream);
effect_s *scan_effect(int n_ent, entity_s **a_ent, int n_eff, effect_s **a_eff, FILE *stream);
entity_s *scan_entity(int n_ent, entity_s **a_ent, int n_eff, effect_s **a_eff, FILE *stream);

void unload_entity(entity_s *s);
// Remove sector from active list alongside with every entity
void unload_sector(sector_s *s);

int entity_get_index(entity_s *s);
void entity_set_index(entity_s *s, int i);
int entity_num_effects(entity_s *s);

void entity_enumerate(entity_s *s, int *ent_id, int *eff_id);

entity_l_s* effect_enlist(effect_s *s);
entity_l_s* entity_enlist(entity_s *s);
entity_l_s* sector_get_block_entities_indirect(sector_s *s, int x, int y, int z);
entity_l_s* entity_l_s_copy(entity_l_s *s);
void entity_l_s_free(entity_l_s *s);

int entity_reachable(entity_s *s, effect_s *limb, entity_s *e);
int entity_reachable_pos(entity_s *s, effect_s *limb, int x, int y, int z);

entity_s* tracer_check_bump(entity_s *s, int x, int y, int z);

#endif
