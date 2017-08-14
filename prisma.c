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
				hero->delta.x = 0;
				hero->delta.y = 0;
				if (e.jhat.value & SDL_HAT_LEFT)  hero->delta.x = -1;
				if (e.jhat.value & SDL_HAT_RIGHT) hero->delta.x =  1;
				if (e.jhat.value & SDL_HAT_UP)    hero->delta.y = -1;
				if (e.jhat.value & SDL_HAT_DOWN)  hero->delta.y =  1;
				break;

			case SDL_JOYAXISMOTION:
				switch (e.jaxis.axis % 2) {
				case 0: hero->delta.x = e.jaxis.value < -4096 ? -1 :
				                          e.jaxis.value >  4096 ?  1 : 0; break;
				case 1: hero->delta.y = e.jaxis.value < -4096 ? -1 :
				                          e.jaxis.value >  4096 ?  1 : 0; break;
				}
				break;

			case SDL_KEYUP:
				switch (e.key.keysym.sym) {
				case SDLK_UP:
				case SDLK_DOWN:  hero->delta.y = 0; break;
				case SDLK_LEFT:
				case SDLK_RIGHT: hero->delta.x = 0; break;
				}
				break;

			case SDL_KEYDOWN:
				switch (e.key.keysym.sym) {
				case SDLK_q:
					done = 1;
					break;

				case SDLK_UP:    hero->delta.y = -1; break;
				case SDLK_DOWN:  hero->delta.y =  1; break;
				case SDLK_LEFT:  hero->delta.x = -1; break;
				case SDLK_RIGHT: hero->delta.x =  1; break;
				}
				break;
			}
		}

		map_draw(map, scr);
		sprite_collide(hero, map, scr);
		scr->x = hero->at.x; scr->y = hero->at.y; /* FIXME */
		draw_tile(scr, sprites, sprite_tile(hero),
			hero->at.x - bounded(0, hero->at.x - scr->width  / 2, map->width  - scr->width),
			hero->at.y - bounded(0, hero->at.y - scr->height / 2, map->height - scr->height));

		screen_draw(scr);
		SDL_Delay(150);
	}

	map_free(map);
	screen_free(scr);
	quit();

	return 0;
}
