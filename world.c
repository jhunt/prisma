#include "prisma.h"

#define TILE_NONE      0
#define TILE_SOLID  0x01

static int inmap(struct map *map, int x, int y);
static int istile(int t);
static void draw(struct world *world, struct tileset *tiles, int t, int x, int y);

static int
inmap(struct map *map, int x, int y)
{
	return !(x < 0 || x > map->width ||
	         y < 0 || y > map->height);
}

static int
istile(int t)
{
	return (t >> 24) != 0;
}

static void
draw(struct world *world, struct tileset *tiles, int t, int x, int y)
{
	assert(world != NULL);
	assert(t >= 0);

	if (tiles == NULL)
		tiles = world->map->tiles;

	SDL_Rect src = {
		.x = tiles->tile.width  * (t % tiles->width),
		.y = tiles->tile.height * (t / tiles->width),
		.w = tiles->tile.width,
		.h = tiles->tile.height,
	};

	SDL_Rect dst = {
		.x = x,
		.y = y,
		.w = tiles->tile.width  * world->scale,
		.h = tiles->tile.height * world->scale,
	};

	SDL_BlitScaled(tiles->surface, &src, world->surface, &dst);
}


struct world * world_new(int scale)
{
	struct world *world;

	world = allocate(1, sizeof(struct world));
	world->scale = scale;
	return world;
}

void world_free(struct world * world)
{
	if (!world) return;

	if (world->window) SDL_DestroyWindow(world->window);
	free(world);
}

void world_unveil(struct world * world, const char *title, int w, int h)
{
	world->window = SDL_CreateWindow(
		title,
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		w, h,
		SDL_WINDOW_FULLSCREEN_DESKTOP
	);
	if (!world->window) {
		fprintf(stderr, "failed to create window: %s\n", SDL_GetError());
		exit(EXIT_INT_FAILURE);
	}
	SDL_GetWindowSize(world->window, &world->viewport.width, &world->viewport.height);

	world->surface = SDL_GetWindowSurface(world->window);
	if (!world->surface) {
		fprintf(stderr, "failed to get surface from window: %s\n", SDL_GetError());
		exit(EXIT_INT_FAILURE);
	}
}

static int
s_solid(struct world * world, int x, int y)
{
	x = x / world->map->tiles->tile.width  / world->scale;
	y = y / world->map->tiles->tile.height / world->scale;

	return x < 0 || x > world->map->width
	    || y < 0 || y > world->map->height
	    || mapat(world->map, 0, x, y) & TILE_SOLID
	    || mapat(world->map, 1, x, y);
}

static int
s_collide(struct world * world, int x, int y)
{
	int dx, dy;
	dx = world->map->tiles->tile.width  * world->scale - 1;
	dy = world->map->tiles->tile.height * world->scale - 1;

	return s_solid(world, x,    y)
	    || s_solid(world, x+dx, y)
	    || s_solid(world, x,    y+dy)
	    || s_solid(world, x+dx, y+dy);
}

static void
s_hero_collision(struct world * world)
{
	int x, y;

	x = world->hero->at.x;
	y = world->hero->at.y;

	if (world->hero->delta.x) {
		x += world->hero->delta.x;
		if (x < 0) x = 0;
		if (!s_collide(world, x, y)) {
			world->hero->at.x += world->hero->delta.x;
		}
	}

	if (world->hero->delta.y) {
		y += world->hero->delta.y;
		if (y < 0) y = 0;
		if (!s_collide(world, x, y)) {
			world->hero->at.y += world->hero->delta.y;
		}
	}
}

static void
s_focus(struct world *world, int x, int y)
{
	int vw, vh, dx, dy;
	vw = world->viewport.width;
	vh = world->viewport.height;
	dx = world->map->tiles->tile.width  * world->scale; /* FIXME: functionalize */
	dy = world->map->tiles->tile.height * world->scale; /* FIXME: functionalize */

	world->viewport.at.x = bounded(0, x - vw / 2, world->map->width  * dx - vw);
	world->viewport.at.y = bounded(0, y - vh / 2, world->map->height * dy - vh);
}

void world_update(struct world * world)
{
	s_hero_collision(world);
	s_focus(world, world->hero->at.x, world->hero->at.y);
}

void world_render(struct world * world)
{
	assert(world != NULL);
	assert(world->map != NULL);

	int x, y, t, dx, dy, ox, oy;
	dx = world->map->tiles->tile.width  * world->scale; /* FIXME: functionalize */
	dy = world->map->tiles->tile.height * world->scale; /* FIXME: functionalize */
	ox = world->viewport.at.x % dx * -1;
	oy = world->viewport.at.y % dy * -1;

	/* draw the tiled map background layer */
	for (x = ox; x <= world->viewport.width; x += dx) {
		for (y = oy; y <= world->viewport.height; y += dy) {
			if (inmap(world->map, (x + world->viewport.at.x) / dx,
			                      (y + world->viewport.at.y) / dy)) {
				t = mapat(world->map, 0, (x + world->viewport.at.x) / dx,
				                         (y + world->viewport.at.y) / dy);
				if (istile(t)) {
					t = (t >> 24) - 1; /* FIXME: implementation detail */
				} else {
					t = 22; /* FIXME: magic number! */
				}
				draw(world, NULL, t, x, y);

				t = mapat(world->map, 1, (x + world->viewport.at.x) / dx,
				                         (y + world->viewport.at.y) / dy);
				if (istile(t)) {
					t = (t >> 24) - 1; /* FIXME: implementation detail */
					draw(world, NULL, t, x, y);
				}
			} else {
				t = 22; /* FIXME: magic number */
				draw(world, NULL, t, x, y);
			}
		}
	}

	/* draw the hero avatar */
	draw(world, world->hero->tileset, sprite_tile(world->hero), world->hero->at.x - world->viewport.at.x, world->hero->at.y - world->viewport.at.y);

	SDL_UpdateWindowSurface(world->window);
}