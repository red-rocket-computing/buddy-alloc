/*
 * Copyright 2015 Stephen Street <stephen@redrocketcomputing.com>
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. 
 */
 
#include <stdio.h>

#include <buddy-alloc.h>

#define BUDDY_MEMORY_SIZE 512
#define BIT_ARRAY_NUM_BITS (8 * sizeof(unsigned long int))
#define BIT_ARRAY_INDEX_SHIFT (__builtin_ctzl(BIT_ARRAY_NUM_BITS))
#define BIT_ARRAY_INDEX_MASK (BIT_ARRAY_NUM_BITS - 1UL)

static unsigned char __attribute__((aligned(BUDDY_MEMORY_SIZE))) memory[BUDDY_MEMORY_SIZE];

static unsigned long int metadata[buddy_sizeof_metadata(BUDDY_MEMORY_SIZE, BUDDY_MIN_LEAF_SIZE) / sizeof(unsigned long)];
static buddy_allocator_t *global_allocator = (buddy_allocator_t *)metadata;

static inline unsigned long int bta_first_of_level(unsigned long int level)
{
	return (1 << level) - 1;
}

static inline unsigned long int bta_last_of_level(unsigned long int level)
{
	return (1 << (level + 1)) - 1;
}

static inline bool bit_array_is_set(const unsigned long int *bit_array, unsigned long int index)
{
	return (bit_array[index >> BIT_ARRAY_INDEX_SHIFT] & (1UL << (index & BIT_ARRAY_INDEX_MASK))) != 0;
}

static inline unsigned long int index_of(const buddy_allocator_t *allocator, const void *ptr, unsigned long int level)
{
	return (1UL << level) + ((ptr - allocator->address) >> (allocator->total_levels - level)) - 1UL;
}

static inline unsigned long int free_index(const buddy_allocator_t *allocator, unsigned long int index)
{
	return (index - 1) >> 1;
}

static inline unsigned long int split_index(const buddy_allocator_t *allocator, unsigned long int index)
{
	return index + (allocator->max_indexes >> 1);
}

static void buddy_dump_free_blocks(const buddy_allocator_t *allocator)
{
	char buffer[128];

	/* Dump the free lists */
	printf("free blocks:\n");
	for (unsigned long int level = 0; level < allocator->max_level + 1; ++level) {
		printf(buffer, "\t%6zu: ", allocator->size >> level);
		for (buddy_block_info_t *cursor = allocator->free_blocks[level].next; cursor != &allocator->free_blocks[level]; cursor = cursor->next) {
			printf(buffer, "%p(%lu) ", cursor, index_of(allocator, cursor, level));
		}
		printf("\n");
	}
	printf("\n");
}

static void buddy_dump_free_index(const buddy_allocator_t *allocator)
{
	char buffer[128];

	printf("free index:\n");

	/* Loop through the levels */
	for (unsigned long int level = 0; level < allocator->max_level; ++level) {

		/* Loop through the indexes at this level */
		printf(buffer, "\t%6u - %4lu:%-4lu: ", allocator->size >> (level + 1), bta_first_of_level(level + 1), bta_last_of_level(level + 1) - 1);
		for (unsigned long int index = bta_first_of_level(level); index < bta_last_of_level(level); ++index)
			printf(bit_array_is_set(allocator->block_index, index) ? "1" : "0");
		printf("\n");
	}
	printf("\n");
}

static void buddy_dump_split_index(const buddy_allocator_t *allocator)
{
	char buffer[128];

	printf("split index:\n");

	/* Loop through the levels */
	for (unsigned long int level = 0; level < allocator->max_level; ++level) {

		/* Loop through the indexes at this level */
		printf(buffer, "\t%6u - %4lu:%-4lu: ", allocator->size >> level, bta_first_of_level(level), bta_last_of_level(level) - 1);
		for (unsigned long int index = bta_first_of_level(level); index < bta_last_of_level(level); ++index)
			printf(bit_array_is_set(allocator->block_index, split_index(allocator, index)) ? "1" : "0");
		printf("\n");
	}
	printf("\n");
}

static void buddy_dump_info(const buddy_allocator_t *allocator)
{
	char buffer[128];

	printf(buffer, "allocator @ %p\n", allocator);
	printf(buffer, "\taddress:        %p\n", allocator->address);
	printf(buffer, "\tsize:           %zu\n", allocator->size);
	printf(buffer, "\ttotal levels:   %lu\n", allocator->total_levels);
	printf(buffer, "\tmax level:      %lu\n", allocator->max_level);
	printf(buffer, "\tmax allocation: %zu\n", allocator->size);
	printf(buffer, "\tmin allocation: %lu\n", allocator->min_allocation);
	printf(buffer, "\tmax indexes:    %lu\n", allocator->max_indexes);
	printf(buffer, "\tavailable:      %zu\n", buddy_available(allocator));
	printf(buffer, "\tused:           %zu\n", buddy_used(allocator));
	printf(buffer, "\tmax available:  %zu\n", buddy_largest_available(allocator));
}

static void buddy_dump_allocator(const buddy_allocator_t *allocator)
{
	buddy_dump_info(allocator);
	buddy_dump_free_blocks(allocator);
	buddy_dump_split_index(allocator);
	buddy_dump_free_index(allocator);
}

int main(int argc, char **argv)
{
	printf("test external metadata allocator\n");
	buddy_init(global_allocator, memory, BUDDY_MEMORY_SIZE);
	printf("starting state\n");
	buddy_dump_allocator(global_allocator);

	void *ptr_1 = buddy_alloc(global_allocator, 32);
	void *ptr_2 = buddy_alloc(global_allocator, 31);
	void *ptr_3 = buddy_alloc(global_allocator, 33);
	void *ptr_4 = buddy_alloc(global_allocator, 8);

	printf("outstanding blocks\n");
	buddy_dump_allocator(global_allocator);

	buddy_free(global_allocator, ptr_3);
	buddy_free(global_allocator, ptr_2);
	buddy_free(global_allocator, ptr_1);
	buddy_free(global_allocator, ptr_4);
	printf("terminating state\n");
	buddy_dump_allocator(global_allocator);

	printf("testing block sized allocations\n");
	void *ptrs[6];
	for (int i = 1; i < 6; ++i) {
		ptrs[i] = buddy_alloc(global_allocator, BUDDY_MEMORY_SIZE >> i);
	}
	for (int i = 1; i < 6; ++i) {
		buddy_free(global_allocator, ptrs[i]);
	}
	printf("terminating state\n");
	buddy_dump_allocator(global_allocator);

	printf("testing internal metadata allocator\n");
	buddy_allocator_t *allocator = buddy_create(memory, BUDDY_MEMORY_SIZE);
	printf("starting state\n");
	buddy_dump_allocator(allocator);

	ptr_1 = buddy_alloc(allocator, 32);
	ptr_2 = buddy_alloc(allocator, 31);
	ptr_3 = buddy_alloc(allocator, 33);
	ptr_4 = buddy_alloc(allocator, 8);
	printf("outstanding blocks\n");
	buddy_dump_allocator(allocator);

	buddy_free(allocator, ptr_3);
	buddy_free(allocator, ptr_2);
	buddy_free(allocator, ptr_1);
	buddy_free(allocator, ptr_4);
	printf("terminating state\n");
	buddy_dump_allocator(allocator);

	return 0;
}
