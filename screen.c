#include "prisma.h"

struct screen *
screen_make(const char *title, int w, int h)
{
	struct screen *s;

	s = allocate(1, sizeof(struct screen));
	s->window = SDL_CreateWindow(
		title,
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		w, h,
		SDL_WINDOW_FULLSCREEN_DESKTOP
	);
	if (!s->window) {
		fprintf(stderr, "failed to create window: %s\n", SDL_GetError());
		exit(EXIT_INT_FAILURE);
	}

	s->surface = SDL_GetWindowSurface(s->window);
	if (!s->surface) {
		fprintf(stderr, "failed to get surface from window: %s\n", SDL_GetError());
		exit(EXIT_INT_FAILURE);
	}

	return s;
}

void
screen_use_map(struct screen * s, struct map * map, int scale)
{
	s->scale  = scale;
	SDL_GetWindowSize(s->window, &s->width, &s->height);
	s->width  = s->width  / s->scale / map->tiles->tile.width;
	s->height = s->height / s->scale / map->tiles->tile.height;
}

void
screen_free(struct screen *s)
{
	if (s && s->window) SDL_DestroyWindow(s->window);
	free(s);
}

void
screen_draw(struct screen *s)
{
	SDL_UpdateWindowSurface(s->window);
}
