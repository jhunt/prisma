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
	rc = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
	if (rc != 0) {
		fprintf(stderr, "sdl_init() failed: %s (rc=%d)\n", SDL_GetError(), rc);
		exit(EXIT_INIT_FAILED);
	}

	rc = IMG_Init(0);
	if (rc != 0) {
		fprintf(stderr, "img_init() failed: %s (rc=%d)\n", SDL_GetError(), rc);
		exit(EXIT_INIT_FAILED);
	}

	if (SDL_NumJoysticks() > 0) {
		SDL_JoystickOpen(0);
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

#define HERO_FRAMES 3
#define HERO_DOWN  0
#define HERO_RIGHT 3
#define HERO_LEFT  6
#define HERO_UP    9

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
	int xd, yd, done, hero, frame;

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
	for (frame = 0; !done; frame = (frame + 1) % (HERO_FRAMES - 1)) {
		while (SDL_PollEvent(&e) != 0) {
			switch (e.type) {
			case SDL_QUIT:
				done = 1;
				break;

			case SDL_JOYHATMOTION:
				switch (e.jhat.value) {
				case SDL_HAT_LEFT:      xd = -1;          break;
				case SDL_HAT_LEFTUP:    xd = -1; yd = -1; break;
				case SDL_HAT_LEFTDOWN:  xd = -1; yd =  1; break;
				case SDL_HAT_RIGHT:     xd =  1;          break;
				case SDL_HAT_RIGHTUP:   xd =  1; yd = -1; break;
				case SDL_HAT_RIGHTDOWN: xd =  1; yd =  1; break;
				case SDL_HAT_UP:                 yd = -1; break;
				case SDL_HAT_DOWN:               yd =  1; break;
				case SDL_HAT_CENTERED:  xd =  0; yd =  0; break;
				}
				break;

			case SDL_JOYAXISMOTION:
				switch (e.jaxis.axis) {
				case 0: xd = e.jaxis.value < -4096 ? -1 :
				             e.jaxis.value >  4096 ?  1 : 0; break;
				case 1: yd = e.jaxis.value < -4096 ? -1 :
				             e.jaxis.value >  4096 ?  1 : 0; break;
				}
				break;

			case SDL_KEYUP:
				switch (e.key.keysym.sym) {
				case SDLK_UP:
				case SDLK_DOWN:  yd =  0; break;
				case SDLK_LEFT:
				case SDLK_RIGHT: xd =  0; break;
				}
				break;

			case SDL_KEYDOWN:
				switch (e.key.keysym.sym) {
				case SDLK_q:
					done = 1;
					break;

				case SDLK_UP:    yd = -1; break;
				case SDLK_DOWN:  yd =  1; break;
				case SDLK_LEFT:  xd = -1; break;
				case SDLK_RIGHT: xd =  1; break;
				}
				break;
			}
		}

		map_draw(map, scr);
		if ((xd || yd) && !issolid(map, scr->x + xd, scr->y + yd)) {
			scr->x += xd;
			scr->y += yd;
		}
		hero = xd > 0 ? HERO_RIGHT
		     : xd < 0 ? HERO_LEFT
		     : yd < 0 ? HERO_UP
		     :          HERO_DOWN;
		draw_tile(scr, sprites, hero + (xd || yd ? frame + 1 : 0),
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
