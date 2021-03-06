#include "prisma.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>

#include <SDL.h>
#include <SDL_image.h>

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

int main(int argc, char **argv)
{
	struct world *world;
	SDL_Event     e;
	int done;

	init();

	world = world_new(4);
	world_load(world, "maps/base", "assets/purple-hair-sprite");
	world_unveil(world, "prismatic", 640, 480);

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
				sprite_move_all(world->hero,
					e.jhat.value & SDL_HAT_LEFT,
					e.jhat.value & SDL_HAT_RIGHT,
					e.jhat.value & SDL_HAT_UP,
					e.jhat.value & SDL_HAT_DOWN);
				break;

			case SDL_JOYAXISMOTION:
				switch (e.jaxis.axis % 2) {
				case 0: sprite_move_x(world->hero, analog(e.jaxis.value)); break;
				case 1: sprite_move_y(world->hero, analog(e.jaxis.value)); break;
				}
				break;

			case SDL_KEYUP:
				switch (e.key.keysym.sym) {
				case SDLK_UP:
				case SDLK_DOWN:  sprite_move_y(world->hero, 0); break;
				case SDLK_LEFT:
				case SDLK_RIGHT: sprite_move_x(world->hero, 0); break;
				}
				break;

			case SDL_KEYDOWN:
				switch (e.key.keysym.sym) {
				case SDLK_q:
					done = 1;
					break;

				case SDLK_UP:    sprite_move_y(world->hero, -1); break;
				case SDLK_DOWN:  sprite_move_y(world->hero,  1); break;
				case SDLK_LEFT:  sprite_move_x(world->hero, -1); break;
				case SDLK_RIGHT: sprite_move_x(world->hero,  1); break;
				}
				break;
			}
		}

		world_update(world);
		world_render(world);
		SDL_Delay(16);
	}

	world_free(world);
	quit();

	return 0;
}
