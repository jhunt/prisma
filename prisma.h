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

struct screen {
	int  width;
	int  height;
	int  scale;
	int  x;
	int  y;

	SDL_Surface  *surface;
	SDL_Window   *window;
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

	struct tileset *tiles;
};

struct coords {
	int x;
	int y;
};

struct sprite {
	struct tileset *tileset;
	int tile;
	int frame;

	struct coords at;
	struct coords delta;
};

int  sprite_moving(struct sprite *sprite);
void sprite_collide(struct sprite *sprite, struct map *map, struct screen *screen);
int  sprite_tile(struct sprite *sprite);

struct screen * screen_make(const char *title, int w, int h);
void            screen_free(struct screen * screen);
void            screen_use_map(struct screen * s, struct map * map, int scale);
void            screen_draw(struct screen * screen);

struct tileset * tileset_read(const char * path);
void             tileset_free(struct tileset * tiles);

#define mapat(map,i,x,y) \
          ((map)->cells[i][(map)->height * (x) + (y)])
struct map * map_read(const char * path);
void         map_draw(struct map * map, struct screen * screen);
void         map_free(struct map * map);
int          map_solid(struct map * map, int x, int y);

void draw_tile(struct screen * screen, struct tileset* tiles, int t, int x, int y);

#endif
