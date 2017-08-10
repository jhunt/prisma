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

#define EXIT_INIT_FAILED 1
#define EXIT_ENV_FAILURE 2
#define EXIT_INT_FAILURE 2

struct screen {
	int  width;
	int  height;
	int  scale;

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

void
free_tileset(struct tileset *t)
{
	if (t && t->surface) SDL_FreeSurface(t->surface);
	free(t);
}

struct tileset *
read_tileset(const char *path)
{
	struct tileset *tiles;
	char *p   = NULL;
	FILE *nfo = NULL;
	int rc;

	tiles = calloc(1, sizeof(struct tileset));
	if (!tiles) {
		fprintf(stderr, "failed to allocate memory: %s (error %d)\n",
				strerror(errno), errno);
		exit(EXIT_INT_FAILURE);
	}

	rc = asprintf(&p, "%s.png", path);
	if (rc < 0) {
		fprintf(stderr, "failed to allocate memory: %s (error %d)\n",
				strerror(errno), errno);
		exit(EXIT_INT_FAILURE);
	}

	tiles->surface = IMG_Load(p);
	if (!tiles->surface) {
		fprintf(stderr, "failed to load tileset image %s: %s (error %d)\n",
				p, strerror(errno), errno);
		goto failed;
	}

	free(p);
	rc = asprintf(&p, "%s.nfo", path);
	if (rc < 0) {
		fprintf(stderr, "failed to allocate memory: %s (error %d)\n",
				strerror(errno), errno);
		exit(EXIT_INT_FAILURE);
	}

	nfo = fopen(p, "r");
	if (!nfo) {
		fprintf(stderr, "failed to load tileset metadata %s: %s (error %d)\n",
				p, strerror(errno), errno);
		goto failed;
	}
	rc = fscanf(nfo, "SPRITES %dx%d %d\n",
		&tiles->tile.width,
		&tiles->tile.height,
		&tiles->width);
	if (rc != 3) {
		fprintf(stderr, "failed to parse tileset metadata from %s: invalid format.\n", p);
		goto failed;
	}

	free(p);
	fclose(nfo);
	return tiles;

failed:
	if (nfo) fclose(nfo);
	free(p);
	free_tileset(tiles);
	return NULL;
}

struct map {
	int *cells;

	int  width;
	int  height;

	struct tileset *tiles;
};

void
draw_tile(struct screen *screen, struct tileset *tiles, int t, int x, int y)
{
	assert(screen != NULL);
	assert(tiles != NULL);
	assert(t >= 0);
	assert(x >= 0);
	assert(y >= 0);

	SDL_Rect src = {
		.x = tiles->tile.width  * (t % tiles->width),
		.y = tiles->tile.height * (t / tiles->width),
		.w = tiles->tile.width,
		.h = tiles->tile.height,
	};

	SDL_Rect dst = {
		.x = x * tiles->tile.width  * screen->scale,
		.y = y * tiles->tile.height * screen->scale,
		.w =     tiles->tile.width  * screen->scale,
		.h =     tiles->tile.height * screen->scale,
	};

	SDL_BlitScaled(tiles->surface, &src, screen->surface, &dst);
}

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

#define TILE_WIDTH  16
#define TILE_HEIGHT 16

struct screen *
make_screen(const char *title, int w, int h, int scale)
{
	struct screen *s;

	s = malloc(sizeof(struct screen));
	if (!s) {
		fprintf(stderr, "failed to allocate memory: %s (error %d)\n",
				strerror(errno), errno);
		exit(EXIT_INT_FAILURE);
	}

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

	SDL_GetWindowSize(s->window, &s->width, &s->height);
	s->scale  = scale;
	s->width  = s->width  / s->scale / TILE_WIDTH;
	s->height = s->height / s->scale / TILE_HEIGHT;

	return s;
}

void
draw_screen(struct screen *s)
{
	SDL_UpdateWindowSurface(s->window);
}

void
free_screen(struct screen *s)
{
	if (s && s->window) SDL_DestroyWindow(s->window);
	free(s);
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
		nread = read(fd, raw + n, READ_BLOCK_SIZE > size ? size : READ_BLOCK_SIZE);
		if (nread == 0) break;
		if (nread < 0) {
			fprintf(stderr, "failed to read map from %s: %s (error %d)\n",
				path, strerror(errno), errno);
			exit(EXIT_ENV_FAILURE);
		}
		n += nread;
		if (n == size) break;
	}

	close(fd);
	return raw;
}

static void
s_mapsize(const char *raw, int *w, int *h)
{
	const char *a, *b;
	int l;

	*w = *h = 0;
	a = raw;
	for (;;) {
		b = strchr(a, '\n');
		if (b) l = b - a;
		else   l = strlen(a);

		if (l > *w) *w = l;
		*h += 1;

		if (!b) break;

		a = ++b;
		if (!*b) break;
	}
}

#define mapat(map,x,y) \
          ((map)->cells[(map)->height * (x) + (y)])

static int
istile(int t)
{
	return (t >> 24) != 0;
}

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

void
free_map(struct map *m)
{
	if (m) free(m->cells);
	free(m);
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
		case 'u': mapat(map, x++, y) = TILE_A_WHOLE_JAR   | TILE_SOLID; break;
		case 'n': mapat(map, x++, y) = TILE_A_TABLE       | TILE_SOLID; break;
		case ' ': mapat(map, x++, y) = TILE_A_FLOOR;                    break;
		default:  mapat(map, x++, y) = TILE_NONE;                       break;
		}
	}

	free(raw);
	return map;
}

int
bounded(int min, int v, int max)
{
	if (v < min) return min;
	if (v > max) return max;
	return v;
}

void
draw_map(struct screen *scr, struct tileset *tiles, struct map *map, int cx, int cy)
{
	assert(scr != NULL);
	assert(tiles != NULL);
	assert(map != NULL);

	int x, y;

	cx = bounded(0, cx - scr->width  / 2, map->width  - scr->width);
	cy = bounded(0, cy - scr->height / 2, map->height - scr->height);

	for (x = 0; x < scr->width; x++) {
		for (y = 0; y < scr->height; y++) {
			if (inmap(map, x+cx, y+cy)) {
				int t = mapat(map, x+cx, y+cy);
				if (istile(t)) {
					draw_tile(scr, tiles, (t >> 24) - 1, x, y);
				} else {
					draw_tile(scr, tiles, 22, x, y);
				}
			} else {
				draw_tile(scr, tiles, 22, x, y);
			}
		}
	}
}

int main(int argc, char **argv)
{
	struct screen  *scr;
	struct map     *map;
	struct tileset *tiles, *sprites;
	SDL_Event      e;
	int x, xd, y, yd, done;

	init();

	scr = make_screen("prismatic", 640, 480, 4);
	if (!scr) {
		fprintf(stderr, "failed to initialize the screen: %s\n", SDL_GetError());
		return 1;
	}

	tiles   = read_tileset("assets/tileset");
	sprites = read_tileset("assets/purple-hair-sprite");
	map     = readmap("maps/base");

	done = 0;
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

				if ((xd || yd) && !issolid(map, x + xd, y + yd)) {
					x += xd;
					y += yd;
				}
			}
		}
		draw_map(scr, tiles, map, x, y);
		int cx, cy;
		cx = bounded(0, x - scr->width  / 2, map->width  - scr->width);
		cy = bounded(0, y - scr->height / 2, map->height - scr->height);
		draw_tile(scr, sprites, HERO_AVATAR, x - cx, y - cy);

		draw_screen(scr);
		SDL_Delay(150);
	}

	free_map(map);
	free_screen(scr);
	quit();

	return 0;
}
