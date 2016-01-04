/*
 * Copyright 2015 Stephen Street <stephen@redrocketcomputing.com>
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. 
 */
 
#include <stdbool.h>
#include <buddy-alloc.h>

#define BIT_ARRAY_NUM_BITS (8 * sizeof(unsigned long int))
#define BIT_ARRAY_INDEX_SHIFT (__builtin_ctzl(BIT_ARRAY_NUM_BITS))
#define BIT_ARRAY_INDEX_MASK (BIT_ARRAY_NUM_BITS - 1UL)

static inline void bit_array_set(unsigned long int *bit_array, unsigned long int index)
{
	bit_array[index >> BIT_ARRAY_INDEX_SHIFT] |= (1UL << (index & BIT_ARRAY_INDEX_MASK));
}

static inline void bit_array_clear(unsigned long int *bit_array, unsigned long int index)
{
	bit_array[index >> BIT_ARRAY_INDEX_SHIFT] &= ~(1UL << (index & BIT_ARRAY_INDEX_MASK));
}

static inline bool bit_array_is_set(const unsigned long int *bit_array, unsigned long int index)
{
	return (bit_array[index >> BIT_ARRAY_INDEX_SHIFT] & (1UL << (index & BIT_ARRAY_INDEX_MASK))) != 0;
}

static inline void bit_array_not(unsigned long int *bit_array, unsigned long int index)
{
	register unsigned long int array_index = index >> BIT_ARRAY_INDEX_SHIFT;
	register unsigned long int bit_value = (1UL << (index & BIT_ARRAY_INDEX_MASK));

	if (bit_array[array_index] & bit_value)
		bit_array[array_index] &= ~bit_value;
	else
		bit_array[array_index] |= bit_value;
}

static inline void list_init(buddy_block_info_t *list)
{
	list->next = list;
	list->prev = list;
}

static inline void list_add(buddy_block_info_t *list, buddy_block_info_t *node)
{
	node->next = list;
	node->prev = list->prev;
	list->prev->next = node;
	list->prev = node;
}

static inline void list_remove(buddy_block_info_t *list)
{
	list->next->prev = list->prev;
	list->prev->next = list->next;
	list->next = list;
	list->prev = list;
}

static inline bool list_empty(buddy_block_info_t *list)
{
	return list->next == list;
}

static inline buddy_block_info_t *list_pop(buddy_block_info_t *list)
{
	buddy_block_info_t *front = NULL;
	if (!list_empty(list))
	{
		front = list->next;
		list_remove(front);
	}
	return front;
}

static inline unsigned int long size_to_level(const buddy_allocator_t *allocator, size_t size)
{
	/* Floor on the minimum allocation size */
	if (size < allocator->min_allocation)
		return allocator->max_level;

	/* Round up to next power of two */
	size = 1 << (BUDDY_NUM_BITS - __builtin_clzl(size - 1));

	/* Delta between the number of levels and the first set bit in the size */
	/* TODO simplify with size rounding */
	return allocator->total_levels - BUDDY_ILOG2(size);
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

static inline void *to_buddy(const buddy_allocator_t *allocator, const void *ptr, unsigned long int level)
{
#ifdef BUDDY_MEMORY_ALIGNED_ON_SIZE
	return (void *)((unsigned long int)ptr ^ (allocator->size >> level));
#else
	return ((ptr - allocator->address) ^ (allocator->size >> level)) + allocator->address;
#endif
}

static void *buddy_alloc_from_level(buddy_allocator_t *allocator, unsigned long int level)
{
	long int block_at_level;
	long int index;
	buddy_block_info_t *block_ptr = 0;
	buddy_block_info_t *buddy_block_ptr = 0;

	/* Search backwards up the levels */
	for (block_at_level = level; block_at_level >= 0; --block_at_level) {
		if (!list_empty(&allocator->free_blocks[block_at_level])) {
			block_ptr = list_pop(&allocator->free_blocks[block_at_level]);
			break;
		}
	}

	/* Did we find a block? */
	if (!block_ptr)
		return block_ptr;

	/* Calculate the index of the block found */
	index = index_of(allocator, block_ptr, block_at_level);

	/* Do we need to split blocks? */
	if (block_at_level != level) {

		/* Split the block until we reach the requested level */
		while (block_at_level < level) {

			/* Mark block as split */
			bit_array_set(allocator->block_index, split_index(allocator, index));

			/* Mark as allocated, level zero does not use a allocation flags */
			if (block_at_level > 0)
				bit_array_not(allocator->block_index, free_index(allocator, index));

			/* Get the buddy pointer */
			buddy_block_ptr = (buddy_block_info_t *)to_buddy(allocator, block_ptr, block_at_level + 1);

			/* Add right side to the free list */
			list_add(&allocator->free_blocks[block_at_level + 1], buddy_block_ptr);

			/* Adjust index to left child */
			index = (index << 1) + 1;

			/* Adjust to the next level */
			++block_at_level;
		}
	}

	/* Mark as allocated, level zero does not use a allocation flag */
	if (level > 0)
		bit_array_not(allocator->block_index, free_index(allocator, index));

	/* All done */
	return block_ptr;
}

static void buddy_release_at_level(buddy_allocator_t *allocator, void *ptr, unsigned long int level)
{
	void *buddy_ptr = to_buddy(allocator, ptr, level);
	unsigned long int index = index_of(allocator, ptr, level);

	/* Flip the allocation bit, level zero does not use a allocation bit */
	if (level > 0)
		bit_array_not(allocator->block_index, free_index(allocator, index));

	/* Consolidate the blocks */
	while (level > 0 && !bit_array_is_set(allocator->block_index, free_index(allocator, index))) {

		/* Clear the split bit */
		bit_array_clear(allocator->block_index, split_index(allocator, index));

		/* Remove it from the list */
		list_remove(buddy_ptr);

		/* Adjust the index and level */
		index = (index - 1) >> 1;
		--level;

		/* Make sure our we are now using the left buddy */
		if (buddy_ptr < ptr)
			ptr = buddy_ptr;

		/* Get the new buddy */
		buddy_ptr = to_buddy(allocator, ptr, level);

		/* Flip the allocation bit */
		if (level > 0)
			bit_array_not(allocator->block_index, free_index(allocator, index));
	}

	/* Clear the split bit */
	bit_array_clear(allocator->block_index, split_index(allocator, index));

	/* Add combined block to it's free list */
	list_add(&allocator->free_blocks[level], ptr);
}

void *buddy_alloc(buddy_allocator_t *allocator, size_t size)
{
	unsigned long int level = size_to_level(allocator, size);
	return buddy_alloc_from_level(allocator, level);
}

void buddy_release(buddy_allocator_t *allocator, void *ptr, size_t size)
{
	/* Do nothing on null pointer */
	if (!ptr)
		return;

	/* Determine level and release */
	buddy_release_at_level(allocator, ptr, size_to_level(allocator, size));
}

void buddy_free(buddy_allocator_t *allocator, void *ptr)
{
	unsigned long int index;

	/* Do nothing on null pointer */
	if (!ptr)
		return;

	/* Determine level and release */
	index = index_of(allocator, ptr, allocator->max_level);
	for (unsigned long int level = allocator->max_level; level > 0; --level) {
		index = (index - 1) >> 1;
		if (bit_array_is_set(allocator->block_index, split_index(allocator, index))) {
			buddy_release_at_level(allocator, ptr, level);
			return;
		}
	}

	/* Must be allocated from the root */
	buddy_release_at_level(allocator, ptr, 0);
}

void buddy_init(buddy_allocator_t *allocator, void *address, size_t size)
{
	int i;

	/* Initialize allocator setup */
	allocator->address = address;
	allocator->size = size;
	allocator->min_allocation = BUDDY_MIN_LEAF_SIZE;
	allocator->total_levels = BUDDY_ILOG2(allocator->size);
	allocator->max_indexes = BUDDY_MAX_INDEXES(allocator->size, allocator->min_allocation);
	allocator->max_level = BUDDY_MAX_LEVELS(allocator->size, allocator->min_allocation);
	allocator->free_blocks = (void *)allocator + sizeof(buddy_allocator_t);
	allocator->block_index = (void *)allocator + sizeof(buddy_allocator_t) + (sizeof(buddy_block_info_t) * (allocator->max_level + 1));
	allocator->extra_metadata = 0;

	/* Initial the block levels */
	for (i = 0; i < allocator->max_level + 1; ++i)
		list_init(&allocator->free_blocks[i]);

	/* Initialize the clear the block index */
	for (i = 0; i < (allocator->max_indexes + (BUDDY_NUM_BITS - 1)) / BUDDY_NUM_BITS; ++i)
		allocator->block_index[i] = 0;

	/* Add memory at level 0 */
	list_add(&allocator->free_blocks[0], (buddy_block_info_t *)address);
}

buddy_allocator_t *buddy_create(void *address, size_t size)
{
	int i;
	buddy_allocator_t *final_allocator = address;

	/* Setup initial alloctor usig the tailing side of the address range */
	size_t metadata_size = buddy_sizeof_metadata(size, BUDDY_MIN_LEAF_SIZE);
	buddy_allocator_t *initial_allocator = (buddy_allocator_t *)((address + size) - metadata_size);

	/* Setup the initial allocator */
	buddy_init(initial_allocator, address, size);

	/* Allocate enough min sized block to cover the meta data, we do not need the actual pointers for anything */
	initial_allocator->extra_metadata = (void *)-1;
	for (i = 0; i < (metadata_size + (BUDDY_MIN_LEAF_SIZE - 1)) / BUDDY_MIN_LEAF_SIZE; ++i)
		(void)buddy_alloc_from_level(initial_allocator, initial_allocator->max_level);

	/* Move the initial allocator into place */
	final_allocator = address;
	final_allocator->address = address;
	final_allocator->size = initial_allocator->size;
	final_allocator->min_allocation = initial_allocator->min_allocation;
	final_allocator->max_indexes = initial_allocator->max_indexes;
	final_allocator->total_levels = initial_allocator->total_levels;
	final_allocator->max_level = initial_allocator->max_level;
	final_allocator->free_blocks = address + sizeof(buddy_allocator_t);
	final_allocator->block_index = address +  sizeof(buddy_allocator_t) + (sizeof(buddy_block_info_t) * (final_allocator->max_level + 1));
	final_allocator->extra_metadata = 0;

	/* Copy the block indexes */
	for (i = 0; i < (final_allocator->max_indexes + (BUDDY_NUM_BITS - 1)) / BUDDY_NUM_BITS; ++i)
		final_allocator->block_index[i] = initial_allocator->block_index[i];

	/* Relink the free blocks */
	for (int level = 0; level < initial_allocator->max_level + 1; ++level) {

		/* Initialize the final allocator free block list for the current level */
		list_init(&final_allocator->free_blocks[level]);

		/* For non-empty free block lists in the initial allocator, rewrite the head and tail pointers */
		if (!list_empty(&initial_allocator->free_blocks[level])) {

			final_allocator->free_blocks[level].next = initial_allocator->free_blocks[level].next;
			final_allocator->free_blocks[level].prev = initial_allocator->free_blocks[level].prev;

			final_allocator->free_blocks[level].next->prev = &final_allocator->free_blocks[level];
			final_allocator->free_blocks[level].prev->next = &final_allocator->free_blocks[level];
		}
	}

	/* All done */
	return final_allocator;
}

size_t buddy_largest_available(const buddy_allocator_t *allocator)
{
	/* Find first level with a block available */
	for (unsigned long int level = 0; level < allocator->max_level + 1; ++level)
		if (!list_empty(&allocator->free_blocks[level]))
			return allocator->size >> level;

	/* No blocks available */
	return 0;
}

size_t buddy_available(const buddy_allocator_t *allocator)
{
	size_t available = 0;
	int blocks_available;

	for (unsigned long int level = 0; level < allocator->max_level + 1; ++level) {
		blocks_available = 0;
		for (buddy_block_info_t *cursor = allocator->free_blocks[level].next; cursor != &allocator->free_blocks[level]; cursor = cursor->next)
			++blocks_available;
		available += blocks_available << (allocator->total_levels - level);
	}

	return available;
}

size_t buddy_used(const buddy_allocator_t *allocator)
{
	return allocator->size - buddy_available(allocator);
}
