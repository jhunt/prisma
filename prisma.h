#ifndef PRISMA_H
#define PRISMA_H

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <SDL.h>
#include <SDL_image.h>

/* failure encountered while initializing the game,
   i.e. with SDL graphic or audio subsystems, map
   reading, tileset parsing, etc. */
#define EXIT_INIT_FAILED 1

/* failure encountered due to something from the
   environment (missing files, mostly). */
#define EXIT_ENV_FAILURE 2

/* failure encountered in the language runtime,
   like failure to allocate memory when asked. */
#define EXIT_INT_FAILURE 2

void * allocate(size_t n, size_t size);
void * astring(const char *fmt, ...);

int bounded(int min, int v, int max);

#define ANALOG_TOLERANCE 4096
int analog(int v);

struct coords {
	int x;
	int y;
};

struct tileset {
	SDL_Surface *surface;
	int width;

	struct {
		int width;
		int height;
	} tile;
};

struct map {
	int *cells[2];

	int  width;
	int  height;

	struct coords entry;

	struct tileset *tiles;
};

struct sprite {
	struct tileset *tileset;
	int tile;
	int frame;

	struct coords at;
	struct coords delta;
};

struct world {
	SDL_Window  *window;
	SDL_Surface *surface;

	int scale;
	int tocks;

	struct {
		struct coords at;
		int width;
		int height;
	} viewport;

	struct map    *map;
	struct sprite *hero;
};

struct world * world_new(int scale);
void           world_free(struct world * world);

void           world_unveil(struct world * world, const char *title, int w, int h);
void           world_load(struct world * world, const char *map, const char *hero);
void           world_update(struct world * world);
void           world_render(struct world * world);

void           world_draw(struct world * world, struct tileset* tiles, int t, int x, int y);

int  sprite_moving(struct sprite *sprite);
int  sprite_tile(struct sprite *sprite);
void sprite_move_x(struct sprite *sprite, int x);
void sprite_move_y(struct sprite *sprite, int y);
void sprite_move_all(struct sprite *sprite, int left, int right, int up, int down);

struct tileset * tileset_read(const char * path);
void             tileset_free(struct tileset * tiles);

#define mapat(map,i,x,y) \
          ((map)->cells[i][(map)->height * (x) + (y)])
struct map * map_read(const char * path);
void         map_free(struct map * map);

#endif
