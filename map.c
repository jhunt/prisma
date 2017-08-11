#include "prisma.h"

#include <unistd.h>
#include <fcntl.h>

/* upper limit of 8MiB on map size */
#define MAX_MAP_SIZE (1024 * 1024 * 8)
#define READ_BLOCK_SIZE 8192


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

static int inmap(struct map *map, int x, int y);
static int istile(int t);
static char * s_readmap(const char *path);
static void s_mapsize(const char *raw, int *w, int *h);


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

static char *
s_readmap(const char *path)
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

	raw = allocate(size + 1, sizeof(char));
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

void
map_free(struct map *m)
{
	if (m) free(m->cells);
	free(m);
}

struct map *
map_read(const char *path)
{
	char *raw, *p;
	struct map *map;
	int x, y;

	raw = s_readmap(path);
	map = allocate(1, sizeof(struct map));
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

void
map_draw(struct map *map, struct screen *scr)
{
	assert(scr != NULL);
	assert(map != NULL);
	assert(map->tiles != NULL);

	int x, y;
	int cx = bounded(0, scr->x - scr->width  / 2, map->width  - scr->width);
	int cy = bounded(0, scr->y - scr->height / 2, map->height - scr->height);

	for (x = 0; x < scr->width; x++) {
		for (y = 0; y < scr->height; y++) {
			if (inmap(map, x+cx, y+cy)) {
				int t = mapat(map, x+cx, y+cy);
				if (istile(t)) {
					draw_tile(scr, map->tiles, (t >> 24) - 1, x, y);
				} else {
					draw_tile(scr, map->tiles, 22, x, y);
				}
			} else {
				draw_tile(scr, map->tiles, 22, x, y);
			}
		}
	}
}

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
