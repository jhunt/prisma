#include "prisma.h"

struct tileset *
tileset_read(const char *path)
{
	struct tileset *tiles;
	char *p   = NULL;
	FILE *nfo = NULL;
	int rc;

	tiles = allocate(1, sizeof(struct tileset));
	p = astring("%s.png", path);

	tiles->surface = IMG_Load(p);
	if (!tiles->surface) {
		fprintf(stderr, "failed to load tileset image %s: %s (error %d)\n",
				p, strerror(errno), errno);
		goto failed;
	}

	free(p);
	p = astring("%s.nfo", path);

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
	tileset_free(tiles);
	return NULL;
}

void
tileset_free(struct tileset *t)
{
	if (t && t->surface) SDL_FreeSurface(t->surface);
	free(t);
}
