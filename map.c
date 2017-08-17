#include "prisma.h"

#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>

/* upper limit of 8MiB on map size */
#define MAX_MAP_SIZE (1024 * 1024 * 8)
#define READ_BLOCK_SIZE 8192

#define T_EOF        0
#define T_KW_MAP     1
#define T_KW_TILESET 2
#define T_KW_DEFAULT 3
#define T_KW_EMPTY   4
#define T_KW_TILE    5
#define T_KW_SOLID   6
#define T_KW_VOID    7
#define T_KW_PLACE   8
#define T_KW_FROM    9
#define T_KW_ENTRY  10
#define T_STRING   128
#define T_NUMBER   129
#define T_SYMBOL   130
#define T_ERROR    131

#define T_ERROR_UNTERMINATED_STRING 1

struct mapobj {
	char symbol;
	struct {
		int x;
		int y;
	} at;
};

struct mapkey {
	char *name;
	char *tileset;

	struct coords entry;
	int   default_tile;
	char  void_tile;
	int   tiles[256];

	int next_object;
	struct mapobj objects[256];
};

struct parser {
	int     fd;
	size_t  len;

	const char *file;
	int         line;
	int         column;

	size_t  there;
	size_t  here;
	char   *source;

	union {
		char *string;
		int   number;
		char  symbol;
		int   error;
	} data;
};

static struct map *    s_parse_map(const char *, struct mapkey *);
static struct mapkey * s_parse_mapkey(const char *);
static int             s_lexer(struct parser *);

static char * s_readmap(const char *path);
static void s_mapsize(const char *raw, int *w, int *h);


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
	if (m) {
		free(m->cells[0]);
		free(m->cells[1]);
	}
	free(m);
}

static struct map *
s_parse_map(const char *path, struct mapkey *key)
{
	char *raw, *p;
	struct map *map;
	int i, x, y;

	raw = s_readmap(path);
	map = allocate(1, sizeof(struct map));
	map->tiles = tileset_read(key->tileset);
	if (!map->tiles) {
		fprintf(stderr, "TILE READ FAILED\n");
	}
	s_mapsize(raw, &map->width, &map->height);
	map->cells[0] = calloc(map->width * map->height, sizeof(int));
	map->cells[1] = calloc(map->width * map->height, sizeof(int));
	map->entry.x = key->entry.x;
	map->entry.y = key->entry.y;

	/* decode the newline-terminated map into a cell-list */
	for (x = y = 0, p = raw; *p; p++) {
		if (*p == '\n') {
			x = 0; y++;
			continue;
		}

		if (*p == key->void_tile) {
			mapat(map, 0, x++, y) = 0x00; /* NONE */
		} else {
			mapat(map, 0, x++, y) = key->tiles[(int)*p]
			                   ? key->tiles[(int)*p]
			                   : key->default_tile;
		}
	}

	for (i = 0; i < key->next_object; i++) {
		mapat(map, 1, key->objects[i].at.x,
		              key->objects[i].at.y) = key->tiles[(int)key->objects[i].symbol]
		                                    ? key->tiles[(int)key->objects[i].symbol]
		                                    : 0x00; /* NONE */
	}

	free(raw);
	return map;
}

static struct mapkey *
s_parse_mapkey(const char *path)
{
	struct parser p;
	struct mapkey *m;
	off_t len;
	int token, solid, idx;
	int x, y;

	p.file = path;
	p.fd = open(p.file, O_RDONLY);
	if (p.fd < 0) goto fail;

	len = lseek(p.fd, 0, SEEK_END);
	if (len < 0) goto fail;
	lseek(p.fd, 0, SEEK_SET);

	p.len = len;
	p.source = mmap(NULL, len, PROT_READ, MAP_PRIVATE, p.fd, 0);
	if (p.source == MAP_FAILED) goto fail;

	p.line = 1;
	p.column = 1;
	p.here = p.there = 0;
	m = allocate(1, sizeof(*m));

	for (;;) {
		token = s_lexer(&p);
		if (token == T_EOF) break;

		switch (token) {
		case T_KW_MAP:
			token = s_lexer(&p);
			if (token != T_STRING) {
				fprintf(stderr, "%s:%d:%d: ", p.file, p.line, p.column);
				fprintf(stderr, "The `map' keyword MUST be followed by the name of the map (as a string)\n");
				goto fail;
			}
			free(m->name);
			m->name = p.data.string;
			break;

		case T_KW_TILESET:
			token = s_lexer(&p);
			if (token != T_STRING) {
				fprintf(stderr, "%s:%d:%d: ", p.file, p.line, p.column);
				fprintf(stderr, "The `tileset' keyword MUST be followed by the path to the tileset (as a string)\n");
				goto fail;
			}
			free(m->tileset);
			m->tileset = p.data.string;
			break;

		case T_KW_DEFAULT:
			token = s_lexer(&p);
			if (token != T_NUMBER) {
				fprintf(stderr, "%s:%d:%d: ", p.file, p.line, p.column);
				fprintf(stderr, "The `default' keyword MUST be followed by a tile index number\n");
				goto fail;
			}
			m->default_tile = ((1 + p.data.number) << 24);
			break;

		case T_KW_VOID:
			token = s_lexer(&p);
			if (token != T_SYMBOL) {
				fprintf(stderr, "%s:%d:%d: ", p.file, p.line, p.column);
				fprintf(stderr, "The `void' keyword MUST be followed by a tile symbol (a single character)\n");
				goto fail;
			}
			m->void_tile = p.data.symbol;
			break;

		case T_KW_TILE:
			token = s_lexer(&p);
			switch (token) {
			case T_KW_SOLID: solid = 1; break;
			case T_KW_EMPTY: solid = 0; break;
			default:
				fprintf(stderr, "%s:%d:%d: ", p.file, p.line, p.column);
				fprintf(stderr, "The `tile` keyword MUST be followed by either the `solid' keyword or the `empty' keyword\n");
				goto fail;
			}

			token = s_lexer(&p);
			if (token != T_SYMBOL) {
				fprintf(stderr, "%s:%d:%d: ", p.file, p.line, p.column);
				fprintf(stderr, "Tiles must be defined `tile (solid|empty) SYMBOL INDEX', where SYMBOL is the tile symbol (a single character)\n");
				goto fail;
			}
			idx = (int)(p.data.symbol);

			token = s_lexer(&p);
			if (token != T_NUMBER) {
				fprintf(stderr, "%s:%d:%d: ", p.file, p.line, p.column);
				fprintf(stderr, "Tiles must be defined `tile (solid|empty) SYMBOL INDEX', where INDEX is the tile index (a number)\n");
				goto fail;
			}
			m->tiles[idx] = solid ? ((1 + p.data.number) << 24) | 0x01
			                      : ((1 + p.data.number) << 24);
			break;

		case T_KW_FROM:
			token = s_lexer(&p);
			if (token != T_NUMBER) {
				fprintf(stderr, "%s:%d:%d: ", p.file, p.line, p.column);
				fprintf(stderr, "The `from' keyword requires both an X and Y coordinate, as numbers\n");
				goto fail;
			}
			x = p.data.number;

			token = s_lexer(&p);
			if (token != T_NUMBER) {
				fprintf(stderr, "%s:%d:%d: ", p.file, p.line, p.column);
				fprintf(stderr, "The `from' keyword requires both an X and Y coordinate, as numbers\n");
				goto fail;
			}
			y = p.data.number;
			break;

		case T_KW_PLACE:
			token = s_lexer(&p);
			if (token != T_SYMBOL) {
				fprintf(stderr, "%s:%d:%d: ", p.file, p.line, p.column);
				fprintf(stderr, "The `place' keyword requires a symbol, and X + Y coordinates\n");
				goto fail;
			}
			m->objects[m->next_object].symbol = p.data.symbol;

			token = s_lexer(&p);
			if (token != T_NUMBER) {
				fprintf(stderr, "%s:%d:%d: ", p.file, p.line, p.column);
				fprintf(stderr, "The `place' keyword requires a symbol, and X + Y coordinates\n");
				goto fail;
			}
			m->objects[m->next_object].at.x = p.data.number + x;

			token = s_lexer(&p);
			if (token != T_NUMBER) {
				fprintf(stderr, "%s:%d:%d: ", p.file, p.line, p.column);
				fprintf(stderr, "The `place' keyword requires a symbol, and X + Y coordinates\n");
				goto fail;
			}
			m->objects[m->next_object].at.y = p.data.number + y;
			m->next_object++;
			break;

		case T_KW_ENTRY:
			token = s_lexer(&p);
			if (token != T_NUMBER) {
				fprintf(stderr, "%s:%d:%d: ", p.file, p.line, p.column);
				fprintf(stderr, "The `entry' keyword requires both an X and Y coordinate, as numbers\n");
			}
			m->entry.x = p.data.number + x;

			token = s_lexer(&p);
			if (token != T_NUMBER) {
				fprintf(stderr, "%s:%d:%d: ", p.file, p.line, p.column);
				fprintf(stderr, "The `entry' keyword requires both an X and Y coordinate, as numbers\n");
			}
			m->entry.y = p.data.number + y;

			break;

		case T_KW_EMPTY:
			fprintf(stderr, "%s:%d:%d: ", p.file, p.line, p.column);
			fprintf(stderr, "Unexpected `empty' keyword found (`empty' MUST follow `tile')\n");
			goto fail;

		case T_KW_SOLID:
			fprintf(stderr, "%s:%d:%d: ", p.file, p.line, p.column);
			fprintf(stderr, "Unexpected `solid' keyword found (`solid' MUST follow `tile')\n");
			goto fail;

		case T_STRING:
			fprintf(stderr, "%s:%d:%d: ", p.file, p.line, p.column);
			fprintf(stderr, "Unexpected string \"%s\" found.\n", p.data.string);
			free(p.data.string);
			goto fail;

		case T_NUMBER:
			fprintf(stderr, "%s:%d:%d: ", p.file, p.line, p.column);
			fprintf(stderr, "Unexpected number '%d' found.\n", p.data.number);
			goto fail;

		case T_SYMBOL:
			fprintf(stderr, "%s:%d:%d: ", p.file, p.line, p.column);
			fprintf(stderr, "Unexpected symbol '%c' found.\n", p.data.symbol);
			goto fail;

		case T_ERROR:
			fprintf(stderr, "%s:%d:%d: ", p.file, p.line, p.column);
			fprintf(stderr, "ERROR: %d\n", p.data.error);
			goto fail;

		default:
			fprintf(stderr, "%s:%d:%d: ", p.file, p.line, p.column);
			fprintf(stderr, "UNKNOWN TOKEN %d\n", token);
			goto fail;
		}
	}

	return m;

fail:
	fprintf(stderr, "failed: %s (error %d)\ncleaning up...\n",
		strerror(errno), errno);
	if (p.source && p.source != MAP_FAILED)
		munmap(p.source, p.len);
	if (p.fd >= 0)
		close(p.fd);
	return NULL;
}

#define s_char(p)   ((p)->source[(p)->here])
#define s_space(p)  (isspace(s_char(p)))
#define s_number(p) (isdigit(s_char(p)))
#define s_graph(p)  (isgraph(s_char(p)))

#define s_done(p) ((p)->here == (p)->len)
#define s_keyword(p,w) (memcmp((p)->source + (p)->there, w, strlen(w)) == 0)

static inline void
s_next(struct parser *p) {
	//fprintf(stderr, "p[%lu] = '%c' (%d)\n", p->here, p->source[p->here], p->source[p->here]);

	if (s_char(p) == '\n') {
		p->line++;
		p->column = 0;
	}
	p->here++;
	p->column++;
}

static inline void
s_skip(struct parser *p) {
	s_next(p);
	p->there++;
}

static inline char
s_peek(struct parser *p)
{
	if (s_done(p)) return '\n';
	return p->source[p->here+1];
}

static inline void
s_stringat(struct parser *p, int a, int b)
{
	p->data.string = allocate(b - a + 2, sizeof(char));
	memcpy(p->data.string, p->source + a, b - a + 1);
}

static inline void
s_string(struct parser *p)
{
	s_stringat(p, p->there, p->here);
}

static inline void
s_qstring(struct parser *p)
{
	char *a, *b;

	s_stringat(p, p->there + 1, p->here - 1);

	/* collapse backslash-escape sequences */
	a = b = p->data.string;
	while (*b) {
		if (*b == '\\') b++;
		*a++ = *b++;
	}
	*a = '\0';
}

static int
s_lexer(struct parser *p)
{
	if (s_done(p)) return T_EOF;
	p->there = p->here;

	for (;;) {
again:
		/* skip comments to end of line */
		if (s_char(p) == ';' && s_peek(p) == ';') {
			while (s_char(p) != '\n') s_skip(p);
			s_skip(p);
			if (s_done(p)) return T_EOF;
			goto again;
		}

		/* skip leading whitespace */
		while (s_space(p)) {
			s_skip(p);
			if (s_done(p)) return T_EOF;
			goto again;
		}

		if (s_number(p)) {
			p->data.number = 0;
			while (!s_done(p) && s_number(p)) {
				p->data.number = (p->data.number * 10)
				               + (s_char(p) - '0');
				s_next(p);
			}
			return T_NUMBER;
		}

		if (s_char(p) == '"') {
			p->data.error = T_ERROR_UNTERMINATED_STRING;
			s_next(p);
			if (s_done(p)) return T_ERROR;
			while (s_char(p) != '"') {
				if (s_char(p) == '\\') {
					s_next(p);
					if (s_done(p)) return T_ERROR;
				}
				s_next(p);
			}
			s_qstring(p);
			s_next(p);
			return T_STRING;
		}
		if (s_graph(p)) {
			if (isspace(s_peek(p))) {
				p->data.symbol = s_char(p);
				s_next(p);
				return T_SYMBOL;
			}

			while (isgraph(s_peek(p)))
				s_next(p);

			if (s_keyword(p, "tileset")) { s_next(p); return T_KW_TILESET; }
			if (s_keyword(p, "default")) { s_next(p); return T_KW_DEFAULT; }
			if (s_keyword(p, "empty"))   { s_next(p); return T_KW_EMPTY;   }
			if (s_keyword(p, "solid"))   { s_next(p); return T_KW_SOLID;   }
			if (s_keyword(p, "place"))   { s_next(p); return T_KW_PLACE;   }
			if (s_keyword(p, "entry"))   { s_next(p); return T_KW_ENTRY;   }
			if (s_keyword(p, "from"))    { s_next(p); return T_KW_FROM;    }
			if (s_keyword(p, "tile"))    { s_next(p); return T_KW_TILE;    }
			if (s_keyword(p, "void"))    { s_next(p); return T_KW_VOID;    }
			if (s_keyword(p, "map"))     { s_next(p); return T_KW_MAP;     }

			s_string(p);
			s_next(p);
			return T_STRING;
		}
	}
	return T_EOF;
}

struct map *
map_read(const char *path)
{
	char *p;
	struct mapkey *key;

	p = astring("%s.mf", path);
	key = s_parse_mapkey(p);
	free(p);
	if (!key) return NULL;

	return s_parse_map(path, key);
}
