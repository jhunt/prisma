#include "prisma.h"

#include <stdarg.h>

void *
allocate(size_t n, size_t size)
{
	void *p = calloc(n, size);
	if (!p) {
		fprintf(stderr, "failed to allocate memory: %s (error %d)\n",
				strerror(errno), errno);
		exit(EXIT_INT_FAILURE);
	}
	return p;
}

void *
astring(const char *fmt, ...)
{
	char *s;
	va_list ap;
	int rc;

	va_start(ap, fmt);
	rc = vasprintf(&s, fmt, ap);
	va_end(ap);

	if (rc <= 0) {
		fprintf(stderr, "failed to allocate memory: %s (error %d)\n",
				strerror(errno), errno);
		exit(EXIT_INT_FAILURE);
	}

	return s;
}

int
bounded(int min, int v, int max)
{
	if (v < min) return min;
	if (v > max) return max;
	return v;
}
