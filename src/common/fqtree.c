/**
 * fqtree.c
 *
 * Copyright (c) 2013
 *	libchewing Core Team. See ChangeLog for details.
 *
 * See the file "COPYING" for information on usage and redistribution
 * of this file.
 */

#include <assert.h>
#include <stdio.h>

#include "fqtree.h"
#include "chewing-private.h"

int serialize_fqforest(struct FQForest *forest, const char *filename)
{
	FILE *fd;
	uint32_t current;
	int i;

	assert(forest);

	fd = fopen(filename, "w");
	if (!fd) {
		return -1;
	}

	fseek(fd, MAX_FQTREE_PHRASE_LEN * sizeof(uint32_t), SEEK_SET);
	for (i = 0; i < MAX_FQTREE_PHRASE_LEN; ++i) {
		current = ftell(fd);
		pwrite(fd, &current, sizeof(current), i * sizeof(uint32_t));
	}

	fclose(fd);
}
