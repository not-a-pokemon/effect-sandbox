#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "entity.h"
#include "omalloc.h"
#include "rng.h"

typedef struct camera_view_s {
	int x;
	int y;
	int z;
	int cursor_x;
	int cursor_y;
	/* cursor_z is identical to z */
	int width;
	int height;
	int blink;
} camera_view_s;

TTF_Font *gr_font = NULL;

int gr_rend_char_width = 26;
int gr_rend_char_height = 32;
#define REND_CHAR_WIDTH gr_rend_char_width
#define REND_CHAR_HEIGHT gr_rend_char_height

int gcd(int x, int y) {
	if (y == 0) {
		return x;
	}
	while (x != 0) {
		int t = y % x;
		y = x;
		x = t;
	}
	return y;
}

int entity_rend_chr(entity_s *ent, char *chr, int *r, int *g, int *b, int *a) {
	effect_s *ef = effect_by_type(ent->effects, EF_RENDER);
	if (ef != NULL) {
		effect_render_data *d = (void*)ef->data;
		*chr = d->chr;
		*r = d->r;
		*g = d->g;
		*b = d->b;
		*a = d->a;
		return 1;
	}
	ef = effect_by_type(ent->effects, EF_PH_LIQUID);
	if (ef != NULL) {
		effect_ph_liquid_data *d = (void*)ef->data;
		*chr = '~';
		if (d->type == LIQ_WATER) {
			*r = 60;
			*b = 200;
			*g = 150;
			*a = 128;
		} else {
			*r = 128;
			*b = 0;
			*g = 200;
			*a = 128;
		}
		return 1;
	}
	return 0;
}

void render_camera(SDL_Renderer *rend, camera_view_s *cam) {
	int prev_cx = INT_MIN;
	int prev_cy = INT_MIN;

	int z = cam->z;
	int cz = 0;
	coord_normalize(&z, &cz);

	SDL_SetRenderDrawColor(rend, 0, 0, 0, 255);
	for (int i = 0; i < cam->width; i ++) {
		for (int j = 0; j < cam->height; j ++) {
			SDL_Rect r = {.x = i * REND_CHAR_WIDTH, .y = j * REND_CHAR_HEIGHT, .w = REND_CHAR_WIDTH, .h = REND_CHAR_HEIGHT};
			SDL_RenderFillRect(rend, &r);
		}
	}
	sector_s *s = NULL;
	for (int i = 0; i < cam->width; i ++) {
		for (int j = 0; j < cam->height; j ++) {
			int cx = 0;
			int x = cam->x + i;
			coord_normalize(&x, &cx);
			int cy = 0;
			int y = cam->y + j;
			coord_normalize(&y, &cy);
			if (!(prev_cx == cx && prev_cy == cy)) {
				s = sector_get_sector(g_sectors, cx, cy, cz);
			}
			if (s != NULL) {
				entity_l_s *t = sector_get_block_entities(s, x, y, z);
				int cnt = 0;
				{
					entity_l_s *c = t;
					while (c != NULL) {
						cnt++;
						c = c->next;
					}
				}
				int target = cam->blink % (cnt != 0 ? cnt : 1);
				while (t != NULL) {
					entity_s *te = t->ent;
					char c_chr;
					int c_r, c_g, c_b, c_a;
					if (entity_rend_chr(te, &c_chr, &c_r, &c_g, &c_b, &c_a)) {
						SDL_Rect r = {.x = i * REND_CHAR_WIDTH, .y = j * REND_CHAR_HEIGHT, .w = REND_CHAR_WIDTH, .h = REND_CHAR_HEIGHT};
						char rr[2] = {c_chr, '\0'};
						SDL_Color colo = {.r = c_r, .g = c_g, .b = c_b, .a = c_a};
						SDL_Surface *surf = TTF_RenderText_Blended(gr_font, rr, colo);
						SDL_Texture *tex = SDL_CreateTextureFromSurface(rend, surf);
						SDL_RenderCopy(rend, tex, NULL, &r);
						SDL_DestroyTexture(tex);
						SDL_FreeSurface(surf);
					}
					effect_s *fi = effect_by_type(te->effects, EF_FIRE);
					if (fi != NULL) {
						SDL_Rect r = {.x = i * REND_CHAR_WIDTH, .y = j * REND_CHAR_HEIGHT, .w = REND_CHAR_WIDTH, .h = REND_CHAR_HEIGHT};
						char rr[2] = {'!', '\0'};
						SDL_Color fire_color = {.r = 255, .g = 0, .b = 0, .a = 192};
						if (cam->blink % 2) {
							rr[0] = ')';
						}
						SDL_Surface *surf = TTF_RenderText_Blended(gr_font, rr, fire_color);
						SDL_Texture *tex = SDL_CreateTextureFromSurface(rend, surf);
						SDL_RenderCopy(rend, tex, NULL, &r);
						SDL_DestroyTexture(tex);
						SDL_FreeSurface(surf);
					}
					t = t->next;
					target--;
				}
			}
			if (i == cam->cursor_x && j == cam->cursor_y) {
				SDL_Rect r = {.x = i * REND_CHAR_WIDTH, .y = j * REND_CHAR_HEIGHT, .w = REND_CHAR_WIDTH, .h = REND_CHAR_HEIGHT};
				SDL_SetRenderDrawColor(rend, 128, 128, 0, 255);
				SDL_RenderDrawRect(rend, &r);
			}
		}
	}
}

void render_status(SDL_Renderer *rend, entity_s *ent, int xb, int yb) {
	char sl[128];
	char *slc = sl;
	memset(sl, 0, 128);
	effect_s *ef = effect_by_type(ent->effects, EF_ATTACK);
	if (ef != NULL) {
		slc += snprintf(slc, 128 - (slc - sl), "[attacking");
		effect_attack_data *d = (void*)ef->data;
		if (d->ent != NULL) {
			effect_s *mat = effect_by_type(d->ent->effects, EF_MATERIAL);
			if (mat != NULL) {
				effect_material_data *mat_d = (void*)mat->data;
				slc += snprintf(slc, 128 - (slc - sl), " target dur:%d", mat_d->dur);
			}
		}
		slc += snprintf(slc, 128 - (slc - sl), "]");
	}
	ef = effect_by_type(ent->effects, EF_LIMB_SLOT);
	if (ef != NULL) {
		effect_limb_slot_data *d = (void*)ef->data;
		entity_s *l = d->item;
		if (l != NULL) {
			effect_s *l_ef = effect_by_type(l->effects, EF_LIMB_HAND);
			if (l_ef != NULL) {
				effect_limb_hand_data *l_d = (void*)l_ef->data;
				if (l_d->item != NULL) {
					slc += snprintf(slc, 128 - (slc - sl), "[holding %p", l_d->item);
					effect_s *l_d_rend = effect_by_type(l_d->item->effects, EF_RENDER);
					if (l_d_rend != NULL) {
						effect_render_data *l_d_rend_d = (void*)l_d_rend->data;
						slc += snprintf(slc, 128 - (slc - sl), "(%c)", l_d_rend_d->chr);
					}
					effect_s *l_d_aim = effect_by_type(l_d->item->effects, EF_AIM);
					if (l_d_aim != NULL) {
						slc += snprintf(slc, 128 - (slc - sl), "aim");
					}
					slc += snprintf(slc, 128 - (slc - sl), "]");
				}
			}
		}
	}
	ef = effect_by_type(ent->effects, EF_FALLING);
	if (ef != NULL) {
		slc += snprintf(slc, 128 - (slc - sl), "[falling");
		int x, y, z;
		if (entity_coords(ent, &x, &y, &z)) {
			if (z < 0) {
				slc += snprintf(slc, 128 - (slc - sl), " in void");
			}
		}
		slc += snprintf(slc, 128 - (slc - sl), "]");
	}
	ef = effect_by_type(ent->effects, EF_BLOCK_MOVE);
	if (ef != NULL) {
		effect_block_move_data *d = (void*)ef->data;
		slc += snprintf(slc, 128 - (slc - sl), "[moving %d %d]", d->x, d->y);
	}
	ef = effect_by_type(ent->effects, EF_STAIR_MOVE);
	if (ef != NULL) {
		effect_stair_move_data *d = (void*)ef->data;
		slc += snprintf(slc, 128 - (slc - sl), "[stair %d]", d->dir);
	}
	{
		slc += snprintf(slc, 128 - (slc - sl), "[Z%d S%d M%d E%d]", eff_zero_nr, eff_small_nr, eff_medium_nr, ent_free_nr);
	}
	if (sl[0] != '\0') {
		SDL_Color colo = {.r = 0, .g = 255, .b = 128};
		SDL_Surface *surf = TTF_RenderText_Blended(gr_font, sl, colo);
		SDL_Texture *tex = SDL_CreateTextureFromSurface(rend, surf);
		SDL_Rect target;
		target.x = xb;
		target.y = yb;
		target.w = surf->w;
		target.h = surf->h;
		SDL_RenderCopy(rend, tex, NULL, &target);
		SDL_DestroyTexture(tex);
		SDL_FreeSurface(surf);
	}
}

void spawn_simple_floor(int x, int y, int z) {
	entity_s *new_ent = o_alloc_entity();
	new_ent->effects = NULL;
	{
		effect_s *ef_ph = alloc_effect(EF_PH_BLOCK);
		effect_ph_block_data *d = (void*)ef_ph->data;
		d->floor = 1;
		d->floor_up = 0;
		d->block_movement = 0;
		d->slope = 0;
		d->x = x;
		d->y = y;
		d->z = z;
		effect_prepend(new_ent, ef_ph);
	}
	{
		effect_s *ef_rend = alloc_effect(EF_RENDER);
		effect_render_data *d = (void*)ef_rend->data;
		d->r = 0;
		d->g = 255;
		d->b = 0;
		d->a = 128;
		d->chr = '_';
		effect_prepend(new_ent, ef_rend);
	}
	{
		effect_s *ef_mat = alloc_effect(EF_MATERIAL);
		effect_material_data *d = (void*)ef_mat->data;
		d->type = MAT_WOOD;
		d->dur = 10;
		d->prop = 0;
		d->tag = 0;
		effect_prepend(new_ent, ef_mat);
	}
	entity_prepend(g_entities, new_ent);
	g_entities = new_ent;
	attach_generic_entity(new_ent);
}

void spawn_pressure_floor(int x, int y, int z, int w_thresold) {
	entity_s *new_ent = o_alloc_entity();
	new_ent->effects = NULL;
	{
		effect_s *ef_ph = alloc_effect(EF_PH_BLOCK);
		effect_ph_block_data *d = (void*)ef_ph->data;
		d->floor = 1;
		d->floor_up = 0;
		d->block_movement = 0;
		d->slope = 0;
		d->x = x;
		d->y = y;
		d->z = z;
		effect_prepend(new_ent, ef_ph);
	}
	{
		effect_s *ef_rend = alloc_effect(EF_RENDER);
		effect_render_data *d = (void*)ef_rend->data;
		d->r = 128;
		d->g = 128;
		d->b = 0;
		d->a = 128;
		d->chr = '_';
		effect_prepend(new_ent, ef_rend);
	}
	{
		effect_s *ef_pr = alloc_effect(EF_A_PRESSURE_PLATE);
		effect_a_pressure_plate_data *d = (void*)ef_pr->data;
		d->thresold = w_thresold;
		effect_prepend(new_ent, ef_pr);
	}
	entity_prepend(g_entities, new_ent);
	g_entities = new_ent;
	attach_generic_entity(new_ent);
}

void spawn_simple_wall(int x, int y, int z) {
	entity_s *new_ent = o_alloc_entity();
	new_ent->effects = NULL;
	{
		effect_s *ef_ph = alloc_effect(EF_PH_BLOCK);
		effect_ph_block_data *d = (void*)ef_ph->data;
		d->floor = 0;
		d->floor_up = 1;
		d->block_movement = 1;
		d->slope = 0;
		d->x = x;
		d->y = y;
		d->z = z;
		effect_prepend(new_ent, ef_ph);
	}
	{
		effect_s *ef_rend = alloc_effect(EF_RENDER);
		effect_render_data *d = (void*)ef_rend->data;
		d->r = 0;
		d->g = 255;
		d->b = 0;
		d->a = 128;
		d->chr = '#';
		effect_prepend(new_ent, ef_rend);
	}
	{
		effect_s *ef_mat = alloc_effect(EF_MATERIAL);
		effect_material_data *d = (void*)ef_mat->data;
		d->type = MAT_WOOD;
		d->dur = 10;
		d->prop = 0;
		d->tag = 0;
		effect_prepend(new_ent, ef_mat);
	}
	entity_prepend(g_entities, new_ent);
	g_entities = new_ent;
	attach_generic_entity(new_ent);
}

void spawn_simple_door(int x, int y, int z) {
	entity_s *new_ent = o_alloc_entity();
	new_ent->effects = NULL;
	{
		effect_s *ef_ph = alloc_effect(EF_PH_BLOCK);
		effect_ph_block_data *d = (void*)ef_ph->data;
		d->floor = 0;
		d->floor_up = 1;
		d->block_movement = 1;
		d->slope = 0;
		d->x = x;
		d->y = y;
		d->z = z;
		effect_prepend(new_ent, ef_ph);
	}
	{
		effect_s *ef_rend = alloc_effect(EF_RENDER);
		effect_render_data *d = (void*)ef_rend->data;
		d->r = 0;
		d->g = 255;
		d->b = 0;
		d->a = 128;
		d->chr = '%';
		effect_prepend(new_ent, ef_rend);
	}
	{
		effect_s *ef_door = alloc_effect(EF_R_TOUCH_TOGGLE_BLOCK);
		effect_prepend(new_ent, ef_door);
	}
	{
		effect_s *ef_mat = alloc_effect(EF_MATERIAL);
		effect_material_data *d = (void*)ef_mat->data;
		d->type = MAT_WOOD;
		d->dur = 10;
		d->prop = 0;
		d->tag = 0;
		effect_prepend(new_ent, ef_mat);
	}
	entity_prepend(g_entities, new_ent);
	g_entities = new_ent;
	attach_generic_entity(new_ent);
}


void spawn_wood_piece(int x, int y, int z) {
	entity_s *new_ent = o_alloc_entity();
	new_ent->effects = NULL;
	{
		effect_s *ef_ph = alloc_effect(EF_PH_ITEM);
		effect_ph_item_data *d = (void*)ef_ph->data;
		d->x = x;
		d->y = y;
		d->z = z;
		d->weight = 3;
		d->parent = NULL;
		effect_prepend(new_ent, ef_ph);
	}
	{
		effect_s *ef_mat = alloc_effect(EF_MATERIAL);
		effect_material_data *d = (void*)ef_mat->data;
		d->type = MAT_WOOD;
		d->dur = 10;
		d->prop = 0;
		d->tag = 0;
		effect_prepend(new_ent, ef_mat);
	}
	{
		effect_s *ef_rend = alloc_effect(EF_RENDER);
		effect_render_data *d = (void*)ef_rend->data;
		d->a = 128;
		d->r = 128;
		d->g = 150;
		d->b = 150;
		d->chr = '=';
		effect_prepend(new_ent, ef_rend);
	}
	entity_prepend(g_entities, new_ent);
	g_entities = new_ent;
	attach_generic_entity(new_ent);
}

void spawn_simple_stair(int x, int y, int z) {
	entity_s *new_ent = o_alloc_entity();
	new_ent->effects = NULL;
	{
		effect_s *ef_ph = alloc_effect(EF_PH_BLOCK);
		effect_ph_block_data *d = (void*)ef_ph->data;
		d->floor = 0;
		d->stair = 1;
		d->floor_up = 0;
		d->block_movement = 0;
		d->x = x;
		d->y = y;
		d->z = z;
		effect_prepend(new_ent, ef_ph);
	}
	{
		effect_s *ef_rend = alloc_effect(EF_RENDER);
		effect_render_data *d = (void*)ef_rend->data;
		d->r = 0;
		d->g = 255;
		d->b = 0;
		d->a = 128;
		d->chr = '^';
		effect_prepend(new_ent, ef_rend);
	}
	entity_prepend(g_entities, new_ent);
	g_entities = new_ent;
	attach_generic_entity(new_ent);
}

void spawn_circle_mover(int x, int y, int z) {
	entity_s *new_ent = o_alloc_entity();
	new_ent->effects = NULL;
	{
		effect_s *ef_ph = alloc_effect(EF_PH_ITEM);
		effect_ph_item_data *d = (void*)ef_ph->data;
		d->x = x;
		d->y = y;
		d->z = z;
		d->weight = 5;
		effect_prepend(new_ent, ef_ph);
	}
	{
		effect_s *ef_rend = alloc_effect(EF_RENDER);
		effect_render_data *d = (void*)ef_rend->data;
		d->r = 0;
		d->g = 128;
		d->b = 255;
		d->a = 128;
		d->chr = 'q';
		effect_prepend(new_ent, ef_rend);
	}
	{
		effect_s *ef_mat = alloc_effect(EF_MATERIAL);
		effect_material_data *d = (void*)ef_mat->data;
		d->type = MAT_STONE;
		d->dur = 10;
		d->prop = 0;
		d->tag = 0;
		effect_prepend(new_ent, ef_mat);
	}
	{
		effect_s *ef_cir = alloc_effect(EF_A_CIRCLE_MOVE);
		effect_prepend(new_ent, ef_cir);
	}
	entity_prepend(g_entities, new_ent);
	g_entities = new_ent;
	attach_generic_entity(new_ent);
}

void setup_field(void) {
	{
		for (int i = -1; i < 3; i ++) {
			for (int j = -1; j < 3; j ++) {
				sector_s *new_sect = o_malloc(sizeof(sector_s));
				new_sect->x = i;
				new_sect->y = j;
				new_sect->z = 0;
				new_sect->snext = NULL;
				new_sect->sprev = NULL;
				memset(new_sect->block_entities, 0, sizeof(entity_l_s*) * G_SECTOR_SIZE * G_SECTOR_SIZE * G_SECTOR_SIZE);
				if (g_sectors != NULL) {
					new_sect->snext = g_sectors;
					g_sectors->sprev = new_sect;
				}
				g_sectors = new_sect;
			}
		}

		entity_s *new_ent = o_alloc_entity();
		new_ent->effects = NULL;
		{
			effect_s *ef_ph = alloc_effect(EF_PH_ITEM);
			effect_ph_item_data *d = (void*)ef_ph->data;
			d->x = 0;
			d->y = 0;
			d->z = 0;
			d->weight = 7;
			d->parent = NULL;
			effect_prepend(new_ent, ef_ph);
		}
		{
			effect_s *ef_rend = alloc_effect(EF_RENDER);
			effect_render_data *d = (void*)ef_rend->data;
			d->r = 255;
			d->g = 0;
			d->b = 0;
			d->a = 255;
			d->chr = '@';
			effect_prepend(new_ent, ef_rend);
		}
		{
			effect_s *ef_stats = alloc_effect(EF_STATS);
			effect_stats_data *d = (void*)ef_stats->data;
			d->str = 64;
			d->spd = 64;
			d->dex = 64;
			effect_prepend(new_ent, ef_stats);
		}
		{
			effect_s *ef_limb_slot = alloc_effect(EF_LIMB_SLOT);
			effect_limb_slot_data *d = (void*)ef_limb_slot->data;
			d->tag = 0;
			{
				entity_s *e_hand = o_alloc_entity();
				e_hand->effects = NULL;
				entity_prepend(g_entities, e_hand);
				g_entities = e_hand;

				effect_s *ef_hand = alloc_effect(EF_LIMB_LEG);
				effect_prepend(e_hand, ef_hand);

				effect_s *ef_item = alloc_effect(EF_PH_ITEM);
				effect_ph_item_data *dt = (void*)ef_item->data;
				dt->weight = 1;
				dt->parent = new_ent;
				dt->parent_type = PARENT_REF_LIMB;
				effect_prepend(e_hand, ef_item);

				d->item = e_hand;
			}
			effect_prepend(new_ent, ef_limb_slot);
		}
		{
			effect_s *ef_limb_slot = alloc_effect(EF_LIMB_SLOT);
			effect_limb_slot_data *d = (void*)ef_limb_slot->data;
			d->tag = 1;
			{
				entity_s *e_hand = o_alloc_entity();
				e_hand->effects = NULL;
				entity_prepend(g_entities, e_hand);
				g_entities = e_hand;

				effect_s *ef_hand = alloc_effect(EF_LIMB_HAND);
				effect_limb_hand_data *hand_d = (void*)ef_hand->data;
				hand_d->item = NULL;
				effect_prepend(e_hand, ef_hand);

				effect_s *ef_item = alloc_effect(EF_PH_ITEM);
				effect_ph_item_data *dt = (void*)ef_item->data;
				dt->weight = 1;
				dt->parent = new_ent;
				dt->parent_type = PARENT_REF_LIMB;
				effect_prepend(e_hand, ef_item);

				d->item = e_hand;
			}
			effect_prepend(new_ent, ef_limb_slot);
		}
		{
			effect_s *ef_limb_slot = alloc_effect(EF_LIMB_SLOT);
			effect_limb_slot_data *d = (void*)ef_limb_slot->data;
			{
				entity_s *e_hand = o_alloc_entity();
				e_hand->effects = NULL;
				entity_prepend(g_entities, e_hand);
				g_entities = e_hand;

				effect_s *ef_hand = alloc_effect(EF_LIMB_HAND);
				effect_limb_hand_data *hand_d = (void*)ef_hand->data;
				hand_d->item = NULL;
				effect_prepend(e_hand, ef_hand);

				effect_s *ef_mat = alloc_effect(EF_MATERIAL);
				effect_material_data *mat_d = (void*)ef_mat->data;
				mat_d->type = MAT_GHOST;
				mat_d->dur = 10;
				mat_d->prop = 0;
				mat_d->tag = 0;
				effect_prepend(e_hand, ef_mat);

				effect_s *ef_item = alloc_effect(EF_PH_ITEM);
				effect_ph_item_data *dt = (void*)ef_item->data;
				dt->weight = 1;
				dt->parent = new_ent;
				dt->parent_type = PARENT_REF_LIMB;
				effect_prepend(e_hand, ef_item);

				d->item = e_hand;
			}
			effect_prepend(new_ent, ef_limb_slot);
		}
		entity_prepend(g_entities, new_ent);
		g_entities = new_ent;
		attach_generic_entity(new_ent);
	}
	{
		entity_s *new_ent = o_alloc_entity();
		new_ent->effects = NULL;
		{
			effect_s *ef_ph = alloc_effect(EF_PH_ITEM);
			effect_ph_item_data *d = (void*)ef_ph->data;
			d->x = 0;
			d->y = 0;
			d->z = 0;
			d->weight = 2;
			d->parent = NULL;
			effect_prepend(new_ent, ef_ph);
		}
		{
			effect_s *ef_tp = alloc_effect(EF_R_TOUCH_RNG_TP);
			effect_prepend(new_ent, ef_tp);
		}
		{
			effect_s *ef_rend = alloc_effect(EF_RENDER);
			effect_render_data *d = (void*)ef_rend->data;
			d->r = 0;
			d->g = 255;
			d->b = 0;
			d->a = 128;
			d->chr = '\'';
			effect_prepend(new_ent, ef_rend);
		}
		entity_prepend(g_entities, new_ent);
		g_entities = new_ent;
		attach_generic_entity(new_ent);
	}
	{
		entity_s *new_ent = o_alloc_entity();
		new_ent->effects = NULL;
		{
			effect_s *new_eff = alloc_effect(EF_ROTATION);
			effect_rotation_data *d = (void*)new_eff->data;
			d->type = RT_DICE;
			d->rotation = 1;
			effect_prepend(new_ent, new_eff);
		}
		{
			effect_s *ef_ph = alloc_effect(EF_PH_ITEM);
			effect_ph_item_data *d = (void*)ef_ph->data;
			d->x = 1;
			d->y = 0;
			d->z = 0;
			d->weight = 1;
			d->parent = NULL;
			effect_prepend(new_ent, ef_ph);
		}
		{
			effect_s *new_eff = alloc_effect(EF_RENDER);
			effect_render_data *d = (void*)new_eff->data;
			d->r = 0;
			d->b = 100;
			d->g = 120;
			d->a = 100;
			d->chr = '1';
			effect_prepend(new_ent, new_eff);
		}
		entity_prepend(g_entities, new_ent);
		g_entities = new_ent;
		attach_generic_entity(new_ent);
	}
	{
		entity_s *new_ent = o_alloc_entity();
		new_ent->effects = NULL;
		{
			effect_s *ef_ph = alloc_effect(EF_PH_ITEM);
			effect_ph_item_data *d = (void*)ef_ph->data;
			d->x = 1;
			d->y = 0;
			d->z = 0;
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
			d->x = G_TRACER_RESOLUTION / 2;
			d->y = G_TRACER_RESOLUTION;
			d->z = 0;
			d->cur_x = 0;
			d->cur_y = 0;
			d->cur_z = 0;
			d->speed = 5;
			effect_prepend(new_ent, new_eff);
		}
		entity_prepend(g_entities, new_ent);
		g_entities = new_ent;
		attach_generic_entity(new_ent);
	}
	{
		entity_s *new_ent = o_alloc_entity();
		new_ent->effects = NULL;
		{
			effect_s *ef_ph = alloc_effect(EF_PH_ITEM);
			effect_ph_item_data *d = (void*)ef_ph->data;
			d->x = 1;
			d->y = 0;
			d->z = 0;
			d->weight = 4;
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
			d->chr = '-';
			effect_prepend(new_ent, new_eff);
		}
		{
			effect_s *new_eff = alloc_effect(EF_R_TOUCH_SHOOT_PROJECTILE);
			effect_prepend(new_ent, new_eff);
		}
		{
			effect_s *new_eff = alloc_effect(EF_AIM);
			effect_aim_data *d = (void*)new_eff->data;
			d->x = G_TRACER_RESOLUTION / 2;
			d->y = G_TRACER_RESOLUTION;
			d->z = 0;
			d->ent = NULL;
			effect_prepend(new_ent, new_eff);
		}
		entity_prepend(g_entities, new_ent);
		g_entities = new_ent;
		attach_generic_entity(new_ent);
	}
	{
		entity_s *new_ent = o_alloc_entity();
		new_ent->effects = NULL;
		{
			effect_s *ef_ph = alloc_effect(EF_PH_ITEM);
			effect_ph_item_data *d = (void*)ef_ph->data;
			d->x = 3;
			d->y = 0;
			d->z = 0;
			d->weight = 8;
			d->parent = NULL;
			effect_prepend(new_ent, ef_ph);
		}
		{
			effect_s *new_eff = alloc_effect(EF_RENDER);
			effect_render_data *d = (void*)new_eff->data;
			d->r = 60;
			d->b = 200;
			d->g = 150;
			d->a = 128;
			d->chr = '=';
			effect_prepend(new_ent, new_eff);
		}
		{
			effect_s *new_eff = alloc_effect(EF_TABLE);
			effect_prepend(new_ent, new_eff);
		}
		entity_prepend(g_entities, new_ent);
		g_entities = new_ent;
		attach_generic_entity(new_ent);
	}
	{
		entity_s *new_ent = o_alloc_entity();
		new_ent->effects = NULL;
		{
			effect_s *ef_ph = alloc_effect(EF_PH_ITEM);
			effect_ph_item_data *d = (void*)ef_ph->data;
			d->x = 2;
			d->y = 2;
			d->z = 1;
			d->weight = 0;
			d->parent = NULL;
			effect_prepend(new_ent, ef_ph);
		}
		{
			effect_s *ef_ph = alloc_effect(EF_PH_LIQUID);
			effect_ph_liquid_data *d = (void*)ef_ph->data;
			d->amount = 200;
			d->type = LIQ_WATER;
			effect_prepend(new_ent, ef_ph);
		}
		entity_prepend(g_entities, new_ent);
		g_entities = new_ent;
		attach_generic_entity(new_ent);
	}
	{
		entity_s *new_ent = o_alloc_entity();
		new_ent->effects = NULL;
		{
			effect_s *ph_item = alloc_effect(EF_PH_ITEM);
			effect_ph_item_data *d = (void*)ph_item->data;
			d->x = 3;
			d->y = 3;
			d->z = 0;
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
			effect_s *cont = alloc_effect(EF_CONTAINER);
			effect_container_data *d = (void*)cont->data;
			d->capacity = 10;
			d->cont_mask = 0;
			effect_prepend(new_ent, cont);
		}
		entity_prepend(g_entities, new_ent);
		g_entities = new_ent;
		attach_generic_entity(new_ent);
	}
	{
		entity_s *new_ent = o_alloc_entity();
		new_ent->effects = NULL;
		{
			effect_s *new_eff = alloc_effect(EF_PH_ITEM);
			effect_ph_item_data *d = (void*)new_eff->data;
			d->x = 2;
			d->y = 2;
			d->z = 0;
			d->weight = 4;
			d->parent = NULL;
			effect_prepend(new_ent, new_eff);
		}
		{
			effect_s *new_eff = alloc_effect(EF_RENDER);
			effect_render_data *d = (void*)new_eff->data;
			d->chr = 's';
			d->r = 128;
			d->g = 64;
			d->b = 64;
			d->a = 0;
			effect_prepend(new_ent, new_eff);
		}
		{
			effect_s *new_eff = alloc_effect(EF_MATERIAL);
			effect_material_data *d = (void*)new_eff->data;
			d->type = MAT_GHOST;
			d->dur = 20;
			d->prop = MATP_SMALL;
			d->tag = 0;
			effect_prepend(new_ent, new_eff);
		}
		{
			effect_s *new_eff = alloc_effect(EF_MATERIAL);
			effect_material_data *d = (void*)new_eff->data;
			d->type = MAT_GHOST;
			d->dur = 20;
			d->prop = MATP_SHARP;
			d->tag = 1;
			effect_prepend(new_ent, new_eff);
		}
		attach_generic_entity(new_ent);
		entity_prepend(g_entities, new_ent);
		g_entities = new_ent;
	}
	for (int i = 0; i < 5; i ++) {
		if (i == 2) {
			spawn_simple_door(i, 5, 0);
		} else {
			spawn_simple_wall(i, 5, 0);
		}
		spawn_simple_wall(5, i, 0);
		spawn_simple_wall(-1, i, 0);
		spawn_simple_wall(i, -1, 0);
		for (int j = 0; j < 10; j ++) {
			spawn_simple_floor(i, j, 0);
			if (i != 1 || j != 1) {
				if (i == 2 && j == 2) {
					spawn_pressure_floor(i, j, 1, 5);
				} else {
					spawn_simple_floor(i, j, 1);
				}
			}
		}
	}
	spawn_simple_wall(5, 5, 0);
	spawn_simple_wall(-1, -1, 0);
	spawn_simple_wall(5, -1, 0);
	spawn_simple_wall(-1, 5, 0);
	spawn_wood_piece(0, 2, 0);
	spawn_simple_stair(1, 1, 0);
	spawn_circle_mover(3, 3, 1);
}

#define INPUT_ARG_MAX 16

typedef enum cmap_arg_t_type {
	CMAP_ARG_ENTITY,
	CMAP_ARG_TILE,
	CMAP_ARG_EFFECT,
	CMAP_ARG_NONE,
} cmap_arg_t_type;

typedef struct cmap_arg_t {
	void *data;
	cmap_arg_t_type type;
} cmap_arg_t;

typedef struct cmap_params_t {
	entity_s *control_ent;
	cmap_arg_t args[INPUT_ARG_MAX];
	int nargs;
	int key;
	int c_id;
} cmap_params_t;

typedef struct cmap_t {
	int key;
	const char *s;
	void (*callback)(cmap_params_t*);
} cmap_t;

cmap_params_t gu_params;

void params_clear(void) {
	gu_params.nargs = 0;
}

void params_push_entity(entity_s *s) {
	if (gu_params.nargs == INPUT_ARG_MAX) {
		fprintf(stderr, "[WARN] Exceeded INPUT_ARG_MAX\n");
		return;
	}
	gu_params.args[gu_params.nargs].type = CMAP_ARG_ENTITY;
	gu_params.args[gu_params.nargs].data = s;
	gu_params.nargs++;
}

void params_push_effect(effect_s *s) {
	if (gu_params.nargs == INPUT_ARG_MAX) {
		fprintf(stderr, "[WARN] Exceeded INPUT_ARG_MAX\n");
		return;
	}
	gu_params.args[gu_params.nargs].type = CMAP_ARG_EFFECT;
	gu_params.args[gu_params.nargs].data = s;
	gu_params.nargs++;
}

typedef struct gu_things_t {
	int skip_moving:1;
	int no_trigger:1;
} gu_things_t;

gu_things_t gu_things;

void cmap_toggle_skip_moving(cmap_params_t *p) {
	(void)p;
	gu_things.skip_moving ^= 1;
	gu_things.no_trigger = 1;
}

void cmap_wait(cmap_params_t *p) {
	(void)p;
}

void cmap_go_up(cmap_params_t *p) {
	trigger_go_up(p->control_ent, 1);
}

void cmap_go_down(cmap_params_t *p) {
	trigger_go_down(p->control_ent, 1);
}

int key_to_direction(int key, int *x, int *y) {
	switch (key) {
	case SDLK_KP_1: {
		*x = -1;
		*y = 1;
	} break;
	case SDLK_KP_2: {
		*x = 0;
		*y = 1;
	} break;
	case SDLK_KP_3: {
		*x = 1;
		*y = 1;
	} break;
	case SDLK_KP_4: {
		*x = -1;
		*y = 0;
	} break;
	case SDLK_KP_6: {
		*x = 1;
		*y = 0;
	} break;
	case SDLK_KP_7: {
		*x = -1;
		*y = -1;
	} break;
	case SDLK_KP_8: {
		*x = 0;
		*y = -1;
	} break;
	case SDLK_KP_9: {
		*x = 1;
		*y = -1;
	} break;
	default: {
		return 0;
	}
	}
	return 1;
}

void cmap_go(cmap_params_t *p) {
	int x, y;
	if (key_to_direction(p->key, &x, &y))
		trigger_move(p->control_ent, x, y, 0);
}

void cmap_put(cmap_params_t *p) {
	if (p->nargs != 2) {
		fprintf(stderr, "Wrong number of arguments to cmap_put\n");
		return;
	}
	if (p->args[0].type != CMAP_ARG_EFFECT || p->args[1].type != CMAP_ARG_ENTITY) {
		fprintf(stderr, "Wrong argument to cmap_put\n");
		return;
	}
	trigger_put(p->control_ent, p->args[0].data, p->args[1].data);
}

void cmap_drop(cmap_params_t *p) {
	if (p->nargs != 1) {
		fprintf(stderr, "Wrong number of arguments to cmap_drop\n");
		return;
	}
	if (p->args[0].type != CMAP_ARG_EFFECT) {
		fprintf(stderr, "Wrong argument to cmap_drop\n");
		return;
	}
	trigger_drop(p->control_ent, p->args[0].data);
}

void cmap_grab(cmap_params_t *p) {
	if (p->nargs != 3) {
		fprintf(stderr, "Wrong number of arguments to cmap_grab\n");
		return;
	}
	if (p->args[0].type != CMAP_ARG_EFFECT || p->args[1].type != CMAP_ARG_ENTITY || p->args[2].type != CMAP_ARG_EFFECT) {
		fprintf(stderr, "Wrong argument to cmap_grab\n");
		return;
	}
	int mat_tag = 0;
	effect_s *t = p->args[2].data;
	if (t != NULL && t->type == EF_MATERIAL) {
		effect_material_data *d = (void*)t->data;
		mat_tag = d->tag;
	}
	trigger_grab(p->control_ent, p->args[0].data, p->args[1].data, mat_tag);
}

void cmap_open_door(cmap_params_t *p) {
	if (p->nargs != 2) {
		fprintf(stderr, "Wrong number of arguments to cmap_open_door, found %d\n", p->nargs);
		return;
	}
	if (p->args[0].type != CMAP_ARG_EFFECT || p->args[1].type != CMAP_ARG_ENTITY) {
		fprintf(stderr, "Wrong arguments to cmap_open_door\n");
		return;
	}
	trigger_touch(p->control_ent, p->args[0].data, p->args[1].data);
}

void cmap_throw(cmap_params_t *p) {
	if (p->nargs != 2) {
		fprintf(stderr, "Wrong number of arguments to cmap_throw\n");
		return;
	}
	if (p->args[0].type != CMAP_ARG_ENTITY || p->args[1].type != CMAP_ARG_ENTITY) {
		fprintf(stderr, "Wrong arguments to cmap_throw\n");
		return;
	}
	/* TODO */
}

void cmap_attack(cmap_params_t *p) {
	if (p->nargs != 3) {
		fprintf(stderr, "Wrong number of arguments to cmap_attack\n");
		return;
	}
	if (p->args[0].type != CMAP_ARG_ENTITY || p->args[1].type != CMAP_ARG_EFFECT || p->args[2].type != CMAP_ARG_EFFECT) {
		fprintf(stderr, "Wrong arguments to cmap_attack\n");
		return;
	}
	effect_limb_slot_data *td = (void*)((effect_s*)p->args[1].data)->data;
	entity_s *used_weapon = NULL;
	if (td->item != NULL) {
		effect_s *t = effect_by_type(td->item->effects, EF_LIMB_HAND);
		if (t != NULL) {
			effect_limb_hand_data *d = (void*)t->data;
			used_weapon = d->item;
		}
	}
	int mat_tag = 0;
	effect_s *ef_mat = p->args[2].data;
	if (ef_mat->type == EF_MATERIAL) {
		effect_material_data *d = (void*)ef_mat->data;
		mat_tag = d->tag;
	}
	trigger_attack(
		p->control_ent,
		p->args[0].data,
		ATK_SWING,
		used_weapon,
		mat_tag
	);
}

const cmap_t command_maps[] = {
	(cmap_t){'/', NULL, cmap_toggle_skip_moving},
	(cmap_t){'w', NULL, cmap_wait},
	(cmap_t){'<', NULL, cmap_go_up},
	(cmap_t){'>', NULL, cmap_go_down},
	(cmap_t){SDLK_KP_1, NULL, cmap_go},
	(cmap_t){SDLK_KP_2, NULL, cmap_go},
	(cmap_t){SDLK_KP_3, NULL, cmap_go},
	(cmap_t){SDLK_KP_4, NULL, cmap_go},
	(cmap_t){SDLK_KP_6, NULL, cmap_go},
	(cmap_t){SDLK_KP_7, NULL, cmap_go},
	(cmap_t){SDLK_KP_8, NULL, cmap_go},
	(cmap_t){SDLK_KP_9, NULL, cmap_go},
	(cmap_t){'p', "He(Put where?)", cmap_put},
	(cmap_t){'d', "H(Drop what?)", cmap_drop},
	(cmap_t){'g', "He(Grab what?)g(By what?)", cmap_grab},
	(cmap_t){'o', "He(Open door?)", cmap_open_door},
	(cmap_t){'t', "He(Into what?)", cmap_throw},
	(cmap_t){'a', "e(Attack what?)Hg", cmap_attack},
};
const int n_command_maps = sizeof(command_maps) / sizeof(command_maps[0]);

typedef enum inputw_type {
	INPUTW_ORIGIN,
	INPUTW_TILE,
	INPUTW_ENTITY,
	INPUTW_DIRECTION,
	INPUTW_LIMB,
	INPUTW_LIMB_DEFAULT,
	INPUTW_EFFECT,
} inputw_type;

typedef struct inputw_tile_s {
	int x;
	int y;
	int z;
} inputw_tile_s;

typedef struct inputw_entity_s {
	int x;
	int y;
	int z;
	entity_l_s *cur_list;
	entity_l_s *cur_sel;
} inputw_entity_s;

typedef enum inputw_effect_sel_type {
	INP_EF_ANY,
	INP_EF_MATERIAL,
} inputw_effect_sel_type;

typedef struct inputw_limb_s {
	entity_s *ent;
	effect_s *cur_sel;
} inputw_limb_s;

typedef struct inputw_effect_s {
	entity_s *ent;
	effect_s *cur_sel;
	inputw_effect_sel_type type;
} inputw_effect_s;

#define INPUTW_DATA_SIZE 24
typedef struct inputw_t {
	inputw_type type;
	char data[INPUTW_DATA_SIZE];
	void *data1;
	union {
		inputw_tile_s u_tile;
		inputw_entity_s u_entity;
		inputw_limb_s u_limb;
		inputw_effect_s u_effect;
	} data_u;
} inputw_t;

#define INP_M_REDRAW 1
#define INP_M_NEXT   2
#define INP_M_RELOAD 4

#define INPUTW_MAXNR 16
inputw_t inputws[INPUTW_MAXNR];
int inputw_n = 0;
inputw_t inputw_queue[INPUTW_MAXNR];
int inputw_queue_n = 0;

void input_queue_flush(void) {
	int exc = 0;
	while (inputw_queue_n > 0) {
		inputw_queue_n--;
		if (inputw_n < INPUTW_MAXNR) {
			memcpy(&inputws[inputw_n], &inputw_queue[inputw_queue_n], sizeof(inputw_t));
			inputw_n++;
		} else {
			exc = 1;
		}
	}
	if (exc) {
		fprintf(stderr, "Exceeded number of input layers\n");
	}
}

void input_queue_entity_select(entity_s *control_ent, const char *msg) {
	if (inputw_queue_n == INPUTW_MAXNR) {
		fprintf(stderr, "Exceeded number of input layers\n");
		return;
	}
	inputw_queue[inputw_queue_n].type = INPUTW_ENTITY;
	int x = 0, y = 0, z = 0;
	if (entity_coords(control_ent, &x, &y, &z)) {
		inputw_entity_s *e = &inputw_queue[inputw_queue_n].data_u.u_entity;
		e->x = x;
		e->y = y;
		e->z = z;
		e->cur_sel = NULL;
	}
	strncpy(inputw_queue[inputw_queue_n].data, msg, INPUTW_DATA_SIZE);
	inputw_queue_n++;
}

void input_queue_limb_select(entity_s *control_ent, const char *msg) {
	(void)control_ent;
	if (inputw_queue_n == INPUTW_MAXNR) {
		fprintf(stderr, "Exceeded number of input layers\n");
		return;
	}
	inputw_queue[inputw_queue_n].type = INPUTW_LIMB;
	strncpy(inputw_queue[inputw_queue_n].data, msg, INPUTW_DATA_SIZE);
	inputw_queue_n++;
}

void input_queue_limb_default(entity_s *control_ent) {
	(void)control_ent;
	if (inputw_queue_n == INPUTW_MAXNR) {
		fprintf(stderr, "Exceeded number of input layers\n");
		return;
	}
	inputw_queue[inputw_queue_n].type = INPUTW_LIMB_DEFAULT;
	inputw_queue_n++;
}

void input_queue_effect(entity_s *control_ent, inputw_effect_sel_type type) {
	(void)control_ent;
	if (inputw_queue_n == INPUTW_MAXNR) {
		fprintf(stderr, "Exceeded number of input layers\n");
		return;
	}
	inputw_queue[inputw_queue_n].type = INPUTW_EFFECT;
	inputw_queue[inputw_queue_n].data_u.u_effect.type = type;
	inputw_queue_n++;
}

void inputw_clear(void) {
	inputw_n = 0;
	/* TODO inputw clear already stored args */
}

int command_parse_message(const char *s, char *buf, int buf_n) {
	int nr = 0, c = 0;
	if (s[nr] == '(') {
		nr++;
		while (s[nr] != '\0' && s[nr] != ')') {
			if (c < buf_n - 1)
				buf[c++] = s[nr];
			nr++;
		}
		if (s[nr] == ')')
			nr++;
	}
	buf[c] = '\0';
	return nr;
}

int command_arg_e(entity_s *control_ent, const char *s) {
	char msg_buf[INPUTW_DATA_SIZE];
	int nr = 0;
	nr = command_parse_message(s, msg_buf, 32);
	printf("arg e message (%s)\n", msg_buf);
	input_queue_entity_select(control_ent, msg_buf);
	return nr;
}

int command_arg_h(entity_s *control_ent, const char *s) {
	char msg_buf[INPUTW_DATA_SIZE];
	int nr = 0;
	nr = command_parse_message(s, msg_buf, 32);
	printf("arg h message (%s)\n", msg_buf);
	input_queue_limb_select(control_ent, msg_buf);
	return nr;
}

int command_arg_h_big(entity_s *control_ent, const char *s) {
	char msg_buf[INPUTW_DATA_SIZE];
	int nr;
	(void)control_ent;
	nr = command_parse_message(s, msg_buf, 32);
	printf("arg H message (%s)\n", msg_buf);
	input_queue_limb_default(control_ent);
	return nr;
}

int command_arg_g(entity_s *control_ent, const char *s) {
	char msg_buf[INPUTW_DATA_SIZE];
	int nr;
	(void)control_ent;
	(void)s;
	nr = command_parse_message(s, msg_buf, 32);
	printf("arg g message (%s)\n", msg_buf);
	input_queue_effect(control_ent, INP_EF_MATERIAL);
	return nr;
}

void cmap_clear_args(void) {
	gu_params.nargs = 0;
}

void command_map_exec(entity_s *control_ent, int n) {
	const char *s = command_maps[n].s;
	gu_params.control_ent = control_ent;
	gu_params.c_id = n;
	cmap_clear_args();
	if (s == NULL) {
		gu_params.key = command_maps[n].key;
		command_maps[n].callback(&gu_params);
		return;
	}
	int t = 0;
	while (s[t] != '\0') {
		switch (s[t]) {
		case 'e': {
			t += 1 + command_arg_e(control_ent, s+t+1);
		} break;
		case 'h': {
			t += 1 + command_arg_h(control_ent, s+t+1);
		} break;
		case 'H': {
			t += 1 + command_arg_h_big(control_ent, s+t+1);
		} break;
		case 'g': {
			t += 1 + command_arg_g(control_ent, s+t+1);
		} break;
		default: {
			fprintf(stderr, "Unrecognised command char '%c'\n", s[t]);
			return;
		}
		}
	}
	input_queue_flush();
}

int inputw_tile_key(SDL_Keycode sym) {
	if (inputw_n <= 0) {
		fprintf(stderr, "No input layers in inputw_tile_key\n");
		return 0;
	}
	inputw_t *l = &inputws[inputw_n-1];
	inputw_tile_s *t = &l->data_u.u_tile;
	int x, y;
	if (key_to_direction(sym, &x, &y)) {
		t->x += x;
		t->y += y;
		return INP_M_REDRAW;
	}
	if (sym == SDLK_ESCAPE) {
		inputw_clear();
		return INP_M_REDRAW;
	}
	return 0;
}

int inputw_entity_key(SDL_Keycode sym) {
	if (inputw_n <= 0) {
		fprintf(stderr, "No input layers in inputw_entity_key\n");
		return 0;
	}
	if (inputws[inputw_n - 1].type != INPUTW_ENTITY) {
		fprintf(stderr, "Input layer isn't entity\n");
		return 0;
	}
	inputw_entity_s *e = &inputws[inputw_n - 1].data_u.u_entity;
	int x, y;
	if (key_to_direction(sym, &x, &y)) {
		e->x += x;
		e->y += y;
		int x = e->x, y = e->y, z = e->z, cx = 0, cy = 0, cz = 0;
		coord_normalize(&x, &cx);
		coord_normalize(&y, &cy);
		coord_normalize(&z, &cz);
		sector_s *sec = sector_get_sector(g_sectors, cx, cy, cz);
		entity_l_s_free(e->cur_list);
		e->cur_list = sector_get_block_entities_indirect(sec, x, y, z);
		e->cur_sel = e->cur_list;
		return INP_M_REDRAW;
	}
	if (sym == SDLK_ESCAPE) {
		inputw_clear();
		return INP_M_REDRAW;
	}
	if (sym == SDLK_DOWN) {
		if (e->cur_sel != NULL && e->cur_sel->next != NULL)
			e->cur_sel = e->cur_sel->next;
		return INP_M_REDRAW;
	}
	if (sym == SDLK_UP) {
		if (e->cur_sel != NULL && e->cur_sel->prev != NULL)
			e->cur_sel = e->cur_sel->prev;
		return INP_M_REDRAW;
	}
	if (sym == SDLK_RETURN) {
		if (e->cur_sel != NULL) {
			params_push_entity(e->cur_sel->ent);
			entity_l_s_free(e->cur_list);
			e->cur_list = NULL;
			return INP_M_NEXT;
		}
		return 0;
	}
	return 0;
}

int inputw_limb_key(SDL_Keycode sym) {
	if (inputw_n <= 0) {
		fprintf(stderr, "No input layers in inputw_limb_key\n");
		return 0;
	}
	if (inputws[inputw_n - 1].type != INPUTW_LIMB) {
		fprintf(stderr, "Input layer isn't limb\n");
		return 0;
	}
	inputw_limb_s *e = &inputws[inputw_n - 1].data_u.u_limb;
	if (sym == SDLK_ESCAPE) {
		inputw_clear();
		return INP_M_REDRAW;
	}
	if (sym == SDLK_DOWN) {
		effect_s *t = next_effect_by_type(e->cur_sel, EF_LIMB_SLOT);
		if (t != NULL)
			e->cur_sel = t;
		return INP_M_REDRAW;
	}
	if (sym == SDLK_UP) {
		effect_s *t = prev_effect_by_type(e->cur_sel, EF_LIMB_SLOT);
		if (t != NULL)
			e->cur_sel = t;
		return INP_M_REDRAW;
	}
	if (sym == SDLK_RETURN) {
		params_push_effect(e->cur_sel);
		return INP_M_NEXT;
	}
	return 0;
}

int inputw_limb_default_key(SDL_Keycode sym) {
	if (inputw_n <= 0) {
		fprintf(stderr, "No input layers in inputw_limb_default_key\n");
		return 0;
	}
	if (inputws[inputw_n - 1].type != INPUTW_LIMB_DEFAULT) {
		fprintf(stderr, "Input layer isn't limb_default\n");
		return 0;
	}
	if (sym == SDLK_ESCAPE) {
		inputw_clear();
		return INP_M_REDRAW;
	}
	if (sym == 'm') {
		memset(&inputws[inputw_n - 1].data_u, 0, sizeof(inputws->data_u));
		inputws[inputw_n - 1].type = INPUTW_LIMB;
		inputws[inputw_n - 1].data_u.u_limb.ent = gu_params.control_ent;
		return INP_M_RELOAD;
	}
	if (sym == SDLK_RETURN) {
		effect_s *slot = effect_by_type(gu_params.control_ent->effects, EF_LIMB_SLOT);
		params_push_effect(slot);
		return INP_M_NEXT;
	}
	return 0;
}

int inputw_effect_key(SDL_Keycode sym) {
	if (inputw_n <= 0) {
		fprintf(stderr, "No input layers in inputw_effect_key\n");
		return 0;
	}
	if (inputws[inputw_n - 1].type != INPUTW_EFFECT) {
		fprintf(stderr, "Input layer isn't effect\n");
		return 0;
	}
	inputw_effect_s *e = &inputws[inputw_n - 1].data_u.u_effect;
	if (sym == SDLK_ESCAPE) {
		inputw_clear();
		return INP_M_REDRAW;
	}
	if (sym == SDLK_DOWN) {
		effect_s *t = next_effect_by_type(e->cur_sel, EF_MATERIAL);
		if (t != NULL)
			e->cur_sel = t;
		return INP_M_REDRAW;
	}
	if (sym == SDLK_UP) {
		effect_s *t = prev_effect_by_type(e->cur_sel, EF_MATERIAL);
		if (t != NULL)
			e->cur_sel = t;
		return INP_M_REDRAW;
	}
	if (sym == SDLK_RETURN) {
		if (e->cur_sel != NULL) {
			params_push_effect(e->cur_sel);
			return INP_M_NEXT;
		}
	}
	return 0;
}

void render_layer_adjust(camera_view_s *cam, entity_s *control_ent) {
	(void)control_ent;
	cam->cursor_x = -1;
	cam->cursor_y = -1;
	switch (inputw_n == 0 ? INPUTW_ORIGIN : inputws[inputw_n - 1].type) {
	case INPUTW_ORIGIN: {
	} break;
	case INPUTW_TILE: {
	} break;
	case INPUTW_ENTITY: {
		inputw_entity_s *e = &inputws[inputw_n - 1].data_u.u_entity;
		cam->x = e->x - 8;
		cam->y = e->y - 8;
		cam->cursor_x = e->x - cam->x;
		cam->cursor_y = e->y - cam->y;
		cam->z = e->z;
	} break;
	default: {
	}
	}
}

void render_list_layers(SDL_Renderer *rend, int x, int y) {
	int i, cy = y;
	char buf[32];
	for (i = 0; i < inputw_n; i++) {
		snprintf(
			buf, 32, "%s",
			inputws[i].type == INPUTW_ORIGIN ? "origin" :
			inputws[i].type == INPUTW_TILE ? "tile" :
			inputws[i].type == INPUTW_ENTITY ? "entity" :
			inputws[i].type == INPUTW_LIMB ? "limb" :
			inputws[i].type == INPUTW_LIMB_DEFAULT ? "limb_default" :
			inputws[i].type == INPUTW_EFFECT ? "effect" :
			"*"
		);
		{
			SDL_Color colo = {.r = 0, .g = 255, .b = 128};
			SDL_Surface *surf = TTF_RenderText_Blended(gr_font, buf, colo);
			SDL_Texture *tex = SDL_CreateTextureFromSurface(rend, surf);
			SDL_Rect target;
			target.x = x;
			target.y = cy;
			target.w = surf->w;
			target.h = surf->h;
			SDL_RenderCopy(rend, tex, NULL, &target);
			SDL_DestroyTexture(tex);
			SDL_FreeSurface(surf);
		}
		cy += gr_rend_char_height;
	}
}

void render_layer_specific(SDL_Renderer *rend, int x, int y) {
	char buf[32];
	if (inputw_n == 0)
		return;

	switch (inputws[inputw_n - 1].type) {
	case INPUTW_TILE: {
	} break;
	case INPUTW_ENTITY: {
		char rend_char = '\0';
		int yc = y;
		entity_l_s *cur = inputws[inputw_n - 1].data_u.u_entity.cur_list;
		while (cur != NULL) {
			entity_s *cur_ent = cur->ent;
			effect_s *e_rend = effect_by_type(cur_ent->effects, EF_RENDER);
			effect_s *e_liq = effect_by_type(cur_ent->effects, EF_PH_LIQUID);
			if (e_liq != NULL) {
				effect_ph_liquid_data *liq_d = (void*)e_liq->data;
				snprintf(
					buf, 32,
					"~%s a:%d",
					liq_d->type == LIQ_WATER ? "water" : "*",
					liq_d->amount
				);
			} else {
				if (e_rend != NULL) {
					effect_render_data *d = (void*)e_rend->data;
					rend_char = d->chr;
				} else {
					rend_char = '\0';
				}
				if (rend_char != '\0') {
					snprintf(buf, 32, "%p %c", cur_ent, rend_char);
				} else {
					snprintf(buf, 32, "%p", cur_ent);
				}
			}
			SDL_Color colo = {.r = 0, .g = 255, .b = 128};
			if (cur != inputws[inputw_n - 1].data_u.u_entity.cur_sel) {
				colo.g /= 2;
				colo.b /= 2;
			}
			SDL_Surface *surf = TTF_RenderText_Blended(gr_font, buf, colo);
			SDL_Texture *tex = SDL_CreateTextureFromSurface(rend, surf);
			SDL_Rect target;
			target.x = x;
			target.y = yc;
			target.w = surf->w;
			target.h = surf->h;
			yc += target.h;
			SDL_RenderCopy(rend, tex, NULL, &target);
			SDL_DestroyTexture(tex);
			SDL_FreeSurface(surf);
			cur = cur->next;
		}
	} break;
	case INPUTW_LIMB: {
		int yc = y;
		effect_s *e = gu_params.control_ent != NULL ? gu_params.control_ent->effects : NULL;
		while (e != NULL) {
			if (e->type == EF_LIMB_SLOT) {
				effect_limb_slot_data *d = (void*)e->data;
				if (d->item != NULL && effect_by_type(d->item->effects, EF_LIMB_HAND) != NULL) {
					snprintf(buf, 32, "%p %d", d->item, d->tag);
					SDL_Color colo = {.r = 0, .g = 255, .b = 128};
					if (e != inputws[inputw_n - 1].data_u.u_limb.cur_sel) {
						colo.g /= 2;
						colo.b /= 2;
					}
					SDL_Surface *surf = TTF_RenderText_Blended(gr_font, buf, colo);
					SDL_Texture *tex = SDL_CreateTextureFromSurface(rend, surf);
					SDL_Rect target;
					target.x = x;
					target.y = yc;
					target.w = surf->w;
					target.h = surf->h;
					yc += target.h;
					SDL_RenderCopy(rend, tex, NULL, &target);
					SDL_DestroyTexture(tex);
					SDL_FreeSurface(surf);
				}
			}
			e = e->next;
		}
	} break;
	case INPUTW_EFFECT: {
		int yc = y;
		entity_s *ent = inputws[inputw_n - 1].data_u.u_effect.ent;
		effect_s *e = ent == NULL ? NULL : ent->effects;
		while (e != NULL) {
			if (e->type == EF_MATERIAL) {
				effect_material_data *d = (void*)e->data;
				snprintf(
					buf, 32, "%p %d %s:%s",
					e,
					d->tag,
					d->prop & MATP_SHARP ? "sharp" : "",
					d->prop & MATP_SMALL ? "small" : ""
				);
				SDL_Color colo = {.r = 0, .g = 255, .b = 128};
				if (e != inputws[inputw_n - 1].data_u.u_effect.cur_sel) {
					colo.g /= 2;
					colo.b /= 2;
				}
				SDL_Surface *surf = TTF_RenderText_Blended(gr_font, buf, colo);
				SDL_Texture *tex = SDL_CreateTextureFromSurface(rend, surf);
				SDL_Rect target;
				target.x = x;
				target.y = yc;
				target.w = surf->w;
				target.h = surf->h;
				yc += target.h;
				SDL_RenderCopy(rend, tex, NULL, &target);
				SDL_DestroyTexture(tex);
				SDL_FreeSurface(surf);
			}
			e = e->next;
		}
	} break;
	default: {
	}
	}
}

void inputw_layer_enter() {
	if (inputw_n == 0)
		return;
	switch (inputws[inputw_n - 1].type) {
	case INPUTW_ENTITY: {
		inputw_entity_s *e = &inputws[inputw_n - 1].data_u.u_entity;
		int x = e->x, y = e->y, z = e->z, cx = 0, cy = 0, cz = 0;
		coord_normalize(&x, &cx);
		coord_normalize(&y, &cy);
		coord_normalize(&z, &cz);
		sector_s *sec = sector_get_sector(g_sectors, cx, cy, cz);
		e->cur_list = sector_get_block_entities_indirect(sec, x, y, z);
		e->cur_sel = e->cur_list;
	} break;
	case INPUTW_LIMB: {
		inputw_limb_s *e = &inputws[inputw_n - 1].data_u.u_limb;
		if (e->ent != NULL)
			e->cur_sel = effect_by_type(e->ent->effects, EF_LIMB_SLOT);
	} break;
	case INPUTW_EFFECT: {
		inputw_effect_s *e = &inputws[inputw_n - 1].data_u.u_effect;
		e->ent = NULL;
		if (gu_params.nargs != 0) {
			switch (gu_params.args[gu_params.nargs - 1].type) {
			case CMAP_ARG_ENTITY: {
				e->ent = gu_params.args[gu_params.nargs - 1].data;
			} break;
			case CMAP_ARG_EFFECT: {
				effect_s *ef = gu_params.args[gu_params.nargs - 1].data;
				if (ef->type == EF_LIMB_SLOT) {
					effect_limb_slot_data *d = (void*)ef->data;
					entity_s *t = d->item;
					if (t != NULL) {
						effect_s *te = effect_by_type(t->effects, EF_LIMB_HAND);
						if (te != NULL) {
							effect_limb_hand_data *dt = (void*)te->data;
							e->ent = dt->item;
						}
					}
				}
			} break;
			default: {
			}
			}
		}
		e->cur_sel = e->ent == NULL ? NULL : effect_by_type(e->ent->effects, EF_MATERIAL);
	} break;
	default: {
	}
	}
}

int main(int argc, char **argv) {
	o_init_allocator();
	gu_things.skip_moving = 1;

	SDL_Init(SDL_INIT_EVERYTHING);
	TTF_Init();
	SDL_Window *win = SDL_CreateWindow("effect-sandbox", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 700, 700, SDL_WINDOW_RESIZABLE);
	SDL_Renderer *rend = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
	SDL_ShowWindow(win);
	SDL_SetRenderDrawBlendMode(rend, SDL_BLENDMODE_BLEND);
	int running = 1;
	camera_view_s cam = {.x = 0, .y = 0, .z = 0, .width = 16, .height = 16, .blink = 0};
	gr_font = TTF_OpenFont("/usr/share/fonts/TTF/IosevkaNerdFont-Regular.ttf", 24);

	/* Monospace fonts only */
	TTF_SizeText(gr_font, "a", &gr_rend_char_width, &gr_rend_char_height);
	{
		int x, y;
		TTF_SizeText(gr_font, "w", &x, &y);
		if (gr_rend_char_width != x || gr_rend_char_height != y) {
			fprintf(stderr, "[WARN] Font is not monospace?\n");
		}
	}

	g_dice = o_malloc(sizeof(rng_state_s));
	rng_init(g_dice);

	if (argc >= 2 && !strcmp(argv[1], "read")) {
		const char *fname = argc >= 3 ? argv[2] : "save";
		FILE *stream = fopen(fname, "rb");
		if (stream != NULL) {
			g_entities = load_sector_list(stream);
			for (int i = -1; i < 3; i ++) {
				for (int j = -1; j < 3; j ++) {
					sector_s *new_sect = o_malloc(sizeof(sector_s));
					new_sect->x = i;
					new_sect->y = j;
					new_sect->z = 0;
					new_sect->snext = NULL;
					new_sect->sprev = NULL;
					memset(new_sect->block_entities, 0, sizeof(new_sect->block_entities));
					if (g_sectors != NULL) {
						new_sect->snext = g_sectors;
						g_sectors->sprev = new_sect;
					}
					g_sectors = new_sect;
				}
			}
			entity_s *c_ent = g_entities;
			while (c_ent != NULL && c_ent->next != NULL) {
				c_ent = c_ent->next;
			}
			while (c_ent != NULL) {
				attach_generic_entity(c_ent);
				c_ent = c_ent->prev;
			}
		} else {
			fprintf(stderr, "Failed to read dump\n");
			return 1;
		}
	} else {
		setup_field();
	}

	entity_s *control_ent = NULL;
	{
		entity_s *c_ent = g_entities;
		while (c_ent != NULL) {
			effect_s *ef_rend = effect_by_type(c_ent->effects, EF_RENDER);
			if (ef_rend != NULL) {
				effect_render_data *data = (void*)ef_rend->data;
				if (data->chr == '@') {
					control_ent = c_ent;
					break;
				}
			}
			c_ent = c_ent->next;
		}
	}

	uint64_t last_blink = 0;
	int need_redraw = 1;
	int skip_tick = 0;

	if (control_ent == NULL) {
		fprintf(stderr, "No controlling entity, quit\n");
		running = 0;
	}

	while (running) {
		unsigned long long cur_ticks = SDL_GetTicks();
		if (cur_ticks > last_blink + 500) {
			last_blink = cur_ticks;
			cam.blink++;
			need_redraw = 1;
		}
		if (need_redraw) {
			{
				int x, y, z;
				if (entity_coords(control_ent, &x, &y, &z)) {
					cam.z = z;
					cam.x = x - 8;
					cam.y = y - 8;
				}
			}
			SDL_SetRenderDrawColor(rend, 0, 0, 0, 0);
			SDL_RenderClear(rend);
			render_layer_adjust(&cam, control_ent);
			render_camera(rend, &cam);
			render_status(rend, control_ent, 0, cam.height * gr_rend_char_height);
			render_list_layers(rend, cam.width * gr_rend_char_width, 0);
			render_layer_specific(rend, cam.width * gr_rend_char_width, inputw_n * gr_rend_char_height);
			SDL_RenderPresent(rend);
			need_redraw = 0;
		} else {
			SDL_Delay(40);
		}
		skip_tick = 0;
		if (gu_things.skip_moving) {
			if (
				effect_by_type(control_ent->effects, EF_BLOCK_MOVE) != NULL ||
				effect_by_type(control_ent->effects, EF_STAIR_MOVE) != NULL
			) {
				skip_tick = 1;
			}
		}
		SDL_Event evt;
		int trigger_done = 0;
		while (SDL_PollEvent(&evt)) {
			if (evt.type == SDL_WINDOWEVENT) {
				switch (evt.window.event) {
				case SDL_WINDOWEVENT_EXPOSED:
				case SDL_WINDOWEVENT_RESIZED: {
					need_redraw = 1;
				} break;
				}
			} else if (evt.type == SDL_KEYDOWN) {
				SDL_Keycode sym = evt.key.keysym.sym;
				if (evt.key.keysym.mod & KMOD_SHIFT) {
					if (sym == ',') sym = '<';
					else if (sym == '.') sym = '>';
				}
				if (inputw_n == 0 && !skip_tick) {
					/* TODO check if it's reaction time for player */
					for (int i = 0; i < n_command_maps; i++) {
						if (command_maps[i].key == sym) {
							command_map_exec(control_ent, i);
							inputw_layer_enter();
							if (inputw_n == 0) {
								if (!gu_things.no_trigger)
									trigger_done = 1;
								else
									gu_things.no_trigger = 0;
							} else {
								need_redraw = 1;
							}
							break;
						}
					}
				} else {
					int mask = 0;
					switch (inputws[inputw_n - 1].type) {
					case INPUTW_TILE: {
						mask = inputw_tile_key(sym);
					} break;
					case INPUTW_ENTITY: {
						mask = inputw_entity_key(sym);
					} break;
					case INPUTW_LIMB: {
						mask = inputw_limb_key(sym);
					} break;
					case INPUTW_LIMB_DEFAULT: {
						mask = inputw_limb_default_key(sym);
					} break;
					case INPUTW_EFFECT: {
						mask = inputw_effect_key(sym);
					} break;
					default: {
					}
					}
					if (mask & INP_M_REDRAW)
						need_redraw = 1;
					if (mask & INP_M_NEXT) {
						need_redraw = 1;
						inputw_n--;
						inputw_layer_enter();
						if (inputw_n == 0) {
							command_maps[gu_params.c_id].callback(&gu_params);
							if (!gu_things.no_trigger)
								trigger_done = 1;
							else
								gu_things.no_trigger = 0;
						}
					}
					if (mask & INP_M_RELOAD) {
						inputw_layer_enter();
						need_redraw = 1;
					}
				}
			}
			if (evt.type == SDL_QUIT) {
				running = 0;
			}
		}
		if (trigger_done > 0 || skip_tick) {
			process_tick(g_entities);
			g_entities = clear_nonexistent(g_entities);
			need_redraw = 1;
		}
	}
	{
		FILE *save_file = fopen("save", "w");
		if (save_file != NULL) {
			dump_sector_list(g_sectors, save_file);
		} else {
			perror("Failed to open save file: ");
		}
	}
	o_free(g_dice);
	TTF_CloseFont(gr_font);
	SDL_DestroyRenderer(rend);
	SDL_DestroyWindow(win);
	SDL_Quit();
}
