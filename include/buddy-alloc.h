/*
 * Copyright 2015 Stephen Street <stephen@redrocketcomputing.com>
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. 
 */

#ifndef BUDDY_ALLOC_H_
#define BUDDY_ALLOC_H_

#include <stddef.h>
#include <stdbool.h>

/* Uncomment this if the memory region for the allocator is aligned on a address boundary equal to the size to
 * enable a minor optimization when calculating the address of the buddy.
#define BUDDY_MEMORY_ALIGNED_ON_SIZE
*/

#define BUDDY_ILOG2(value) ((BUDDY_NUM_BITS - 1UL) - __builtin_clzl(value))
#define BUDDY_NUM_BITS (8 * sizeof(unsigned long int))
#define BUDDY_MIN_LEAF_SIZE (sizeof(void *) << 1UL)
#define BUDDY_LEAF_LEVEL_OFFSET (BUDDY_ILOG2(BUDDY_MIN_LEAF_SIZE))
#define BUDDY_MAX_LEVELS(total_size, min_size) (BUDDY_ILOG2(total_size) - BUDDY_ILOG2(min_size))
#define BUDDY_MAX_INDEXES(total_size, min_size) (1UL << (BUDDY_MAX_LEVELS(total_size, min_size) + 1))
#define BUDDY_BLOCK_INDEX_SIZE(total_size, min_size) ((BUDDY_MAX_INDEXES(total_size, min_size) + (BUDDY_NUM_BITS - 1)) / BUDDY_NUM_BITS)

typedef struct buddy_block_info
{
	struct buddy_block_info *next;
	struct buddy_block_info *prev;
} buddy_block_info_t;

typedef struct buddy_allocator
{
	void *address;
	size_t size;
	unsigned long int min_allocation;
	unsigned long int max_indexes;
	unsigned long int total_levels;
	unsigned long int max_level;
	buddy_block_info_t *free_blocks;
	unsigned long int *block_index;
	void *extra_metadata;
} buddy_allocator_t;

#define buddy_sizeof_metadata(total_size, min_size) (sizeof(buddy_allocator_t) + \
                                                    (sizeof(buddy_block_info_t) * (BUDDY_MAX_LEVELS(total_size, min_size) + 1)) + \
                                                    BUDDY_BLOCK_INDEX_SIZE(total_size, min_size) * (BUDDY_NUM_BITS >> 3))

#define BUDDY_DECLARE_ALLOCATOR(name, size) unsigned long int name ## _metadata[buddy_sizeof_metadata(size, BUDDY_MIN_LEAF_SIZE)/ sizeof(unsigned long int)]; \
											buddy_allocator_t * name = (buddy_allocator_t *)name ## _metadata


void buddy_init(buddy_allocator_t *allocator, void *address, size_t size);
buddy_allocator_t *buddy_create(void *address, size_t size);
void *buddy_alloc(buddy_allocator_t *allocator, size_t size);
void buddy_release(buddy_allocator_t *allocator, void *ptr, size_t size);
void buddy_free(buddy_allocator_t *allocator, void *ptr);

size_t buddy_largest_available(const buddy_allocator_t *allocator);
size_t buddy_available(const buddy_allocator_t *allocator);
size_t buddy_used(const buddy_allocator_t *allocator);

#endif /* BUDDY_ALLOC_H_ */
