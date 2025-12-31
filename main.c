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

int entity_rend_chr(ent_ptr ent, char *chr, int *r, int *g, int *b, int *a) {
	{
		effect_render_data d;
		if (entity_load_effect(ent, EF_RENDER, &d)) {
			*chr = d.chr;
			*r = d.r;
			*g = d.g;
			*b = d.b;
			*a = d.a;
			return 1;
		}
	}
	{
		effect_rain_data rd;
		if (entity_load_effect(ent, EF_RAIN, &rd)) {
			if (rd.type == 0) {
				*chr = '\'';
				*r = 13;
				*g = 90;
				*b = 232;
				*a = 128;
			} else {
				*chr = '*';
				*r = 227;
				*g = 237;
				*b = 237;
				*a = 128;
			}
			return 1;
		}
	}
	{
		effect_ph_liquid_data d;
		if (entity_load_effect(ent, EF_PH_LIQUID, &d)) {
			*chr = '~';
			if (d.type == LIQ_WATER) {
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
	}
	{
		effect_pile_data d;
		if (entity_load_effect(ent, EF_PILE, &d)) {
			*chr = ',';
			switch (d.type) {
			case PILE_SNOW: {
				*r = 227;
				*g = 237;
				*b = 237;
				*a = 128;
			} break;
			}
			return 1;
		}
	}
	return 0;
}

void render_camera(SDL_Renderer *rend, camera_view_s *cam) {
	int prev_cx = INT_MIN;
	int prev_cy = INT_MIN;

	int z = cam->z;
	int cz = 0;
	int z_below = z - 1;
	int cz_below = 0;
	coord_normalize(&z, &cz);
	coord_normalize(&z_below, &cz_below);

	SDL_SetRenderDrawColor(rend, 0, 0, 0, 255);
	for (int i = 0; i < cam->width; i ++) {
		for (int j = 0; j < cam->height; j ++) {
			SDL_Rect r = {.x = i * REND_CHAR_WIDTH, .y = j * REND_CHAR_HEIGHT, .w = REND_CHAR_WIDTH, .h = REND_CHAR_HEIGHT};
			SDL_RenderFillRect(rend, &r);
		}
	}
	sector_s *s = NULL, *s_below = NULL;
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
				if (cz_below != cz)
					s_below = sector_get_sector(g_sectors, cx, cy, cz_below);
				else
					s_below = s;
			}
			SDL_Rect cur_rect = {.x = i * REND_CHAR_WIDTH, .y = j * REND_CHAR_HEIGHT, .w = REND_CHAR_WIDTH, .h = REND_CHAR_HEIGHT};
			if (s_below != NULL) {
				int done_floor = 0;
				effect_ph_block_data bd;
				if (
					s_below->block_blocks[x][y][z_below].type != BLK_EMPTY &&
					entity_load_effect(ent_cptr(s_below, x, y, z_below), EF_PH_BLOCK, &bd) &&
					(bd.prop & PB_FLOOR_UP)
				) {
					done_floor = 1;
					char c_chr;
					int c_r, c_g, c_b, c_a;
					if (entity_rend_chr(ent_cptr(s_below, x, y, z_below), &c_chr, &c_r, &c_g, &c_b, &c_a)) {
						char rr[2] = {'.', '\0'};
						c_a /= 2;
						SDL_Color colo = {.r = c_r, .g = c_g, .b = c_b, .a = c_a};
						SDL_Surface *surf = TTF_RenderText_Blended(gr_font, rr, colo);
						SDL_Texture *tex = SDL_CreateTextureFromSurface(rend, surf);
						SDL_RenderCopy(rend, tex, NULL, &cur_rect);
						SDL_DestroyTexture(tex);
						SDL_FreeSurface(surf);
					}
				}
				if (!done_floor) {
					entity_l_s *bl = sector_get_block_entities(s_below, x, y, z_below);
					char c_chr;
					int c_r, c_g, c_b, c_a;
					while (bl != NULL) {
						if (
							entity_load_effect(bl->ent, EF_PH_BLOCK, &bd) &&
							(bd.prop && PB_FLOOR_UP) &&
							entity_rend_chr(bl->ent, &c_chr, &c_r, &c_g, &c_b, &c_a)
						) {
							char rr[2] = {'.', '\0'};
							c_a /= 2;
							SDL_Color colo = {.r = c_r, .g = c_g, .b = c_b, .a = c_a};
							SDL_Surface *surf = TTF_RenderText_Blended(gr_font, rr, colo);
							SDL_Texture *tex = SDL_CreateTextureFromSurface(rend, surf);
							SDL_RenderCopy(rend, tex, NULL, &cur_rect);
							SDL_DestroyTexture(tex);
							SDL_FreeSurface(surf);
							done_floor = 1;
							break;
						}
						bl = bl->next;
					}
				}
			}
			if (s != NULL) {
				if (s->block_blocks[x][y][z].type != BLK_EMPTY) {
					char c_chr;
					int c_r, c_g, c_b, c_a;
					if (entity_rend_chr(ent_cptr(s, x, y, z), &c_chr, &c_r, &c_g, &c_b, &c_a)) {
						char rr[2] = {c_chr, '\0'};
						SDL_Color colo = {.r = c_r, .g = c_g, .b = c_b, .a = c_a};
						SDL_Surface *surf = TTF_RenderText_Blended(gr_font, rr, colo);
						SDL_Texture *tex = SDL_CreateTextureFromSurface(rend, surf);
						SDL_RenderCopy(rend, tex, NULL, &cur_rect);
						SDL_DestroyTexture(tex);
						SDL_FreeSurface(surf);
					}
				}
				entity_l_s *t = sector_get_block_entities(s, x, y, z);
				for (; t != NULL; t = t->next) {
					if (entity_has_effect(t->ent, EF_RAIN) && rng_next_g(g_dice) % 16 != 7)
						continue;
					entity_s *te = ent_aptr(t->ent);
					char c_chr;
					int c_r, c_g, c_b, c_a;
					if (entity_rend_chr(t->ent, &c_chr, &c_r, &c_g, &c_b, &c_a)) {
						char rr[2] = {c_chr, '\0'};
						SDL_Color colo = {.r = c_r, .g = c_g, .b = c_b, .a = c_a};
						SDL_Surface *surf = TTF_RenderText_Blended(gr_font, rr, colo);
						SDL_Texture *tex = SDL_CreateTextureFromSurface(rend, surf);
						SDL_RenderCopy(rend, tex, NULL, &cur_rect);
						SDL_DestroyTexture(tex);
						SDL_FreeSurface(surf);
					}
					effect_s *fi = effect_by_type(te->effects, EF_FIRE);
					if (fi != NULL) {
						char rr[2] = {'!', '\0'};
						SDL_Color fire_color = {.r = 255, .g = 0, .b = 0, .a = 192};
						if (cam->blink % 2) {
							rr[0] = ' ';
						}
						SDL_Surface *surf = TTF_RenderText_Blended(gr_font, rr, fire_color);
						SDL_Texture *tex = SDL_CreateTextureFromSurface(rend, surf);
						SDL_RenderCopy(rend, tex, NULL, &cur_rect);
						SDL_DestroyTexture(tex);
						SDL_FreeSurface(surf);
					}
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
		if (d->ent != ENT_NULL) {
			effect_material_data mat_d;
			if (entity_load_effect(d->ent, EF_MATERIAL, &mat_d)) {
				slc += snprintf(slc, 128 - (slc - sl), " target dur:%d", mat_d.dur);
			}
		}
		slc += snprintf(slc, 128 - (slc - sl), "]");
	}
	ef = effect_by_type(ent->effects, EF_LIMB_SLOT);
	if (ef != NULL) {
		effect_limb_slot_data *d = (void*)ef->data;
		ent_ptr l = d->item;
		if (l != ENT_NULL) {
			effect_limb_hand_data l_d;
			if (entity_load_effect(l, EF_LIMB_HAND, &l_d)) {
				if (l_d.item != ENT_NULL) {
					slc += snprintf(slc, 128 - (slc - sl), "[holding %lu", l_d.item);
					{
						effect_render_data l_d_rend_d;
						if (entity_load_effect(l_d.item, EF_RENDER, &l_d_rend_d)) {
							slc += snprintf(slc, 128 - (slc - sl), "(%c)", l_d_rend_d.chr);
						}
					}
					{
						if (entity_has_effect(l_d.item, EF_AIM)) {
							slc += snprintf(slc, 128 - (slc - sl), "aim");
						}
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
		if (entity_coords(ent_sptr(ent), &x, &y, &z)) {
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

void spawn_comp_block(int x, int y, int z, block_type t, int dur) {
	int xc = 0, yc = 0, zc = 0;
	coord_normalize(&x, &xc);
	coord_normalize(&y, &yc);
	coord_normalize(&z, &zc);
	sector_s *sec = sector_get_sector(g_sectors, xc, yc, zc);
	if (sec != NULL) {
		sec->block_blocks[x][y][z].type = t;
		sec->block_blocks[x][y][z].dur = dur;
	}
}

void spawn_simple_floor_uncomp(int x, int y, int z) {
	entity_s *new_ent = o_alloc_entity();
	new_ent->effects = NULL;
	new_ent->common_type = CT_B_FLOOR;
	((int*)new_ent->common_data)[0] = x;
	((int*)new_ent->common_data)[1] = y;
	((int*)new_ent->common_data)[2] = z;
	((int*)new_ent->common_data)[3] = 10;
	entity_prepend(g_entities, new_ent);
	g_entities = new_ent;
	attach_generic_entity(ent_sptr(new_ent));
}

void spawn_simple_floor(int x, int y, int z) {
	int xc = 0, yc = 0, zc = 0;
	coord_normalize(&x, &xc);
	coord_normalize(&y, &yc);
	coord_normalize(&z, &zc);
	sector_s *sec = sector_get_sector(g_sectors, xc, yc, zc);
	if (sec != NULL) {
		if (sec->block_blocks[x][y][z].type == BLK_EMPTY) {
			sec->block_blocks[x][y][z].type = BLK_FLOOR;
			sec->block_blocks[x][y][z].dur = 10;
		} else {
			spawn_simple_floor_uncomp(
				x + xc * G_SECTOR_SIZE,
				y + yc * G_SECTOR_SIZE,
				z + zc * G_SECTOR_SIZE
			);
		}
	}
}

void spawn_pressure_floor(int x, int y, int z, int w_thresold) {
	entity_s *new_ent = o_alloc_entity();
	new_ent->effects = NULL;
	{
		effect_s *ef_ph = alloc_effect(EF_PH_BLOCK);
		effect_ph_block_data *d = (void*)ef_ph->data;
		d->prop = PB_FLOOR;
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
	attach_generic_entity(ent_sptr(new_ent));
}

void spawn_simple_wall_uncomp(int x, int y, int z) {
	entity_s *new_ent = o_alloc_entity();
	new_ent->effects = NULL;
	new_ent->common_type = CT_B_WALL;
	((int*)new_ent->common_data)[0] = x;
	((int*)new_ent->common_data)[1] = y;
	((int*)new_ent->common_data)[2] = z;
	((int*)new_ent->common_data)[3] = 10;
	entity_prepend(g_entities, new_ent);
	g_entities = new_ent;
	attach_generic_entity(ent_sptr(new_ent));
}

void spawn_simple_wall(int x, int y, int z) {
	int xc = 0, yc = 0, zc = 0;
	coord_normalize(&x, &xc);
	coord_normalize(&y, &yc);
	coord_normalize(&z, &zc);
	sector_s *sec = sector_get_sector(g_sectors, xc, yc, zc);
	if (sec != NULL) {
		if (sec->block_blocks[x][y][z].type == BLK_EMPTY) {
			sec->block_blocks[x][y][z].type = BLK_WALL;
			sec->block_blocks[x][y][z].dur = 10;
		} else {
			spawn_simple_wall_uncomp(
				x + xc * G_SECTOR_SIZE,
				y + yc * G_SECTOR_SIZE,
				z + zc * G_SECTOR_SIZE
			);
		}
	}
}
void spawn_simple_soil_uncomp(int x, int y, int z) {
	entity_s *new_ent = o_alloc_entity();
	new_ent->effects = NULL;
	new_ent->common_type = CT_B_SOIL;
	((int*)new_ent->common_data)[0] = x;
	((int*)new_ent->common_data)[1] = y;
	((int*)new_ent->common_data)[2] = z;
	((int*)new_ent->common_data)[3] = 10;
	entity_prepend(g_entities, new_ent);
	g_entities = new_ent;
	attach_generic_entity(ent_sptr(new_ent));
}

void spawn_simple_soil(int x, int y, int z) {
	int xc = 0, yc = 0, zc = 0;
	coord_normalize(&x, &xc);
	coord_normalize(&y, &yc);
	coord_normalize(&z, &zc);
	sector_s *sec = sector_get_sector(g_sectors, xc, yc, zc);
	if (sec != NULL) {
		if (sec->block_blocks[x][y][z].type == BLK_EMPTY) {
			sec->block_blocks[x][y][z].type = BLK_SOIL;
			sec->block_blocks[x][y][z].dur = 10;
		} else {
			spawn_simple_soil_uncomp(
				x + xc * G_SECTOR_SIZE,
				y + yc * G_SECTOR_SIZE,
				z + zc * G_SECTOR_SIZE
			);
		}
	}
}

void spawn_simple_door(int x, int y, int z) {
	entity_s *new_ent = o_alloc_entity();
	new_ent->effects = NULL;
	{
		effect_s *ef_ph = alloc_effect(EF_PH_BLOCK);
		effect_ph_block_data *d = (void*)ef_ph->data;
		d->prop = PB_FLOOR_UP | PB_BLOCK_MOVEMENT;
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
		effect_s *ef_door = alloc_effect(EF_DOOR);
		effect_door_data *d = (void*)ef_door->data;
		d->opened = 0;
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
	attach_generic_entity(ent_sptr(new_ent));
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
		d->parent = ENT_NULL;
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
	attach_generic_entity(ent_sptr(new_ent));
}

void spawn_simple_stair(int x, int y, int z) {
	entity_s *new_ent = o_alloc_entity();
	new_ent->effects = NULL;
	{
		effect_s *ef_ph = alloc_effect(EF_PH_BLOCK);
		effect_ph_block_data *d = (void*)ef_ph->data;
		d->prop = PB_STAIR;
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
	attach_generic_entity(ent_sptr(new_ent));
}

void spawn_circle_mover(int x, int y, int z, char chr) {
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
		d->chr = chr;
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
	attach_generic_entity(ent_sptr(new_ent));
}

void setup_field(void) {
	{
		for (int i = -1; i < 20; i ++) {
			for (int j = -1; j < 20; j ++) {
				for (int z = -1; z <= 1; z++) {
					sector_s *new_sect = o_alloc_sector();
					new_sect->x = i;
					new_sect->y = j;
					new_sect->z = z;
					new_sect->prio = rng_bigrange(g_dice);
					memset(new_sect->ch, 0, sizeof(new_sect->ch));
					memset(new_sect->block_entities, 0, sizeof(new_sect->block_entities));
					memset(new_sect->block_blocks, 0, sizeof(new_sect->block_blocks));
					g_sectors = sector_insert(g_sectors, new_sect);
				}
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
			d->parent = ENT_NULL;
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
		for (int i = 0; i < 2; i++) {
			effect_s *ef_limb_slot = alloc_effect(EF_LIMB_SLOT);
			effect_limb_slot_data *d = (void*)ef_limb_slot->data;
			d->tag = i;
			{
				entity_s *e_leg = o_alloc_entity();
				e_leg->effects = NULL;
				entity_prepend(g_entities, e_leg);
				g_entities = e_leg;

				effect_s *ef_hand = alloc_effect(EF_LIMB_LEG);
				effect_prepend(e_leg, ef_hand);

				effect_s *ef_mat = alloc_effect(EF_MATERIAL);
				effect_material_data *mat_d = (void*)ef_mat->data;
				mat_d->type = MAT_GHOST;
				mat_d->dur = 10;
				mat_d->prop = 0;
				mat_d->tag = 0;
				effect_prepend(e_leg, ef_mat);

				effect_s *ef_item = alloc_effect(EF_PH_ITEM);
				effect_ph_item_data *dt = (void*)ef_item->data;
				dt->weight = 1;
				dt->parent = ent_sptr(new_ent);
				dt->parent_type = PARENT_REF_LIMB;
				effect_prepend(e_leg, ef_item);

				d->item = ent_sptr(e_leg);
			}
			effect_prepend(new_ent, ef_limb_slot);
		}
		for (int i = 0; i < 2; i++) {
			effect_s *ef_limb_slot = alloc_effect(EF_LIMB_SLOT);
			effect_limb_slot_data *d = (void*)ef_limb_slot->data;
			d->tag = i + 2;
			{
				entity_s *e_hand = o_alloc_entity();
				e_hand->effects = NULL;
				entity_prepend(g_entities, e_hand);
				g_entities = e_hand;

				effect_s *ef_hand = alloc_effect(EF_LIMB_HAND);
				effect_limb_hand_data *hand_d = (void*)ef_hand->data;
				hand_d->item = ENT_NULL;
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
				dt->parent = ent_sptr(new_ent);
				dt->parent_type = PARENT_REF_LIMB;
				effect_prepend(e_hand, ef_item);

				d->item = ent_sptr(e_hand);
			}
			effect_prepend(new_ent, ef_limb_slot);
		}
		entity_prepend(g_entities, new_ent);
		g_entities = new_ent;
		attach_generic_entity(ent_sptr(new_ent));
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
			d->parent = ENT_NULL;
			effect_prepend(new_ent, ef_ph);
		}
		{
			effect_s *ef_mat = alloc_effect(EF_MATERIAL);
			effect_material_data *d = (void*)ef_mat;
			d->type = MAT_WOOD;
			d->dur = 10;
			d->prop = 0;
			d->tag = 0;
			effect_prepend(new_ent, ef_mat);
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
		{
			effect_s *ef_r = alloc_effect(EF_R_BOTTLE_DISPENSER);
			effect_r_bottle_dispenser_data *d = (void*)ef_r->data;
			d->mat_tag = 0;
			effect_prepend(new_ent, ef_r);
		}
		entity_prepend(g_entities, new_ent);
		g_entities = new_ent;
		attach_generic_entity(ent_sptr(new_ent));
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
			d->parent = ENT_NULL;
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
		{
			effect_s *new_eff = alloc_effect(EF_MATERIAL);
			effect_material_data *d = (void*)new_eff->data;
			d->type = MAT_STONE;
			d->tag = 0;
			d->prop = 0;
			effect_prepend(new_ent, new_eff);
		}
		entity_prepend(g_entities, new_ent);
		g_entities = new_ent;
		attach_generic_entity(ent_sptr(new_ent));
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
			d->parent = ENT_NULL;
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
		attach_generic_entity(ent_sptr(new_ent));
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
			d->parent = ENT_NULL;
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
#if 0
		{
			effect_s *new_eff = alloc_effect(EF_R_TOUCH_SHOOT_PROJECTILE);
			effect_prepend(new_ent, new_eff);
		}
#endif
		{
			effect_s *new_eff = alloc_effect(EF_AIM);
			effect_aim_data *d = (void*)new_eff->data;
			d->x = G_TRACER_RESOLUTION / 2;
			d->y = G_TRACER_RESOLUTION;
			d->z = 0;
			d->ent = ENT_NULL;
			effect_prepend(new_ent, new_eff);
		}
		entity_prepend(g_entities, new_ent);
		g_entities = new_ent;
		attach_generic_entity(ent_sptr(new_ent));
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
			d->parent = ENT_NULL;
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
		attach_generic_entity(ent_sptr(new_ent));
	}
	{
		entity_s *new_ent = o_alloc_entity();
		new_ent->common_type = CT_LIQUID;
		{
			effect_s *ef_ph = alloc_effect(EF_PH_ITEM);
			effect_ph_item_data *d = (void*)ef_ph->data;
			d->x = 10;
			d->y = 5;
			d->z = 2;
			d->weight = 0;
			d->parent = ENT_NULL;
			effect_prepend(new_ent, ef_ph);
		}
		{
			effect_ph_liquid_data d;
			d.amount = G_PUDDLE_MAX * 5;
			d.type = LIQ_WATER;
			entity_store_effect(ent_sptr(new_ent), EF_PH_LIQUID, &d);
		}
		entity_prepend(g_entities, new_ent);
		g_entities = new_ent;
		attach_generic_entity(ent_sptr(new_ent));
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
			d->parent = ENT_NULL;
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
		attach_generic_entity(ent_sptr(new_ent));
		entity_prepend(g_entities, new_ent);
		g_entities = new_ent;
	}
	{
		entity_s *new_ent = o_alloc_entity();
		new_ent->effects = NULL;
		{
			effect_s *new_eff = alloc_effect(EF_RENDER);
			effect_render_data *d = (void*)new_eff->data;
			d->chr = '|';
			d->r = 128;
			d->g = 128;
			d->b = 128;
			d->a = 128;
			effect_prepend(new_ent, new_eff);
		}
		{
			effect_s *new_eff = alloc_effect(EF_PH_ITEM);
			effect_ph_item_data *d = (void*)new_eff->data;
			d->x = 10;
			d->y = 5;
			d->z = 0;
			d->weight = 1;
			d->parent = ENT_NULL;
			effect_prepend(new_ent, new_eff);
		}
		{
			effect_s *new_eff = alloc_effect(EF_MATERIAL);
			effect_material_data *d = (void*)new_eff->data;
			d->type = MAT_PLANT;
			d->dur = 20;
			d->prop = 0;
			d->tag = 0;
			effect_prepend(new_ent, new_eff);
		}
		{
			effect_s *new_eff = alloc_effect(EF_PLANT);
			effect_plant_data *d = (void*)new_eff->data;
			d->plant_type = PLANT_TREE;
			d->stored_energy = 10;
			d->stored_water = 10;
			d->growth = 0;
			d->cycle_time = 2355;
			effect_prepend(new_ent, new_eff);
		}
		{
			effect_s *new_eff = alloc_effect(EF_ROOTED);
			effect_rooted_data *d = (void*)new_eff->data;
			d->dur = 1;
			// the cptr is created even before the block there appears
			d->ent = ent_cptr(sector_get_sector(g_sectors, 10/8, 5/8, -1), 10 % 8, 5 % 8, 7);
			effect_prepend(new_ent, new_eff);
		}
		attach_generic_entity(ent_sptr(new_ent));
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
			if (i != 1 || j != 1) {
				if (i == 2 && j == 2) {
					spawn_pressure_floor(i, j, 1, 5);
				} else {
					spawn_simple_floor(i, j, 1);
				}
			}
		}
	}
	for (int i = 0; i < G_SECTOR_SIZE * 5; i++) {
		for (int j = 0; j < G_SECTOR_SIZE * 5; j++) {
			// spawn_simple_floor(i, j, 0);
			for (int k = -1; k >= -6; k--) {
				spawn_simple_soil(i, j, k);
			}
		}
	}
	char ch = 'a';
	for (int i = 7; i < G_SECTOR_SIZE * 2; i++) {
		for (int j = 7; j < G_SECTOR_SIZE * 2; j++) {
			spawn_circle_mover(i, j, 0, ch);
			ch++;
			if (ch == 'z' + 1) ch = 'a';
		}
	}
	spawn_simple_wall(5, 5, 0);
	spawn_simple_wall(-1, -1, 0);
	spawn_simple_wall(5, -1, 0);
	spawn_simple_wall(-1, 5, 0);
	spawn_wood_piece(0, 2, 0);
	spawn_simple_stair(1, 1, 0);
	spawn_circle_mover(3, 3, 1, 'q');
}

#define INPUT_ARG_MAX 16

typedef enum cmap_arg_t_type {
	CMAP_ARG_NONE,
	CMAP_ARG_ENTITY,
	CMAP_ARG_TILE,
	CMAP_ARG_EFFECT,
	CMAP_ARG_ATTACK,
	CMAP_ARG_NUMBER,
} cmap_arg_t_type;

typedef struct cmap_arg_t {
	union {
		void *data;
		ent_ptr data_p;
		long long data_l;
	};
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

void params_push_entity(ent_ptr s) {
	if (gu_params.nargs == INPUT_ARG_MAX) {
		fprintf(stderr, "[WARN] Exceeded INPUT_ARG_MAX\n");
		return;
	}
	gu_params.args[gu_params.nargs].type = CMAP_ARG_ENTITY;
	gu_params.args[gu_params.nargs].data_p = s;
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

void params_push_attack(attack_l_s *s) {
	if (gu_params.nargs == INPUT_ARG_MAX) {
		fprintf(stderr, "[WARN] Exceeded INPUT_ARG_MAX\n");
		return;
	}
	gu_params.args[gu_params.nargs].type = CMAP_ARG_ATTACK;
	gu_params.args[gu_params.nargs].data = s;
	gu_params.nargs++;
}

void params_push_number(long long x) {
	if (gu_params.nargs == INPUT_ARG_MAX) {
		fprintf(stderr, "[WARN] Exceeded INPUT_ARG_MAX\n");
		return;
	}
	gu_params.args[gu_params.nargs].type = CMAP_ARG_NUMBER;
	gu_params.args[gu_params.nargs].data_l = x;
	gu_params.nargs++;
}

void cmap_params_cleanup(void) {
	int i;
	for (i = 0; i < gu_params.nargs; i++) {
		if (gu_params.args[i].type == CMAP_ARG_ATTACK) {
			o_free(gu_params.args[i].data);
			gu_params.args[i].data = NULL;
		}
		gu_params.args[i].type = -1;
	}
	gu_params.nargs = 0;
}

typedef struct gu_things_t {
	unsigned skip_moving:1;
	unsigned no_trigger:1;
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
	trigger_go_up(ent_sptr(p->control_ent));
}

void cmap_go_down(cmap_params_t *p) {
	trigger_go_down(ent_sptr(p->control_ent));
}

int key_to_direction(int key, int *x, int *y, int *z) {
	*x = 0;
	*y = 0;
	*z = 0;
	switch (key) {
	case SDLK_KP_1: {
		*x = -1;
		*y = 1;
	} break;
	case SDLK_KP_2: {
		*y = 1;
	} break;
	case SDLK_KP_3: {
		*x = 1;
		*y = 1;
	} break;
	case SDLK_KP_4: {
		*x = -1;
	} break;
	case SDLK_KP_6: {
		*x = 1;
	} break;
	case SDLK_KP_7: {
		*x = -1;
		*y = -1;
	} break;
	case SDLK_KP_8: {
		*y = -1;
	} break;
	case SDLK_KP_9: {
		*x = 1;
		*y = -1;
	} break;
	case 'j': {
		*z = -1;
	} break;
	case 'k': {
		*z = 1;
	} break;
	default: {
		return 0;
	}
	}
	return 1;
}

void cmap_go(cmap_params_t *p) {
	int x, y, z;
	if (key_to_direction(p->key, &x, &y, &z) && z == 0) {
		trigger_move(ent_sptr(p->control_ent), x, y, 0);
	}
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
	trigger_put(ent_sptr(p->control_ent), p->args[0].data, p->args[1].data_p);
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
	trigger_drop(ent_sptr(p->control_ent), p->args[0].data);
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
	trigger_grab(ent_sptr(p->control_ent), p->args[0].data, p->args[1].data_p, mat_tag);
}

void cmap_grab_pile(cmap_params_t *p) {
	if (p->nargs != 3) {
		fprintf(stderr, "Wrong number of arguments to cmap_grab_pile\n");
		return;
	}
	if (p->args[0].type != CMAP_ARG_EFFECT || p->args[1].type != CMAP_ARG_ENTITY || p->args[2].type != CMAP_ARG_NUMBER) {
		fprintf(stderr, "Wrong argument to cmap_grab_pile\n");
		return;
	}
	trigger_grab_pile(ent_sptr(p->control_ent), p->args[0].data, p->args[1].data_p, (int)(intptr_t)p->args[2].data);
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
	trigger_open_door(ent_sptr(p->control_ent), p->args[0].data, p->args[1].data_p, 64);
}

void cmap_close_door(cmap_params_t *p) {
	if (p->nargs != 2) {
		fprintf(stderr, "Wrong number of arguments to cmap_open_door, found %d\n", p->nargs);
		return;
	}
	if (p->args[0].type != CMAP_ARG_EFFECT || p->args[1].type != CMAP_ARG_ENTITY) {
		fprintf(stderr, "Wrong arguments to cmap_open_door\n");
		return;
	}
	trigger_open_door(ent_sptr(p->control_ent), p->args[0].data, p->args[1].data_p, -64);
}

void cmap_throw(cmap_params_t *p) {
	if (p->nargs != 2) {
		fprintf(stderr, "Wrong number of arguments to cmap_throw\n");
		return;
	}
	if (p->args[0].type != CMAP_ARG_EFFECT || p->args[1].type != CMAP_ARG_ENTITY) {
		fprintf(stderr, "Wrong arguments to cmap_throw\n");
		return;
	}
	effect_s *used_hand = p->args[0].data;
	ent_ptr target = p->args[1].data_p;
	if (target != ENT_NULL) {
		int x, y, z, xd, yd, zd, xs, ys, zs;
		if (entity_coords(target, &x, &y, &z) && entity_coords(ent_sptr(p->control_ent), &xs, &ys, &zs)) {
			xd = x - xs;
			yd = y - ys;
			zd = z - zs;
			int g = gcd(gcd(abs(xd), abs(yd)), abs(zd));
			xd /= g;
			yd /= g;
			zd /= g;
			int xa = abs(xd), ya = abs(yd), za = abs(zd);
			int
				x_mul = xa ? (G_TRACER_RESOLUTION + xa - 1) / xa : G_TRACER_RESOLUTION + 1,
				y_mul = ya ? (G_TRACER_RESOLUTION + ya - 1) / ya : G_TRACER_RESOLUTION + 1,
				z_mul = za ? (G_TRACER_RESOLUTION + za - 1) / za : G_TRACER_RESOLUTION + 1;
			int t = x_mul;
			if (y_mul < t)
				t = y_mul;
			if (z_mul < t)
				t = z_mul;
			if (t == G_TRACER_RESOLUTION + 1)
				t = 0;
			xd *= t;
			yd *= t;
			zd *= t;
			int maxdist = 0;
			if (maxdist < xa * g)
				maxdist = xa * g;
			if (maxdist < ya * g)
				maxdist = ya * g;
			if (maxdist < za * g)
				maxdist = za * g;
			maxdist *= 2;
			trigger_throw(ent_sptr(p->control_ent), used_hand, xd, yd, zd, maxdist);
		}
	}
}

void cmap_attack(cmap_params_t *p) {
	if (p->nargs != 2) {
		fprintf(stderr, "Wrong number of arguments to cmap_attack\n");
		return;
	}
	if (p->args[0].type != CMAP_ARG_ENTITY || p->args[1].type != CMAP_ARG_ATTACK) {
		fprintf(stderr, "Wrong arguments to cmap_attack\n");
		return;
	}
	attack_l_s *a = p->args[1].data;
	uint32_t mat_tag = a->tool_mat_tag;
	trigger_attack(
		ent_sptr(p->control_ent),
		p->args[0].data_p,
		a->type,
		a->limb_slot_tag,
		mat_tag
	);
}

void cmap_fill_container(cmap_params_t *p) {
	if (p->nargs != 2) {
		fprintf(stderr, "Wrong number of arguments to cmap_fill_container\n");
		return;
	}
	if (p->args[0].type != CMAP_ARG_EFFECT || p->args[1].type != CMAP_ARG_ENTITY) {
		fprintf(stderr, "Wrong arguments to cmap_fill_container\n");
		return;
	}
	trigger_fill_cont(ent_sptr(p->control_ent), p->args[0].data, p->args[1].data_p);
}

void cmap_empty_container(cmap_params_t *p) {
	if (p->nargs != 1) {
		fprintf(stderr, "Wrong number of arguments to cmap_empty_container\n");
		return;
	}
	if (p->args[0].type != CMAP_ARG_EFFECT) {
		fprintf(stderr, "Wrong arguments to cmap_empty_container\n");
		return;
	}
	trigger_empty_cont(ent_sptr(p->control_ent), p->args[0].data);
}

void cmap_press_button(cmap_params_t *p) {
	if (p->nargs != 3) {
		fprintf(stderr, "Wrong number of arguments to cmap_press_button\n");
		return;
	}
	if (p->args[0].type != CMAP_ARG_EFFECT || p->args[1].type != CMAP_ARG_ENTITY || p->args[2].type != CMAP_ARG_EFFECT) {
		fprintf(stderr, "Wrong arguments to cmap_press_button\n");
		return;
	}
	trigger_press_button(ent_sptr(p->control_ent), p->args[0].data, p->args[1].data_p, p->args[2].data);
}

void cmap_wear(cmap_params_t *p) {
	// TODO cmap_wear
	if (p->nargs != 2) {
		fprintf(stderr, "Wrong number of arguments to cmap_wear\n");
		return;
	}
	if (p->args[0].type != CMAP_ARG_EFFECT || p->args[1].type != CMAP_ARG_ENTITY) {
		fprintf(stderr, "Wrong arguments to cmap_wear\n");
		return;
	}
	// trigger_wear(ent_sptr(p->contol_ent), p->args[0].data, p->args[1].data_p);
}

const cmap_t command_maps[] = {
	{'/', NULL, cmap_toggle_skip_moving},
	{'.', NULL, cmap_wait},
	{'<', NULL, cmap_go_up},
	{'>', NULL, cmap_go_down},
	{SDLK_KP_1, NULL, cmap_go},
	{SDLK_KP_2, NULL, cmap_go},
	{SDLK_KP_3, NULL, cmap_go},
	{SDLK_KP_4, NULL, cmap_go},
	{SDLK_KP_6, NULL, cmap_go},
	{SDLK_KP_7, NULL, cmap_go},
	{SDLK_KP_8, NULL, cmap_go},
	{SDLK_KP_9, NULL, cmap_go},
	{'p', "He(Put where?)", cmap_put},
	{'d', "H(Drop what?)", cmap_drop},
	{'g', "He(Grab what?)g(By what?)", cmap_grab},
	{'G', "He(Grab from pile?)n(How much?)", cmap_grab_pile},
	{'o', "He(Open door?)", cmap_open_door},
	{'O', "He(Close door?)", cmap_close_door},
	{'t', "He(Into what?)", cmap_throw},
	{'a', "e(Attack what?)a", cmap_attack},
	{'f', "H(Fill which container?)e(Where to fill from?)", cmap_fill_container},
	{'e', "H(Which container to empty?)", cmap_empty_container},
	{'r', "H(Which limb?)e(Where to press?)g(Which button?)", cmap_press_button},
	{'w', "H(What to wear?)b(Where to wear?)", cmap_wear},
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
	INPUTW_ATTACK,
	INPUTW_NUMBER,
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
	ent_ptr ent;
	effect_s *cur_sel;
} inputw_limb_s;

typedef struct inputw_effect_s {
	ent_ptr ent;
	effect_s *cur_sel;
	inputw_effect_sel_type type;
} inputw_effect_s;

typedef struct inputw_attack_s {
	attack_l_s *attacks;
	attack_l_s *cur_sel;
} inputw_attack_s;

typedef struct inputw_number_s {
	char val[20];
	int len;
} inputw_number_s;

#define INPUTW_MSG_LEN 24
typedef struct inputw_t {
	inputw_type type;
	char msg[INPUTW_MSG_LEN];
	union {
		inputw_tile_s u_tile;
		inputw_entity_s u_entity;
		inputw_limb_s u_limb;
		inputw_effect_s u_effect;
		inputw_attack_s u_attack;
		inputw_number_s u_number;
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
char inputw_message[INPUTW_MSG_LEN];

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

void input_queue_entity_select(void) {
	if (inputw_queue_n == INPUTW_MAXNR) {
		fprintf(stderr, "Exceeded number of input layers\n");
		return;
	}
	inputw_queue[inputw_queue_n].type = INPUTW_ENTITY;
	strncpy(inputw_queue[inputw_queue_n].msg, inputw_message, INPUTW_MSG_LEN);
	int x = 0, y = 0, z = 0;
	if (entity_coords(ent_sptr(gu_params.control_ent), &x, &y, &z)) {
		inputw_entity_s *e = &inputw_queue[inputw_queue_n].data_u.u_entity;
		e->x = x;
		e->y = y;
		e->z = z;
		e->cur_sel = NULL;
	}
	inputw_queue_n++;
}

void input_queue_limb_select(void) {
	if (inputw_queue_n == INPUTW_MAXNR) {
		fprintf(stderr, "Exceeded number of input layers\n");
		return;
	}
	inputw_queue[inputw_queue_n].type = INPUTW_LIMB;
	strncpy(inputw_queue[inputw_queue_n].msg, inputw_message, INPUTW_MSG_LEN);
	inputw_queue_n++;
}

void input_queue_limb_default(void) {
	if (inputw_queue_n == INPUTW_MAXNR) {
		fprintf(stderr, "Exceeded number of input layers\n");
		return;
	}
	inputw_queue[inputw_queue_n].type = INPUTW_LIMB_DEFAULT;
	strncpy(inputw_queue[inputw_queue_n].msg, inputw_message, INPUTW_MSG_LEN);
	inputw_queue_n++;
}

void input_queue_effect(inputw_effect_sel_type type) {
	if (inputw_queue_n == INPUTW_MAXNR) {
		fprintf(stderr, "Exceeded number of input layers\n");
		return;
	}
	inputw_queue[inputw_queue_n].type = INPUTW_EFFECT;
	strncpy(inputw_queue[inputw_queue_n].msg, inputw_message, INPUTW_MSG_LEN);
	inputw_queue[inputw_queue_n].data_u.u_effect.type = type;
	inputw_queue_n++;
}

void input_queue_attack(void) {
	if (inputw_queue_n == INPUTW_MAXNR) {
		fprintf(stderr, "Exceeded number of input layers\n");
		return;
	}
	inputw_queue[inputw_queue_n].type = INPUTW_ATTACK;
	strncpy(inputw_queue[inputw_queue_n].msg, inputw_message, INPUTW_MSG_LEN);
	inputw_queue_n++;
}

void input_queue_number(void) {
	if (inputw_queue_n == INPUTW_MAXNR) {
		fprintf(stderr, "Exceeded number of input layers\n");
		return;
	}
	inputw_queue[inputw_queue_n].type = INPUTW_NUMBER;
	strncpy(inputw_queue[inputw_queue_n].msg, inputw_message, INPUTW_MSG_LEN);
	inputw_queue_n++;
}

void input_queue_body_part(void) {
	if (inputw_queue_n == INPUTW_MAXNR) {
		fprintf(stderr, "Exceeded number of input layers\n");
		return;
	}
	// inputw_queue[inputw_queue_n].type = ...;
	// strncpy(inputw_queue[inputw_queue_n].msg, inputw_message, INPUTW_MSG_LEN);
	// inputw_queue_n++;
}

void inputw_clear(void) {
	inputw_n = 0;
	cmap_params_cleanup();
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

int command_arg_e(const char *s) {
	int nr;
	nr = command_parse_message(s, inputw_message, INPUTW_MSG_LEN);
	input_queue_entity_select();
	return nr;
}

int command_arg_h(const char *s) {
	int nr;
	nr = command_parse_message(s, inputw_message, INPUTW_MSG_LEN);
	input_queue_limb_select();
	return nr;
}

int command_arg_h_big(const char *s) {
	int nr;
	nr = command_parse_message(s, inputw_message, INPUTW_MSG_LEN);
	input_queue_limb_default();
	return nr;
}

int command_arg_g(const char *s) {
	int nr;
	nr = command_parse_message(s, inputw_message, INPUTW_MSG_LEN);
	input_queue_effect(INP_EF_MATERIAL);
	return nr;
}

int command_arg_a(const char *s) {
	int nr;
	nr = command_parse_message(s, inputw_message, INPUTW_MSG_LEN);
	input_queue_attack();
	return nr;
}

int command_arg_n(const char *s) {
	int nr;
	nr = command_parse_message(s, inputw_message, INPUTW_MSG_LEN);
	input_queue_number();
	return nr;
}

int command_arg_b(const char *s) {
	int nr;
	nr = command_parse_message(s, inputw_message, INPUTW_MSG_LEN);
	input_queue_body_part();
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
			t += 1 + command_arg_e(s+t+1);
		} break;
		case 'h': {
			t += 1 + command_arg_h(s+t+1);
		} break;
		case 'H': {
			t += 1 + command_arg_h_big(s+t+1);
		} break;
		case 'g': {
			t += 1 + command_arg_g(s+t+1);
		} break;
		case 'a': {
			t += 1 + command_arg_a(s+t+1);
		} break;
		case 'n': {
			t += 1 + command_arg_n(s+t+1);
		} break;
		case 'b': {
			t += 1 + command_arg_b(s+t+1);
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
	int x, y, z;
	if (key_to_direction(sym, &x, &y, &z)) {
		t->x += x;
		t->y += y;
		t->z += z;
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
	int x, y, z;
	if (key_to_direction(sym, &x, &y, &z)) {
		e->x += x;
		e->y += y;
		e->z += z;
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
		entity_l_s_free(e->cur_list);
		e->cur_list = NULL;
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
		inputws[inputw_n - 1].data_u.u_limb.ent = ent_sptr(gu_params.control_ent);
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

int inputw_attack_key(SDL_Keycode sym) {
	if (inputw_n <= 0) {
		fprintf(stderr, "No input layers in inputw_attack_key\n");
		return 0;
	}
	if (inputws[inputw_n - 1].type != INPUTW_ATTACK) {
		fprintf(stderr, "Input layer isn't attack\n");
		return 0;
	}
	inputw_attack_s *e = &inputws[inputw_n - 1].data_u.u_attack;
	if (sym == SDLK_ESCAPE) {
		inputw_clear();
		return INP_M_REDRAW;
	}
	if (sym == SDLK_UP) {
		if (e->cur_sel != NULL && e->cur_sel->prev != NULL)
			e->cur_sel = e->cur_sel->prev;
		return INP_M_REDRAW;
	}
	if (sym == SDLK_DOWN) {
		if (e->cur_sel != NULL && e->cur_sel->next != NULL)
			e->cur_sel = e->cur_sel->next;
		return INP_M_REDRAW;
	}
	if (sym == SDLK_RETURN) {
		if (e->cur_sel != NULL) {
			params_push_attack(e->cur_sel);
			attack_l_s *t = e->attacks;
			while (t != NULL) {
				attack_l_s *nxt = t->next;
				if (t != e->cur_sel) {
					o_free(t);
				}
				t = nxt;
			}
			e->cur_sel->prev = NULL;
			e->cur_sel->next = NULL;
			return INP_M_NEXT;
		}
	}
	return 0;
}

int inputw_number_key(SDL_Keycode sym) {
	if (inputw_n <= 0) {
		fprintf(stderr, "No input layers in inputw_number_key\n");
		return 0;
	}
	if (inputws[inputw_n - 1].type != INPUTW_NUMBER) {
		fprintf(stderr, "Input layer isn't number\n");
		return 0;
	}
	inputw_number_s *e = &inputws[inputw_n - 1].data_u.u_number;
	if (sym == SDLK_ESCAPE) {
		inputw_clear();
		return INP_M_REDRAW;
	}
	if (sym >= '0' && sym <= '9') {
		if (e->len < 19) {
			e->val[e->len++] = sym;
		}
		return INP_M_REDRAW;
	}
	if (sym == SDLK_BACKSPACE) {
		if (e->len > 0)
			e->len--;
		return INP_M_REDRAW;
	}
	if (sym == SDLK_RETURN) {
		e->val[e->len] = '\0';
		params_push_number(strtoll(e->val, NULL, 10));
		return INP_M_NEXT;
	}
	return 0;
}

void render_layer_adjust(camera_view_s *cam) {
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
			buf, 32, "%s %s",
			inputws[i].type == INPUTW_ORIGIN ? "origin" :
			inputws[i].type == INPUTW_TILE ? "tile" :
			inputws[i].type == INPUTW_ENTITY ? "entity" :
			inputws[i].type == INPUTW_LIMB ? "limb" :
			inputws[i].type == INPUTW_LIMB_DEFAULT ? "limb_default" :
			inputws[i].type == INPUTW_EFFECT ? "effect" :
			inputws[i].type == INPUTW_ATTACK ? "attack" :
			inputws[i].type == INPUTW_NUMBER ? "number" :
			"*",
			inputws[i].msg
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
			ent_ptr cur_ent = cur->ent;
			effect_ph_liquid_data liq_d;
			effect_wet_block_data wd;
			if (entity_load_effect(cur_ent, EF_PH_LIQUID, &liq_d)) {
				snprintf(
					buf, 32,
					"~%s a:%d",
					liq_d.type == LIQ_WATER ? "water" : "*",
					liq_d.amount
				);
			} else if (entity_load_effect(cur_ent, EF_WET_BLOCK, &wd)) {
				snprintf(
					buf, 32,
					"wet %s a:%d",
					wd.type == LIQ_WATER ? "water" : "*",
					wd.amount
				);
			} else {
				int r, g, b, a;
				if (!entity_rend_chr(cur_ent, &rend_char, &r, &g, &b, &a)) {
					rend_char = '\0';
				}
				if (rend_char != '\0') {
					snprintf(buf, 32, "%lu %c", cur_ent, rend_char);
				} else {
					snprintf(buf, 32, "%lu", cur_ent);
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
				if (d->item != ENT_NULL && entity_has_effect(d->item, EF_LIMB_HAND)) {
					snprintf(buf, 32, "%lu %d", d->item, d->tag);
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
		entity_s *ent = ent_aptr(inputws[inputw_n - 1].data_u.u_effect.ent);
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
	case INPUTW_ATTACK: {
		attack_l_s *l = inputws[inputw_n - 1].data_u.u_attack.attacks;
		attack_l_s *cur = inputws[inputw_n - 1].data_u.u_attack.cur_sel;
		int yc = y;
		while (l != NULL) {
			effect_s *tool_mat = entity_material_by_tag(l->tool, l->tool_mat_tag);
			effect_material_data *tool_mat_d = (void*)tool_mat->data;
			snprintf(
				buf, 32, "%s %d %d %s:%s",
				attack_type_string[l->type],
				l->limb_slot_tag,
				l->tool_mat_tag,
				tool_mat_d->prop & MATP_SHARP ? "sharp" : "",
				tool_mat_d->prop & MATP_SMALL ? "small" : ""
			);
			SDL_Color colo = {.r = 0, .g = 255, .b = 128};
			if (l != cur) {
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
			l = l->next;
		}
	} break;
	case INPUTW_NUMBER: {
		inputw_number_s *e = &inputws[inputw_n - 1].data_u.u_number;
		/*int i;
		for (i = 0; i < e->len; i++)
			buf[i] = e->val[i];
		buf[e->len] = '\0';*/
		snprintf(buf, 32, "%d %.*s", e->len, e->len, e->val);
		SDL_Color colo = {.r = 0, .g = 255, .b = 128};
		SDL_Surface *surf = TTF_RenderText_Blended(gr_font, buf, colo);
		SDL_Texture *tex = SDL_CreateTextureFromSurface(rend, surf);
		SDL_Rect target;
		target.x = x;
		target.y = y;
		target.w = surf->w;
		target.h = surf->h;
		SDL_RenderCopy(rend, tex, NULL, &target);
		SDL_DestroyTexture(tex);
		SDL_FreeSurface(surf);
	} break;
	default: {
	}
	}
}

void inputw_layer_enter(void) {
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
		entity_s *t = ent_aptr(e->ent);
		if (t != NULL)
			e->cur_sel = effect_by_type(t->effects, EF_LIMB_SLOT);
	} break;
	case INPUTW_EFFECT: {
		inputw_effect_s *e = &inputws[inputw_n - 1].data_u.u_effect;
		e->ent = ENT_NULL;
		if (gu_params.nargs != 0) {
			switch (gu_params.args[gu_params.nargs - 1].type) {
			case CMAP_ARG_ENTITY: {
				e->ent = gu_params.args[gu_params.nargs - 1].data_p;
			} break;
			case CMAP_ARG_EFFECT: {
				effect_s *ef = gu_params.args[gu_params.nargs - 1].data;
				if (ef->type == EF_LIMB_SLOT) {
					effect_limb_slot_data *d = (void*)ef->data;
					entity_s *t = ent_aptr(d->item);
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
		entity_s *t = ent_aptr(e->ent);
		e->cur_sel = t == NULL ? NULL : effect_by_type(t->effects, EF_MATERIAL);
	} break;
	case INPUTW_ATTACK: {
		inputw_attack_s *e = &inputws[inputw_n - 1].data_u.u_attack;
		entity_s *target = NULL;
		if (gu_params.nargs != 0 && gu_params.args[gu_params.nargs - 1].type == CMAP_ARG_ENTITY) {
			target = gu_params.args[gu_params.nargs - 1].data;
		}
		e->attacks = entity_list_attacks(ent_sptr(gu_params.control_ent), ent_sptr(target));
		e->cur_sel = e->attacks;
	} break;
	case INPUTW_NUMBER: {
		inputw_number_s *e = &inputws[inputw_n - 1].data_u.u_number;
		e->len = 0;
		memset(e->val, 0, sizeof(e->val));
	} break;
	default: {
	}
	}
}

const char upper_cased[256] = {
	[','] = '<', ['.'] = '>', ['a'] = 'A', ['b'] = 'B', ['c'] = 'C',
	['d'] = 'D', ['e'] = 'E', ['f'] = 'F', ['g'] = 'G', ['h'] = 'H',
	['i'] = 'I', ['j'] = 'J', ['k'] = 'K', ['l'] = 'L', ['m'] = 'M',
	['n'] = 'N', ['o'] = 'O', ['p'] = 'P', ['q'] = 'Q', ['r'] = 'R',
	['s'] = 'S', ['t'] = 'T', ['u'] = 'U', ['v'] = 'V', ['w'] = 'W',
	['x'] = 'X', ['y'] = 'Y', ['z'] = 'Z',
};

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
	gr_font = TTF_OpenFont("/usr/share/fonts/TTF/Iosevka-Regular.ttc", 24);

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
			for (int i = -1; i < 20; i ++) {
				for (int j = -1; j < 20; j ++) {
					for (int z = -1; z <= 1; z++) {
						if (sector_get_sector(g_sectors, i, j, z) == NULL) {
							sector_s *new_sect = o_alloc_sector();
							new_sect->x = i;
							new_sect->y = j;
							new_sect->z = z;
							new_sect->prio = rng_bigrange(g_dice);
							memset(new_sect->ch, 0, sizeof(new_sect->ch));
							memset(new_sect->block_entities, 0, sizeof(new_sect->block_entities));
							g_sectors = sector_insert(g_sectors, new_sect);
						}
					}
				}
			}
			entity_s *c_ent = g_entities;
			while (c_ent != NULL && c_ent->next != NULL) {
				c_ent = c_ent->next;
			}
			while (c_ent != NULL) {
				attach_generic_entity(ent_sptr(c_ent));
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

	if (g_entities == NULL) {
		fputs("g_entities is null, exit\n", stderr);
	}

	while (running) {
		unsigned long long cur_ticks = SDL_GetTicks();
		if (cur_ticks > last_blink + 500) {
			last_blink = cur_ticks;
			cam.blink++;
			need_redraw = 1;
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
		if (need_redraw && !skip_tick) {
			{
				int x, y, z;
				if (entity_coords(ent_sptr(control_ent), &x, &y, &z)) {
					cam.z = z;
					cam.x = x - 8;
					cam.y = y - 8;
				}
			}
			SDL_SetRenderDrawColor(rend, 0, 0, 0, 0);
			SDL_RenderClear(rend);
			render_layer_adjust(&cam);
			render_camera(rend, &cam);
			render_status(rend, control_ent, 0, cam.height * gr_rend_char_height);
			render_list_layers(rend, cam.width * gr_rend_char_width, 0);
			render_layer_specific(rend, cam.width * gr_rend_char_width, inputw_n * gr_rend_char_height);
			SDL_RenderPresent(rend);
			need_redraw = 0;
		} else {
			SDL_Delay(40);
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
					if (sym >= 0 && sym < 256 && upper_cased[sym] != '\0')
						sym = upper_cased[sym];
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
					case INPUTW_ATTACK: {
						mask = inputw_attack_key(sym);
					} break;
					case INPUTW_NUMBER: {
						mask = inputw_number_key(sym);
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
							cmap_params_cleanup();
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
#if 0
			for (int i = 0; i < 10; i++) {
				for (int j = 0; j < 10; j++) {
					entity_s *new_ent = o_alloc_entity();
					new_ent->common_type = CT_RAIN;
					((int*)new_ent->common_data)[0] = 20;
					((int*)new_ent->common_data)[1] = 1;
					effect_s *new_eff = alloc_effect(EF_PH_ITEM);
					effect_ph_item_data *pd = (void*)new_eff->data;
					pd->x = i;
					pd->y = j;
					pd->z = 10;
					pd->weight = 0;
					pd->parent = ENT_NULL;
					effect_prepend(new_ent, new_eff);
					attach_generic_entity(ent_sptr(new_ent));
					entity_prepend(g_entities, new_ent);
					g_entities = new_ent;
				}
			}
#endif
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
	/* TODO redo, as the sectors are no longer in a linked list
	{
		sector_s *t = g_sectors;
		while (t != NULL) {
			unload_sector(t);
			t = t->snext;
		}
		g_entities = clear_nonexistent(g_entities);
		t = g_sectors;
		while (t != NULL) {
			sector_s *nxt = t->snext;
			o_free_sector(t);
			t = nxt;
		}
		g_sectors = NULL;
	}*/
	o_free(g_dice);
	TTF_CloseFont(gr_font);
	SDL_DestroyRenderer(rend);
	SDL_DestroyWindow(win);
	SDL_Quit();
}
