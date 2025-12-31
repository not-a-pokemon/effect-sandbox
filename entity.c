#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "entity.h"
#include "omalloc.h"
#include "rng.h"

sector_s *g_sectors;
entity_s *g_entities;
rng_state_s *g_dice;

const char *attack_type_string[ATK_N_COUNT] = {
	[ATK_SWING] = "swing",
	[ATK_THRUST] = "thrust",
	[ATK_HAND_PUNCH] = "punch(hand)",
	[ATK_KICK] = "kick",
};

effect_s* alloc_effect(effect_type t) {
#ifdef DEBUG_EFFECT_ALLOCS
	fprintf(stderr, "[MSG] allocating effect %d, %d bytes\n", t, sizeof(effect_s) + (t != EF_UNKNOWN ? effect_data_size[t] : 32));
#endif
	effect_s *r;
	if (t == EF_UNKNOWN) {
		r = o_alloc_effect_i(32);
	} else {
		r = o_alloc_effect_i(effect_data_size[t]);
	}
	if (r == NULL) {
		fprintf(stderr, "Failed to allocate effect, size %d, type %d\n", effect_data_size[t], t);
		return NULL;
	}
	r->type = t;
	return r;
}

void free_effect(effect_s *s) {
#ifdef DEBUG_EFFECT_ALLOCS
	fprintf(stderr, "[MSG] freeing effect %d\n", s->type);
#endif
	o_free_effect_i(s, effect_data_size[s->type]);
}

void coord_normalize(int *x, int *cx) {
	int t = (*x) / G_SECTOR_SIZE;
	(*cx) += t;
	(*x) -= t * G_SECTOR_SIZE;
	if ((*x) < 0) {
		(*x) += G_SECTOR_SIZE;
		(*cx) --;
	}
}

void entity_prepend(entity_s *g, entity_s *s) {
	s->prev = NULL;
	s->next = g;
	if (g != NULL) g->prev = s;
}

sector_s *sector_get_sector(sector_s *s, int x, int y, int z) {
	if (s == NULL)
		return NULL;
	if (s->x == x && s->y == y && s->z == z)
		return s;
	int c_ind;
	if (s->x != x) {
		c_ind = s->x < x;
	} else if (s->y != y) {
		c_ind = s->y < y;
	} else {
		c_ind = s->z < z;
	}
	return sector_get_sector(s->ch[c_ind], x, y, z);
}

void sector_split(sector_s *s, int at_x, int at_y, int at_z, sector_s **buf) {
	if (s == NULL) {
		buf[0] = NULL;
		buf[1] = NULL;
		return;
	}
	int c_ind;
	if (s->x != at_x) {
		c_ind = s->x < at_x;
	} else if (s->y != at_y) {
		c_ind = s->y < at_y;
	} else {
		c_ind = s->z < at_z;
	}
	sector_s *lbuf[2];
	sector_split(s->ch[c_ind], at_x, at_y, at_z, lbuf);
	buf[c_ind] = lbuf[c_ind];
	s->ch[c_ind] = lbuf[!c_ind];
	buf[!c_ind] = s;
}

sector_s* sector_insert(sector_s *s, sector_s *n) {
	if (s == NULL)
		return n;
	if (n == NULL)
		return s;
	if (s->prio > n->prio) {
		int c_ind;
		if (s->x != n->x) {
			c_ind = s->x < n->x;
		} else if (s->y != n->y) {
			c_ind = s->y < n->y;
		} else {
			c_ind = s->z < n->z;
		}
		s->ch[c_ind] = sector_insert(s->ch[c_ind], n);
		return s;
	} else {
		sector_split(s, n->x, n->y, n->z, n->ch);
		return n;
	}
}

entity_l_s *sector_get_block_entities(sector_s *s, int x, int y, int z) {
	if (s == NULL)
		return NULL;
	return s->block_entities[x][y][z];
}

int sector_get_block_floor_up(sector_s *s, int x, int y, int z) {
	entity_l_s *e = sector_get_block_entities(s, x, y, z);
	while (e != NULL) {
		effect_ph_block_data d;
		if (entity_load_effect(e->ent, EF_PH_BLOCK, &d)) {
			if (d.prop & PB_FLOOR_UP) {
				return 1;
			}
		}
		e = e->next;
	}
	{
		effect_ph_block_data d;
		if (entity_load_effect(ent_cptr(s, x, y, z), EF_PH_BLOCK, &d)) {
			if (d.prop & PB_FLOOR_UP)
				return 1;
		}
	}
	return 0;
}

int sector_get_block_floor(sector_s *s, int x, int y, int z) {
	entity_l_s *e = sector_get_block_entities(s, x, y, z);
	while (e != NULL) {
		effect_ph_block_data d;
		if (entity_load_effect(e->ent, EF_PH_BLOCK, &d)) {
			if (d.prop & PB_FLOOR) {
				return 1;
			}
		}
		e = e->next;
	}
	{
		effect_ph_block_data d;
		if (entity_load_effect(ent_cptr(s, x, y, z), EF_PH_BLOCK, &d)) {
			if (d.prop & PB_FLOOR)
				return 1;
		}
	}
	return 0;
}

int block_fallable(int x, int y, int z) {
	int cx = 0, cy = 0, cz = 0;
	coord_normalize(&x, &cx);
	coord_normalize(&y, &cy);
	coord_normalize(&z, &cz);
	sector_s *this = sector_get_sector(g_sectors, cx, cy, cz);
	if (this != NULL && sector_get_block_floor(this, x, y, z)) {
		return 1;
	}
	z --;
	coord_normalize(&z, &cz);
	this = sector_get_sector(g_sectors, cx, cy, cz);
	if (this != NULL && sector_get_block_stairs(this, x, y, z)) {
		return 1;
	}
	if (this != NULL && sector_get_block_floor_up(this, x, y, z)) {
		return 1;
	}
	return 0;
}

int sector_get_block_blocked_movement(sector_s *s, int x, int y, int z) {
	entity_l_s *e = sector_get_block_entities(s, x, y, z);
	while (e != NULL) {
		effect_ph_block_data d;
		if (entity_load_effect(e->ent, EF_PH_BLOCK, &d)) {
			if (d.prop & PB_BLOCK_MOVEMENT) {
				return 1;
			}
		}
		e = e->next;
	}
	{
		effect_ph_block_data d;
		if (entity_load_effect(ent_cptr(s, x, y, z), EF_PH_BLOCK, &d)) {
			if (d.prop & PB_BLOCK_MOVEMENT)
				return 1;
		}
	}
	return 0;
}

int sector_get_block_stairs(sector_s *s, int x, int y, int z) {
	entity_l_s *e = sector_get_block_entities(s, x, y, z);
	while (e != NULL) {
		effect_ph_block_data d;
		if (entity_load_effect(e->ent, EF_PH_BLOCK, &d)) {
			if (d.prop & PB_STAIR) {
				return 1;
			}
		}
		e = e->next;
	}
	{
		effect_ph_block_data d;
		if (entity_load_effect(ent_cptr(s, x, y, z), EF_PH_BLOCK, &d)) {
			if (d.prop & PB_STAIR)
				return 1;
		}
	}
	return 0;
}

int sector_get_block_slope(sector_s *s, int x, int y, int z) {
	entity_l_s *e = sector_get_block_entities(s, x, y, z);
	while (e != NULL) {
		effect_ph_block_data d;
		if (entity_load_effect(e->ent, EF_PH_BLOCK, &d)) {
			if (d.prop & PB_SLOPE) {
				return 1;
			}
		}
		e = e->next;
	}
	{
		effect_ph_block_data d;
		if (entity_load_effect(ent_cptr(s, x, y, z), EF_PH_BLOCK, &d)) {
			if (d.prop & PB_SLOPE)
				return 1;
		}
	}
	return 0;
}

int entity_has_effect(ent_ptr sp, effect_type t) {
	entity_s *s = ent_aptr(sp);
	if (s != NULL) {
		return entity_common_has_effect(s, t) || effect_by_type(s->effects, t) != NULL;
	}
	sector_s *sec;
	int x, y, z;
	if ((sec = ent_acptr(sp, &x, &y, &z)) != NULL) {
		return entity_block_has_effect(sec, x, y, z, t);
	}
	return 0;
}

int entity_load_effect(ent_ptr sp, effect_type t, void *d) {
	if (sp == ENT_NULL)
		return 0;
	entity_s *s = ent_aptr(sp);
	if (s != NULL) {
		if (entity_common_load_effect(s, t, d))
			return 1;
		effect_s *e = effect_by_type(s->effects, t);
		if (e == NULL)
			return 0;
		memcpy(d, e->data, effect_data_size[t]);
		return 1;
	}
	int x, y, z;
	sector_s *sec = ent_acptr(sp, &x, &y, &z);
	if (sec != NULL)
		return entity_block_load_effect(sec, x, y, z, t, d);
	return 0;
}

int entity_store_effect(ent_ptr sp, effect_type t, void *d) {
	entity_s *s = ent_aptr(sp);
	sector_s *sec;
	int x, y, z;
	if (s == NULL) {
		fprintf(stderr, "Attempt to store at a non-aptr\n");
		// TODO is this right?
		if (t == EF_MATERIAL) {
			effect_material_data *dm = d;
			if (dm->tag == 0 && (sec = ent_acptr(sp, &x, &y, &z)) != NULL) {
				sec->block_blocks[x][y][z].dur = dm->dur;
				return 1;
			}
		}
		return 0;
	}
	if (entity_common_store_effect(s, t, d))
		return 1;
	effect_s *e = effect_by_type(s->effects, t);
	if (e == NULL)
		return 0;
	memcpy(e->data, d, effect_data_size[t]);
	return 1;
}

effect_s *effect_by_type(effect_s *s, effect_type t) {
	while (s != NULL) {
		if (s->type == t) {
			return s;
		}
		s = s->next;
	}
	return NULL;
}

effect_s *effect_by_ptr(effect_s *s, effect_s *f) {
	while (s != NULL) {
		if (s == f) {
			return f;
		}
		s = s->next;
	}
	return NULL;
}

effect_s* next_effect_by_type(effect_s *s, effect_type t) {
	if (s == NULL)
		return NULL;
	if (s->next == NULL)
		return NULL;
	s = s->next;
	while (s != NULL) {
		if (s->type == t)
			return s;
		s = s->next;
	}
	return NULL;
}

effect_s* prev_effect_by_type(effect_s *s, effect_type t) {
	if (s == NULL)
		return NULL;
	if (s->prev == NULL)
		return NULL;
	s = s->prev;
	while (s != NULL) {
		if (s->type == t)
			return s;
		s = s->prev;
	}
	return NULL;
}

void apply_triggers(ent_ptr s) {
	{
		// TODO is this a trigger?
		effect_plant_data plant_d;
		if (entity_load_effect(s, EF_PLANT, &plant_d)) {
			effect_rooted_data root_d;
			if (entity_load_effect(s, EF_ROOTED, &root_d)) {
				// TODO limit the absorption both in rate and total amount
				// If 's' is rooted in a block, iterate over WET_BLOCK entities of xyz
				// Otherwise iterate over effects of root_d.ent
				sector_s *sec;
				int x, y, z;
				entity_s *t;
				if ((sec = ent_acptr(root_d.ent, &x, &y, &z)) != NULL) {
					for (entity_l_s *l = sector_get_block_entities(sec, x, y, z); l != NULL; l = l->next) {
						effect_wet_block_data wd;
						if (entity_load_effect(l->ent, EF_WET_BLOCK, &wd)) {
							// TODO unfix the amount
							if (wd.amount >= 1) {
								wd.amount--;
								plant_d.stored_water++;
								entity_store_effect(l->ent, EF_WET_BLOCK, &wd);
							}
						}
					}
				} else if ((t = ent_aptr(root_d.ent)) != NULL) {
					effect_s *e = t->effects;
					while (e != NULL) {
						if (e->type == EF_WET) {
							effect_wet_block_data *d = (void*)e->data;
							// TODO make it not only water?
							if (d->type == LIQ_WATER) {
								// TODO make the amount not fixed, possibly add a ef_rooted property?
								d->amount--;
								plant_d.stored_water++;
								if (d->amount == 0) {
									effect_s *nxt = e->next;
									effect_unlink(t, e);
									e = nxt;
									goto NO_NEXT;
								}
							}
						}
						e = e->next;
NO_NEXT:
						;
					}
				} else {
					// this shouldn't be reached
				}
			}
			// TODO calculate sunlight
			{
				plant_d.stored_energy++;
			}
			// TODO redo the water amounts
			while (plant_d.stored_energy >= 64 && plant_d.stored_water >= 16) {
				plant_d.stored_energy -= 64;
				plant_d.stored_water -= 16;
				plant_d.growth++;
			}
			if (plant_d.growth > 256)
				plant_d.growth = 256;
			plant_d.cycle_time--;
			if (plant_d.cycle_time <= 0) {
				plant_d.cycle_time = rng_bigrange(g_dice) % 2400;
				plant_d.growth--;
				// TODO sometimes heal the plant and reinforce it
				if (plant_d.growth >= 15) {
					// TODO grow a new part
					plant_d.growth -= 5;
				}
			}
			if (plant_d.growth < -10) {
				entity_s *sa = ent_aptr(s);
				// TODO plant dies
				effect_s *ef_plant = effect_by_type(sa->effects, EF_PLANT);
				if (ef_plant != NULL)
					effect_unlink(sa, ef_plant);
			}
			entity_store_effect(s, EF_PLANT, &plant_d);
		}
	}
	{
		effect_a_pressure_plate_data press_d;
		if (entity_load_effect(s, EF_A_PRESSURE_PLATE, &press_d)) {
			int pressure = 0;
			int x, y, z;
			entity_coords(s, &x, &y, &z);
			int cx = 0, cy = 0, cz = 0;
			coord_normalize(&x, &cx);
			coord_normalize(&y, &cy);
			coord_normalize(&z, &cz);
			sector_s *sec = sector_get_sector(g_sectors, cx, cy, cz);
			entity_l_s *l = sector_get_block_entities(sec, x, y, z);
			while (l != NULL) {
				pressure += entity_weight(l->ent);
				l = l->next;
			}
			effect_ph_block_data d;
			if (entity_load_effect(s, EF_PH_BLOCK, &d)) {
				if (pressure < press_d.thresold) {
					d.prop |= PB_FLOOR;
				} else {
					d.prop &= ~PB_FLOOR;
				}
				entity_store_effect(s, EF_PH_BLOCK, &d);
			}
		}
	}
	{
		if (entity_has_effect(s, EF_A_CIRCLE_MOVE)) {
			if (!entity_has_effect(s, EF_BLOCK_MOVE)) {
				int x, y, z;
				entity_coords(s, &x, &y, &z);
				int wx = 0, wy = 0;
				switch (((x & 1) << 1) | (y & 1)) {
					case 0: {
						wx = 1;
					} break;
					case 2: {
						wy = -1;
					} break;
					case 3: {
						wx = -1;
					} break;
					case 1: {
						wy = 1;
					} break;
				}
				trigger_move(s, wx, wy, 0);
			}
		}
	}
}

void apply_instants(ent_ptr sp) {
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return;
	{
		effect_s *ef = effect_by_type(s->effects, EF_FIRE);
		if (ef != NULL) {
			effect_material_data mat_d;
			if (!entity_load_effect(sp, EF_MATERIAL, &mat_d) || (mat_d.type != MAT_WOOD && mat_d.type != MAT_PLANT)) {
				effect_unlink(s, ef);
				free_effect(ef);
			} else {
				dmg_deal(sp, DMGT_FIRE, 1);
			}
		}
	}
	{
		effect_s *ef = effect_by_type(s->effects, EF_M_GRAB);
		if (ef != NULL) {
			effect_m_grab_data *d = (void*)ef->data;
			hand_grab(sp, entity_limb_by_tag(s, d->eff_tag), d->ent, d->mat_tag);
			effect_unlink(s, ef);
			free_effect(ef);
		}
	}
	{
		effect_s *ef = effect_by_type(s->effects, EF_M_GRAB_PILE);
		if (ef != NULL) {
			effect_m_grab_pile_data *d = (void*)ef->data;
			hand_grab_pile(sp, entity_limb_by_tag(s, d->eff_tag), d->ent, d->amount);
			effect_unlink(s, ef);
			free_effect(ef);
		}
	}
	{
		effect_s *ef = effect_by_type(s->effects, EF_M_DROP);
		if (ef != NULL) {
			effect_m_drop_data *d = (void*)ef->data;
			hand_drop(sp, entity_limb_by_tag(s, d->eff_tag));
			effect_unlink(s, ef);
			free_effect(ef);
		}
	}
	{
		effect_s *ef = effect_by_type(s->effects, EF_M_PUT);
		if (ef != NULL) {
			effect_m_put_data *d = (void*)ef->data;
			hand_put(sp, entity_limb_by_tag(s, d->eff_tag), d->where);
			effect_unlink(s, ef);
			free_effect(ef);
		}
	}
	{
		effect_s *ef = effect_by_type(s->effects, EF_M_THROW);
		if (ef != NULL) {
			effect_m_throw_data *d = (void*)ef->data;
			hand_throw(sp, entity_limb_by_tag(s, d->eff_tag), d->x, d->y, d->z, d->speed);
			effect_unlink(s, ef);
			free_effect(ef);
		}
	}
	{
		effect_s *ef = effect_by_type(s->effects, EF_M_FILL_CONT);
		if (ef != NULL) {
			effect_m_fill_cont_data *d = (void*)ef->data;
			hand_fill_cont(sp, entity_limb_by_tag(s, d->hand_tag), d->target);
			effect_unlink(s, ef);
			free_effect(ef);
		}
	}
	{
		effect_s *ef = effect_by_type(s->effects, EF_M_EMPTY_CONT);
		if (ef != NULL) {
			effect_m_empty_cont_data *d = (void*)ef->data;
			hand_empty_cont(sp, entity_limb_by_tag(s, d->hand_tag));
			effect_unlink(s, ef);
			free_effect(ef);
		}
	}
	{
		effect_s *ef = effect_by_type(s->effects, EF_M_PRESS_BUTTON);
		if (ef != NULL) {
			effect_m_press_button_data *d = (void*)ef->data;
			hand_press_button(sp, entity_limb_by_tag(s, d->hand_tag), d->target, d->mat_tag);
			effect_unlink(s, ef);
			free_effect(ef);
		}
	}
	{
		effect_s *ef = effect_by_type(s->effects, EF_M_OPEN_DOOR);
		if (ef != NULL) {
			effect_m_open_door_data *d = (void*)ef->data;
			effect_door_data td;
			if (entity_reachable(sp, entity_limb_by_tag(s, d->hand_tag), d->target) && entity_load_effect(d->target, EF_DOOR, &td)) {
				td.opened += d->dir;
				if (td.opened < 0)
					td.opened = 0;
				if (td.opened > 64)
					td.opened = 64;
				entity_store_effect(d->target, EF_DOOR, &td);
			}
			effect_unlink(s, ef);
			free_effect(ef);
		}
	}
	{
		effect_s *ef = effect_by_type(s->effects, EF_M_WEAR);
		if (ef != NULL) {
			effect_m_wear_data *d = (void*)ef->data;
			// TODO wear
			(void)d;
			effect_unlink(s, ef);
			free_effect(ef);
		}
	}
}

void apply_reactions(ent_ptr sp) {
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return;
	effect_s *eft;
	eft = effect_by_type(s->effects, EF_S_BUMP);
	if (eft != NULL) {
		effect_s_bump_data *eft_d = (void*)eft->data;
		{
			effect_s *ee = effect_by_type(s->effects, EF_ROTATION);
			if (ee != NULL) {
				effect_rotation_data *d = (void*)ee->data;
				if (d->type == RT_DICE) {
					d->rotation = rng_next(g_dice) % 6;
					effect_s *er = effect_by_type(s->effects, EF_RENDER);
					if (er != NULL) {
						effect_render_data *dr = (void*)er->data;
						dr->chr = '0' + d->rotation;
					}
				}
			}
		}
		{
			effect_s *ee = effect_by_type(s->effects, EF_TRACER);
			if (ee != NULL) {
				effect_unlink(s, ee);
				free_effect(ee);
			}
		}
		{
			effect_s *ee = effect_by_type(s->effects, EF_AIM);
			if (ee != NULL) {
				effect_unlink(s, ee);
				free_effect(ee);
			}
		}
		{
			if (eft_d->ent != ENT_NULL) {
				dmg_deal(eft_d->ent, DMGT_BLUNT, eft_d->force);
			}
		}
		if (entity_has_effect(sp, EF_TABLE)) {
			effect_s *t = s->effects;
			while (t != NULL) {
				if (t->type == EF_TABLE_ITEM) {
					effect_table_item_data *d = (void*)t->data;
					t = t->next;
					unparent_attach_entity(d->item);
				} else {
					t = t->next;
				}
			}
		}
		{
			effect_rain_data d;
			if (entity_load_effect(sp, EF_RAIN, &d)) {
				effect_s *new_eff;
				switch (d.type) {
				case 0: {
					int x, y, z;
					if (entity_coords(sp, &x, &y, &z)) {
						top_add_liquid(x, y, z, LIQ_WATER, d.n);
					}
				} break;
				case 1: {
					int x, y, z;
					if (entity_coords(sp, &x, &y, &z)) {
						top_add_pile(x, y, z, PILE_SNOW, d.n);
					}
				} break;
				default: {
				}
				}
				new_eff = alloc_effect(EF_B_NONEXISTENT);
				effect_prepend(s, new_eff);
				d.n = 0;
				entity_store_effect(sp, EF_RAIN, &d);
			}
		}
		effect_unlink(s, eft);
		free_effect(eft);
	}
	while ((eft = effect_by_type(s->effects, EF_S_PRESS_BUTTON)) != NULL) {
		effect_s_press_button_data *press_d = (void*)eft->data;
		effect_s *ef_r = effect_by_type(s->effects, EF_R_BOTTLE_DISPENSER);
		if (ef_r != NULL) {
			effect_r_bottle_dispenser_data *d = (void*)ef_r->data;
			if (d->mat_tag == press_d->mat_tag) {
				entity_s *new_ent = o_alloc_entity();
				new_ent->effects = NULL;
				new_ent->next = NULL;
				new_ent->prev = NULL;
				{
					effect_s *ph_item = alloc_effect(EF_PH_ITEM);
					effect_ph_item_data *d = (void*)ph_item->data;
					entity_coords(ent_sptr(s), &d->x, &d->y, &d->z);
					d->parent = ENT_NULL;
					d->weight = 3;
					effect_prepend(new_ent, ph_item);
				}
				{
					effect_s *rend = alloc_effect(EF_RENDER);
					effect_render_data *d = (void*)rend->data;
					d->r = 60;
					d->b = 200;
					d->g = 150;
					d->a = 128;
					d->chr = 'c';
					effect_prepend(new_ent, rend);
				}
				{
					effect_s *mat = alloc_effect(EF_MATERIAL);
					effect_material_data *d = (void*)mat->data;
					d->type = MAT_GLASS;
					d->prop = 0;
					d->tag = 0;
					d->dur = 10;
					effect_prepend(new_ent, mat);
				}
				{
					effect_s *cont = alloc_effect(EF_CONTAINER);
					effect_container_data *d = (void*)cont->data;
					d->capacity = 10;
					d->min_size = 10;
					d->cont_mask = 0;
					effect_prepend(new_ent, cont);
				}
				entity_prepend(g_entities, new_ent);
				g_entities = new_ent;
				attach_generic_entity(ent_sptr(new_ent));
			}
		}
		effect_unlink(s, eft);
		free_effect(eft);
	}
	if ((eft = effect_by_type(s->effects, EF_DOOR)) != NULL) {
		effect_door_data *d = (void*)eft->data;
		effect_s *r;
		effect_ph_block_data bd;
		if (entity_load_effect(ent_sptr(s), EF_PH_BLOCK, &bd)) {
			if (d->opened < 64) {
				bd.prop |= PB_BLOCK_MOVEMENT;
			} else {
				bd.prop &= ~PB_BLOCK_MOVEMENT;
			}
			if ((r = effect_by_type(s->effects, EF_RENDER)) != NULL) {
				effect_render_data *d = (void*)r->data;
				d->r = (bd.prop & PB_BLOCK_MOVEMENT) ? 0 : 255;
			}
			entity_store_effect(ent_sptr(s), EF_PH_BLOCK, &bd);
		}
	}
}

void process_tick(entity_s *sl) {
	entity_s *s = sl;
	while (s != NULL) {
		apply_triggers(ent_sptr(s));
		s = s->next;
	}
	s = sl;
	while (s != NULL) {
		apply_instants(ent_sptr(s));
		s = s->next;
	}
	s = sl;
	while (s != NULL) {
		apply_reactions(ent_sptr(s));
		s = s->next;
	}
	s = sl;
	while (s != NULL) {
		apply_physics(ent_sptr(s));
		s = s->next;
	}
}

entity_s* clear_nonexistent(entity_s *sl) {
	entity_s *s = sl;
	while (s != NULL) {
		effect_s *e = s->effects;
		while (e != NULL) {
			effect_s *next = e->next;
			effect_rem_t f = effect_rem_functions[e->type];
			if (f != NULL) {
				if (f(s, e)) {
					effect_unlink(s, e);
					free_effect(e);
				}
			}
			e = next;
		}
		s = s->next;
	}
	s = sl;
	entity_s *t;
	while (s != NULL) {
		t = s->next;
		if (effect_by_type(s->effects, EF_B_NONEXISTENT) != NULL) {
			if (s->prev != NULL) {
				s->prev->next = s->next;
			} else {
				sl = s->next;
			}
			if (s->next != NULL)
				s->next->prev = s->prev;
			unparent_entity(ent_sptr(s));
			detach_generic_entity(ent_sptr(s));
			effect_s *e = s->effects;
			while (e != NULL) {
				effect_s *nxt = e->next;
				free_effect(e);
				e = nxt;
			}
			o_free_entity(s);
		}
		s = t;
	}
	return sl;
}

int entity_coords(ent_ptr s, int *x, int *y, int *z) {
	{
		effect_ph_block_data d;
		if (entity_load_effect(s, EF_PH_BLOCK, &d)) {
			*x = d.x;
			*y = d.y;
			*z = d.z;
			return 1;
		}
	}
	effect_ph_item_data data;
	if (entity_load_effect(s, EF_PH_ITEM, &data)) {
		if (data.parent != ENT_NULL) {
			return entity_coords(data.parent, x, y, z);
		} else {
			*x = data.x;
			*y = data.y;
			*z = data.z;
		}
		return 1;
	}
	return 0;
}

int entity_set_coords(ent_ptr s, int x, int y, int z) {
	{
		effect_ph_block_data d;
		if (entity_load_effect(s, EF_PH_BLOCK, &d)) {
			d.x = x;
			d.y = y;
			d.z = z;
			entity_store_effect(s, EF_PH_BLOCK, &d);
			return 1;
		}
	}
	effect_ph_item_data data;
	if (entity_load_effect(s, EF_PH_ITEM, &data)) {
		data.x = x;
		data.y = y;
		data.z = z;
		entity_store_effect(s, EF_PH_ITEM, &data);
		return 1;
	}
	return 0;
}

entity_s* entity_copy(ent_ptr sp) {
	// works only for `aptr' entities
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return NULL;
	entity_s *t = o_alloc_entity();
	t->effects = NULL;
	t->common_type = s->common_type;
	memcpy(t->common_data, s->common_data, sizeof(t->common_data));
	effect_s *e = s->effects;
	while (e != NULL) {
		effect_s *z = alloc_effect(e->type);
		memcpy(z, e, sizeof(effect_s) + effect_data_size[e->type]);
		effect_prepend(t, z);
		e = e->next;
	}
	entity_prepend(g_entities, t);
	g_entities = t;
	return t;
}

void detach_entity(ent_ptr s, int x, int y, int z) {
	int cx = 0;
	int cy = 0;
	int cz = 0;
	coord_normalize(&x, &cx);
	coord_normalize(&y, &cy);
	coord_normalize(&z, &cz);
	sector_s *sec = sector_get_sector(g_sectors, cx, cy, cz);
	if (sec != NULL) {
		entity_l_s *e = sector_get_block_entities(sec, x, y, z);
		while (e != NULL) {
			if (e->ent == s) {
				if (e->prev != NULL) {
					e->prev->next = e->next;
				}
				if (e->next != NULL) {
					e->next->prev = e->prev;
				}
				if (e->prev == NULL) {
					sec->block_entities[x][y][z] = e->next;
				}
				o_free(e);
				break;
			}
			e = e->next;
		}
	}
}

void attach_entity(ent_ptr s, int x, int y, int z) {
	int cx = 0;
	int cy = 0;
	int cz = 0;
	coord_normalize(&x, &cx);
	coord_normalize(&y, &cy);
	coord_normalize(&z, &cz);
	sector_s *sec = sector_get_sector(g_sectors, cx, cy, cz);
	if (sec != NULL) {
		entity_l_s *e = sector_get_block_entities(sec, x, y, z);
		entity_l_s *prepend = o_malloc(sizeof(entity_l_s));
		prepend->next = e;
		if (e != NULL) {
			e->prev = prepend;
		}
		prepend->prev = NULL;
		prepend->ent = s;
		sec->block_entities[x][y][z] = prepend;
	}
}

void detach_generic_entity(ent_ptr s) {
	int x;
	int y;
	int z;
	if (entity_coords(s, &x, &y, &z)) {
		detach_entity(s, x, y, z);
	}
}

void attach_generic_entity(ent_ptr s) {
	int x;
	int y;
	int z;
	int has_parent = 0;
	effect_ph_item_data d;
	if (entity_load_effect(s, EF_PH_ITEM, &d)) {
		if (d.parent != ENT_NULL) {
			has_parent = 1;
		}
	}
	if (has_parent && entity_has_effect(s, EF_WET_BLOCK) && entity_coords(s, &x, &y, &z)) {
		attach_entity(s, x, y, z);
		return;
	}
	if (!has_parent && !entity_has_effect(s, EF_NOPHYSICS) && entity_coords(s, &x, &y, &z)) {
		attach_entity(s, x, y, z);
	}
}

void effect_unlink(entity_s *s, effect_s *e) {
	if (e->prev == NULL) {
		s->effects = e->next;
	} else {
		e->prev->next = e->next;
	}
	if (e->next != NULL) {
		e->next->prev = e->prev;
	}
	e->prev = NULL;
	e->next = NULL;
}

void effect_prepend(entity_s *s, effect_s *e) {
	e->prev = NULL;
	e->next = s->effects;
	if (s->effects != NULL) {
		s->effects->prev = e;
	}
	s->effects = e;
}

effect_s* entity_limb_by_tag(entity_s *s, uint32_t tag) {
	if (s == NULL)
		return NULL;
	effect_s *e = s->effects;
	while (e != NULL) {
		if (e->type == EF_LIMB_SLOT) {
			effect_limb_slot_data *d = (void*)e->data;
			if (d->tag == tag)
				return e;
		}
		e = e->next;
	}
	return NULL;
}

effect_s* entity_limb_by_entity(entity_s *s, ent_ptr t) {
	if (s == NULL)
		return NULL;
	effect_s *e = s->effects;
	while (e != NULL) {
		if (e->type == EF_LIMB_SLOT) {
			effect_limb_slot_data *d = (void*)e->data;
			if (d->item == t)
				return e;
		}
		e = e->next;
	}
	return NULL;
}

effect_s* entity_material_by_tag(entity_s *s, uint32_t tag) {
	if (s == NULL)
		return NULL;
	effect_s *e = s->effects;
	while (e != NULL) {
		if (e->type == EF_MATERIAL) {
			effect_material_data *d = (void*)e->data;
			if (d->tag == tag)
				return e;
		}
		e = e->next;
	}
	return NULL;
}

void apply_gravity(ent_ptr sp) {
	if (entity_has_effect(sp, EF_TRACER))
		return;
	entity_s *s = ent_aptr(sp);
	int x, y, z;
	int cx = 0, cy = 0, cz = 0;
	int ox, oy, oz;
	if (entity_coords(ent_sptr(s), &x, &y, &z)) {
		ox = x;
		oy = y;
		oz = z;
		coord_normalize(&ox, &cx);
		coord_normalize(&oy, &cy);
		coord_normalize(&oz, &cz);
		if (!block_fallable(x, y, z)) {
			detach_generic_entity(ent_sptr(s));
			entity_set_coords(ent_sptr(s), x, y, z - 1);
			if (effect_by_type(s->effects, EF_FALLING) == NULL) {
				effect_s *new_ef = alloc_effect(EF_FALLING);
				effect_prepend(s, new_ef);
			}
			attach_generic_entity(ent_sptr(s));
			effect_s *ef = effect_by_type(s->effects, EF_BLOCK_MOVE);
			if (ef != NULL) {
				effect_unlink(s, ef);
				free_effect(ef);
			}
		} else {
			effect_s *ef = effect_by_type(s->effects, EF_FALLING);
			if (ef != NULL) {
				effect_s *new_ef = alloc_effect(EF_S_BUMP);
				effect_s_bump_data *d = (void*)new_ef->data;
				/* TODO bump into what */
				d->ent = ENT_NULL;
				d->force = 1;
				effect_prepend(s, new_ef);
				effect_unlink(s, ef);
				free_effect(ef);
			}
		}
	}
}

void apply_block_move(ent_ptr sp) {
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return;
	effect_s *ef_stats = effect_by_type(s->effects, EF_STATS);
	int speed = 64;
	if (ef_stats != NULL) {
		effect_stats_data *stats_d = (void*)ef_stats->data;
		speed = stats_d->spd;
	}
	effect_s *ef_move = effect_by_type(s->effects, EF_BLOCK_MOVE);
	if (ef_move != NULL) {
		effect_s *ef_crea = effect_by_type(s->effects, EF_PH_ITEM);
		if (ef_crea != NULL) {
			effect_ph_item_data *ef_crea_d = (void*)ef_crea->data;
			if (ef_crea_d->parent != ENT_NULL && ef_crea_d->parent_type == PARENT_REF_PLACE) {
				lift_entity(ent_sptr(s));
			}
			if (ef_crea_d->parent != ENT_NULL) {
				goto CANT_MOVE;
			}
			if (effect_by_type(s->effects, EF_TRACER) != NULL)
				goto CANT_MOVE;
			if (effect_by_type(s->effects, EF_FALLING) != NULL)
				goto CANT_MOVE;
			effect_block_move_data *ef_move_d = (void*)ef_move->data;
			if (ef_move_d->delay > 0) {
				ef_move_d->delay -= speed;
			}
			if (ef_move_d->delay <= 0) {
				int x = ef_crea_d->x + ef_move_d->x;
				int y = ef_crea_d->y + ef_move_d->y;
				int z = ef_crea_d->z + ef_move_d->z;
				int ox = x;
				int oy = y;
				int oz = z;
				int cx = 0;
				int cy = 0;
				int cz = 0;
				coord_normalize(&x, &cx);
				coord_normalize(&y, &cy);
				coord_normalize(&z, &cz);
				sector_s *move_sector = sector_get_sector(g_sectors, cx, cy, cz);
				if (move_sector != NULL) {
					if (!sector_get_block_blocked_movement(move_sector, x, y, z)) {
						detach_entity(ent_sptr(s), ef_crea_d->x, ef_crea_d->y, ef_crea_d->z);
						ef_crea_d->x = ox;
						ef_crea_d->y = oy;
						ef_crea_d->z = oz;
						attach_entity(ent_sptr(s), ef_crea_d->x, ef_crea_d->y, ef_crea_d->z);
					} else if (sector_get_block_slope(move_sector, x, y, z)) {
						z ++;
						oz ++;
						coord_normalize(&z, &cz);
						move_sector = sector_get_sector(g_sectors, cx, cy, cz);
						if (!sector_get_block_blocked_movement(move_sector, x, y, z)) {
							detach_entity(ent_sptr(s), ef_crea_d->x, ef_crea_d->y, ef_crea_d->z);
							ef_crea_d->x = ox;
							ef_crea_d->y = oy;
							ef_crea_d->z = oz;
							attach_entity(ent_sptr(s), ef_crea_d->x, ef_crea_d->y, ef_crea_d->z);
						}
					}
				}
				effect_unlink(s, ef_move);
				free_effect(ef_move);
			}
		} else {
CANT_MOVE:
			effect_unlink(s, ef_move);
			free_effect(ef_move);
		}
	}
}

void apply_stair_move(ent_ptr sp) {
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return;
	effect_s *ef_stats = effect_by_type(s->effects, EF_STATS);
	int speed = 64;
	if (ef_stats != NULL) {
		effect_stats_data *stats_d = (void*)ef_stats->data;
		speed = stats_d->spd;
	}
	effect_s *ef_move = effect_by_type(s->effects, EF_STAIR_MOVE);
	if (ef_move != NULL) {
		effect_s *ef_crea = effect_by_type(s->effects, EF_PH_ITEM);
		if (ef_crea != NULL) {
			effect_stair_move_data *ef_move_d = (void*)ef_move->data;
			if (ef_move_d->delay > 0) {
				ef_move_d->delay -= speed;
			}
			if (ef_move_d->delay <= 0) {
				effect_ph_item_data *ef_crea_d = (void*)ef_crea->data;
				int x = ef_crea_d->x;
				int y = ef_crea_d->y;
				int z = ef_crea_d->z + ef_move_d->dir;
				int ox = x;
				int oy = y;
				int oz = z;
				int cx = 0;
				int cy = 0;
				int cz = 0;
				coord_normalize(&x, &cx);
				coord_normalize(&y, &cy);
				coord_normalize(&z, &cz);
				int tx = ef_crea_d->x;
				int ty = ef_crea_d->y;
				int tz = ef_crea_d->z;
				int tcx = 0;
				int tcy = 0;
				int tcz = 0;
				if (ef_move_d->dir == -1) {
					tz --;
				}
				coord_normalize(&tx, &tcx);
				coord_normalize(&ty, &tcy);
				coord_normalize(&tz, &tcz);
				sector_s *move_sector = sector_get_sector(g_sectors, cx, cy, cz);
				sector_s *this_sector = sector_get_sector(g_sectors, tcx, tcy, tcz);
				if (move_sector != NULL && this_sector != NULL) {
					if (
						!sector_get_block_blocked_movement(move_sector, x, y, z) &&
						sector_get_block_stairs(
							this_sector,
							tx, ty, tz
						)
					) {
						detach_entity(ent_sptr(s), ef_crea_d->x, ef_crea_d->y, ef_crea_d->z);
						ef_crea_d->x = ox;
						ef_crea_d->y = oy;
						ef_crea_d->z = oz;
						attach_entity(ent_sptr(s), ef_crea_d->x, ef_crea_d->y, ef_crea_d->z);
					}
				}
				effect_unlink(s, ef_move);
				free_effect(ef_move);
			}
		} else {
			effect_unlink(s, ef_move);
			free_effect(ef_move);
		}
	}
}

void apply_tracer(ent_ptr sp) {
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return;
	effect_s *ph = effect_by_type(s->effects, EF_PH_ITEM);
	if (ph == NULL) {
		return;
	}
	effect_s *tracer = effect_by_type(s->effects, EF_TRACER);
	if (tracer == NULL) {
		return;
	}
	effect_ph_item_data *ph_d = (void*)ph->data;
	effect_tracer_data *tracer_d = (void*)tracer->data;
	detach_generic_entity(sp);
	for (int i = 0; i < tracer_d->speed; i ++) {
		int nx = ph_d->x;
		int ny = ph_d->y;
		int nz = ph_d->z;
		int old_z = ph_d->z;
		tracer_d->cur_x += tracer_d->x;
		if (tracer_d->cur_x >= G_TRACER_RESOLUTION) {
			tracer_d->cur_x -= G_TRACER_RESOLUTION;
			nx ++;
		}
		if (tracer_d->cur_x <= -G_TRACER_RESOLUTION) {
			tracer_d->cur_x += G_TRACER_RESOLUTION;
			nx --;
		}
		tracer_d->cur_y += tracer_d->y;
		if (tracer_d->cur_y >= G_TRACER_RESOLUTION) {
			tracer_d->cur_y -= G_TRACER_RESOLUTION;
			ny ++;
		}
		if (tracer_d->cur_y <= -G_TRACER_RESOLUTION) {
			tracer_d->cur_y += G_TRACER_RESOLUTION;
			ny --;
		}
		tracer_d->cur_z += tracer_d->z;
		if (tracer_d->cur_z >= G_TRACER_RESOLUTION) {
			tracer_d->cur_z -= G_TRACER_RESOLUTION;
			nz ++;
		}
		if (tracer_d->cur_z <= -G_TRACER_RESOLUTION) {
			tracer_d->cur_z += G_TRACER_RESOLUTION;
			nz --;
		}
		int nxc = 0, nyc = 0, nzc = 0;
		int nxe = nx, nye = ny, nze = nz;
		coord_normalize(&nxe, &nxc);
		coord_normalize(&nye, &nyc);
		coord_normalize(&nze, &nzc);
		sector_s *sect = sector_get_sector(g_sectors, nxc, nyc, nzc);
		if (sect == NULL || sector_get_block_blocked_movement(sect, nxe, nye, nze)) {
			effect_s *ef_bump = alloc_effect(EF_S_BUMP);
			effect_s_bump_data *d = (void*)ef_bump->data;
			d->ent = ENT_NULL;
			d->force = tracer_d->speed;
			effect_prepend(s, ef_bump);
			break;
		}
		if (nz < old_z) {
			if (nz + 1 != old_z) {
				nz = old_z - 1;
			}
			if (block_fallable(nx, ny, ph_d->z)) {
				ph_d->x = nx;
				ph_d->y = ny;
				/* ph_d->z = old_z; */
				effect_s *ef_bump = alloc_effect(EF_S_BUMP);
				effect_s_bump_data *d = (void*)ef_bump->data;
				d->ent = ENT_NULL;
				d->force = tracer_d->speed;
				effect_prepend(s, ef_bump);
				break;
			}
		}
		ph_d->x = nx;
		ph_d->y = ny;
		ph_d->z = nz;
		ent_ptr w = tracer_check_bump(sp, nx, ny, nz);
		if (w != ENT_NULL) {
			effect_s *ef_bump = alloc_effect(EF_S_BUMP);
			effect_s_bump_data *d = (void*)ef_bump->data;
			d->ent = w;
			d->force = tracer_d->speed;
			effect_prepend(s, ef_bump);
			break;
		}
	}
	attach_generic_entity(sp);
	effect_unlink(s, tracer);
	free_effect(tracer);
}

void apply_movement(ent_ptr s) {
	apply_block_move(s);
	apply_stair_move(s);
	apply_tracer(s);
}

int attack_type_possible(ent_ptr sp, uint32_t limb_tag, attack_type type, uint32_t used_tag) {
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return 0;
	effect_s *limb_slot = entity_limb_by_tag(s, limb_tag);
	if (limb_slot == NULL)
		return 0;
	effect_limb_slot_data *limb_slot_d = (void*)limb_slot->data;
	ent_ptr used_limb = limb_slot_d->item;
	switch (type) {
	case ATK_HAND_PUNCH:
		return entity_has_effect(used_limb, EF_LIMB_HAND);
	case ATK_KICK:
		return entity_has_effect(used_limb, EF_LIMB_LEG);
	case ATK_SWING:
	case ATK_THRUST: {
		entity_s *used_limb_p = ent_aptr(used_limb);
		if (used_limb_p == NULL)
			return 0;
		effect_s *e = effect_by_type(used_limb_p->effects, EF_LIMB_HAND);
		if (e == NULL)
			return 0;
		effect_limb_hand_data *d = (void*)e->data;
		return
			d->item != ENT_NULL &&
			entity_material_by_tag(ent_aptr(d->item), used_tag) != NULL;
	} break;
	default:
		return 0;
	}
}

ent_ptr attack_used_tool(ent_ptr sp, uint32_t limb_tag, attack_type type) {
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return ENT_NULL;
	effect_s *limb_slot = entity_limb_by_tag(s, limb_tag);
	if (limb_slot == NULL)
		return ENT_NULL;
	effect_limb_slot_data *d = (void*)limb_slot->data;
	entity_s *used_limb = ent_aptr(d->item);
	if (used_limb == NULL)
		return ENT_NULL;
	switch (type) {
	case ATK_HAND_PUNCH:
	case ATK_KICK:
		return ent_sptr(used_limb);
	case ATK_SWING:
	case ATK_THRUST: {
		effect_s *e = effect_by_type(used_limb->effects, EF_LIMB_HAND);
		if (e == NULL)
			return ENT_NULL;
		effect_limb_hand_data *d = (void*)e->data;
		return d->item;
	} break;
	default:
		return ENT_NULL;
	}
}

void apply_attack(ent_ptr sp) {
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return;
	effect_s *e = effect_by_type(s->effects, EF_ATTACK);
	if (e == NULL)
		return;
	effect_attack_data *data = (void*)e->data;
	if (!attack_type_possible(sp, data->limb_tag, data->type, data->weapon_mat)) {
		effect_unlink(s, e);
		free_effect(e);
		return;
	}
	entity_s *used_tool = ent_aptr(attack_used_tool(sp, data->limb_tag, data->type));
	effect_s *mat = entity_material_by_tag(used_tool, data->weapon_mat);
	if (mat == NULL) {
		effect_unlink(s, e);
		free_effect(e);
		return;
	}
	effect_material_data *mat_d = (void*)mat->data;
	/* TODO cancel attack sometimes */
	if (data->delay > 0) {
		data->delay--;
		return;
	}
	if (!entity_reachable(sp, entity_limb_by_tag(s, data->limb_tag), data->ent)) {
		effect_unlink(s, e);
		free_effect(e);
		return;
	}
	if (data->ent != ENT_NULL) {
		//entity_damage_calc(data->type, &new_data->type, &new_data->val);
		damage_type type;
		int val;
		if (mat_d->prop & MATP_SHARP) {
			type = DMGT_CUT;
			val = 2;
		} else {
			type = DMGT_BLUNT;
			val = 1;
		}
		dmg_deal(data->ent, type, val);
	}
	effect_unlink(s, e);
	free_effect(e);
}

void liquid_deduplicate(ent_ptr sp) {
	/*
	 * Works for both top-level and child-level liquids
	 * Deduplicates liquid objects, only checking for the
	 * type of liquid.
	 */
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return;
	effect_s *ph_item = effect_by_type(s->effects, EF_PH_ITEM);
	if (ph_item == NULL)
		return;
	effect_ph_item_data *item_d = (void*)ph_item->data;
	entity_l_s *layer_l = entity_enlist(ent_aptr(item_d->parent));
	effect_ph_liquid_data li_d;
	if (!entity_load_effect(sp, EF_PH_LIQUID, &li_d))
		return;
	if (li_d.amount == 0)
		return;
	int x, y, z, cx = 0, cy = 0, cz = 0;
	entity_coords(sp, &x, &y, &z);
	coord_normalize(&x, &cx);
	coord_normalize(&y, &cy);
	coord_normalize(&z, &cz);
	sector_s *sec = sector_get_sector(g_sectors, cx, cy, cz);
	entity_l_s *e = layer_l == NULL ? sector_get_block_entities(sec, x, y, z) : layer_l;
	while (e != NULL) {
		entity_s *a_ent = ent_aptr(e->ent);
		if (e->ent != sp && a_ent != NULL) {
			effect_ph_liquid_data d;
			if (entity_load_effect(e->ent, EF_PH_LIQUID, &d) && d.type == li_d.type) {
				li_d.amount += d.amount;
				d.amount = 0;
				entity_store_effect(e->ent, EF_PH_LIQUID, &d);
				effect_s *new_eff = alloc_effect(EF_B_NONEXISTENT);
				effect_prepend(a_ent, new_eff);
			}
		}
		e = e->next;
	}
	entity_store_effect(sp, EF_PH_LIQUID, &li_d);
	if (li_d.amount == 0) {
		if (!entity_has_effect(sp, EF_B_NONEXISTENT))
			effect_prepend(s, alloc_effect(EF_B_NONEXISTENT));
	}
	entity_l_s_free(layer_l);
}

void apply_liquid_movement(ent_ptr sp) {
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return;
	effect_s *ph_item = effect_by_type(s->effects, EF_PH_ITEM);
	if (ph_item == NULL)
		return;
	effect_ph_item_data *item_d = (void*)ph_item->data;
	if (item_d->parent != ENT_NULL)
		return;
	effect_ph_liquid_data li_d;
	if (!entity_load_effect(sp, EF_PH_LIQUID, &li_d))
		return;
	int x, y, z;
	entity_coords(sp, &x, &y, &z);
	if (!block_fallable(x, y, z)) {
		detach_generic_entity(sp);
		entity_set_coords(sp, x, y, z - 1);
		attach_generic_entity(sp);
		return;
	}
	{
		int tx = x, ty = y, tz = z - 1, cx = 0, cy = 0, cz = 0;
		coord_normalize(&tx, &cx);
		coord_normalize(&ty, &cy);
		coord_normalize(&tz, &cz);
		sector_s *sec = sector_get_sector(g_sectors, cx, cy, cz);
		if (sec != NULL) {
			if (sec->block_blocks[tx][ty][tz].type != BLK_EMPTY) {
				effect_material_data mat_d;
				effect_ph_block_data blk_d;
				if (
					entity_load_effect(ent_cptr(sec, tx, ty, tz), EF_PH_BLOCK, &blk_d) &&
					(blk_d.prop & PB_FLOOR_UP) &&
					entity_load_effect(ent_cptr(sec, tx, ty, tz), EF_MATERIAL, &mat_d) &&
					mat_d.type == MAT_SOIL // TODO not only soil
				) {
					entity_s *w = NULL;
					effect_wet_block_data wet_d;
					for (entity_l_s *l = sector_get_block_entities(sec, tx, ty, tz); l != NULL; l = l->next) {
						if (
							entity_load_effect(l->ent, EF_WET_BLOCK, &wet_d) &&
							wet_d.type == li_d.type &&
							wet_d.ent == ent_cptr(sec, tx, ty, tz)
						) {
							w = ent_aptr(l->ent);
							break;
						}
					}
					if (w != NULL) {
						wet_d.amount++;
						li_d.amount--;
						entity_store_effect(ent_sptr(w), EF_WET_BLOCK, &wet_d);
					} else {
						entity_s *new_ent = o_alloc_entity();
						new_ent->effects = NULL;
						effect_s *ef_wet = alloc_effect(EF_WET_BLOCK);
						effect_wet_block_data *d = (void*)ef_wet->data;
						d->type = li_d.type;
						d->amount = 1;
						d->ent = ent_cptr(sec, tx, ty, tz);
						effect_prepend(new_ent, ef_wet);
						effect_s *ef_ph = alloc_effect(EF_PH_ITEM);
						effect_ph_item_data *dd = (void*)ef_ph->data;
						dd->parent_type = PARENT_REF_BLOCK_WET;
						dd->parent = ent_cptr(sec, tx, ty, tz);
						effect_prepend(new_ent, ef_ph);
						li_d.amount--;
						entity_prepend(g_entities, new_ent);
						g_entities = new_ent;
						attach_generic_entity(ent_sptr(new_ent));
					}
				}
			}
			for (entity_l_s *l = sector_get_block_entities(sec, tx, ty, tz); l != NULL; l = l->next) {
				effect_material_data mat_d;
				effect_ph_block_data blk_d;
				if (
					entity_load_effect(l->ent, EF_PH_BLOCK, &blk_d) &&
					(blk_d.prop & PB_FLOOR_UP) &&
					entity_load_effect(l->ent, EF_MATERIAL, &mat_d) &&
					mat_d.type == MAT_SOIL // TODO check for materials' general wet-ability coefficient
				) {
					effect_s *w = NULL;
					effect_wet_data *wet_d = NULL;
					for (effect_s *t = ent_aptr(l->ent)->effects; t != NULL; t = t->next) {
						if (t->type == EF_WET) {
							wet_d = (void*)t->data;
							if (wet_d->type == li_d.type) {
								w = t;
								break;
							}
						}
					}
					if (w != NULL) {
						wet_d->amount++;
						li_d.amount--;
					} else {
						effect_s *new_ef = alloc_effect(EF_WET);
						effect_wet_data *d = (void*)new_ef->data;
						d->type = LIQ_WATER;
						d->amount = 1;
						effect_prepend(ent_aptr(l->ent), new_ef);
						li_d.amount--;
					}
				}
			}
		}
	}
	// TODO calculate the maximal amount of liquid based on the surface below
	if (li_d.amount > G_PUDDLE_MAX + 3) {
		int over = li_d.amount - G_PUDDLE_MAX, amount_lost = 0;
		int valid_dir[4];
		int ndirs = 0;
		for (int i = 0; i < 4; i++) {
			int tx = x, ty = y, tz = z;
			if (i == 0)
				tx++;
			if (i == 2)
				tx--;
			if (i == 1)
				ty++;
			if (i == 3)
				ty--;
			int sx = 0, sy = 0, sz = 0;
			coord_normalize(&tx, &sx);
			coord_normalize(&ty, &sy);
			coord_normalize(&tz, &sz);
			sector_s *sec = sector_get_sector(g_sectors, sx, sy, sz);
			valid_dir[i] = !sector_get_block_blocked_movement(sec, tx, ty, tz);
			if (valid_dir[i])
				ndirs++;
		}
		if (over / ndirs != 0) {
			for (int i = 0; i < 4; i++) {
				if (!valid_dir[i])
					continue;
				int tx = x, ty = y, tz = z;
				if (i == 0)
					tx++;
				if (i == 2)
					tx--;
				if (i == 1)
					ty++;
				if (i == 3)
					ty--;
				entity_s *dup = entity_copy(sp);
				ent_ptr sdup = ent_sptr(dup);
				entity_set_coords(sdup, tx, ty, tz);
				attach_generic_entity(sdup);
				effect_ph_liquid_data dup_liq_d;
				if (entity_load_effect(ent_sptr(dup), EF_PH_LIQUID, &dup_liq_d)) {
					dup_liq_d.amount = over / ndirs;
					amount_lost += over / ndirs;
					entity_store_effect(ent_sptr(dup), EF_PH_LIQUID, &dup_liq_d);
					liquid_deduplicate(sdup);
				} else {
					/* something gone really wrong */
				}
			}
		}
		li_d.amount -= amount_lost;
	}
	entity_store_effect(sp, EF_PH_LIQUID, &li_d);
}

void pile_deduplicate(ent_ptr sp) {
	/*
	 * In contrast to liquids, piles only need to be deduplicated in case if
	 * there's a pile large enough to cover all the surface or it's in a
	 * contatiner.
	 */
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return;
	effect_ph_item_data pd;
	if (!entity_load_effect(sp, EF_PH_ITEM, &pd))
		return;
	entity_l_s *layer_l = entity_enlist(ent_aptr(pd.parent));
	int x, y, z, cx = 0, cy = 0, cz = 0;
	entity_coords(sp, &x, &y, &z);
	coord_normalize(&x, &cx);
	coord_normalize(&y, &cy);
	coord_normalize(&z, &cz);
	sector_s *sec = sector_get_sector(g_sectors, cx, cy, cz);
	entity_l_s *e = layer_l == NULL ? sector_get_block_entities(sec, x, y, z) : layer_l;
	// TODO
	entity_l_s_free(layer_l);
}

void apply_physics(ent_ptr sp) {
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return;
	if (effect_by_type(s->effects, EF_NOPHYSICS) == NULL) {
		apply_movement(sp);
		if (entity_has_effect(sp, EF_PH_ITEM)) {
			if (entity_has_effect(sp, EF_PH_LIQUID)) {
				apply_liquid_movement(sp);
				liquid_deduplicate(sp);
			} else {
				apply_gravity(sp);
			}
			if (entity_has_effect(sp, EF_PILE)) {
				pile_deduplicate(sp);
			}
		}
	}
	apply_attack(sp);
#if 0
	{
		// TODO is this a reaction, or is it physics? Maybe it could get processed even later?
		effect_wet_block_data wd;
		if (entity_load_effect(sp, EF_WET_BLOCK, &wd)) {
			if (wd.amount <= 0 && !entity_has_effect(sp, EF_B_NONEXISTENT)) {
				effect_s *new_eff = alloc_effect(EF_B_NONEXISTENT);
				effect_prepend(s, new_eff);
			}
		}
	}
#endif
	{
		effect_pile_data pd;
		if (entity_load_effect(sp, EF_PILE, &pd)) {
			if (pd.amount <= 0 && !entity_has_effect(sp, EF_B_NONEXISTENT)) {
				effect_s *new_eff = alloc_effect(EF_B_NONEXISTENT);
				effect_prepend(s, new_eff);
			}
		}
	}
}

void trigger_move(ent_ptr sp, int x, int y, int z) {
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return;
	effect_s *ef = effect_by_type(s->effects, EF_BLOCK_MOVE);
	if (ef == NULL) {
		ef = alloc_effect(EF_BLOCK_MOVE);
		effect_prepend(s, ef);
	}
	effect_block_move_data *ed = (void*)ef->data;
	ed->delay = G_MOVE_START_DELAY;
	ed->x = x;
	ed->y = y;
	ed->z = z;
}

void trigger_go_up(ent_ptr sp) {
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return;
	effect_s *ef = effect_by_type(s->effects, EF_STAIR_MOVE);
	if (ef == NULL) {
		ef = alloc_effect(EF_STAIR_MOVE);
		ef->prev = NULL;
		ef->next = s->effects;
		s->effects = ef;
	}
	effect_stair_move_data *ed = (void*)ef->data;
	ed->delay = G_MOVE_START_DELAY;
	ed->dir = 1;
}

void trigger_go_down(ent_ptr sp) {
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return;
	effect_s *ef = effect_by_type(s->effects, EF_STAIR_MOVE);
	if (ef == NULL) {
		ef = alloc_effect(EF_STAIR_MOVE);
		ef->prev = NULL;
		ef->next = s->effects;
		s->effects = ef;
	}
	effect_stair_move_data *ed = (void*)ef->data;
	ed->delay = G_MOVE_START_DELAY;
	ed->dir = -1;
}

void trigger_grab(ent_ptr sp, effect_s *h, ent_ptr wp, uint32_t tag) {
	entity_s *s = ent_aptr(sp);
	entity_s *w = ent_aptr(wp);
	if (s == NULL || w == NULL)
		return;
	if (h->type != EF_LIMB_SLOT)
		return;
	effect_limb_slot_data *ds = (void*)h->data;
	effect_s *new_eff = alloc_effect(EF_M_GRAB);
	effect_m_grab_data *d = (void*)new_eff->data;
	d->eff_tag = ds->tag;
	d->ent = wp;
	d->mat_tag = tag;
	effect_prepend(s, new_eff);
}

void trigger_grab_pile(ent_ptr s, effect_s *h, ent_ptr w, int amount) {
	entity_s *sa = ent_aptr(s);
	if (sa == NULL)
		return;
	if (h->type != EF_LIMB_SLOT)
		return;
	effect_limb_slot_data *ds = (void*)h->data;
	effect_s *new_eff = alloc_effect(EF_M_GRAB_PILE);
	effect_m_grab_pile_data *d = (void*)new_eff->data;
	d->eff_tag = ds->tag;
	d->ent = w;
	d->amount = amount;
	effect_prepend(sa, new_eff);
}

void trigger_drop(ent_ptr sp, effect_s *h) {
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return;
	if (h->type != EF_LIMB_SLOT)
		return;
	effect_limb_slot_data *ds = (void*)h->data;
	effect_s *new_eff = alloc_effect(EF_M_DROP);
	effect_m_drop_data *d = (void*)new_eff->data;
	d->eff_tag = ds->tag;
	effect_prepend(s, new_eff);
}

void trigger_put(ent_ptr sp, effect_s *h, ent_ptr wp) {
	entity_s *s = ent_aptr(sp);
	entity_s *w = ent_aptr(wp);
	if (s == NULL || w == NULL)
		return;
	if (h->type != EF_LIMB_SLOT)
		return;
	effect_limb_slot_data *ds = (void*)h->data;
	effect_s *new_eff = alloc_effect(EF_M_PUT);
	effect_m_put_data *d = (void*)new_eff->data;
	d->eff_tag = ds->tag;
	d->where = wp;
	effect_prepend(s, new_eff);
}

void trigger_throw(ent_ptr sp, effect_s *h, int x, int y, int z, int speed) {
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return;
	if (h->type != EF_LIMB_SLOT)
		return;
	effect_limb_slot_data *ds = (void*)h->data;
	effect_s *new_eff = alloc_effect(EF_M_THROW);
	effect_prepend(s, new_eff);
	effect_m_throw_data *d = (void*)new_eff->data;
	d->eff_tag = ds->tag;
	d->x = x;
	d->y = y;
	d->z = z;
	d->speed = speed;
}

void trigger_attack(ent_ptr sp, ent_ptr ep, attack_type type, uint32_t limb_tag, uint32_t weapon_mat) {
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return;
	if (effect_by_type(s->effects, EF_ATTACK) != NULL)
		return;
	effect_s *new_eff = alloc_effect(EF_ATTACK);
	effect_attack_data *d = (void*)new_eff->data;
	d->ent = ep;
	d->delay = 1;
	d->type = type;
	d->limb_tag = limb_tag;
	d->weapon_mat = weapon_mat;
	effect_prepend(s, new_eff);
}

void trigger_fill_cont(ent_ptr sp, effect_s *h, ent_ptr tp) {
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return;
	if (h->type != EF_LIMB_SLOT)
		return;
	effect_limb_slot_data *dh = (void*)h->data;
	effect_s *new_eff = alloc_effect(EF_M_FILL_CONT);
	effect_m_fill_cont_data *d = (void*)new_eff->data;
	d->hand_tag = dh->tag;
	d->target = tp;
	effect_prepend(s, new_eff);
}

void trigger_empty_cont(ent_ptr sp, effect_s *h) {
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return;
	if (h->type != EF_LIMB_SLOT)
		return;
	effect_limb_slot_data *dh = (void*)h->data;
	effect_s *new_eff = alloc_effect(EF_M_EMPTY_CONT);
	effect_m_empty_cont_data *d = (void*)new_eff->data;
	d->hand_tag = dh->tag;
	effect_prepend(s, new_eff);
}


void trigger_press_button(ent_ptr sp, effect_s *h, ent_ptr tp, effect_s *w) {
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return;
	if (h->type != EF_LIMB_SLOT || w->type != EF_MATERIAL)
		return;
	effect_limb_slot_data *dh = (void*)h->data;
	effect_material_data *dw = (void*)w->data;
	effect_s *new_eff = alloc_effect(EF_M_PRESS_BUTTON);
	effect_m_press_button_data *d = (void*)new_eff->data;
	d->hand_tag = dh->tag;
	d->target = tp;
	d->mat_tag = dw->tag;
	effect_prepend(s, new_eff);
}

void trigger_open_door(ent_ptr sp, effect_s *h, ent_ptr tp, int dir) {
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return;
	if (h->type != EF_LIMB_SLOT)
		return;
	effect_limb_slot_data *dh = (void*)h->data;
	effect_s *new_eff = alloc_effect(EF_M_OPEN_DOOR);
	effect_m_open_door_data *d = (void*)new_eff->data;
	d->dir = dir;
	d->hand_tag = dh->tag;
	d->target = tp;
	effect_prepend(s, new_eff);
}

void trigger_wear(ent_ptr sp, effect_s *h, ent_ptr b) {
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return;
	if (h->type != EF_LIMB_SLOT)
		return;
	effect_limb_slot_data *dh = (void*)h->data;
	effect_s *new_eff = alloc_effect(EF_M_WEAR);
	effect_m_wear_data *d = (void*)new_eff->data;
	d->hand_tag = dh->tag;
	d->body_part = b;
	effect_prepend(s, new_eff);
}

void hand_grab(ent_ptr ent, effect_s *hand, ent_ptr item, uint32_t tag) {
	if (hand->type != EF_LIMB_SLOT)
		return;
	effect_limb_slot_data *hand_data = (void*)hand->data;
	if (item == hand_data->item || item == ent)
		return;
	effect_limb_hand_data lhand_data;
	effect_ph_item_data ph_data;
	if (
		entity_load_effect(hand_data->item, EF_LIMB_HAND, &lhand_data) &&
		entity_load_effect(item, EF_PH_ITEM, &ph_data) &&
		entity_reachable(ent, hand, item) &&
		!entity_has_effect(item, EF_ROOTED)
		// TODO check if the object is possible to grab & lift
	) {
		effect_s *mat = entity_material_by_tag(ent_aptr(item), tag);
		if (
			lhand_data.item == ENT_NULL &&
			(ph_data.parent == ENT_NULL || ph_data.parent_type == PARENT_REF_PLACE) &&
			mat != NULL
		) {
			unparent_entity(item);
			detach_generic_entity(item);
			ph_data.parent = hand_data->item;
			ph_data.parent_type = PARENT_REF_HELD;
			lhand_data.item = item;
			lhand_data.grab_type = tag;
			entity_store_effect(hand_data->item, EF_LIMB_HAND, &lhand_data);
			entity_store_effect(item, EF_PH_ITEM, &ph_data);
		}
	}
}

void hand_grab_pile(ent_ptr s, effect_s *hand, ent_ptr item, int amount) {
	if (hand->type != EF_LIMB_SLOT)
		return;
	effect_limb_slot_data *hand_data = (void*)hand->data;
	effect_limb_hand_data lhand_data;
	effect_pile_data pd;
	if (
		!entity_load_effect(hand_data->item, EF_LIMB_HAND, &lhand_data) ||
		!entity_load_effect(item, EF_PILE, &pd) ||
		!entity_reachable(s, hand, item)
	) {
		return;
	}
	if (amount > pd.amount)
		amount = pd.amount;
	pd.amount -= amount;
	entity_s *new_ent = o_alloc_entity();
	{
		effect_s *new_eff = alloc_effect(EF_PH_ITEM);
		effect_ph_item_data *d = (void*)new_eff->data;
		d->parent = hand_data->item;
		d->parent_type = PARENT_REF_HELD;
		effect_prepend(new_ent, new_eff);
	}
	{
		effect_s *new_eff = alloc_effect(EF_PILE);
		effect_pile_data *d = (void*)new_eff->data;
		d->type = pd.type;
		d->amount = amount;
		effect_prepend(new_ent, new_eff);
	}
	entity_prepend(g_entities, new_ent);
	g_entities = new_ent;
	entity_store_effect(item, EF_PILE, &pd);
	lhand_data.item = ent_sptr(new_ent);
	entity_store_effect(hand_data->item, EF_LIMB_HAND, &lhand_data);
}

void hand_drop(ent_ptr ent, effect_s *hand) {
	(void)ent;
	if (hand->type != EF_LIMB_SLOT)
		return;
	effect_limb_slot_data *slot_data = (void*)hand->data;
	if (slot_data->item == ENT_NULL)
		return;
	effect_limb_hand_data hand_data;
	if (!entity_load_effect(slot_data->item, EF_LIMB_HAND, &hand_data))
		return;
	if (hand_data.item == ENT_NULL)
		return;
	lift_entity(hand_data.item);
	entity_s *itemp = ent_aptr(hand_data.item);
	if (itemp != NULL && !entity_has_effect(hand_data.item, EF_FALLING)) {
		effect_s *ef_fall = alloc_effect(EF_FALLING);
		effect_prepend(itemp, ef_fall);
	}
	hand_data.item = ENT_NULL;
}

void hand_put(ent_ptr ent, effect_s *hand, ent_ptr wp) {
	entity_s *w = ent_aptr(wp);
	if (w == NULL)
		return;
	if (hand->type != EF_LIMB_SLOT)
		return;
	effect_limb_slot_data *slot_data = (void*)hand->data;
	if (slot_data->item == ENT_NULL)
		return;
	effect_limb_hand_data hand_data;
	if (!entity_load_effect(slot_data->item, EF_LIMB_HAND, &hand_data))
		return;
	if (hand_data.item == ENT_NULL)
		return;
	effect_ph_item_data item_data;
	if (!entity_load_effect(hand_data.item, EF_PH_ITEM, &item_data))
		return;
	if (!entity_has_effect(wp, EF_TABLE) || !entity_reachable(ent, hand, wp))
		return;

	int x, y, z;
	entity_coords(ent, &x, &y, &z);
	entity_set_coords(hand_data.item, x, y, z);
	item_data.parent = wp;
	item_data.parent_type = PARENT_REF_PLACE;
	effect_s *new_eff = alloc_effect(EF_TABLE_ITEM);
	effect_table_item_data *d = (void*)new_eff->data;
	d->item = hand_data.item;
	effect_prepend(w, new_eff);
	entity_store_effect(hand_data.item, EF_PH_ITEM, &item_data);
	hand_data.item = ENT_NULL;
	entity_store_effect(slot_data->item, EF_LIMB_HAND, &hand_data);
}

void hand_throw(ent_ptr s, effect_s *h, int x, int y, int z, int speed) {
	if (h->type != EF_LIMB_SLOT)
		return;
	effect_limb_slot_data *limb_d = (void*)h->data;
	if (limb_d->item == ENT_NULL)
		return;
	effect_limb_hand_data hand_d;
	if (!entity_load_effect(limb_d->item, EF_LIMB_HAND, &hand_d))
		return;
	if (hand_d.item == ENT_NULL)
		return;
	effect_s *ef_tracer = alloc_effect(EF_TRACER);
	effect_tracer_data *tracer_d = (void*)ef_tracer->data;
	tracer_d->cur_x = 0;
	tracer_d->cur_y = 0;
	tracer_d->cur_z = 0;
	tracer_d->x = x;
	tracer_d->y = y;
	tracer_d->z = z;
	tracer_d->speed = speed;
	entity_s *hi = ent_aptr(hand_d.item);
	if (hi != NULL) {
		effect_prepend(hi, ef_tracer);
		effect_s *ph_item = effect_by_type(hi->effects, EF_PH_ITEM);
		if (ph_item != NULL) {
			effect_ph_item_data *item_data = (void*)ph_item->data;
			int x, y, z;
			entity_coords(s, &x, &y, &z);
			entity_set_coords(hand_d.item, x, y, z);
			item_data->parent = ENT_NULL;
			attach_generic_entity(hand_d.item);
		}
	}
	hand_d.item = ENT_NULL;
	entity_store_effect(limb_d->item, EF_LIMB_HAND, &hand_d);
}

void hand_fill_cont(ent_ptr s, effect_s *h, ent_ptr t) {
	if (h->type != EF_LIMB_SLOT)
		return;
	effect_limb_slot_data *hd = (void*)h->data;
	entity_s *hitem = ent_aptr(hd->item);
	if (hitem == NULL)
		return;
	effect_s *ef_lhand = effect_by_type(hitem->effects, EF_LIMB_HAND);
	if (ef_lhand == NULL)
		return;
	effect_limb_hand_data *lhd = (void*)ef_lhand->data;
	entity_s *lhitem = ent_aptr(lhd->item);
	if (lhitem == NULL)
		return;
	effect_s *held_item_cont = effect_by_type(lhitem->effects, EF_CONTAINER);
	if (held_item_cont == NULL)
		return;
	effect_container_data *cont_d = (void*)held_item_cont->data;
	if (cont_d->capacity <= container_get_amount(lhd->item))
		return;
	if (!entity_reachable(s, h, t))
		return;
	entity_s *ts = ent_aptr(t);
	if (ts == NULL)
		return;
	effect_s *t_liq = effect_by_type(ts->effects, EF_PH_LIQUID);
	if (t_liq == NULL)
		return;
	effect_ph_liquid_data *liq_d = (void*)t_liq->data;
	if (liq_d->amount == 0)
		return;
	liq_d->amount--;
	liquid_deduplicate(t);
	container_add_liquid(lhd->item, liq_d->type, 1);
}

void hand_empty_cont(ent_ptr s, effect_s *h) {
	(void)s;
	if (h->type != EF_LIMB_SLOT)
		return;
	effect_limb_slot_data *hd = (void*)h->data;
	entity_s *hitem = ent_aptr(hd->item);
	if (hitem == NULL)
		return;
	effect_s *ef_lhand = effect_by_type(hitem->effects, EF_LIMB_HAND);
	if (ef_lhand == NULL)
		return;
	effect_limb_hand_data *lhd = (void*)ef_lhand->data;
	entity_s *lhitem = ent_aptr(lhd->item);
	if (lhitem == NULL)
		return;
	effect_s *held_item_cont = effect_by_type(lhitem->effects, EF_CONTAINER);
	if (held_item_cont == NULL)
		return;
	for (effect_s *c = lhitem->effects; c != NULL; c = c->next) {
		if (c->type != EF_CONTAINER_ITEM)
			continue;
		effect_container_item_data *d = (void*)c->data;
		if (d->item != ENT_NULL)
			unparent_attach_entity(d->item);
	}
}

void hand_press_button(ent_ptr s, effect_s *h, ent_ptr tp, uint32_t mat_tag) {
	// TODO implement this for entities stored non-directly
	(void)s;
	entity_s *t = ent_aptr(tp);
	if (t == NULL)
		return;
	if (h->type != EF_LIMB_SLOT)
		return;
	effect_limb_slot_data *hd = (void*)h->data;
	entity_s *hitem = ent_aptr(hd->item);
	if (hitem == NULL)
		return;
	effect_s *ef_lhand = effect_by_type(hitem->effects, EF_LIMB_HAND);
	if (ef_lhand == NULL)
		return;
	effect_s *mat = entity_material_by_tag(t, mat_tag);
	if (mat == NULL)
		return;
	// effect_material_data *mat_d = (void*)mat->data;
	effect_s *new_eff = alloc_effect(EF_S_PRESS_BUTTON);
	effect_s_press_button_data *nd = (void*)new_eff->data;
	nd->mat_tag = mat_tag;
	effect_prepend(t, new_eff);
}

void dump_effect(effect_s *e, FILE *stream) {
	fwrite(&e->type, sizeof(int), 1, stream);
	effect_dump_t f = effect_dump_functions[e->type];
	if (f != NULL) {
		f(e, stream);
	} else if (effect_data_size[e->type] != 0) {
		fprintf(stderr, "[MSG] Un-dumpable effect %d\n", e->type);
	}
}

void dump_entity(entity_s *s, FILE *stream) {
	int ind = entity_get_index(s);
	fwrite(&ind, sizeof(int), 1, stream);
	fwrite(&s->common_type, sizeof(int), 1, stream);
	if (common_type_size[s->common_type] > 0) {
		fwrite(s->common_data, common_type_size[s->common_type], 1, stream);
	}
	int cnt = entity_num_effects(s) - 1; /* -1 for B_INDEX */
	fwrite(&cnt, sizeof(int), 1, stream);

	effect_s *e = s->effects;
	while (e != NULL) {
		if (e->type != EF_B_INDEX) {
			dump_effect(e, stream);
		}
		e = e->next;
	}
	e = s->effects;
	while (e != NULL) {
		entity_l_s *ls = effect_enlist(e);
		entity_l_s *lsc = ls;
		while (lsc != NULL) {
			dump_entity(ent_aptr(lsc->ent), stream);
			ls = lsc;
			lsc = lsc->next;
			o_free(ls);
		}
		e = e->next;
	}
}

void dump_sector(sector_s *s, FILE *stream) {
	for (int i = 0; i < G_SECTOR_SIZE; i ++) {
		for (int j = 0; j < G_SECTOR_SIZE; j ++) {
			for (int k = 0; k < G_SECTOR_SIZE; k ++) {
				entity_l_s *e = s->block_entities[i][j][k];
				while (e != NULL) {
					dump_entity(ent_aptr(e->ent), stream);
					e = e->next;
				}
			}
		}
	}
}

void dump_sector_bslice(sector_s *s, FILE *stream) {
	fwrite(&s->stored_id, sizeof(int), 1, stream);
	fwrite(&s->x, sizeof(int), 1, stream);
	fwrite(&s->y, sizeof(int), 1, stream);
	fwrite(&s->z, sizeof(int), 1, stream);
	int nskip = 0;
	for (int i = 0; i < G_SECTOR_SIZE; i++) {
		for (int k = 0; k < G_SECTOR_SIZE; k++) {
			for (int j = 0; j < G_SECTOR_SIZE; j++) {
				if (s->block_blocks[i][j][k].type != BLK_EMPTY) {
					if (nskip != -1) {
						fwrite(&nskip, sizeof(int), 1, stream);
					}
					fwrite(&s->block_blocks[i][j][k].type, sizeof(int), 1, stream);
					fwrite(&s->block_blocks[i][j][k].dur, sizeof(int), 1, stream);
					nskip = -1;
				} else {
					if (nskip == -1) {
						int t = -1;
						fwrite(&t, sizeof(int), 1, stream);
						nskip = 0;
					}
					nskip++;
				}
			}
		}
	}
	int t = -1;
	if (nskip == -1)
		fwrite(&t, sizeof(int), 1, stream);
	fwrite(&t, sizeof(int), 1, stream);
}

void entity_enumerate(entity_s *s, int *ent_id) {
	entity_set_index(s, *ent_id);
	(*ent_id)++;
	effect_s *ef = s->effects;
	while (ef != NULL) {
		{
			entity_l_s *ls = effect_enlist(ef);
			entity_l_s *lsc = ls;
			while (lsc != NULL) {
				entity_enumerate(ent_aptr(lsc->ent), ent_id);
				ls = lsc;
				lsc = lsc->next;
				o_free(ls);
			}
		}
		ef = ef->next;
	}
}

void sector_enumerate_rec(sector_s *s, int *ent_id, int *bslice_id) {
	if (s == NULL)
		return;
	for (int i = 0; i < G_SECTOR_SIZE; i ++) {
		for (int j = 0; j < G_SECTOR_SIZE; j ++) {
			for (int k = 0; k < G_SECTOR_SIZE; k ++) {
				entity_l_s *e = s->block_entities[i][j][k];
				while (e != NULL) {
					entity_enumerate(ent_aptr(e->ent), ent_id);
					e = e->next;
				}
			}
		}
	}
	s->stored_id = *bslice_id;
	++*bslice_id;
	sector_enumerate_rec(s->ch[0], ent_id, bslice_id);
	sector_enumerate_rec(s->ch[1], ent_id, bslice_id);
}

void sector_dump_rec(sector_s *s, FILE *stream) {
	if (s == NULL)
		return;
	dump_sector(s, stream);
	sector_dump_rec(s->ch[0], stream);
	sector_dump_rec(s->ch[1], stream);
}

void sector_dump_bslice_rec(sector_s *s, FILE *stream) {
	if (s == NULL)
		return;
	dump_sector_bslice(s, stream);
	sector_dump_bslice_rec(s->ch[0], stream);
	sector_dump_bslice_rec(s->ch[1], stream);
}

void dump_sector_list(sector_s *s, FILE *stream) {
	int ent_id = 0, bslice_id = 0;
	sector_enumerate_rec(s, &ent_id, &bslice_id);
	fwrite(&ent_id, sizeof(int), 1, stream);
	fwrite(&bslice_id, sizeof(int), 1, stream);
	sector_dump_bslice_rec(s, stream);
	char magic[4] = { 'A', 'B', 'C', 'D' };
	fwrite(magic, 4, 1, stream);
	sector_dump_rec(s, stream);
}

void unload_entity(entity_s *s) {
	if (effect_by_type(s->effects, EF_B_NONEXISTENT) == NULL) {
		effect_s *new_eff = alloc_effect(EF_B_NONEXISTENT);
		effect_prepend(s, new_eff);
	}
	effect_s *c = s->effects;
	while (c != NULL) {
		entity_l_s *ls = effect_enlist(c);
		entity_l_s *lsc = ls;
		while (lsc != NULL) {
			unload_entity(ent_aptr(lsc->ent));
			ls = lsc;
			lsc = lsc->next;
			o_free(ls);
		}
		c = c->next;
	}
}

entity_s *load_sector_list(FILE *stream) {
	int n_ent, n_bslices;
	fread(&n_ent, sizeof(int), 1, stream);
	fread(&n_bslices, sizeof(int), 1, stream);
	// there is exactly one bslice per each stored sector
	sector_s **a_sec = o_malloc(sizeof(sector_s*) * n_bslices);
	for (int i = 0; i < n_bslices; i++) {
		scan_bslice(stream, a_sec, n_bslices);
	}
	char magic[4];
	fread(magic, 4, 1, stream);
	if (magic[0] != 'A' || magic[1] != 'B' || magic[2] != 'C' || magic[3] != 'D') {
		fprintf(stderr, "Boundary bslice/entity magic mismatch %.4s\n", magic);
		o_free(a_sec);
		return NULL;
	}
	entity_s **a_ent = o_malloc(sizeof(entity_s*) * n_ent);
	entity_s *prev_ent = NULL;
	for (int i = 0; i < n_ent; i ++) {
		a_ent[i] = o_alloc_entity();
		a_ent[i]->effects = NULL;
		a_ent[i]->prev = prev_ent;
		prev_ent = a_ent[i];
	}
	for (int i = 0; i + 1 < n_ent; i ++) {
		a_ent[i]->next = a_ent[i + 1];
	}
	for (int i = 0; i < n_ent; i ++) {
		scan_entity(n_ent, a_ent, n_bslices, a_sec, stream);
	}
	entity_s *t = a_ent[0];
	o_free(a_ent);
	return t;
}

effect_s* scan_effect(int n_ent, entity_s **a_ent, int n_sec, sector_s **a_sec, FILE *stream) {
	int type;
	fread(&type, sizeof(int), 1, stream);
	effect_s *eff = alloc_effect(type);
	effect_scan_t scanner = effect_scan_functions[type];
	if (scanner != NULL) {
		scanner(eff, n_ent, a_ent, n_sec, a_sec, stream);
	}
	return eff;
}

entity_s* scan_entity(int n_ent, entity_s **a_ent, int n_sec, sector_s **a_sec, FILE *stream) {
	int id;
	fread(&id, sizeof(int), 1, stream);
	if (id >= n_ent) {
		/* Fail badly */
		fprintf(stderr, "Failed badly\n");
		fflush(stderr);
		return NULL;
	} else {
		fread(&a_ent[id]->common_type, sizeof(int), 1, stream);
		if (common_type_size[a_ent[id]->common_type] > 0) {
			fread(a_ent[id]->common_data, common_type_size[a_ent[id]->common_type], 1, stream);
		}
		int id_eff;
		fread(&id_eff, sizeof(int), 1, stream);
		effect_s *la = a_ent[id]->effects;
		for (int i = 0; i < id_eff; i ++) {
			effect_s *c = scan_effect(n_ent, a_ent, n_sec, a_sec, stream);
			c->prev = la;
			c->next = NULL;
			if (la != NULL) {
				la->next = c;
			} else {
				a_ent[id]->effects = c;
			}
			la = c;
		}
		return a_ent[id];
	}
}

void scan_bslice(FILE *stream, sector_s **id_tags, int n_id_tags) {
	int co[3], id;
	fread(&id, sizeof(int), 1, stream);
	fread(co, sizeof(int), 3, stream);
	sector_s *sec = sector_get_sector(g_sectors, co[0], co[1], co[2]);
	if (sec == NULL) {
		sec = o_alloc_sector();
		sec->stored_id = id;
		sec->x = co[0];
		sec->y = co[1];
		sec->z = co[2];
		sec->prio = rng_bigrange(g_dice);
		sec->ch[0] = NULL;
		sec->ch[1] = NULL;
		memset(sec->block_entities, 0, sizeof(sec->block_entities));
		memset(sec->block_blocks, 0, sizeof(sec->block_blocks));
		g_sectors = sector_insert(g_sectors, sec);
		if (id < n_id_tags && id >= 0) {
			id_tags[id] = sec;
		} else {
			fprintf(stderr, "sector id too large %d/%d\n", id, n_id_tags);
		}
	}
	int nskip;
	int c = 0;
	while (fread(&nskip, sizeof(int), 1, stream) && nskip != -1) {
		c += nskip;
		int block_type, block_dur;
		while (fread(&block_type, sizeof(int), 1, stream) && block_type != -1) {
			fread(&block_dur, sizeof(int), 1, stream);
			int
				a = c / G_SECTOR_SIZE / G_SECTOR_SIZE,
				b = c % G_SECTOR_SIZE,
				c_ = c / G_SECTOR_SIZE % G_SECTOR_SIZE;
			sec->block_blocks[a][b][c_].type = block_type;
			sec->block_blocks[a][b][c_].dur = block_dur;
			c++;
		}
	}
}

void unload_sector(sector_s *s) {
	for (int i = 0; i < G_SECTOR_SIZE; i ++) {
		for (int j = 0; j < G_SECTOR_SIZE; j ++) {
			for (int k = 0; k < G_SECTOR_SIZE; k ++) {
				entity_l_s *c = s->block_entities[i][j][k];
				while (c != NULL) {
					unload_entity(ent_aptr(c->ent));
					c = c->next;
				}
				entity_l_s_free(s->block_entities[i][j][k]);
				s->block_entities[i][j][k] = NULL;
			}
		}
	}
}

int effect_rem_ph_item(entity_s *s, effect_s *e) {
	effect_ph_item_data *d = (void*)e->data;
	while (d->parent != ENT_NULL && entity_has_effect(d->parent, EF_B_NONEXISTENT)) {
		lift_entity(ent_sptr(s));
	}
	return 0;
}

int entity_get_index(entity_s *s) {
	int t;
	effect_s *eff = effect_by_type(s->effects, EF_B_INDEX);
	if (eff == NULL) {
		t = -1;
	} else {
		effect_b_index_data *d = (void*)eff->data;
		t = d->index;
	}
	return t;
}

void entity_set_index(entity_s *s, int i) {
	effect_s *eff = effect_by_type(s->effects, EF_B_INDEX);
	if (eff == NULL) {
		eff = alloc_effect(EF_B_INDEX);
		effect_prepend(s, eff);
	}
	effect_b_index_data *d = (void*)eff->data;
	d->index = i;
}

int entity_num_effects(entity_s *s) {
	int r = 0;
	effect_s *e = s->effects;
	while (e != NULL) {
		e = e->next;
		r ++;
	}
	return r;
}

entity_l_s* effect_enlist(effect_s *s) {
	if (s->type == EF_LIMB_SLOT) {
		effect_limb_slot_data *d = (void*)s->data;
		if (d->item != ENT_NULL) {
			entity_l_s *r = o_malloc(sizeof(entity_l_s));
			r->prev = NULL;
			r->next = NULL;
			r->ent = d->item;
			return r;
		}
		return NULL;
	}
	if (s->type == EF_LIMB_HAND) {
		effect_limb_hand_data *d = (void*)s->data;
		if (d->item != ENT_NULL) {
			entity_l_s *r = o_malloc(sizeof(entity_l_s));
			r->prev = NULL;
			r->next = NULL;
			r->ent = d->item;
			return r;
		}
		return NULL;
	}
	if (s->type == EF_TABLE_ITEM) {
		effect_table_item_data *d = (void*)s->data;
		if (d->item != ENT_NULL) {
			entity_l_s *r = o_malloc(sizeof(entity_l_s));
			r->prev = NULL;
			r->next = NULL;
			r->ent = d->item;
			return r;
		}
		return NULL;
	}
	if (s->type == EF_CONTAINER_ITEM) {
		effect_container_item_data *d = (void*)s->data;
		if (d->item != ENT_NULL) {
			entity_l_s *r = o_malloc(sizeof(entity_l_s));
			r->prev = NULL;
			r->next = NULL;
			r->ent = d->item;
			return r;
		}
		return NULL;
	}
	return NULL;
}

entity_l_s* entity_enlist(entity_s *s) {
	if (s == NULL)
		return NULL;
	effect_s *c = s->effects;
	entity_l_s *x = NULL;
	while (c != NULL) {
		entity_l_s *t = effect_enlist(c);
		while (t != NULL) {
			if (x != NULL) {
				x->next = t;
			}
			t->prev = x;
			x = t;
			t = t->next;
		}
		c = c->next;
	}
	if (x != NULL) {
		while (x->prev != NULL)
			x = x->prev;
	}
	return x;
}

entity_l_s* sector_get_block_entities_indirect(sector_s *s, int x, int y, int z) {
	if (s == NULL)
		return NULL;
	entity_l_s *el_orig = sector_get_block_entities(s, x, y, z);
	entity_l_s *el;
	if (el_orig == NULL) {
		el = NULL;
	} else {
		el = entity_l_s_copy(el_orig);
		entity_l_s *t = el;
		while (t->next != NULL)
			t = t->next;
		entity_l_s *h = t;
		while (t != NULL) {
			entity_l_s *a = entity_enlist(ent_aptr(t->ent));
			while (a != NULL) {
				if (h != NULL) {
					h->next = a;
				}
				a->prev = h;
				h = a;
				a = a->next;
			}
			t = t->prev;
		}
	}
	if (s->block_blocks[x][y][z].type != BLK_EMPTY) {
		entity_l_s *a = o_malloc(sizeof(entity_l_s));
		a->ent = ent_cptr(s, x, y, z);
		a->prev = NULL;
		a->next = el;
		if (el != NULL)
			el->prev = a;
		el = a;
	}
	return el;
}

entity_l_s* entity_l_s_copy(entity_l_s *eo) {
	if (eo == NULL)
		return NULL;
	entity_l_s *e = NULL;
	while (eo != NULL) {
		entity_l_s *et = o_malloc(sizeof(entity_l_s));
		if (e != NULL) {
			e->next = et;
		}
		et->prev = e;
		et->next = NULL;
		et->ent = eo->ent;
		e = et;
		eo = eo->next;
	}
	while (e->prev != NULL)
		e = e->prev;
	return e;
}

void entity_l_s_free(entity_l_s *s) {
	while (s != NULL) {
		entity_l_s *n = s->next;
		o_free(s);
		s = n;
	}
}

int entity_reachable(ent_ptr s, effect_s *limb, ent_ptr e) {
	if (limb->type != EF_LIMB_SLOT) {
		return 0;
	}
	effect_limb_slot_data *limb_data = (void*)limb->data;
	if (limb_data->item == ENT_NULL) {
		return 0;
	}
	int xs, ys, zs;
	int xe, ye, ze;
	if (!entity_coords(s, &xs, &ys, &zs))
		return 0;
	if (!entity_coords(e, &xe, &ye, &ze))
		return 0;
	if (abs(xs - xe) > 1 || abs(ys - ye) > 1 || abs(zs - ze) > 1) {
		return 0;
	}
	if (xs == xe && ys == ye && zs == ze) {
		return 1;
	}
	if (entity_has_effect(e, EF_PH_BLOCK)) {
		return 1;
	}
	/* TODO high/low limb types */
	return 0;
}

ent_ptr tracer_check_bump(ent_ptr s, int x, int y, int z) {
	int cx = 0, cy = 0, cz = 0, sx = x, sy = y, sz = z;
	int ex, ey, ez;
	if (!entity_coords(s, &ex, &ey, &ez)) {
		return ENT_NULL;
	}
	coord_normalize(&sx, &cx);
	coord_normalize(&sy, &cy);
	coord_normalize(&sz, &cz);
	sector_s *sect = sector_get_sector(g_sectors, cx, cy, cz);
	if (sect == NULL)
		return ENT_NULL;
	entity_l_s *el = sector_get_block_entities(sect, sx, sy, sz);
	entity_l_s *cur;
	int total_hit_val = 0;
	for (cur = el; cur != NULL; cur = cur->next) {
		if (entity_has_effect(cur->ent, EF_PH_ITEM)) {
			total_hit_val += entity_size(cur->ent);
		}
	}
	int mx = 256;
	if (mx < total_hit_val)
		mx = total_hit_val + 1;
	// TODO rng_next_range
	int random_val = rng_next(g_dice) % mx;
	if (random_val > total_hit_val)
		return ENT_NULL;
	int cur_hit_val = 0;
	for (cur = el; cur != NULL; cur = cur->next) {
		if (entity_has_effect(cur->ent, EF_PH_ITEM)) {
			cur_hit_val += entity_size(cur->ent);
			if (cur_hit_val >= random_val)
				return cur->ent;
		}
	}
	return ENT_NULL;
}

void unparent_entity(ent_ptr sp) {
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return;
	effect_s *ph = effect_by_type(s->effects, EF_PH_ITEM);
	if (ph == NULL)
		return;
	effect_ph_item_data *d = (void*)ph->data;
	if (d->parent == ENT_NULL)
		return;
	switch (d->parent_type) {
	case PARENT_REF_HELD: {
		int x, y, z;
		entity_s *p = ent_aptr(d->parent);
		effect_s *t = p->effects;
		if (entity_coords(sp, &x, &y, &z)) {
			d->parent = ENT_NULL;
			entity_set_coords(sp, x, y, z);
		} else {
			d->parent = ENT_NULL;
		}
		while (t != NULL) {
			if (t->type == EF_LIMB_HAND) {
				effect_limb_hand_data *td = (void*)t->data;
				if (td->item == sp) {
					td->item = ENT_NULL;
					break;
				}
			}
			t = t->next;
		}
	} break;
	case PARENT_REF_PLACE: {
		int x, y, z;
		entity_s *p = ent_aptr(d->parent);
		effect_s *t = p->effects;
		if (entity_coords(sp, &x, &y, &z)) {
			d->parent = ENT_NULL;
			entity_set_coords(sp, x, y, z);
		} else {
			d->parent = ENT_NULL;
		}
		while (t != NULL) {
			if (t->type == EF_TABLE_ITEM) {
				effect_table_item_data *td = (void*)t->data;
				if (td->item == sp) {
					effect_unlink(p, t);
					free_effect(t);
					break;
				}
			}
			t = t->next;
		}
	} break;
	case PARENT_REF_CONT: {
		int x, y, z;
		entity_s *p = ent_aptr(d->parent);
		effect_s *t = p->effects;
		if (entity_coords(sp, &x, &y, &z)) {
			d->parent = ENT_NULL;
			entity_set_coords(sp, x, y, z);
		} else {
			d->parent = ENT_NULL;
		}
		while (t != NULL) {
			if (t->type == EF_CONTAINER_ITEM) {
				effect_table_item_data *td = (void*)t->data;
				if (td->item == sp) {
					effect_unlink(p, t);
					free_effect(t);
					break;
				}
			}
			t = t->next;
		}
	} break;
	case PARENT_REF_LIMB: {
		int x, y, z;
		entity_s *p = ent_aptr(d->parent);
		effect_s *t = p->effects;
		if (entity_coords(sp, &x, &y, &z)) {
			d->parent = ENT_NULL;
			entity_set_coords(sp, x, y, z);
		} else {
			d->parent = ENT_NULL;
		}
		while (t != NULL) {
			if (t->type == EF_LIMB_SLOT) {
				effect_limb_slot_data *td = (void*)t->data;
				if (td->item == sp) {
					td->item = ENT_NULL;
					break;
				}
			}
			t = t->next;
		}
	} break;
	case PARENT_REF_BLOCK_WET: {
		// TODO what to do?
	} break;
	}
}

void unparent_attach_entity(ent_ptr s) {
	unparent_entity(s);
	attach_generic_entity(s);
}

void lift_entity(ent_ptr sp) {
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return;
	effect_s *ph = effect_by_type(s->effects, EF_PH_ITEM);
	if (ph == NULL)
		return;
	effect_ph_item_data *d = (void*)ph->data;
	if (d->parent == ENT_NULL)
		return;
	// TODO redo this for non-aptr parent (if non-aptr parents are allowed at all)
	entity_s *p = ent_aptr(d->parent);
	parent_ref_type pt = d->parent_type;
	unparent_entity(sp);
	do {
		effect_s *pph = effect_by_type(p->effects, EF_PH_ITEM);
		if (pph == NULL) {
			p = NULL;
			break;
		}
		effect_ph_item_data *pd = (void*)pph->data;
		p = ent_aptr(pd->parent);
		pt = pd->parent_type;
	} while (p != NULL && (pt == PARENT_REF_HELD || pt == PARENT_REF_LIMB));
	if (p == NULL) {
		attach_generic_entity(sp);
		return;
	}
	switch (pt) {
	case PARENT_REF_HELD: break;
	case PARENT_REF_LIMB: break;
	case PARENT_REF_PLACE: {
		effect_s *new_ef = alloc_effect(EF_TABLE_ITEM);
		effect_table_item_data *d = (void*)new_ef->data;
		d->item = sp;
		effect_prepend(p, new_ef);
	} break;
	case PARENT_REF_CONT: {
		effect_s *new_ef = alloc_effect(EF_CONTAINER_ITEM);
		effect_container_item_data *d = (void*)new_ef->data;
		d->item = sp;
		effect_prepend(p, new_ef);
	} break;
	case PARENT_REF_BLOCK_WET: {
		// TODO this one isn't to be lifted
	} break;
	}
}

int entity_weight(ent_ptr s) {
	effect_ph_item_data d;
	if (!entity_load_effect(s, EF_PH_ITEM, &d))
		return 0;
	effect_ph_liquid_data dl;
	if (entity_load_effect(s, EF_PH_LIQUID, &dl))
		return dl.amount;
	return d.weight;
}

int entity_size(ent_ptr s) {
	effect_ph_item_data d;
	if (!entity_load_effect(s, EF_PH_ITEM, &d))
		return 0;
	effect_ph_liquid_data dl;
	if (entity_load_effect(s, EF_PH_LIQUID, &dl)) {
		return dl.amount;
	}
	return 1;
}

// List the attacks that `s' can try to perform at `o'
attack_l_s* entity_list_attacks(ent_ptr sp, ent_ptr o) {
	(void)o;
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return NULL;
	attack_l_s *r = NULL;
	attack_type t;
	effect_s *ef = s->effects;
	while (ef != NULL) {
		if (ef->type == EF_LIMB_SLOT) {
			effect_limb_slot_data *d = (void*)ef->data;
			for (t = 0; t < ATK_N_COUNT; t++) {
				entity_s *tool = ent_aptr(attack_used_tool(sp, d->tag, t));
				if (tool == NULL)
					continue;
				effect_s *ef1 = tool->effects;
				while (ef1 != NULL) {
					if (ef1->type == EF_MATERIAL) {
						effect_material_data *mat_d = (void*)ef1->data;
						uint32_t sel_tag = mat_d->tag;
						if (attack_type_possible(sp, d->tag, t, sel_tag)) {
							attack_l_s *th = o_malloc(sizeof(attack_l_s));
							th->limb_slot_tag = d->tag;
							th->limb_entity = ent_aptr(d->item);
							th->type = t;
							th->tool = tool;
							th->tool_mat_tag = sel_tag;
							th->prev = NULL;
							th->next = r;
							if (r != NULL)
								r->prev = th;
							r = th;
						}
					}
					ef1 = ef1->next;
				}
			}
		}
		ef = ef->next;
	}
	return r;
}

int container_get_amount(ent_ptr sp) {
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return 0;
	int r = 0;
	effect_s *t = effect_by_type(s->effects, EF_CONTAINER);
	if (t == NULL)
		return 0;
	effect_s *c = s->effects;
	while (c != NULL) {
		if (c->type == EF_CONTAINER_ITEM) {
			effect_container_item_data *d = (void*)c->data;
			if (d->item != ENT_NULL)
				r += entity_size(d->item);
		}
		c = c->next;
	}
	return r;
}

void container_add_liquid(ent_ptr sp, liquid_type t, int amount) {
	entity_s *s = ent_aptr(sp);
	if (s == NULL)
		return;
	effect_s *e = effect_by_type(s->effects, EF_CONTAINER);
	if (e == NULL)
		return;
	effect_s *c = s->effects;
	while (c != NULL) {
		if (c->type == EF_CONTAINER_ITEM) {
			effect_container_item_data *d = (void*)c->data;
			if (d->item != ENT_NULL) {
				effect_s *liq = effect_by_type(ent_aptr(d->item)->effects, EF_PH_LIQUID);
				if (liq != NULL) {
					effect_ph_liquid_data *liq_d = (void*)liq->data;
					if (liq_d->type == t) {
						liq_d->amount += amount;
						return;
					}
				}
			}
		}
		c = c->next;
	}
	entity_s *new_item = o_alloc_entity();
	new_item->effects = NULL;
	new_item->next = NULL;
	new_item->prev = NULL;
	{
		effect_s *ef_liq = alloc_effect(EF_PH_LIQUID);
		effect_ph_liquid_data *d = (void*)ef_liq->data;
		d->amount = amount;
		d->type = t;
		effect_prepend(new_item, ef_liq);
	}
	{
		effect_s *ef_ph = alloc_effect(EF_PH_ITEM);
		effect_ph_item_data *d = (void*)ef_ph->data;
		d->weight = 0;
		d->x = 0;
		d->y = 0;
		d->z = 0;
		d->parent = ent_sptr(s);
		d->parent_type = PARENT_REF_CONT;
		effect_prepend(new_item, ef_ph);
	}
	entity_prepend(g_entities, new_item);
	g_entities = new_item;
	effect_s *new_eff = alloc_effect(EF_CONTAINER_ITEM);
	effect_container_item_data *d = (void*)new_eff->data;
	d->item = ent_sptr(new_item);
	effect_prepend(s, new_eff);
}

void top_add_liquid(int x, int y, int z, liquid_type t, int amount) {
	int xc = 0, yc = 0, zc = 0;
	coord_normalize(&x, &xc);
	coord_normalize(&y, &yc);
	coord_normalize(&z, &zc);
	sector_s *sec = sector_get_sector(g_sectors, xc, yc, zc);
	if (sec == NULL)
		return;
	entity_l_s *l = sector_get_block_entities(sec, x, y, z);
	for (; l != NULL; l = l->next) {
		effect_ph_liquid_data ld;
		if (entity_load_effect(l->ent, EF_PH_LIQUID, &ld) && ld.type == t) {
			ld.amount += amount;
			entity_store_effect(l->ent, EF_PH_LIQUID, &ld);
			return;
		}
	}
	entity_s *new_ent = o_alloc_entity();
	// TODO this doesn't get processed by apply_liquid_movement & friends
	new_ent->common_type = CT_LIQUID;
	{
		effect_ph_liquid_data ld;
		ld.type = t;
		ld.amount = amount;
		entity_store_effect(ent_sptr(new_ent), EF_PH_LIQUID, &ld);
	}
	effect_s *new_eff = alloc_effect(EF_PH_ITEM);
	{
		effect_ph_item_data *d = (void*)new_eff->data;
		d->x = x + xc * G_SECTOR_SIZE;
		d->y = y + yc * G_SECTOR_SIZE;
		d->z = z + zc * G_SECTOR_SIZE;
		d->weight = 0;
		d->parent = ENT_NULL;
		effect_prepend(new_ent, new_eff);
	}
	entity_prepend(g_entities, new_ent);
	g_entities = new_ent;
	attach_generic_entity(ent_sptr(new_ent));
}

void top_add_pile(int x, int y, int z, pile_type t, int amount) {
	int xc = 0, yc = 0, zc = 0;
	coord_normalize(&x, &xc);
	coord_normalize(&y, &yc);
	coord_normalize(&z, &zc);
	sector_s *sec = sector_get_sector(g_sectors, xc, yc, zc);
	if (sec == NULL)
		return;
	entity_l_s *l = sector_get_block_entities(sec, x, y, z);
	for (; l != NULL; l = l->next) {
		effect_pile_data pd;
		if (entity_load_effect(l->ent, EF_PILE, &pd) && pd.type == t) {
			pd.amount += amount;
			entity_store_effect(l->ent, EF_PILE, &pd);
			return;
		}
	}
	entity_s *new_ent = o_alloc_entity();
	effect_s *new_eff = alloc_effect(EF_PILE);
	{
		effect_pile_data *pd = (void*)new_eff->data;
		pd->type = t;
		pd->amount = amount;
		effect_prepend(new_ent, new_eff);
	}
	new_eff = alloc_effect(EF_PH_ITEM);
	{
		effect_ph_item_data *d = (void*)new_eff->data;
		d->x = x + xc * G_SECTOR_SIZE;
		d->y = y + yc * G_SECTOR_SIZE;
		d->z = z + zc * G_SECTOR_SIZE;
		d->weight = 0;
		d->parent = ENT_NULL;
		effect_prepend(new_ent, new_eff);
	}
	entity_prepend(g_entities, new_ent);
	g_entities = new_ent;
	attach_generic_entity(ent_sptr(new_ent));
}

void dmg_deal(ent_ptr s, damage_type t, int v) {
	// TODO if it triggers a complicated reaction, delay the signal
	// by sending it as an effect
	// Currently, EF_S_DMG isn't processed at all
	// TODO make this work for entities with multiple EF_MATERIAL
	if (s == ENT_NULL)
		return;
	effect_material_data mat_d;
	if (!entity_load_effect(s, EF_MATERIAL, &mat_d))
		return;
	int dmg_val = v;
	if (t == DMGT_FIRE && mat_d.type != MAT_WOOD && mat_d.type != MAT_PLANT)
		goto NO_DMG;
	if ((t == DMGT_BLUNT || t == DMGT_CUT) && mat_d.type == MAT_GLASS)
		dmg_val *= 5;
	mat_d.dur -= dmg_val;
	entity_store_effect(s, EF_MATERIAL, &mat_d);
NO_DMG:
	if (mat_d.dur <= 0) {
		entity_s *sa = ent_aptr(s);
		sector_s *sec;
		int x, y, z;
		if (sa != NULL) {
			effect_s *new_ef = alloc_effect(EF_B_NONEXISTENT);
			effect_prepend(ent_aptr(s), new_ef);
		} else if ((sec = ent_acptr(s, &x, &y, &z)) != NULL) {
			sec->block_blocks[x][y][z].type = BLK_EMPTY;
			sec->block_blocks[x][y][z].dur = 0;
		}
	}
}

#include "gen-loaders.h"
