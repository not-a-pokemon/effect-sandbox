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

void render_camera(SDL_Renderer *rend, camera_view_s *cam) {
	int prev_cx = INT_MIN;
	int prev_cy = INT_MIN;

	int z = cam->z;
	int cz = 0;
	coord_normalize(&z, &cz);

	SDL_SetRenderDrawColor(rend, 0, 0, 0, 255);
	for (int i = 0; i < cam->width; i ++) {
		for (int j = 0; j < cam->height; j ++) {
			SDL_Rect r = (SDL_Rect){.x = i * REND_CHAR_WIDTH, .y = j * REND_CHAR_HEIGHT, .w = REND_CHAR_WIDTH, .h = REND_CHAR_HEIGHT};
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
					effect_s *ef = effect_by_type(te->effects, EF_RENDER);
					if (ef != NULL) {
						effect_render_data *rend_data = (void*)ef->data;
						SDL_Rect r = (SDL_Rect){.x = i * REND_CHAR_WIDTH, .y = j * REND_CHAR_HEIGHT, .w = REND_CHAR_WIDTH, .h = REND_CHAR_HEIGHT};
						char rr[2] = {rend_data->chr, '\0'};
						SDL_Surface *surf = TTF_RenderText_Blended(gr_font, rr, (SDL_Color){.r = rend_data->r, .g = rend_data->g, .b = rend_data->b, .a = rend_data->a});
						SDL_Texture *tex = SDL_CreateTextureFromSurface(rend, surf);
						SDL_RenderCopy(rend, tex, NULL, &r);
						SDL_DestroyTexture(tex);
						SDL_FreeSurface(surf);
						/* SDL_SetRenderDrawColor(rend, rend_data->r, rend_data->g, rend_data->b, rend_data->a); */
						/* SDL_RenderFillRect(rend, &r); */
					}
					effect_s *fi = effect_by_type(te->effects, EF_FIRE);
					if (fi != NULL) {
						SDL_Rect r = (SDL_Rect){.x = i * REND_CHAR_WIDTH, .y = j * REND_CHAR_HEIGHT, .w = REND_CHAR_WIDTH, .h = REND_CHAR_HEIGHT};
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
				SDL_Rect r = (SDL_Rect){.x = i * REND_CHAR_WIDTH, .y = j * REND_CHAR_HEIGHT, .w = REND_CHAR_WIDTH, .h = REND_CHAR_HEIGHT};
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
		slc += snprintf(slc, 128 - (slc - sl), "falling");
		int x, y, z;
		if (entity_coords(ent, &x, &y, &z)) {
			if (z < 0) {
				slc += snprintf(slc, 128 - (slc - sl), " in void");
			}
		}
	}
	if (sl[0] != '\0') {
		SDL_Surface *surf = TTF_RenderText_Blended(gr_font, sl, (SDL_Color){.r = 0, .g = 255, .b = 128});
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

int select_tile(SDL_Renderer *rend, camera_view_s *cam, int *x, int *y, int *z) {
	int rx = *x;
	int ry = *y;
	int rz = *z;
	SDL_Event evt;
	while (1) {
		cam->cursor_x = *x;
		cam->cursor_y = *y;
		cam->z = *z;
		{
			SDL_SetRenderDrawColor(rend, 0, 0, 0, 0);
			SDL_RenderClear(rend);
			render_camera(rend, cam);
			/*
			SDL_Surface *surf = TTF_RenderText_Blended(gr_font, "[select tile]", (SDL_Color){.r = 255, .g = 0, .b = 0, .a = 0});
			SDL_Texture *tex = SDL_CreateTextureFromSurface(rend, surf);
			int surf_w = surf->w;
			int surf_h = surf->h;
			SDL_FreeSurface(surf);
			SDL_Rect rec = (SDL_Rect){.x = 100, .y = 100, .w = surf_w, .h = surf_h};
			SDL_RenderCopy(rend, tex, NULL, &rec);
			SDL_DestroyTexture(tex);
			*/
			SDL_RenderPresent(rend);
		}
		if (!SDL_WaitEvent(&evt)) {
			break;
		}
		if (evt.type == SDL_KEYDOWN) {
			switch (evt.key.keysym.sym) {
			case SDLK_r: {
				*x = rx;
				*y = ry;
				*z = rz;
			} break;
			case SDLK_KP_8: {
				(*y) --;
			} break;
			case SDLK_KP_4: {
				(*x) --;
			} break;
			case SDLK_KP_2: {
				(*y) ++;
			} break;
			case SDLK_KP_6: {
				(*x) ++;
			} break;
			case SDLK_j: {
				(*z) --;
			} break;
			case SDLK_k: {
				(*z) ++;
			} break;
			case SDLK_ESCAPE: {
				cam->cursor_x = -1;
				cam->cursor_y = -1;
				return 0;
			} break;
			case SDLK_RETURN: {
				cam->cursor_x = -1;
				cam->cursor_y = -1;
				(*x) += cam->x;
				(*y) += cam->y;
				return 1;
			} break;
			default: {
			}
			}
		}
	}
	cam->cursor_x = -1;
	cam->cursor_y = -1;
	return 1;
}

int select_tile_entity(SDL_Renderer *rend, int x, int y, int z, entity_s **ent) {
	SDL_Event evt;
	entity_l_s *c_ent = NULL;
	int cx = 0;
	int cy = 0;
	int cz = 0;
	coord_normalize(&x, &cx);
	coord_normalize(&y, &cy);
	coord_normalize(&z, &cz);
	sector_s *sec = sector_get_sector(g_sectors, cx, cy, cz);
	if (sec == NULL) {
		return 0;
	}
	int ret = 0;
	entity_l_s *el = sector_get_block_entities_indirect(sec, x, y, z);
	if (el == NULL) {
		ret = 0;
		goto RET;
	}
	c_ent = el;
	while (1) {
		int screen_y = 0;
		SDL_SetRenderDrawColor(rend, 0, 0, 0, 0);
		SDL_RenderClear(rend);
		entity_l_s *cur = el;
		while (cur != NULL) {
			entity_s *cur_e = cur->ent;
			char data_cur[256];
			memset(data_cur, 0, 256);
			effect_s *ef_rend = effect_by_type(cur_e->effects, EF_RENDER);
			effect_render_data *ef_rend_data = ef_rend != NULL ? (void*)ef_rend->data : NULL;
			effect_s *ef_l_hand = effect_by_type(cur_e->effects, EF_LIMB_HAND);
			if (ef_rend != NULL) {
				snprintf(data_cur, 255, "%p (%c)", cur_e, ef_rend_data->chr);
			} else if (ef_l_hand != NULL) {
				snprintf(data_cur, 255, "%p hand", cur_e);
			} else {
				snprintf(data_cur, 255, "%p", cur_e);
			}
			SDL_Surface *surf = TTF_RenderText_Blended(gr_font, data_cur, (SDL_Color){.r = 0, .g = 127 * (cur == c_ent ? 2 : 1), .b = 0, .a = 0});
			SDL_Texture *tex = SDL_CreateTextureFromSurface(rend, surf);
			SDL_RenderCopy(rend, tex, NULL, &(SDL_Rect){.x = 0, .y = screen_y, .h = surf->h, .w = surf->w});
			if (ef_rend != NULL) {
				SDL_SetRenderDrawColor(rend, ef_rend_data->r, ef_rend_data->g, ef_rend_data->b, ef_rend_data->a);
				SDL_RenderFillRect(rend, &(SDL_Rect){.x = surf->w, .y = screen_y, .h = surf->h, .w = surf->h});
			}
			screen_y += surf->h;
			SDL_FreeSurface(surf);
			SDL_DestroyTexture(tex);
			cur = cur->next;
		}
		SDL_RenderPresent(rend);
		if (!SDL_WaitEvent(&evt)) {
			break;
		}
		if (evt.type == SDL_KEYDOWN) {
			switch (evt.key.keysym.sym) {
				case SDLK_KP_8: {
					if (c_ent->prev != NULL) {
						c_ent = c_ent->prev;
					}
				} break;
				case SDLK_KP_2: {
					if (c_ent->next != NULL) {
						c_ent = c_ent->next;
					}
				} break;
				case SDLK_RETURN: {
					*ent = c_ent->ent;
					ret = 1;
					goto RET;
				} break;
				case SDLK_ESCAPE: {
					ret = 0;
					goto RET;
				} break;
				default: {
				}
			}
		}
	}
RET:
	entity_l_s_free(el);
	return ret;
}

int select_input_string(SDL_Renderer *rend, char *buf, int n, const char *msg) {
	int c = 0;
	int edit_start;
	int prev_end = 0;
	memset(buf, 0, n);
	while (1) {
		SDL_SetRenderDrawColor(rend, 0, 0, 0, 255);
		SDL_RenderClear(rend);
		{
			SDL_SetRenderDrawColor(rend, 0, 0, 0, 255);
			SDL_Surface *surf = TTF_RenderText_Blended(gr_font, msg, (SDL_Color){.r = 0, .g = 128, .b = 0});
			SDL_Texture *tex = SDL_CreateTextureFromSurface(rend, surf);
			SDL_RenderFillRect(rend, &(SDL_Rect){.x = 0, .y = 0, .h = surf->h, .w = surf->w});
			SDL_RenderCopy(rend, tex, NULL, &(SDL_Rect){.x = 0, .y = 0, .h = surf->h, .w = surf->w});
			edit_start = surf->w;
			SDL_FreeSurface(surf);
			SDL_DestroyTexture(tex);
		}
		{
			SDL_Surface *surf = TTF_RenderText_Blended(gr_font, buf, (SDL_Color){.r = 0, .g = 128, .b = 64});
			if (surf != NULL) {
				SDL_Texture *tex = SDL_CreateTextureFromSurface(rend, surf);
				SDL_SetRenderDrawColor(rend, 0, 0, 0, 255);
				if (prev_end < surf->w) {
					prev_end = surf->w;
				}
				SDL_RenderFillRect(rend, &(SDL_Rect){.x = edit_start, .y = 0, .h = surf->h, .w = prev_end});
				SDL_RenderCopy(rend, tex, NULL, &(SDL_Rect){.x = edit_start, .y = 0, .h = surf->h, .w = surf->w});
				SDL_FreeSurface(surf);
				SDL_DestroyTexture(tex);
			}
		}
		SDL_RenderPresent(rend);
		SDL_Event evt;
		if (!SDL_WaitEvent(&evt)) {
			break;
		}
		if (evt.type == SDL_KEYDOWN) {
			switch (evt.key.keysym.sym) {
				case SDLK_BACKSPACE: {
					if (c != 0) {
						buf[-- c] = '\0';
					}
				} break;
				case SDLK_RETURN: {
					return 1;
				} break;
				case SDLK_ESCAPE: {
					buf[0] = '\0';
					return 0;
				} break;
				default: {
					char ch = evt.key.keysym.sym;
					if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || (ch == '_' || ch == '!')) {
						if (c + 1 < n) {
							buf[c ++] = ch;
						}
					}
				} break;
			}
		}
	}
	return 0;
}

void spawn_simple_floor(int x, int y, int z) {
	entity_s *new_ent = o_malloc(sizeof(entity_s));
	new_ent->effects = NULL;
	{
		effect_s *ef_ph = alloc_effect(EF_PH_BLOCK);
		ef_ph->type = EF_PH_BLOCK;
		ef_ph->prev = NULL;
		ef_ph->next = new_ent->effects;
		effect_ph_block_data *d = (void*)ef_ph->data;
		d->floor = 1;
		d->floor_up = 0;
		d->block_movement = 0;
		d->slope = 0;
		d->x = x;
		d->y = y;
		d->z = z;
		if (new_ent->effects != NULL) {
			new_ent->effects->prev = ef_ph;
		}
		new_ent->effects = ef_ph;
	}
	{
		effect_s *ef_rend = alloc_effect(EF_RENDER);
		ef_rend->type = EF_RENDER;
		ef_rend->prev = NULL;
		ef_rend->next = new_ent->effects;
		effect_render_data *d = (void*)ef_rend->data;
		d->r = 0;
		d->g = 255;
		d->b = 0;
		d->a = 128;
		d->chr = '_';
		if (new_ent->effects != NULL) {
			new_ent->effects->prev = ef_rend;
		}
		new_ent->effects = ef_rend;
	}
	{
		effect_s *ef_mat = alloc_effect(EF_MATERIAL);
		effect_material_data *d = (void*)ef_mat->data;
		d->type = MAT_WOOD;
		d->dur = 10;
		effect_prepend(new_ent, ef_mat);
	}
	new_ent->prev = NULL;
	new_ent->next = g_entities;
	if (g_entities != NULL) {
		g_entities->prev = new_ent;
	}
	g_entities = new_ent;
	attach_generic_entity(new_ent);
}

void spawn_pressure_floor(int x, int y, int z, int w_thresold) {
	entity_s *new_ent = o_malloc(sizeof(entity_s));
	new_ent->effects = NULL;
	{
		effect_s *ef_ph = alloc_effect(EF_PH_BLOCK);
		ef_ph->type = EF_PH_BLOCK;
		ef_ph->prev = NULL;
		ef_ph->next = new_ent->effects;
		effect_ph_block_data *d = (void*)ef_ph->data;
		d->floor = 1;
		d->floor_up = 0;
		d->block_movement = 0;
		d->slope = 0;
		d->x = x;
		d->y = y;
		d->z = z;
		if (new_ent->effects != NULL) {
			new_ent->effects->prev = ef_ph;
		}
		new_ent->effects = ef_ph;
	}
	{
		effect_s *ef_rend = alloc_effect(EF_RENDER);
		ef_rend->type = EF_RENDER;
		ef_rend->prev = NULL;
		ef_rend->next = new_ent->effects;
		effect_render_data *d = (void*)ef_rend->data;
		d->r = 128;
		d->g = 128;
		d->b = 0;
		d->a = 128;
		d->chr = '_';
		if (new_ent->effects != NULL) {
			new_ent->effects->prev = ef_rend;
		}
		new_ent->effects = ef_rend;
	}
	{
		effect_s *ef_pr = alloc_effect(EF_A_PRESSURE_PLATE);
		ef_pr->type = EF_A_PRESSURE_PLATE;
		effect_a_pressure_plate_data *d = (void*)ef_pr->data;
		d->thresold = w_thresold;
		effect_prepend(new_ent, ef_pr);
	}
	new_ent->prev = NULL;
	new_ent->next = g_entities;
	if (g_entities != NULL) {
		g_entities->prev = new_ent;
	}
	g_entities = new_ent;
	attach_generic_entity(new_ent);
}

void spawn_simple_wall(int x, int y, int z) {
	entity_s *new_ent = o_malloc(sizeof(entity_s));
	new_ent->effects = NULL;
	{
		effect_s *ef_ph = alloc_effect(EF_PH_BLOCK);
		ef_ph->type = EF_PH_BLOCK;
		ef_ph->prev = NULL;
		ef_ph->next = new_ent->effects;
		effect_ph_block_data *d = (void*)ef_ph->data;
		d->floor = 0;
		d->floor_up = 1;
		d->block_movement = 1;
		d->slope = 0;
		d->x = x;
		d->y = y;
		d->z = z;
		if (new_ent->effects != NULL) {
			new_ent->effects->prev = ef_ph;
		}
		new_ent->effects = ef_ph;
	}
	{
		effect_s *ef_rend = alloc_effect(EF_RENDER);
		ef_rend->type = EF_RENDER;
		ef_rend->prev = NULL;
		ef_rend->next = new_ent->effects;
		effect_render_data *d = (void*)ef_rend->data;
		d->r = 0;
		d->g = 255;
		d->b = 0;
		d->a = 128;
		d->chr = '#';
		if (new_ent->effects != NULL) {
			new_ent->effects->prev = ef_rend;
		}
		new_ent->effects = ef_rend;
	}
	{
		effect_s *ef_mat = alloc_effect(EF_MATERIAL);
		ef_mat->type = EF_MATERIAL;
		effect_material_data *d = (void*)ef_mat->data;
		d->type = MAT_WOOD;
		d->dur = 10;
		effect_prepend(new_ent, ef_mat);
	}
	new_ent->prev = NULL;
	new_ent->next = g_entities;
	if (g_entities != NULL) {
		g_entities->prev = new_ent;
	}
	g_entities = new_ent;
	attach_generic_entity(new_ent);
}

void spawn_simple_door(int x, int y, int z) {
	entity_s *new_ent = o_malloc(sizeof(entity_s));
	new_ent->effects = NULL;
	{
		effect_s *ef_ph = alloc_effect(EF_PH_BLOCK);
		ef_ph->type = EF_PH_BLOCK;
		ef_ph->prev = NULL;
		ef_ph->next = new_ent->effects;
		effect_ph_block_data *d = (void*)ef_ph->data;
		d->floor = 0;
		d->floor_up = 1;
		d->block_movement = 1;
		d->slope = 0;
		d->x = x;
		d->y = y;
		d->z = z;
		if (new_ent->effects != NULL) {
			new_ent->effects->prev = ef_ph;
		}
		new_ent->effects = ef_ph;
	}
	{
		effect_s *ef_rend = alloc_effect(EF_RENDER);
		ef_rend->type = EF_RENDER;
		ef_rend->prev = NULL;
		ef_rend->next = new_ent->effects;
		effect_render_data *d = (void*)ef_rend->data;
		d->r = 0;
		d->g = 255;
		d->b = 0;
		d->a = 128;
		d->chr = '%';
		if (new_ent->effects != NULL) {
			new_ent->effects->prev = ef_rend;
		}
		new_ent->effects = ef_rend;
	}
	{
		effect_s *ef_door = alloc_effect(EF_R_TOUCH_TOGGLE_BLOCK);
		ef_door->type = EF_R_TOUCH_TOGGLE_BLOCK;
		ef_door->prev = NULL;
		ef_door->next = new_ent->effects;
		if (new_ent->effects != NULL) {
			new_ent->effects->prev = ef_door;
		}
		new_ent->effects = ef_door;
	}
	{
		effect_s *ef_mat = alloc_effect(EF_MATERIAL);
		ef_mat->type = EF_MATERIAL;
		effect_material_data *d = (void*)ef_mat->data;
		d->type = MAT_WOOD;
		d->dur = 10;
		effect_prepend(new_ent, ef_mat);
	}
	new_ent->prev = NULL;
	new_ent->next = g_entities;
	if (g_entities != NULL) {
		g_entities->prev = new_ent;
	}
	g_entities = new_ent;
	attach_generic_entity(new_ent);
}


void spawn_wood_piece(int x, int y, int z) {
	entity_s *new_ent = o_malloc(sizeof(entity_s));
	new_ent->effects = NULL;
	{
		effect_s *ef_ph = alloc_effect(EF_PH_ITEM);
		ef_ph->type = EF_PH_ITEM;
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
		ef_mat->type = EF_MATERIAL;
		effect_material_data *d = (void*)ef_mat->data;
		d->type = MAT_WOOD;
		d->dur = 10;
		effect_prepend(new_ent, ef_mat);
	}
	{
		effect_s *ef_rend = alloc_effect(EF_RENDER);
		ef_rend->type = EF_RENDER;
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
	entity_s *new_ent = o_malloc(sizeof(entity_s));
	new_ent->effects = NULL;
	{
		effect_s *ef_ph = alloc_effect(EF_PH_BLOCK);
		ef_ph->type = EF_PH_BLOCK;
		ef_ph->prev = NULL;
		ef_ph->next = new_ent->effects;
		effect_ph_block_data *d = (void*)ef_ph->data;
		d->floor = 0;
		d->stair = 1;
		d->floor_up = 0;
		d->block_movement = 0;
		d->x = x;
		d->y = y;
		d->z = z;
		if (new_ent->effects != NULL) {
			new_ent->effects->prev = ef_ph;
		}
		new_ent->effects = ef_ph;
	}
	{
		effect_s *ef_rend = alloc_effect(EF_RENDER);
		ef_rend->type = EF_RENDER;
		ef_rend->prev = NULL;
		ef_rend->next = new_ent->effects;
		effect_render_data *d = (void*)ef_rend->data;
		d->r = 0;
		d->g = 255;
		d->b = 0;
		d->a = 128;
		d->chr = '^';
		if (new_ent->effects != NULL) {
			new_ent->effects->prev = ef_rend;
		}
		new_ent->effects = ef_rend;
	}
	new_ent->prev = NULL;
	new_ent->next = g_entities;
	if (g_entities != NULL) {
		g_entities->prev = new_ent;
	}
	g_entities = new_ent;
	attach_generic_entity(new_ent);
}

void spawn_circle_mover(int x, int y, int z) {
	entity_s *new_ent = o_malloc(sizeof(entity_s));
	new_ent->effects = NULL;
	{
		effect_s *ef_ph = alloc_effect(EF_PH_ITEM);
		ef_ph->type = EF_PH_ITEM;
		ef_ph->prev = NULL;
		ef_ph->next = new_ent->effects;
		effect_ph_item_data *d = (void*)ef_ph->data;
		d->x = x;
		d->y = y;
		d->z = z;
		d->weight = 5;
		if (new_ent->effects != NULL) {
			new_ent->effects->prev = ef_ph;
		}
		new_ent->effects = ef_ph;
	}
	{
		effect_s *ef_rend = alloc_effect(EF_RENDER);
		ef_rend->type = EF_RENDER;
		ef_rend->prev = NULL;
		ef_rend->next = new_ent->effects;
		effect_render_data *d = (void*)ef_rend->data;
		d->r = 0;
		d->g = 128;
		d->b = 255;
		d->a = 128;
		d->chr = 'q';
		if (new_ent->effects != NULL) {
			new_ent->effects->prev = ef_rend;
		}
		new_ent->effects = ef_rend;
	}
	{
		effect_s *ef_rend = alloc_effect(EF_A_CIRCLE_MOVE);
		ef_rend->type = EF_A_CIRCLE_MOVE;
		ef_rend->prev = NULL;
		ef_rend->next = new_ent->effects;
		if (new_ent->effects != NULL) {
			new_ent->effects->prev = ef_rend;
		}
		new_ent->effects = ef_rend;
	}
	new_ent->prev = NULL;
	new_ent->next = g_entities;
	if (g_entities != NULL) {
		g_entities->prev = new_ent;
	}
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

		entity_s *new_ent = o_malloc(sizeof(entity_s));
		new_ent->next = NULL;
		new_ent->prev = NULL;
		new_ent->effects = NULL;
		{
			effect_s *ef_ph = alloc_effect(EF_PH_ITEM);
			ef_ph->type = EF_PH_ITEM;
			ef_ph->prev = NULL;
			ef_ph->next = new_ent->effects;
			effect_ph_item_data *d = (void*)ef_ph->data;
			d->x = 0;
			d->y = 0;
			d->z = 0;
			d->weight = 7;
			d->parent = NULL;
			new_ent->effects = ef_ph;
		}
		{
			effect_s *ef_rend = alloc_effect(EF_RENDER);
			ef_rend->type = EF_RENDER;
			ef_rend->prev = NULL;
			ef_rend->next = new_ent->effects;
			effect_render_data *d = (void*)ef_rend->data;
			d->r = 255;
			d->g = 0;
			d->b = 0;
			d->a = 255;
			d->chr = '@';
			effect_prepend(new_ent, ef_rend);
		}
		{
			effect_s *ef_limb_slot = alloc_effect(EF_LIMB_SLOT);
			ef_limb_slot->type = EF_LIMB_SLOT;
			effect_limb_slot_data *d = (void*)ef_limb_slot->data;
			{
				entity_s *e_hand = o_malloc(sizeof(entity_s));
				e_hand->effects = NULL;
				entity_prepend(g_entities, e_hand);
				g_entities = e_hand;

				effect_s *ef_hand = alloc_effect(EF_LIMB_LEG);
				ef_hand->type = EF_LIMB_LEG;
				effect_prepend(e_hand, ef_hand);

				effect_s *ef_item = alloc_effect(EF_PH_ITEM);
				ef_item->type = EF_PH_ITEM;
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
			ef_limb_slot->type = EF_LIMB_SLOT;
			effect_limb_slot_data *d = (void*)ef_limb_slot->data;
			{
				entity_s *e_hand = o_malloc(sizeof(entity_s));
				e_hand->effects = NULL;
				entity_prepend(g_entities, e_hand);
				g_entities = e_hand;

				effect_s *ef_hand = alloc_effect(EF_LIMB_HAND);
				ef_hand->type = EF_LIMB_HAND;
				effect_limb_hand_data *hand_d = (void*)ef_hand->data;
				hand_d->item = NULL;
				effect_prepend(e_hand, ef_hand);

				effect_s *ef_item = alloc_effect(EF_PH_ITEM);
				ef_item->type = EF_PH_ITEM;
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
			ef_limb_slot->type = EF_LIMB_SLOT;
			effect_limb_slot_data *d = (void*)ef_limb_slot->data;
			{
				entity_s *e_hand = o_malloc(sizeof(entity_s));
				e_hand->effects = NULL;
				entity_prepend(g_entities, e_hand);
				g_entities = e_hand;

				effect_s *ef_hand = alloc_effect(EF_LIMB_HAND);
				ef_hand->type = EF_LIMB_HAND;
				effect_limb_hand_data *hand_d = (void*)ef_hand->data;
				hand_d->item = NULL;
				effect_prepend(e_hand, ef_hand);

				effect_s *ef_mat = alloc_effect(EF_MATERIAL);
				effect_material_data *mat_d = (void*)ef_mat->data;
				mat_d->type = MAT_GHOST;
				mat_d->dur = 10;
				effect_prepend(e_hand, ef_mat);

				effect_s *ef_item = alloc_effect(EF_PH_ITEM);
				ef_item->type = EF_PH_ITEM;
				effect_ph_item_data *dt = (void*)ef_item->data;
				dt->weight = 1;
				dt->parent = new_ent;
				dt->parent_type = PARENT_REF_LIMB;
				effect_prepend(e_hand, ef_item);

				d->item = e_hand;
			}
			effect_prepend(new_ent, ef_limb_slot);
		}
		new_ent->next = g_entities;
		if (g_entities != NULL) {
			g_entities->prev = new_ent;
		}
		g_entities = new_ent;
		attach_generic_entity(new_ent);
	}
	{
		entity_s *new_ent = o_malloc(sizeof(entity_s));
		new_ent->effects = NULL;
		{
			effect_s *ef_ph = alloc_effect(EF_PH_ITEM);
			ef_ph->type = EF_PH_ITEM;
			ef_ph->prev = NULL;
			ef_ph->next = new_ent->effects;
			effect_ph_item_data *d = (void*)ef_ph->data;
			d->x = 0;
			d->y = 0;
			d->z = 0;
			d->weight = 2;
			d->parent = NULL;
			if (new_ent->effects != NULL) {
				new_ent->effects->prev = ef_ph;
			}
			new_ent->effects = ef_ph;
		}
		{
			effect_s *ef_rend = alloc_effect(EF_R_TOUCH_RNG_TP);
			ef_rend->type = EF_R_TOUCH_RNG_TP;
			ef_rend->prev = NULL;
			ef_rend->next = new_ent->effects;
			if (new_ent->effects != NULL) {
				new_ent->effects->prev = ef_rend;
			}
			new_ent->effects = ef_rend;
		}
		{
			effect_s *ef_rend = alloc_effect(EF_RENDER);
			ef_rend->type = EF_RENDER;
			ef_rend->prev = NULL;
			ef_rend->next = new_ent->effects;
			effect_render_data *d = (void*)ef_rend->data;
			d->r = 0;
			d->g = 255;
			d->b = 0;
			d->a = 128;
			d->chr = '\'';
			if (new_ent->effects != NULL) {
				new_ent->effects->prev = ef_rend;
			}
			new_ent->effects = ef_rend;
		}
		new_ent->prev = NULL;
		new_ent->next = g_entities;
		if (g_entities != NULL) {
			g_entities->prev = new_ent;
		}
		g_entities = new_ent;
		attach_generic_entity(new_ent);
	}
	{
		entity_s *new_ent = o_malloc(sizeof(entity_s));
		new_ent->effects = NULL;
		{
			effect_s *new_eff = alloc_effect(EF_ROTATION);
			new_eff->type = EF_ROTATION;
			effect_rotation_data *d = (void*)new_eff->data;
			d->type = RT_DICE;
			d->rotation = 1;
			effect_prepend(new_ent, new_eff);
		}
		{
			effect_s *ef_ph = alloc_effect(EF_PH_ITEM);
			ef_ph->type = EF_PH_ITEM;
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
			new_eff->type = EF_RENDER;
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
		entity_s *new_ent = o_malloc(sizeof(entity_s));
		{
			effect_s *ef_ph = alloc_effect(EF_PH_ITEM);
			ef_ph->type = EF_PH_ITEM;
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
			new_eff->type = EF_RENDER;
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
			new_eff->type = EF_TRACER;
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
		entity_s *new_ent = o_malloc(sizeof(entity_s));
		{
			effect_s *ef_ph = alloc_effect(EF_PH_ITEM);
			ef_ph->type = EF_PH_ITEM;
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
			new_eff->type = EF_RENDER;
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
			new_eff->type = EF_R_TOUCH_SHOOT_PROJECTILE;
			effect_prepend(new_ent, new_eff);
		}
		{
			effect_s *new_eff = alloc_effect(EF_AIM);
			new_eff->type = EF_AIM;
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
		entity_s *new_ent = o_malloc(sizeof(entity_s));
		{
			effect_s *ef_ph = alloc_effect(EF_PH_ITEM);
			ef_ph->type = EF_PH_ITEM;
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
			new_eff->type = EF_RENDER;
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
			new_eff->type = EF_TABLE;
			effect_prepend(new_ent, new_eff);
		}
		entity_prepend(g_entities, new_ent);
		g_entities = new_ent;
		attach_generic_entity(new_ent);
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

int main(int argc, char **argv) {
	SDL_Init(SDL_INIT_EVERYTHING);
	TTF_Init();
	SDL_Window *win = SDL_CreateWindow("effect-sandbox", 0, 0, 500, 500, SDL_WINDOW_RESIZABLE);
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
					memset(new_sect->block_entities, 0, sizeof(entity_l_s*) * G_SECTOR_SIZE * G_SECTOR_SIZE * G_SECTOR_SIZE);
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

	while (running) {
		if (SDL_GetTicks() > last_blink + 500) {
			last_blink = SDL_GetTicks();
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
			render_camera(rend, &cam);
			render_status(rend, control_ent, 0, cam.height * gr_rend_char_height);
			SDL_RenderPresent(rend);
			need_redraw = 0;
		}
		SDL_Event evt;
		int trigger_done = 0;
		while (SDL_PollEvent(&evt)) {
			if (evt.type == SDL_WINDOWEVENT) {
				if (evt.window.event == SDL_WINDOWEVENT_RESIZED) {
					need_redraw = 1;
				}
			} else if (evt.type == SDL_KEYDOWN) {
				switch (evt.key.keysym.sym) {
					case SDLK_KP_6: {
						trigger_move(control_ent, 1, 1, 0, 0);
						trigger_done = 2;
					} break;
					case SDLK_KP_2: {
						trigger_move(control_ent, 1, 0, 1, 0);
						trigger_done = 2;
					} break;
					case SDLK_KP_8: {
						trigger_move(control_ent, 1, 0, -1, 0);
						trigger_done = 2;
					} break;
					case SDLK_KP_4: {
						trigger_move(control_ent, 1, -1, 0, 0);
						trigger_done = 2;
					} break;
					case SDLK_KP_7: {
						trigger_move(control_ent, 1, -1, -1, 0);
						trigger_done = 2;
					} break;
					case SDLK_KP_9: {
						trigger_move(control_ent, 1, 1, -1, 0);
						trigger_done = 2;
					} break;
					case SDLK_KP_3: {
						trigger_move(control_ent, 1, 1, 1, 0);
						trigger_done = 2;
					} break;
					case SDLK_KP_1: {
						trigger_move(control_ent, 1, -1, 1, 0);
						trigger_done = 2;
					} break;
					case SDLK_w: {
						trigger_done = 1;
					} break;
					case SDLK_h: {
						if (evt.key.keysym.mod & KMOD_SHIFT) {
							int sel_x = 8;
							int sel_y = 8;
							int sel_z = 0;
							entity_coords(control_ent, &sel_x, &sel_y, &sel_z);
							sel_x = 8;
							sel_y = 8;
							entity_s *sel_ent = NULL;
							if (select_tile(rend, &cam, &sel_x, &sel_y, &sel_z)) {
								if (select_tile_entity(rend, sel_x, sel_y, sel_z, &sel_ent)) {
									assert(sel_ent != NULL);
									trigger_grab(control_ent, effect_by_type(control_ent->effects, EF_LIMB_SLOT), sel_ent);
									trigger_done = 1;
								}
							}
						} else {
							trigger_drop(control_ent, effect_by_type(control_ent->effects, EF_LIMB_SLOT));
							trigger_done = 1;
						}
					} break;
					case SDLK_g: {
						int sel_x = 8;
						int sel_y = 8;
						int sel_z = 0;
						entity_coords(control_ent, &sel_x, &sel_y, &sel_z);
						sel_x = 8;
						sel_y = 8;
						entity_s *sel_ent = NULL;
						if (select_tile(rend, &cam, &sel_x, &sel_y, &sel_z)) {
							if (select_tile_entity(rend, sel_x, sel_y, sel_z, &sel_ent)) {
								assert(sel_ent != NULL);
								trigger_put(
									control_ent,
									effect_by_type(control_ent->effects, EF_LIMB_SLOT),
									sel_ent
								);
								trigger_done = 1;
							}
						}
					} break;
					case SDLK_t: {
						int sel_x;
						int sel_y;
						int sel_z;
						int ent_x;
						int ent_y;
						int ent_z;
						entity_coords(control_ent, &sel_x, &sel_y, &sel_z);
						ent_x = sel_x;
						ent_y = sel_y;
						ent_z = sel_z;
						sel_x = 8;
						sel_y = 8;
						if (select_tile(rend, &cam, &sel_x, &sel_y, &sel_z)) {
							sel_x -= ent_x;
							sel_y -= ent_y;
							sel_z -= ent_z;
							if (sel_x != 0 || sel_y != 0 || sel_z != 0) {
								int rel_x = 0;
								int rel_y = 0;
								int rel_z = 0;
								int sgcd = gcd(gcd(abs(sel_x), abs(sel_y)), abs(sel_z));
								sel_x /= sgcd;
								sel_y /= sgcd;
								sel_z /= sgcd;
								while (abs(rel_x + sel_x) <= 256 && abs(rel_y + sel_y) <= 256 && abs(rel_z + sel_z) <= 256) {
									rel_x += sel_x;
									rel_y += sel_y;
									rel_z += sel_z;
								}
								char buf[16];
								if (select_input_string(rend, buf, 16, "pow:")) {
									int power = strtoll(buf, NULL, 10);
									trigger_throw(control_ent, effect_by_type(control_ent->effects, EF_LIMB_SLOT), rel_x, rel_y, rel_z, power);
									trigger_done = 1;
								}
							}
						}
					} break;
					case SDLK_a: {
						int sel_x;
						int sel_y;
						int sel_z;
						int ent_x;
						int ent_y;
						int ent_z;
						entity_coords(control_ent, &sel_x, &sel_y, &sel_z);
						ent_x = sel_x;
						ent_y = sel_y;
						ent_z = sel_z;
						sel_x = 8;
						sel_y = 8;
						if (select_tile(rend, &cam, &sel_x, &sel_y, &sel_z)) {
							sel_x -= ent_x;
							sel_y -= ent_y;
							sel_z -= ent_z;
							if (sel_x != 0 || sel_y != 0 || sel_z != 0) {
								entity_s *aim_ent;
								if (!select_tile_entity(rend, sel_x + ent_x, sel_y + ent_y, sel_z + ent_z, &aim_ent)) {
									aim_ent = NULL;
								}
								int rel_x = 0;
								int rel_y = 0;
								int rel_z = 0;
								int sgcd = gcd(gcd(abs(sel_x), abs(sel_y)), abs(sel_z));
								sel_x /= sgcd;
								sel_y /= sgcd;
								sel_z /= sgcd;
								while (abs(rel_x + sel_x) <= 256 && abs(rel_y + sel_y) <= 256 && abs(rel_z + sel_z) <= 256) {
									rel_x += sel_x;
									rel_y += sel_y;
									rel_z += sel_z;
								}
								trigger_aim(control_ent, effect_by_type(control_ent->effects, EF_LIMB_SLOT), rel_x, rel_y, rel_z, aim_ent);
								trigger_done = 1;
							}
						}
					} break;
					case SDLK_u: {
						if (evt.key.keysym.mod & KMOD_SHIFT) {
							effect_s *ef_hand = effect_by_type(control_ent->effects, EF_LIMB_SLOT);
							if (ef_hand != NULL) {
								effect_limb_slot_data *h_d = (void*)ef_hand->data;
								effect_s *ef_lhand = effect_by_type(h_d->item->effects, EF_LIMB_HAND);
								if (ef_lhand != NULL) {
									effect_limb_hand_data *lh_d = (void*)ef_lhand->data;
									if (lh_d->item != NULL) {
										trigger_touch(
											control_ent,
											ef_hand,
											lh_d->item
										);
										trigger_done = 1;
									}
								}
							}
						} else {
							int sel_x;
							int sel_y;
							int sel_z;
							entity_coords(control_ent, &sel_x, &sel_y, &sel_z);
							sel_x = 8;
							sel_y = 8;
							entity_s *sel_ent = NULL;
							if (select_tile(rend, &cam, &sel_x, &sel_y, &sel_z)) {
								if (select_tile_entity(rend, sel_x, sel_y, sel_z, &sel_ent)) {
									assert(sel_ent != NULL);
									trigger_touch(
										control_ent,
										/* TODO add limb selection menu*/
										effect_by_type(control_ent->effects, EF_LIMB_SLOT),
										sel_ent
									);
									trigger_done = 1;
								}
							}
						}
					} break;
					case SDLK_d: {
						int sel_x = 8;
						int sel_y = 8;
						int sel_z = 0;
						entity_coords(control_ent, &sel_x, &sel_y, &sel_z);
						sel_x = 8;
						sel_y = 8;
						entity_s *sel_ent = NULL;
						if (select_tile(rend, &cam, &sel_x, &sel_y, &sel_z)) {
							if (select_tile_entity(rend, sel_x, sel_y, sel_z, &sel_ent)) {
								assert(sel_ent != NULL);
								trigger_attack(
									control_ent,
									sel_ent
								);
								trigger_done = 1;
							}
						}
					} break;
					case SDLK_r: {
						int sel_x = 8;
						int sel_y = 8;
						int sel_z = 0;
						entity_coords(control_ent, &sel_x, &sel_y, &sel_z);
						sel_x = 8;
						sel_y = 8;
						entity_s *sel_ent = NULL;
						if (select_tile(rend, &cam, &sel_x, &sel_y, &sel_z)) {
							if (select_tile_entity(rend, sel_x, sel_y, sel_z, &sel_ent)) {
								assert(sel_ent != NULL);
								effect_s *new_ef = alloc_effect(EF_FIRE);
								new_ef->type = EF_FIRE;
								effect_prepend(sel_ent, new_ef);
								trigger_done = 1;
							}
						}
					} break;
					case SDLK_k: {
						trigger_go_up(control_ent, 1);
						trigger_done = 2;
					} break;
					case SDLK_j: {
						trigger_go_down(control_ent, 1);
						trigger_done = 2;
					} break;
					default: {}
				}
			}
			if (evt.type == SDL_QUIT) {
				running = 0;
			}
		}
		if (trigger_done > 0) {
			process_tick(g_entities);
			if (trigger_done == 2) {
				process_tick(g_entities);
			}
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
	TTF_CloseFont(gr_font);
	SDL_DestroyRenderer(rend);
	SDL_DestroyWindow(win);
	SDL_Quit();
}
