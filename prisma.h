#ifndef PRISMA_H
#define PRISMA_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* failure encountered while initializing the game,
   i.e. with SDL graphic or audio subsystems, map
   reading, tileset parsing, etc. */
#define EXIT_INIT_FAILED 1

/* failure encountered due to something from the
   environment (missing files, mostly). */
#define EXIT_ENV_FAILURE 2

/* failure encountered in the language runtime,
   like failure to allocate memory when asked. */
#define EXIT_INT_FAILURE 2

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

#endif
