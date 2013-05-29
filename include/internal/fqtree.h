/**
 * fqtree.h
 *
 * Copyright (c) 2013
 *	libchewing Core Team. See ChangeLog for details.
 *
 * See the file "COPYING" for information on usage and redistribution
 * of this file.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef HAVE_INTTYPES_H
#  include <inttypes.h>
#elif defined HAVE_STDINT_H
#  include <stdint.h>
#endif

#include "chewing-private.h"

#define MAX_FQTREE_PHRASE_LEN (6)

struct FQTreeNode {
	enum { INTERNAL_NODE, LEAF_NODE } type;
	union {
		struct {
			uint32_t children[MAX_FQTREE_PHRASE_LEN * ZUIN_SIZE + 1];
		} internal;
		struct {
			uint32_t phrase_index[MAX_FQTREE_PHRASE_LEN * ZUIN_SIZE + 1];
		} leaf;
	};
};

struct FQTree {
	uint32_t root;
	uint32_t depth;
	uint32_t key_phrase_index[];
};

struct FQForest {
	struct FQTree *fqtree[MAX_FQTREE_PHRASE_LEN];
};
