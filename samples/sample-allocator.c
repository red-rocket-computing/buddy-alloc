/*
 * Copyright 2015 Stephen Street <stephen@redrocketcomputing.com>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <buddy-alloc.h>

#define MEMORY_REGION_SIZE 1048576

static unsigned long int memory_region[MEMORY_REGION_SIZE / sizeof(unsigned long int)];

int main(int argc, char **argv)
{
	/* Declare allocator for external metadata */
	BUDDY_DECLARE_ALLOCATOR(allocator, MEMORY_REGION_SIZE);
	void *ptr;

	/* Create an allocate with external metadata */
	buddy_init(allocator, memory_region, MEMORY_REGION_SIZE);

	/* This will allocate a block 16K in size */
	ptr = buddy_alloc(allocator, 13773);

	/* This will determine the size of the memory block, by searching the
	 free index at each level, somewhat slower */
	buddy_free(allocator, ptr);

	/* This will allocate a block 16K in size */
	ptr = buddy_alloc(allocator, 13773);

	/* This frees the memory block, using the provided size, a little
	 faster as the block level can be derived from the size */
	buddy_release(allocator, ptr, 13773);

	/* All done */
	return 0;
}
