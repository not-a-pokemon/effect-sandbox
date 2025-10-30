#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "entity.h"
#include "omalloc.h"
#include "rng.h"

sectors_s *g_sectors;
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

sector_s *sector_get_sector(sectors_s *s, int x, int y, int z) {
	while (s != NULL) {
		if (s->x == x && s->y == y && s->z == z) {
			return s;
		}
		s = s->snext;
	}
	return NULL;
}

entity_l_s *sector_get_block_entities(sector_s *s, int x, int y, int z) {
	if (s == NULL)
		return NULL;
	return s->block_entities[x][y][z];
}

int sector_get_block_floor_up(sector_s *s, int x, int y, int z) {
	entity_l_s *e = sector_get_block_entities(s, x, y, z);
	while (e != NULL) {
		effect_s *ef = effect_by_type(e->ent->effects, EF_PH_BLOCK);
		if (ef != NULL) {
			effect_ph_block_data *d = (void*)ef->data;
			if (d->floor_up) {
				return 1;
			}
		}
		e = e->next;
	}
	return 0;
}

int sector_get_block_floor(sector_s *s, int x, int y, int z) {
	entity_l_s *e = sector_get_block_entities(s, x, y, z);
	while (e != NULL) {
		effect_s *ef = effect_by_type(e->ent->effects, EF_PH_BLOCK);
		if (ef != NULL) {
			effect_ph_block_data *d = (void*)ef->data;
			if (d->floor) {
				return 1;
			}
		}
		e = e->next;
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
		effect_s *ef = effect_by_type(e->ent->effects, EF_PH_BLOCK);
		if (ef != NULL) {
			effect_ph_block_data *d = (void*)ef->data;
			if (d->block_movement) {
				return 1;
			}
		}
		e = e->next;
	}
	return 0;
}

int sector_get_block_stairs(sector_s *s, int x, int y, int z) {
	entity_l_s *e = sector_get_block_entities(s, x, y, z);
	while (e != NULL) {
		effect_s *ef = effect_by_type(e->ent->effects, EF_PH_BLOCK);
		if (ef != NULL) {
			effect_ph_block_data *d = (void*)ef->data;
			if (d->stair) {
				return 1;
			}
		}
		e = e->next;
	}
	return 0;
}

int sector_get_block_slope(sector_s *s, int x, int y, int z) {
	entity_l_s *e = sector_get_block_entities(s, x, y, z);
	while (e != NULL) {
		effect_s *ef = effect_by_type(e->ent->effects, EF_PH_BLOCK);
		if (ef != NULL) {
			effect_ph_block_data *d = (void*)ef->data;
			if (d->slope) {
				return 1;
			}
		}
		e = e->next;
	}
	return 0;
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

void apply_triggers(entity_s *s) {
	{
		effect_s *ef = effect_by_type(s->effects, EF_A_PRESSURE_PLATE);
		if (ef != NULL) {
			effect_a_pressure_plate_data *press_d = (void*)ef->data;
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
			effect_s *ph_b = effect_by_type(s->effects, EF_PH_BLOCK);
			if (ph_b != NULL) {
				effect_ph_block_data *d = (void*)ph_b->data;
				d->floor = pressure < press_d->thresold;
			}
		}
	}
	{
		effect_s *ef = effect_by_type(s->effects, EF_A_CIRCLE_MOVE);
		if (ef != NULL) {
			if (effect_by_type(s->effects, EF_BLOCK_MOVE) == NULL) {
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

void apply_instants(entity_s *s) {
	{
		effect_s *ef = effect_by_type(s->effects, EF_FIRE);
		if (ef != NULL) {
			effect_s *mat = effect_by_type(s->effects, EF_MATERIAL);
			effect_material_data *mat_d = mat == NULL ? NULL : (void*)mat->data;
			if (mat == NULL || (mat_d->type != MAT_WOOD && mat_d->type != MAT_PLANT)) {
				effect_unlink(s, ef);
				free_effect(ef);
			} else {
				effect_s *new_ef = alloc_effect(EF_S_DMG);
				effect_s_dmg_data *d = (void*)new_ef->data;
				d->type = DMGT_FIRE;
				d->val = 1;
				effect_prepend(s, new_ef);
			}
		}
	}
	{
		effect_s *ef = effect_by_type(s->effects, EF_M_GRAB);
		if (ef != NULL) {
			effect_m_grab_data *d = (void*)ef->data;
			hand_grab(s, entity_limb_by_tag(s, d->eff_tag), d->ent, d->mat_tag);
			effect_unlink(s, ef);
			free_effect(ef);
		}
	}
	{
		effect_s *ef = effect_by_type(s->effects, EF_M_DROP);
		if (ef != NULL) {
			effect_m_drop_data *d = (void*)ef->data;
			hand_drop(s, entity_limb_by_tag(s, d->eff_tag));
			effect_unlink(s, ef);
			free_effect(ef);
		}
	}
	{
		effect_s *ef = effect_by_type(s->effects, EF_M_PUT);
		if (ef != NULL) {
			effect_m_put_data *d = (void*)ef->data;
			hand_put(s, entity_limb_by_tag(s, d->eff_tag), d->where);
			effect_unlink(s, ef);
			free_effect(ef);
		}
	}
	{
		effect_s *ef = effect_by_type(s->effects, EF_M_THROW);
		if (ef != NULL) {
			effect_m_throw_data *d = (void*)ef->data;
			hand_throw(s, entity_limb_by_tag(s, d->eff_tag), d->x, d->y, d->z, d->speed);
			effect_unlink(s, ef);
			free_effect(ef);
		}
	}
#if 0
	{
		effect_s *ef = effect_by_type(s->effects, EF_M_TOUCH);
		if (ef != NULL) {
			effect_m_touch_data *d = (void*)ef->data;
			if (entity_reachable(s, entity_limb_by_tag(s, d->eff_tag), d->ent)) {
				entity_s *target = d->ent;
				effect_s *new_ef = alloc_effect(EF_S_TOUCH);
				effect_prepend(target, new_ef);
			}
			effect_unlink(s, ef);
			free_effect(ef);
		}
	}
#endif
	{
		effect_s *ef = effect_by_type(s->effects, EF_M_FILL_CONT);
		if (ef != NULL) {
			effect_m_fill_cont_data *d = (void*)ef->data;
			hand_fill_cont(s, entity_limb_by_tag(s, d->hand_tag), d->target);
			effect_unlink(s, ef);
			free_effect(ef);
		}
	}
	{
		effect_s *ef = effect_by_type(s->effects, EF_M_EMPTY_CONT);
		if (ef != NULL) {
			effect_m_empty_cont_data *d = (void*)ef->data;
			hand_empty_cont(s, entity_limb_by_tag(s, d->hand_tag));
			effect_unlink(s, ef);
			free_effect(ef);
		}
	}
	{
		effect_s *ef = effect_by_type(s->effects, EF_M_PRESS_BUTTON);
		if (ef != NULL) {
			effect_m_press_button_data *d = (void*)ef->data;
			hand_press_button(s, entity_limb_by_tag(s, d->hand_tag), d->target, d->mat_tag);
			effect_unlink(s, ef);
			free_effect(ef);
		}
	}
	{
		effect_s *ef = effect_by_type(s->effects, EF_M_OPEN_DOOR);
		if (ef != NULL) {
			effect_m_open_door_data *d = (void*)ef->data;
			effect_s *t = effect_by_type(d->target->effects, EF_DOOR);
			if (t != NULL) {
				effect_door_data *td = (void*)t->data;
				td->opened += d->dir;
				if (td->opened < 0)
					td->opened = 0;
				if (td->opened > 64)
					td->opened = 64;
			}
			effect_unlink(s, ef);
			free_effect(ef);
		}
	}
}

void apply_reactions(entity_s *s) {
	effect_s *eft;
#if 0
	eft = effect_by_type(s->effects, EF_S_TOUCH);
	if (eft != NULL) {
		effect_s *ef = effect_by_type(s->effects, EF_R_TOUCH_RNG_TP);
		if (ef != NULL) {
			effect_s *ph = effect_by_type(s->effects, EF_PH_ITEM);
			if (ph != NULL) {
				effect_ph_item_data *d = (void*)ph->data;
				unparent_entity(s);
				detach_generic_entity(s);
				int rng_x = rng_next(g_dice) % 3 - 1;
				int rng_y = rng_next(g_dice) % 3 - 1;
				d->x += rng_x;
				d->y += rng_y;
				attach_generic_entity(s);
			}
		}
		ef = effect_by_type(s->effects, EF_R_TOUCH_SHOOT_PROJECTILE);
		effect_s *ef1 = effect_by_type(s->effects, EF_AIM);
		effect_s *ef2 = effect_by_type(s->effects, EF_PH_ITEM);
		if (ef != NULL && ef1 != NULL && ef2 != NULL) {
			effect_aim_data *aim_d = (void*)ef1->data;
			/* effect_ph_item_data *ph_d = (void*)ef2->data; */
			entity_s *new_ent = o_alloc_entity();
			new_ent->effects = NULL;
			{
				effect_s *ef_ph = alloc_effect(EF_PH_ITEM);
				effect_ph_item_data *d = (void*)ef_ph->data;
				entity_coords(s, &d->x, &d->y, &d->z);
				d->weight = 1;
				d->parent = NULL;
				effect_prepend(new_ent, ef_ph);
			}
			{
				effect_s *new_eff = alloc_effect(EF_RENDER);
				effect_render_data *d = (void*)new_eff->data;
				d->r = 0;
				d->b = 100;
				d->g = 200;
				d->a = 100;
				d->chr = '*';
				effect_prepend(new_ent, new_eff);
			}
			{
				effect_s *new_eff = alloc_effect(EF_TRACER);
				effect_tracer_data *d = (void*)new_eff->data;
				d->x = aim_d->x;
				d->y = aim_d->y;
				d->z = aim_d->z;
				d->cur_x = 0;
				d->cur_y = 0;
				d->cur_z = 0;
				d->speed = 5;
				effect_prepend(new_ent, new_eff);
			}
			{
				effect_s *new_eff = alloc_effect(EF_AIM);
				effect_aim_data *d = (void*)new_eff->data;
				d->x = 0;
				d->y = 0;
				d->z = 0;
				d->ent = aim_d->ent;
				effect_prepend(new_ent, new_eff);
			}
			entity_prepend(g_entities, new_ent);
			g_entities = new_ent;
			attach_generic_entity(new_ent);
		}

		effect_unlink(s, eft);
		free_effect(eft);
	}
#endif
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
			if (eft_d->ent != NULL) {
				effect_s *new_ef = alloc_effect(EF_S_DMG);
				effect_s_dmg_data *new_ef_d = (void*)new_ef->data;
				new_ef_d->type = DMGT_BLUNT;
				new_ef_d->val = eft_d->force;
				effect_prepend(eft_d->ent, new_ef);
			}
		}
		{
			effect_s *ee = effect_by_type(s->effects, EF_TABLE);
			if (ee != NULL) {
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
		}
		effect_unlink(s, eft);
		free_effect(eft);
	}
	while ((eft = effect_by_type(s->effects, EF_S_DMG)) != NULL) {
		effect_s_dmg_data *dmg_d = (void*)eft->data;
		effect_s *ef_mat = effect_by_type(s->effects, EF_MATERIAL);
		if (ef_mat != NULL) {
			effect_material_data *mat_d = (void*)ef_mat->data;
			int dmg_val = dmg_d->val;
			if (dmg_d->type == DMGT_FIRE && mat_d->type != MAT_WOOD && mat_d->type != MAT_PLANT)
				goto NO_DMG;
			if ((dmg_d->type == DMGT_BLUNT || dmg_d->type == DMGT_CUT) && mat_d->type == MAT_GLASS)
				dmg_val *= 5;
			mat_d->dur -= dmg_val;
NO_DMG:
			if (mat_d->dur <= 0) {
				effect_s *new_ef = alloc_effect(EF_B_NONEXISTENT);
				effect_prepend(s, new_ef);
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
					entity_coords(s, &d->x, &d->y, &d->z);
					d->parent = NULL;
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
				attach_generic_entity(new_ent);
			}
		}
		effect_unlink(s, eft);
		free_effect(eft);
	}
	if ((eft = effect_by_type(s->effects, EF_DOOR)) != NULL) {
		effect_door_data *d = (void*)eft->data;
		effect_s *bl, *r;
		if ((bl = effect_by_type(s->effects, EF_PH_BLOCK)) != NULL) {
			effect_ph_block_data *bd = (void*)bl->data;
			bd->block_movement = d->opened < 64;
			if ((r = effect_by_type(s->effects, EF_RENDER)) != NULL) {
				effect_render_data *d = (void*)r->data;
				d->r = bd->block_movement ? 0 : 255;
			}
		}
	}
}

void process_tick(entity_s *sl) {
	entity_s *s = sl;
	while (s != NULL) {
		apply_triggers(s);
		s = s->next;
	}
	s = sl;
	while (s != NULL) {
		apply_instants(s);
		s = s->next;
	}
	s = sl;
	while (s != NULL) {
		apply_reactions(s);
		s = s->next;
	}
	s = sl;
	while (s != NULL) {
		apply_physics(s);
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
			unparent_entity(s);
			detach_generic_entity(s);
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

int entity_coords(entity_s *s, int *x, int *y, int *z) {
	effect_s *ph_effect;
	ph_effect = effect_by_type(s->effects, EF_PH_BLOCK);
	if (ph_effect != NULL) {
		effect_ph_block_data *data = (void*)ph_effect->data;
		*x = data->x;
		*y = data->y;
		*z = data->z;
		return 1;
	}
	ph_effect = effect_by_type(s->effects, EF_PH_ITEM);
	if (ph_effect != NULL) {
		effect_ph_item_data *data = (void*)ph_effect->data;
		if (data->parent != NULL) {
			return entity_coords(data->parent, x, y, z);
		} else {
			*x = data->x;
			*y = data->y;
			*z = data->z;
		}
		return 1;
	}
	return 0;
}

int entity_set_coords(entity_s *s, int x, int y, int z) {
	effect_s *ph_effect;
	ph_effect = effect_by_type(s->effects, EF_PH_BLOCK);
	if (ph_effect != NULL) {
		effect_ph_block_data *data = (void*)ph_effect->data;
		data->x = x;
		data->y = y;
		data->z = z;
		return 1;
	}
	ph_effect = effect_by_type(s->effects, EF_PH_ITEM);
	if (ph_effect != NULL) {
		effect_ph_item_data *data = (void*)ph_effect->data;
		data->x = x;
		data->y = y;
		data->z = z;
		return 1;
	}
	return 0;
}

entity_s* entity_copy(entity_s *s) {
	entity_s *t = o_alloc_entity();
	t->effects = NULL;
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

void detach_entity(entity_s *s, int x, int y, int z) {
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

void attach_entity(entity_s *s, int x, int y, int z) {
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

void detach_generic_entity(entity_s *s) {
	int x;
	int y;
	int z;
	if (entity_coords(s, &x, &y, &z)) {
		detach_entity(s, x, y, z);
	}
}

void attach_generic_entity(entity_s *s) {
	int x;
	int y;
	int z;
	int has_parent = 0;
	effect_s *ph_item = effect_by_type(s->effects, EF_PH_ITEM);
	if (ph_item != NULL) {
		effect_ph_item_data *d = (void*)ph_item->data;
		if (d->parent != NULL) {
			has_parent = 1;
		}
	}
	if (!has_parent && effect_by_type(s->effects, EF_NOPHYSICS) == NULL && entity_coords(s, &x, &y, &z)) {
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

effect_s* entity_limb_by_entity(entity_s *s, entity_s *t) {
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

void apply_gravity(entity_s *s) {
	if (effect_by_type(s->effects, EF_TRACER) != NULL)
		return;
	int x, y, z;
	int cx = 0, cy = 0, cz = 0;
	int ox, oy, oz;
	if (entity_coords(s, &x, &y, &z)) {
		ox = x;
		oy = y;
		oz = z;
		coord_normalize(&ox, &cx);
		coord_normalize(&oy, &cy);
		coord_normalize(&oz, &cz);
		if (!block_fallable(x, y, z)) {
			detach_generic_entity(s);
			entity_set_coords(s, x, y, z - 1);
			if (effect_by_type(s->effects, EF_FALLING) == NULL) {
				effect_s *new_ef = alloc_effect(EF_FALLING);
				effect_prepend(s, new_ef);
			}
			attach_generic_entity(s);
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
				d->ent = NULL;
				d->force = 1;
				effect_prepend(s, new_ef);
				effect_unlink(s, ef);
				free_effect(ef);
			}
		}
	}
}

void apply_block_move(entity_s *s) {
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
			if (ef_crea_d->parent != NULL && ef_crea_d->parent_type == PARENT_REF_PLACE) {
				lift_entity(s);
			}
			if (ef_crea_d->parent != NULL) {
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
						detach_entity(s, ef_crea_d->x, ef_crea_d->y, ef_crea_d->z);
						ef_crea_d->x = ox;
						ef_crea_d->y = oy;
						ef_crea_d->z = oz;
						attach_entity(s, ef_crea_d->x, ef_crea_d->y, ef_crea_d->z);
					} else if (sector_get_block_slope(move_sector, x, y, z)) {
						z ++;
						oz ++;
						coord_normalize(&z, &cz);
						move_sector = sector_get_sector(g_sectors, cx, cy, cz);
						if (!sector_get_block_blocked_movement(move_sector, x, y, z)) {
							detach_entity(s, ef_crea_d->x, ef_crea_d->y, ef_crea_d->z);
							ef_crea_d->x = ox;
							ef_crea_d->y = oy;
							ef_crea_d->z = oz;
							attach_entity(s, ef_crea_d->x, ef_crea_d->y, ef_crea_d->z);
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

void apply_stair_move(entity_s *s) {
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
						detach_entity(s, ef_crea_d->x, ef_crea_d->y, ef_crea_d->z);
						ef_crea_d->x = ox;
						ef_crea_d->y = oy;
						ef_crea_d->z = oz;
						attach_entity(s, ef_crea_d->x, ef_crea_d->y, ef_crea_d->z);
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

void apply_tracer(entity_s *s) {
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
	detach_generic_entity(s);
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
			d->ent = NULL;
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
				d->ent = NULL;
				d->force = tracer_d->speed;
				effect_prepend(s, ef_bump);
				break;
			}
		}
		ph_d->x = nx;
		ph_d->y = ny;
		ph_d->z = nz;
		entity_s *w = tracer_check_bump(s, nx, ny, nz);
		if (w != NULL) {
			effect_s *ef_bump = alloc_effect(EF_S_BUMP);
			effect_s_bump_data *d = (void*)ef_bump->data;
			d->ent = w;
			d->force = tracer_d->speed;
			effect_prepend(s, ef_bump);
			break;
		}
	}
	attach_generic_entity(s);
	effect_unlink(s, tracer);
	free_effect(tracer);
}

void apply_movement(entity_s *s) {
	apply_block_move(s);
	apply_stair_move(s);
	apply_tracer(s);
}

int attack_type_possible(entity_s *s, entity_s *used_limb, attack_type type, uint32_t used_tag) {
	if (entity_limb_by_entity(s, used_limb) == NULL)
		return 0;
	switch (type) {
	case ATK_HAND_PUNCH:
		return effect_by_type(used_limb->effects, EF_LIMB_HAND) != NULL;
	case ATK_KICK:
		return effect_by_type(used_limb->effects, EF_LIMB_LEG) != NULL;
	case ATK_SWING:
	case ATK_THRUST: {
		effect_s *e = effect_by_type(used_limb->effects, EF_LIMB_HAND);
		if (e == NULL)
			return 0;
		effect_limb_hand_data *d = (void*)e->data;
		return
			d->item != NULL &&
			entity_material_by_tag(d->item, used_tag) != NULL;
	} break;
	default:
		return 0;
	}
}

entity_s* attack_used_tool(entity_s *s, entity_s *used_limb, attack_type type) {
	if (entity_limb_by_entity(s, used_limb) == NULL)
		return 0;
	switch (type) {
	case ATK_HAND_PUNCH:
	case ATK_KICK:
		return used_limb;
	case ATK_SWING:
	case ATK_THRUST: {
		effect_s *e = effect_by_type(used_limb->effects, EF_LIMB_HAND);
		if (e == NULL)
			return NULL;
		effect_limb_hand_data *d = (void*)e->data;
		return d->item;
	} break;
	default:
		return NULL;
	}
}

void apply_attack(entity_s *s) {
	effect_s *e = effect_by_type(s->effects, EF_ATTACK);
	if (e == NULL) return;
	effect_attack_data *data = (void*)e->data;
	if (!attack_type_possible(s, data->used_limb, data->type, data->weapon_mat)) {
		effect_unlink(s, e);
		free_effect(e);
		return;
	}
	entity_s *used_tool = attack_used_tool(s, data->used_limb, data->type);
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
	if (!entity_reachable(s, entity_limb_by_entity(s, data->used_limb), data->ent)) {
		effect_unlink(s, e);
		free_effect(e);
		return;
	}
	if (data->ent != NULL) {
		effect_s *new_eff = alloc_effect(EF_S_DMG);
		effect_s_dmg_data *new_data = (void*)new_eff->data;
		//entity_damage_calc(data->type, &new_data->type, &new_data->val);
		if (mat_d->prop & MATP_SHARP) {
			new_data->type = DMGT_CUT;
			new_data->val = 2;
		} else {
			new_data->type = DMGT_BLUNT;
			new_data->val = 1;
		}
		effect_prepend(data->ent, new_eff);
	}
	effect_unlink(s, e);
	free_effect(e);
}

void liquid_deduplicate(entity_s *s) {
	/*
	 * Works for both top-level and child-level liquids
	 * Deduplicates liquid objects, only checking for the
	 * type of liquid.
	 */
	effect_s *ph_item = effect_by_type(s->effects, EF_PH_ITEM);
	if (ph_item == NULL)
		return;
	effect_ph_item_data *item_d = (void*)ph_item->data;
	entity_l_s *layer_l = NULL;
	if (item_d->parent != NULL) {
		layer_l = entity_enlist(item_d->parent);
	}
	effect_s *li = effect_by_type(s->effects, EF_PH_LIQUID);
	if (li == NULL)
		return;
	effect_ph_liquid_data *li_d = (void*)li->data;
	int x, y, z, cx = 0, cy = 0, cz = 0;
	entity_coords(s, &x, &y, &z);
	coord_normalize(&x, &cx);
	coord_normalize(&y, &cy);
	coord_normalize(&z, &cz);
	sector_s *sec = sector_get_sector(g_sectors, cx, cy, cz);
	entity_l_s *e = layer_l == NULL ? sector_get_block_entities(sec, x, y, z) : layer_l;
	while (e != NULL) {
		if (e->ent != s) {
			effect_s *e_li = effect_by_type(e->ent->effects, EF_PH_LIQUID);
			if (e_li != NULL) {
				effect_ph_liquid_data *d = (void*)e_li->data;
				if (d->type == li_d->type) {
					li_d->amount += d->amount;
					effect_unlink(e->ent, e_li);
					free_effect(e_li);
					effect_s *new_eff = alloc_effect(EF_B_NONEXISTENT);
					effect_prepend(e->ent, new_eff);
				}
			}
		}
		e = e->next;
	}
	if (li_d->amount == 0) {
		if (effect_by_type(s->effects, EF_B_NONEXISTENT) == NULL)
			effect_prepend(s, alloc_effect(EF_B_NONEXISTENT));
	}
	if (layer_l != NULL)
		entity_l_s_free(layer_l);
}

void apply_liquid_movement(entity_s *s) {
	effect_s *ph_item = effect_by_type(s->effects, EF_PH_ITEM);
	if (ph_item == NULL)
		return;
	effect_ph_item_data *item_d = (void*)ph_item->data;
	if (item_d->parent != NULL)
		return;
	effect_s *li = effect_by_type(s->effects, EF_PH_LIQUID);
	if (li == NULL)
		return;
	effect_ph_liquid_data *li_d = (void*)li->data;
	int x, y, z;
	entity_coords(s, &x, &y, &z);
	if (!block_fallable(x, y, z)) {
		detach_generic_entity(s);
		entity_set_coords(s, x, y, z - 1);
		attach_generic_entity(s);
		return;
	}
	if (li_d->amount > G_PUDDLE_MAX + 3) {
		int over = li_d->amount - G_PUDDLE_MAX, amount_lost = 0;
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
				entity_s *dup = entity_copy(s);
				entity_set_coords(dup, tx, ty, tz);
				attach_generic_entity(dup);
				effect_s *dup_liq = effect_by_type(dup->effects, EF_PH_LIQUID);
				if (dup_liq != NULL) {
					effect_ph_liquid_data *dup_liq_d = (void*)dup_liq->data;
					dup_liq_d->amount = over / ndirs;
					amount_lost += over / ndirs;
					liquid_deduplicate(dup);
				} else {
					/* something gone really wrong */
				}
			}
		}
		li_d->amount -= amount_lost;
	}
}

void apply_physics(entity_s *s) {
	if (effect_by_type(s->effects, EF_NOPHYSICS) == NULL) {
		apply_movement(s);
		effect_s *ph_item = effect_by_type(s->effects, EF_PH_ITEM);
		effect_s *ph_liquid = effect_by_type(s->effects, EF_PH_LIQUID);
		if (ph_item != NULL) {
			if (ph_liquid != NULL) {
				apply_liquid_movement(s);
				liquid_deduplicate(s);
			} else {
				apply_gravity(s);
			}
		}
	}
	apply_attack(s);
}

void trigger_move(entity_s *s, int x, int y, int z) {
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

void trigger_go_up(entity_s *s) {
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

void trigger_go_down(entity_s *s) {
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

void trigger_grab(entity_s *s, effect_s *h, entity_s *w, uint32_t tag) {
	if (h->type != EF_LIMB_SLOT)
		return;
	effect_limb_slot_data *ds = (void*)h->data;
	effect_s *new_eff = alloc_effect(EF_M_GRAB);
	effect_m_grab_data *d = (void*)new_eff->data;
	d->eff_tag = ds->tag;
	d->ent = w;
	d->mat_tag = tag;
	effect_prepend(s, new_eff);
}

void trigger_drop(entity_s *s, effect_s *h) {
	if (h->type != EF_LIMB_SLOT)
		return;
	effect_limb_slot_data *ds = (void*)h->data;
	effect_s *new_eff = alloc_effect(EF_M_DROP);
	effect_m_drop_data *d = (void*)new_eff->data;
	d->eff_tag = ds->tag;
	effect_prepend(s, new_eff);
}

void trigger_put(entity_s *s, effect_s *h, entity_s *w) {
	if (h->type != EF_LIMB_SLOT)
		return;
	effect_limb_slot_data *ds = (void*)h->data;
	effect_s *new_eff = alloc_effect(EF_M_PUT);
	effect_m_put_data *d = (void*)new_eff->data;
	d->eff_tag = ds->tag;
	d->where = w;
	effect_prepend(s, new_eff);
}

void trigger_throw(entity_s *s, effect_s *h, int x, int y, int z, int speed) {
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

void trigger_attack(entity_s *s, entity_s *e, attack_type type, entity_s *used_limb, uint32_t weapon_mat) {
	if (effect_by_type(s->effects, EF_ATTACK) != NULL)
		return;
	effect_s *new_eff = alloc_effect(EF_ATTACK);
	effect_attack_data *d = (void*)new_eff->data;
	d->ent = e;
	d->delay = 1;
	d->type = type;
	d->used_limb = used_limb;
	d->weapon_mat = weapon_mat;
	effect_prepend(s, new_eff);
}

void trigger_fill_cont(entity_s *s, effect_s *h, entity_s *t) {
	if (h->type != EF_LIMB_SLOT)
		return;
	effect_limb_slot_data *dh = (void*)h->data;
	effect_s *new_eff = alloc_effect(EF_M_FILL_CONT);
	effect_m_fill_cont_data *d = (void*)new_eff->data;
	d->hand_tag = dh->tag;
	d->target = t;
	effect_prepend(s, new_eff);
}

void trigger_empty_cont(entity_s *s, effect_s *h) {
	if (h->type != EF_LIMB_SLOT)
		return;
	effect_limb_slot_data *dh = (void*)h->data;
	effect_s *new_eff = alloc_effect(EF_M_EMPTY_CONT);
	effect_m_empty_cont_data *d = (void*)new_eff->data;
	d->hand_tag = dh->tag;
	effect_prepend(s, new_eff);
}


void trigger_press_button(entity_s *s, effect_s *h, entity_s *t, effect_s *w) {
	if (h->type != EF_LIMB_SLOT || w->type != EF_MATERIAL)
		return;
	effect_limb_slot_data *dh = (void*)h->data;
	effect_material_data *dw = (void*)w->data;
	effect_s *new_eff = alloc_effect(EF_M_PRESS_BUTTON);
	effect_m_press_button_data *d = (void*)new_eff->data;
	d->hand_tag = dh->tag;
	d->target = t;
	d->mat_tag = dw->tag;
	effect_prepend(s, new_eff);
}

void trigger_open_door(entity_s *s, effect_s *h, entity_s *t, int dir) {
	if (h->type != EF_LIMB_SLOT)
		return;
	effect_limb_slot_data *dh = (void*)h->data;
	effect_s *new_eff = alloc_effect(EF_M_OPEN_DOOR);
	effect_m_open_door_data *d = (void*)new_eff->data;
	d->dir = dir;
	d->hand_tag = dh->tag;
	d->target = t;
	effect_prepend(s, new_eff);
}

void hand_grab(entity_s *ent, effect_s *hand, entity_s *item, uint32_t tag) {
	if (hand->type == EF_LIMB_SLOT) {
		effect_limb_slot_data *hand_data = (void*)hand->data;
		if (item == hand_data->item || item == ent)
			return;
		effect_s *ef_hand = effect_by_type(hand_data->item->effects, EF_LIMB_HAND);
		effect_s *ef_ph = effect_by_type(item->effects, EF_PH_ITEM);
		if (ef_hand != NULL && ef_ph != NULL && entity_reachable(ent, hand, item)) {
			effect_ph_item_data *ph_data = (void*)ef_ph->data;
			effect_limb_hand_data *lhand_data = (void*)ef_hand->data;
			effect_s *mat = entity_material_by_tag(item, tag);
			if (
				lhand_data->item == NULL &&
				(ph_data->parent == NULL || ph_data->parent_type == PARENT_REF_PLACE) &&
				mat != NULL
			) {
				unparent_entity(item);
				detach_generic_entity(item);
				ph_data->parent = hand_data->item;
				ph_data->parent_type = PARENT_REF_HELD;
				lhand_data->item = item;
				lhand_data->grab_type = tag;
			}
		}
	}
}

void hand_drop(entity_s *ent, effect_s *hand) {
	if (hand->type == EF_LIMB_SLOT) {
		effect_limb_slot_data *slot_data = (void*)hand->data;
		if (slot_data->item != NULL) {
			effect_s *ef_hand = effect_by_type(slot_data->item->effects, EF_LIMB_HAND);
			if (ef_hand != NULL) {
				effect_limb_hand_data *hand_data = (void*)ef_hand->data;
				if (hand_data->item != NULL) {
					effect_s *ph_item = effect_by_type(hand_data->item->effects, EF_PH_ITEM);
					if (ph_item != NULL) {
						effect_ph_item_data *item_data = (void*)ph_item->data;
						int x, y, z;
						entity_coords(ent, &x, &y, &z);
						entity_set_coords(hand_data->item, x, y, z);
						item_data->parent = NULL;
						attach_generic_entity(hand_data->item);
					}
					if (effect_by_type(hand_data->item->effects, EF_FALLING) == NULL) {
						effect_s *ef_fall = alloc_effect(EF_FALLING);
						effect_prepend(hand_data->item, ef_fall);
					}
					hand_data->item = NULL;
				}
			}
		}
	}
}

void hand_put(entity_s *ent, effect_s *hand, entity_s *w) {
	if (hand->type == EF_LIMB_SLOT) {
		effect_limb_slot_data *slot_data = (void*)hand->data;
		if (slot_data->item != NULL) {
			effect_s *ef_hand = effect_by_type(slot_data->item->effects, EF_LIMB_HAND);
			if (ef_hand != NULL) {
				effect_limb_hand_data *hand_data = (void*)ef_hand->data;
				if (hand_data->item != NULL) {
					effect_s *ph_item = effect_by_type(hand_data->item->effects, EF_PH_ITEM);
					if (effect_by_type(w->effects, EF_TABLE) == NULL || !entity_reachable(ent, hand, w)) {
						w = NULL;
					}
					if (ph_item != NULL && w != NULL) {
						effect_ph_item_data *item_data = (void*)ph_item->data;
						int x, y, z;
						entity_coords(ent, &x, &y, &z);
						entity_set_coords(hand_data->item, x, y, z);
						item_data->parent = w;
						item_data->parent_type = PARENT_REF_PLACE;
						effect_s *new_eff = alloc_effect(EF_TABLE_ITEM);
						effect_table_item_data *d = (void*)new_eff->data;
						d->item = hand_data->item;
						effect_prepend(w, new_eff);
						hand_data->item = NULL;
					}
				}
			}
		}
	}
}

void hand_throw(entity_s *s, effect_s *h, int x, int y, int z, int speed) {
	if (h->type != EF_LIMB_SLOT) {
		return;
	}
	effect_limb_slot_data *limb_d = (void*)h->data;
	if (limb_d->item == NULL) {
		return;
	}
	effect_s *ef_hand = effect_by_type(limb_d->item->effects, EF_LIMB_HAND);
	if (ef_hand == NULL) {
		return;
	}
	effect_limb_hand_data *hand_d = (void*)ef_hand->data;
	if (hand_d->item == NULL) {
		return;
	}
	effect_s *ef_tracer = alloc_effect(EF_TRACER);
	effect_tracer_data *tracer_d = (void*)ef_tracer->data;
	tracer_d->cur_x = 0;
	tracer_d->cur_y = 0;
	tracer_d->cur_z = 0;
	tracer_d->x = x;
	tracer_d->y = y;
	tracer_d->z = z;
	tracer_d->speed = speed;
	effect_prepend(hand_d->item, ef_tracer);
	effect_s *ph_item = effect_by_type(hand_d->item->effects, EF_PH_ITEM);
	if (ph_item != NULL) {
		effect_ph_item_data *item_data = (void*)ph_item->data;
		int x, y, z;
		entity_coords(s, &x, &y, &z);
		entity_set_coords(hand_d->item, x, y, z);
		item_data->parent = NULL;
		attach_generic_entity(hand_d->item);
	}
	hand_d->item = NULL;
}

void hand_fill_cont(entity_s *s, effect_s *h, entity_s *t) {
	if (h->type != EF_LIMB_SLOT)
		return;
	effect_limb_slot_data *hd = (void*)h->data;
	if (hd->item == NULL)
		return;
	effect_s *ef_lhand = effect_by_type(hd->item->effects, EF_LIMB_HAND);
	if (ef_lhand == NULL)
		return;
	effect_limb_hand_data *lhd = (void*)ef_lhand->data;
	if (lhd->item == NULL)
		return;
	effect_s *held_item_cont = effect_by_type(lhd->item->effects, EF_CONTAINER);
	if (held_item_cont == NULL)
		return;
	effect_container_data *cont_d = (void*)held_item_cont->data;
	if (cont_d->capacity <= container_get_amount(lhd->item))
		return;
	if (!entity_reachable(s, h, t))
		return;
	effect_s *t_liq = effect_by_type(t->effects, EF_PH_LIQUID);
	if (t_liq == NULL)
		return;
	effect_ph_liquid_data *liq_d = (void*)t_liq->data;
	if (liq_d->amount == 0)
		return;
	liq_d->amount--;
	liquid_deduplicate(t);
	container_add_liquid(lhd->item, liq_d->type, 1);
}

void hand_empty_cont(entity_s *s, effect_s *h) {
	(void)s;
	if (h->type != EF_LIMB_SLOT)
		return;
	effect_limb_slot_data *hd = (void*)h->data;
	if (hd->item == NULL)
		return;
	effect_s *ef_lhand = effect_by_type(hd->item->effects, EF_LIMB_HAND);
	if (ef_lhand == NULL)
		return;
	effect_limb_hand_data *lhd = (void*)ef_lhand->data;
	if (lhd->item == NULL)
		return;
	effect_s *held_item_cont = effect_by_type(lhd->item->effects, EF_CONTAINER);
	if (held_item_cont == NULL)
		return;
	for (effect_s *c = lhd->item->effects; c != NULL; c = c->next) {
		if (c->type != EF_CONTAINER_ITEM)
			continue;
		effect_container_item_data *d = (void*)c->data;
		if (d->item != NULL)
			unparent_attach_entity(d->item);
	}
}

void hand_press_button(entity_s *s, effect_s *h, entity_s *t, uint32_t mat_tag) {
	(void)s;
	if (h->type != EF_LIMB_SLOT)
		return;
	effect_limb_slot_data *hd = (void*)h->data;
	if (hd->item == NULL)
		return;
	effect_s *ef_lhand = effect_by_type(hd->item->effects, EF_LIMB_HAND);
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
			dump_entity(lsc->ent, stream);
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
					dump_entity(e->ent, stream);
					e = e->next;
				}
			}
		}
	}
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
				entity_enumerate(lsc->ent, ent_id);
				ls = lsc;
				lsc = lsc->next;
				o_free(ls);
			}
		}
		ef = ef->next;
	}
}

void dump_sector_list(sector_s *s, FILE *stream) {
	sector_s *c = s;
	int ent_id = 0;
	while (c != NULL) {
		for (int i = 0; i < G_SECTOR_SIZE; i ++) {
			for (int j = 0; j < G_SECTOR_SIZE; j ++) {
				for (int k = 0; k < G_SECTOR_SIZE; k ++) {
					entity_l_s *e = c->block_entities[i][j][k];
					while (e != NULL) {
						entity_enumerate(e->ent, &ent_id);
						e = e->next;
					}
				}
			}
		}
		c = c->snext;
	}
	c = s;
	fwrite(&ent_id, sizeof(int), 1, stream);
	while (c != NULL) {
		dump_sector(c, stream);
		c = c->snext;
	}
}

void effect_dump_ph_block(effect_s *e, FILE *stream) {
	effect_ph_block_data *d = (void*)e->data;
	fwrite(&d->x, sizeof(int), 1, stream);
	fwrite(&d->y, sizeof(int), 1, stream);
	fwrite(&d->z, sizeof(int), 1, stream);
	unsigned t = d->floor | (d->block_movement << 1) | (d->floor_up << 2) | (d->stair << 3) | (d->slope << 4);
	fwrite(&t, sizeof(int), 1, stream);
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
			unload_entity(lsc->ent);
			ls = lsc;
			lsc = lsc->next;
			o_free(ls);
		}
		c = c->next;
	}
}

entity_s *load_sector_list(FILE *stream) {
	int n_ent;
	fread(&n_ent, sizeof(int), 1, stream);
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
		scan_entity(n_ent, a_ent, stream);
	}
	entity_s *t = a_ent[0];
	o_free(a_ent);
	return t;
}

effect_s* scan_effect(int n_ent, entity_s **a_ent, FILE *stream) {
	int type;
	fread(&type, sizeof(int), 1, stream);
	effect_s *eff = alloc_effect(type);
	effect_scan_t scanner = effect_scan_functions[type];
	if (scanner != NULL) {
		scanner(eff, n_ent, a_ent, stream);
	}
	return eff;
}

entity_s* scan_entity(int n_ent, entity_s **a_ent, FILE *stream) {
	int id;
	fread(&id, sizeof(int), 1, stream);
	if (id >= n_ent) {
		/* Fail badly */
		fprintf(stderr, "Failed badly\n");
		fflush(stderr);
		return NULL;
	} else {
		int id_eff;
		fread(&id_eff, sizeof(int), 1, stream);
		effect_s *la = a_ent[id]->effects;
		for (int i = 0; i < id_eff; i ++) {
			effect_s *c = scan_effect(n_ent, a_ent, stream);
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

void unload_sector(sector_s *s) {
	for (int i = 0; i < G_SECTOR_SIZE; i ++) {
		for (int j = 0; j < G_SECTOR_SIZE; j ++) {
			for (int k = 0; k < G_SECTOR_SIZE; k ++) {
				entity_l_s *c = s->block_entities[i][j][k];
				while (c != NULL) {
					unload_entity(c->ent);
					c = c->next;
				}
				entity_l_s_free(s->block_entities[i][j][k]);
				s->block_entities[i][j][k] = NULL;
			}
		}
	}
}

void effect_scan_ph_block(effect_s *e, int n_ent, entity_s **a_ent, FILE *stream) {
	(void)n_ent;
	(void)a_ent;
	effect_ph_block_data *d = (void*)e->data;
	fread(&d->x, sizeof(int), 1, stream);
	fread(&d->y, sizeof(int), 1, stream);
	fread(&d->z, sizeof(int), 1, stream);
	unsigned t;
	fread(&t, sizeof(unsigned), 1, stream);
	d->floor = t & 1;
	d->block_movement = (t >> 1) & 1;
	d->floor_up = (t >> 2) & 1;
	d->stair = (t >> 3) & 1;
	d->slope = (t >> 4) & 1;
}

int effect_rem_ph_item(entity_s *s, effect_s *e) {
	effect_ph_item_data *d = (void*)e->data;
	while (d->parent != NULL && effect_by_type(d->parent->effects, EF_B_NONEXISTENT) != NULL) {
		lift_entity(s);
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
		if (d->item != NULL) {
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
		if (d->item != NULL) {
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
		if (d->item != NULL) {
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
		if (d->item != NULL) {
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
	entity_l_s *el_orig = sector_get_block_entities(s, x, y, z);
	if (el_orig == NULL)
		return NULL;
	entity_l_s *el = entity_l_s_copy(el_orig);
	entity_l_s *t = el;
	while (t->next != NULL)
		t = t->next;
	entity_l_s *h = t;
	while (t != NULL) {
		entity_l_s *a = entity_enlist(t->ent);
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

int entity_reachable(entity_s *s, effect_s *limb, entity_s *e) {
	if (limb->type != EF_LIMB_SLOT) {
		return 0;
	}
	effect_limb_slot_data *limb_data = (void*)limb->data;
	if (limb_data->item == NULL) {
		return 0;
	}
	int xs, ys, zs;
	int xe, ye, ze;
	entity_coords(s, &xs, &ys, &zs);
	entity_coords(e, &xe, &ye, &ze);
	if (abs(xs - xe) > 1 || abs(ys - ye) > 1 || abs(zs - ze) > 1) {
		return 0;
	}
	if (xs == xe && ys == ye && zs == ze) {
		return 1;
	}
	if (effect_by_type(e->effects, EF_PH_BLOCK) != NULL) {
		return 1;
	}
	/* TODO high/low limb types */
	return 0;
}

entity_s* tracer_check_bump(entity_s *s, int x, int y, int z) {
	int cx = 0, cy = 0, cz = 0, sx = x, sy = y, sz = z;
	int ex, ey, ez;
	if (!entity_coords(s, &ex, &ey, &ez)) {
		return NULL;
	}
	coord_normalize(&sx, &cx);
	coord_normalize(&sy, &cy);
	coord_normalize(&sz, &cz);
	sector_s *sect = sector_get_sector(g_sectors, cx, cy, cz);
	if (sect == NULL)
		return NULL;
	entity_l_s *el = sector_get_block_entities(sect, sx, sy, sz);
	entity_l_s *cur;
	int total_hit_val = 0;
	for (cur = el; cur != NULL; cur = cur->next) {
		effect_s *ef_ph = effect_by_type(cur->ent->effects, EF_PH_ITEM);
		if (ef_ph != NULL) {
			total_hit_val += entity_size(cur->ent);
		}
	}
	int mx = 256;
	if (mx < total_hit_val)
		mx = total_hit_val + 1;
	// TODO rng_next_range
	int random_val = rng_next(g_dice) % mx;
	if (random_val > total_hit_val)
		return NULL;
	int cur_hit_val = 0;
	for (cur = el; cur != NULL; cur = cur->next) {
		effect_s *ef_ph = effect_by_type(cur->ent->effects, EF_PH_ITEM);
		if (ef_ph != NULL) {
			cur_hit_val += entity_size(cur->ent);
			if (cur_hit_val >= random_val)
				return cur->ent;
		}
	}
	return NULL;
}

void unparent_entity(entity_s *s) {
	effect_s *ph = effect_by_type(s->effects, EF_PH_ITEM);
	if (ph == NULL) {
		return;
	}
	effect_ph_item_data *d = (void*)ph->data;
	if (d->parent == NULL) {
		return;
	}
	switch (d->parent_type) {
	case PARENT_REF_HELD: {
		int x, y, z;
		entity_s *p = d->parent;
		effect_s *t = p->effects;
		if (entity_coords(s, &x, &y, &z)) {
			d->parent = NULL;
			entity_set_coords(s, x, y, z);
		} else {
			d->parent = NULL;
		}
		while (t != NULL) {
			if (t->type == EF_LIMB_HAND) {
				effect_limb_hand_data *td = (void*)t->data;
				if (td->item == s) {
					td->item = NULL;
					break;
				}
			}
			t = t->next;
		}
	} break;
	case PARENT_REF_PLACE: {
		int x, y, z;
		entity_s *p = d->parent;
		effect_s *t = p->effects;
		if (entity_coords(s, &x, &y, &z)) {
			d->parent = NULL;
			entity_set_coords(s, x, y, z);
		} else {
			d->parent = NULL;
		}
		while (t != NULL) {
			if (t->type == EF_TABLE_ITEM) {
				effect_table_item_data *td = (void*)t->data;
				if (td->item == s) {
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
		entity_s *p = d->parent;
		effect_s *t = p->effects;
		if (entity_coords(s, &x, &y, &z)) {
			d->parent = NULL;
			entity_set_coords(s, x, y, z);
		} else {
			d->parent = NULL;
		}
		while (t != NULL) {
			if (t->type == EF_CONTAINER_ITEM) {
				effect_table_item_data *td = (void*)t->data;
				if (td->item == s) {
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
		entity_s *p = d->parent;
		effect_s *t = p->effects;
		if (entity_coords(s, &x, &y, &z)) {
			d->parent = NULL;
			entity_set_coords(s, x, y, z);
		} else {
			d->parent = NULL;
		}
		while (t != NULL) {
			if (t->type == EF_LIMB_SLOT) {
				effect_limb_slot_data *td = (void*)t->data;
				if (td->item == s) {
					td->item = NULL;
					break;
				}
			}
			t = t->next;
		}
	} break;
	}
}

void unparent_attach_entity(entity_s *s) {
	unparent_entity(s);
	attach_generic_entity(s);
}

void lift_entity(entity_s *s) {
	effect_s *ph = effect_by_type(s->effects, EF_PH_ITEM);
	if (ph == NULL)
		return;
	effect_ph_item_data *d = (void*)ph->data;
	if (d->parent == NULL)
		return;
	entity_s *p = d->parent;
	parent_ref_type pt = d->parent_type;
	unparent_entity(s);
	do {
		effect_s *pph = effect_by_type(p->effects, EF_PH_ITEM);
		if (pph == NULL) {
			p = NULL;
			break;
		}
		effect_ph_item_data *pd = (void*)pph->data;
		p = pd->parent;
		pt = pd->parent_type;
	} while (p != NULL && (pt == PARENT_REF_HELD || pt == PARENT_REF_LIMB));
	if (p == NULL) {
		attach_generic_entity(s);
		return;
	}
	switch (pt) {
	case PARENT_REF_HELD: break;
	case PARENT_REF_LIMB: break;
	case PARENT_REF_PLACE: {
		effect_s *new_ef = alloc_effect(EF_TABLE_ITEM);
		effect_table_item_data *d = (void*)new_ef->data;
		d->item = s;
		effect_prepend(p, new_ef);
	} break;
	case PARENT_REF_CONT: {
		effect_s *new_ef = alloc_effect(EF_CONTAINER_ITEM);
		effect_container_item_data *d = (void*)new_ef->data;
		d->item = s;
		effect_prepend(p, new_ef);
	} break;
	}
}

int entity_weight(entity_s *s) {
	effect_s *ph_item = effect_by_type(s->effects, EF_PH_ITEM);
	if (ph_item == NULL)
		return 0;
	effect_ph_item_data *d = (void*)ph_item->data;
	effect_s *ph_liq = effect_by_type(s->effects, EF_PH_LIQUID);
	if (ph_liq != NULL) {
		effect_ph_liquid_data *dl = (void*)ph_liq->data;
		return dl->amount;
	}
	return d->weight;
}

int entity_size(entity_s *s) {
	effect_s *ph_item = effect_by_type(s->effects, EF_PH_ITEM);
	if (ph_item == NULL)
		return 0;
	// effect_ph_item_data *d = (void*)ph_item->data;
	effect_s *ph_liq = effect_by_type(s->effects, EF_PH_LIQUID);
	if (ph_liq != NULL) {
		effect_ph_liquid_data *dl = (void*)ph_liq->data;
		return dl->amount;
	}
	return 1;
}

// List the attacks that `s' can try to perform at `o'
attack_l_s* entity_list_attacks(entity_s *s, entity_s *o) {
	(void)o;
	attack_l_s *r = NULL;
	attack_type t;
	effect_s *ef = s->effects;
	while (ef != NULL) {
		if (ef->type == EF_LIMB_SLOT) {
			effect_limb_slot_data *d = (void*)ef->data;
			for (t = 0; t < ATK_N_COUNT; t++) {
				entity_s *tool = attack_used_tool(s, d->item, t);
				if (tool == NULL)
					continue;
				effect_s *ef1 = tool->effects;
				while (ef1 != NULL) {
					if (ef1->type == EF_MATERIAL) {
						effect_material_data *mat_d = (void*)ef1->data;
						uint32_t sel_tag = mat_d->tag;
						if (attack_type_possible(s, d->item, t, sel_tag)) {
							attack_l_s *th = o_malloc(sizeof(attack_l_s));
							th->limb_slot_tag = d->tag;
							th->limb_entity = d->item;
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

int container_get_amount(entity_s *s) {
	int r = 0;
	effect_s *t = effect_by_type(s->effects, EF_CONTAINER);
	if (t == NULL)
		return 0;
	effect_s *c = s->effects;
	while (c != NULL) {
		if (c->type == EF_CONTAINER_ITEM) {
			effect_container_item_data *d = (void*)c->data;
			if (d->item != NULL) {
				effect_s *t = effect_by_type(d->item->effects, EF_PH_LIQUID);
				if (t != NULL) {
					effect_ph_liquid_data *td = (void*)t->data;
					r += td->amount;
				} else {
					// TODO get object size
					r += 1;
				}
			}
		}
		c = c->next;
	}
	return r;
}

void container_add_liquid(entity_s *s, liquid_type t, int amount) {
	effect_s *e = effect_by_type(s->effects, EF_CONTAINER);
	if (e == NULL)
		return;
	effect_s *c = s->effects;
	while (c != NULL) {
		if (c->type == EF_CONTAINER_ITEM) {
			effect_container_item_data *d = (void*)c->data;
			if (d->item != NULL) {
				effect_s *liq = effect_by_type(d->item->effects, EF_PH_LIQUID);
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
		d->parent = s;
		d->parent_type = PARENT_REF_CONT;
		effect_prepend(new_item, ef_ph);
	}
	entity_prepend(g_entities, new_item);
	g_entities = new_item;
	effect_s *new_eff = alloc_effect(EF_CONTAINER_ITEM);
	effect_container_item_data *d = (void*)new_eff->data;
	d->item = new_item;
	effect_prepend(s, new_eff);
}

#include "gen-loaders.h"
