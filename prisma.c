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
#define TOP_WALL     0
#define SIDE_WALL    1
#define BOTTOM_WALL  2
#define TOP_CORNER   3
#define FLOOR        8
#define WHOLE_JAR   27
#define TABLE       54
#define CABINET     55

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

char *
readmap(const char *path)
{
	int fd;
	off_t size;
	char *map;
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

	map = calloc(size + 1, sizeof(char));
	if (!map) {
		fprintf(stderr, "failed to allocate memory: %s (error %d)\n",
				strerror(errno), errno);
		exit(EXIT_INT_FAILURE);
	}

	n = 0;
	for (;;) {
		nread = read(fd, map + n, READ_BLOCK_SIZE);
		if (nread == 0) break;
		if (nread < 0) {
			fprintf(stderr, "failed to read map from %s: %s (error %d)\n",
				path, strerror(errno), errno);
			exit(EXIT_ENV_FAILURE);
		}
		n += nread;
	}

	return map;
}

void
drawmap(SDL_Surface *dst, SDL_Surface *tiles, const char *map)
{
	int i, len, x, y;
	x = y = i = 0;
	len = strlen(map);
	for (; i < len; i++) {
		x++;
		switch (map[i]) {
		case '\n':
			y++;
			x = 0;
			break;

		case '+':
			tile(dst, x, y, tiles, TOP_CORNER);
			break;
		case '-':
			tile(dst, x, y, tiles, TOP_WALL);
			break;
		case '=':
			tile(dst, x, y, tiles, BOTTOM_WALL);
			break;
		case '|':
			tile(dst, x, y, tiles, SIDE_WALL);
			break;

		case 'c':
			tile(dst, x, y, tiles, CABINET);
			break;
		case 't':
			tile(dst, x, y, tiles, TABLE);
			break;

		case ' ':
			tile(dst, x, y, tiles, FLOOR);
			break;
		}
	}
}

int
solid(const char *map, int x, int y)
{
	const char *p;

	/* ran off the beginning of the map */
	if (x < 0 || y < 0) return 1;

	p = map;
	while (p && y--) {
		p = strchr(p, '\n');
		if (!p) return 1;
		p++;
	}
	while (*p && *p != '\n' && --x) p++;

	/* only floors are not solid */
	return p && *p != ' ';
}

#define SCREEN_WIDTH  (16 * 16 * TILE_SCALE)
#define SCREEN_HEIGHT (16 *  6 * TILE_SCALE)

int main(int argc, char **argv)
{
	SDL_Window  *win;
	SDL_Surface *sfc, *tileset;
	SDL_Event    e;
	int x, xd, y, yd, done;
	char *map;

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

				if ((xd || yd) && !solid(map, x + xd, y + yd)) {
					x += xd;
					y += yd;
				}
			}
		}
		drawmap(sfc, tileset, map);
		tile(sfc, x, y, tileset, WHOLE_JAR);

		SDL_UpdateWindowSurface(win);
		SDL_Delay(150);
	}

	SDL_DestroyWindow(win);
	quit();

	return 0;
}
