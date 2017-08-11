#include "prisma.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>

#include <SDL.h>
#include <SDL_image.h>

/* upper limit of 8MiB on map size */
#define MAX_MAP_SIZE (1024 * 1024 * 8)
#define READ_BLOCK_SIZE 8192


void init()
{
	int rc;
	rc = SDL_Init(SDL_INIT_VIDEO);
	if (rc != 0) {
		fprintf(stderr, "sdl_init() failed: %s (rc=%d)\n", SDL_GetError(), rc);
		exit(EXIT_INIT_FAILED);
	}

	rc = IMG_Init(0);
	if (rc != 0) {
		fprintf(stderr, "img_init() failed: %s (rc=%d)\n", SDL_GetError(), rc);
		exit(EXIT_INIT_FAILED);
	}
}

void quit()
{
	IMG_Quit();
	SDL_Quit();
}

/* tiles! */
#define TILE_NONE            0
#define TILE_A_TOP_WALL    ( 1 << 24)
#define TILE_A_SIDE_WALL   ( 2 << 24)
#define TILE_A_BOTTOM_WALL ( 3 << 24)
#define TILE_A_TOP_CORNER  ( 4 << 24)
#define TILE_A_FLOOR       ( 9 << 24)
#define TILE_A_WHOLE_JAR   (28 << 24)
#define TILE_A_TABLE       (55 << 24)
#define TILE_A_CABINET     (56 << 24)

#define HERO_AVATAR 0

#define TILE_SOLID          0x01

static int
inmap(struct map *map, int x, int y)
{
	return !(x < 0 || x > map->width ||
	         y < 0 || y > map->height);
}

static int
issolid(struct map *map, int x, int y)
{
	return !inmap(map, x, y) || mapat(map, x, y) & TILE_SOLID;
}

int main(int argc, char **argv)
{
	struct screen  *scr;
	struct map     *map;
	struct tileset *tiles, *sprites;
	SDL_Event      e;
	int xd, yd, done;

	init();

	scr = screen_make("prismatic", 640, 480);
	if (!scr) {
		fprintf(stderr, "failed to initialize the screen: %s\n", SDL_GetError());
		return 1;
	}

	tiles   = tileset_read("assets/tileset");
	sprites = tileset_read("assets/purple-hair-sprite");
	map     = map_read("maps/base");
	map->tiles = tiles;
	screen_use_map(scr, map, 4);

	done = 0;
	scr->x = scr->y = 3;
	while (!done) {
		while (SDL_PollEvent(&e) != 0) {
			switch (e.type) {
			case SDL_QUIT:
				done = 1;
				break;
			case SDL_KEYDOWN:
				xd = yd = 0;
				switch (e.key.keysym.sym) {
				case SDLK_q:
					done = 1;
					break;

				case SDLK_UP:
					yd = -1;
					break;
				case SDLK_DOWN:
					yd = 1;
					break;
				case SDLK_LEFT:
					xd = -1;
					break;
				case SDLK_RIGHT:
					xd = 1;
					break;
				}

				if ((xd || yd) && !issolid(map, scr->x + xd, scr->y + yd)) {
					scr->x += xd;
					scr->y += yd;
				}
			}
		}
		map_draw(map, scr);
		draw_tile(scr, sprites, HERO_AVATAR,
			scr->x - bounded(0, scr->x - scr->width  / 2, map->width  - scr->width),
			scr->y - bounded(0, scr->y - scr->height / 2, map->height - scr->height));

		screen_draw(scr);
		SDL_Delay(150);
	}

	map_free(map);
	screen_free(scr);
	quit();

	return 0;
}
