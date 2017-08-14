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

int main(int argc, char **argv)
{
	struct screen  *scr;
	struct map     *map;
	struct tileset *sprites;
	struct sprite  *hero;
	SDL_Event      e;
	int done;

	init();

	scr = screen_make("prismatic", 640, 480);
	if (!scr) {
		fprintf(stderr, "failed to initialize the screen: %s\n", SDL_GetError());
		return 1;
	}

	sprites = tileset_read("assets/purple-hair-sprite");
	hero    = allocate(1, sizeof(*hero));
	map     = map_read("maps/base");
	screen_use_map(scr, map, 4);

	hero->at.x = 3;
	hero->at.y = 3;
	tile2pixel(&hero->at.x, &hero->at.y, scr, map);

	done = 0;
	while (!done) {
		while (SDL_PollEvent(&e) != 0) {
			switch (e.type) {
			case SDL_QUIT:
				done = 1;
				break;

			case SDL_JOYDEVICEADDED:
				SDL_JoystickOpen(e.jdevice.which);
				break;

			case SDL_JOYHATMOTION:
				sprite_move_all(hero,
					e.jhat.value & SDL_HAT_LEFT,
					e.jhat.value & SDL_HAT_RIGHT,
					e.jhat.value & SDL_HAT_UP,
					e.jhat.value & SDL_HAT_DOWN);
				break;

			case SDL_JOYAXISMOTION:
				switch (e.jaxis.axis % 2) {
				case 0: sprite_move_x(hero, analog(e.jaxis.value)); break;
				case 1: sprite_move_y(hero, analog(e.jaxis.value)); break;
				}
				break;

			case SDL_KEYUP:
				switch (e.key.keysym.sym) {
				case SDLK_UP:
				case SDLK_DOWN:  sprite_move_y(hero, 0); break;
				case SDLK_LEFT:
				case SDLK_RIGHT: sprite_move_x(hero, 0); break;
				}
				break;

			case SDL_KEYDOWN:
				switch (e.key.keysym.sym) {
				case SDLK_q:
					done = 1;
					break;

				case SDLK_UP:    sprite_move_y(hero, -1); break;
				case SDLK_DOWN:  sprite_move_y(hero,  1); break;
				case SDLK_LEFT:  sprite_move_x(hero, -1); break;
				case SDLK_RIGHT: sprite_move_x(hero,  1); break;
				}
				break;
			}
		}

		/* FIXME: this is hacky as hell, and needs map+screen changes */
		sprite_collide(hero, map, scr);
		scr->x = hero->at.x; scr->y = hero->at.y;
		pixel2tile(&scr->x, &scr->y, scr, map);
		int cx = bounded(0, scr->x - scr->width  / 2, map->width  - scr->width);
		int cy = bounded(0, scr->y - scr->height / 2, map->height - scr->height);
		tile2pixel(&cx, &cy, scr, map);
		printf("cx,cy = (%d, %d)\n", cx, cy);
		if (cx > hero->at.x) cx = hero->at.x;
		if (cy > hero->at.y) cy = hero->at.y;

		map_draw(map, scr);
		draw_at(scr, sprites, sprite_tile(hero), hero->at.x - cx, hero->at.y - cy);
		screen_draw(scr);
		SDL_Delay(16);
	}

	map_free(map);
	screen_free(scr);
	quit();

	return 0;
}
