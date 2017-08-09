#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <SDL.h>
#include <SDL_image.h>

/* upper limit of 8MiB on map size */
#define MAX_MAP_SIZE (1024 * 1024 * 8)
#define READ_BLOCK_SIZE 8192

#define EXIT_INIT_FAILED 1
#define EXIT_ENV_FAILURE 2
#define EXIT_INT_FAILURE 2

struct map {
	int *cells;

	int  width;
	int  height;

	int  offset_x;
	int  offset_y;
};

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

#define WHOLE_JAR 27

#define TILE_SOLID          0x01

#define TILE_WIDTH  16
#define TILE_HEIGHT 16
#define TILE_SCALE  4

#define TILESET_WIDTH 8

void tile(SDL_Surface *dst, int x, int y, SDL_Surface *tiles, int t)
{
	SDL_Rect srect = {
		.x = TILE_WIDTH  * (t % TILESET_WIDTH),
		.y = TILE_HEIGHT * (t / TILESET_WIDTH),
		.w = TILE_WIDTH,
		.h = TILE_HEIGHT,
	};

	SDL_Rect drect = {
		.x = x * TILE_WIDTH  * TILE_SCALE,
		.y = y * TILE_HEIGHT * TILE_SCALE,
		.w =     TILE_WIDTH  * TILE_SCALE,
		.h =     TILE_HEIGHT * TILE_SCALE,
	};

	SDL_BlitScaled(tiles, &srect, dst, &drect);
}

static char *
s_readmap_raw(const char *path)
{
	int fd;
	off_t size;
	char *raw;
	size_t n;
	ssize_t nread;

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "failed to read map from %s: %s (error %d)\n",
			path, strerror(errno), errno);
		exit(EXIT_ENV_FAILURE);
	}

	size = lseek(fd, 0, SEEK_END);
	if (size < 0) {
		fprintf(stderr, "failed to read map from %s: %s (error %d)\n",
			path, strerror(errno), errno);
		exit(EXIT_ENV_FAILURE);
	}
	if (lseek(fd, 0, SEEK_SET) != 0) {
		fprintf(stderr, "failed to read map from %s: %s (error %d)\n",
			path, strerror(errno), errno);
		exit(EXIT_ENV_FAILURE);
	}

	if (size > MAX_MAP_SIZE) {
		fprintf(stderr, "map %s is too large\n", path);
		exit(EXIT_ENV_FAILURE);
	}

	raw = calloc(size + 1, sizeof(char));
	if (!raw) {
		fprintf(stderr, "failed to allocate memory: %s (error %d)\n",
				strerror(errno), errno);
		exit(EXIT_INT_FAILURE);
	}

	n = 0;
	for (;;) {
		nread = read(fd, raw + n, READ_BLOCK_SIZE);
		if (nread == 0) break;
		if (nread < 0) {
			fprintf(stderr, "failed to read map from %s: %s (error %d)\n",
				path, strerror(errno), errno);
			exit(EXIT_ENV_FAILURE);
		}
		n += nread;
	}

	close(fd);
	return raw;
}

static void
s_mapsize(const char *raw, int *w, int *h)
{
	const char *a, *b;
	int l;

	a = raw;
	for (;;) {
		b = strchr(a, '\n');
		if (b) l = b - a;
		else   l = strlen(a);

		if (l > *w) *w = l;
		*h += 1;

		if (!b) break;
		a = ++b;
	}
}

#define mapat(map,x,y) \
          ((map)->cells[(y) * (map)->height + (x)])

static int
istile(int t)
{
	return (t >> 24) != 0;
}

static int
issolid(struct map *map, int x, int y)
{
	/* out-of-bounds is SOLID */
	if (x < 0 || x > map->width ||
	    y < 0 || y > map->height) return 1;

	return mapat(map, x, y) & TILE_SOLID;
}

struct map *
readmap(const char *path)
{
	char *raw, *p;
	struct map *map;
	int x, y;

	raw = s_readmap_raw(path);
	map = calloc(1, sizeof(struct map));
	if (!map) {
		fprintf(stderr, "failed to allocate memory: %s (error %d)\n",
				strerror(errno), errno);
		exit(EXIT_INT_FAILURE);
	}

	s_mapsize(raw, &map->width, &map->height);
	map->cells = calloc(map->width * map->height, sizeof(int));

	/* decode the newline-terminated map into a cell-list */
	for (x = y = 0, p = raw; *p; p++) {
		switch (*p) {
		case '\n': x = 0; y++; break;
		case '+': mapat(map, x++, y) = TILE_A_TOP_CORNER  | TILE_SOLID; break;
		case '-': mapat(map, x++, y) = TILE_A_TOP_WALL    | TILE_SOLID; break;
		case '=': mapat(map, x++, y) = TILE_A_BOTTOM_WALL | TILE_SOLID; break;
		case '|': mapat(map, x++, y) = TILE_A_SIDE_WALL   | TILE_SOLID; break;
		case 'c': mapat(map, x++, y) = TILE_A_CABINET     | TILE_SOLID; break;
		case 't': mapat(map, x++, y) = TILE_A_TABLE       | TILE_SOLID; break;
		case ' ': mapat(map, x++, y) = TILE_A_FLOOR;                    break;
		default:  mapat(map, x++, y) = TILE_NONE;                       break;
		}
	}

	free(raw);
	return map;
}

void
drawmap(SDL_Surface *dst, SDL_Surface *tiles, struct map *map, int cx, int cy)
{
	int x, y;

	for (x = 0; x < map->width; x++) {
		for (y = 0; y < map->height; y++) {
			if (istile(mapat(map, x, y))) {
				tile(dst, x, y, tiles, (mapat(map, x, y) >> 24) - 1);
			}
		}
	}
}

#define SCREEN_WIDTH  (16 * 16 * TILE_SCALE)
#define SCREEN_HEIGHT (16 *  6 * TILE_SCALE)

int main(int argc, char **argv)
{
	SDL_Window  *win;
	SDL_Surface *sfc, *tileset;
	SDL_Event    e;
	int x, xd, y, yd, done;
	struct map *map;

	init();

	win = SDL_CreateWindow(
		"SDL Tutorial",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		SCREEN_WIDTH,
		SCREEN_HEIGHT,
		SDL_WINDOW_FULLSCREEN_DESKTOP
	);
	if (!win) {
		fprintf(stderr, "failed to create window: %s\n", SDL_GetError());
		return 1;
	}

	sfc = SDL_GetWindowSurface(win);
	if (!sfc) {
		fprintf(stderr, "failed to get surface from window: %s\n", SDL_GetError());
		return 1;
	}

	tileset = IMG_Load("assets/tileset.png");
	if (!tileset) {
		fprintf(stderr, "failed to load tileset.png: %s\n", SDL_GetError());
		return 1;
	}

	map = readmap("maps/base");
	done = 0;

	int w, h;
	SDL_GetWindowSize(win, &w, &h);
	fprintf(stderr, "window is %d wide by %d high\n", w, h);
	fprintf(stderr, "(which is %d tiles wide by %d tiles high)\n",
			w / TILE_WIDTH / TILE_SCALE, h / TILE_HEIGHT / TILE_SCALE);

	x = y = 3;
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

				if ((xd || yd) &&  issolid(map, x + xd, y + yd)) {
					fprintf(stderr, "tile at (%d, %d) is SOLID\n", x + xd, y + yd);
				}
				if ((xd || yd) && !issolid(map, x + xd, y + yd)) {
					x += xd;
					y += yd;
				}
			}
		}
		drawmap(sfc, tileset, map, 0, 0);
		tile(sfc, x, y, tileset, WHOLE_JAR);

		SDL_UpdateWindowSurface(win);
		SDL_Delay(150);
	}

	SDL_DestroyWindow(win);
	quit();

	return 0;
}
