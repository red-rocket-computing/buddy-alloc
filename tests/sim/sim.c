/*
 * Copyright 2015 Stephen Street <stephen@redrocketcomputing.com>
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. 
 */
 
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <buddy-alloc.h>

#define SIM_MEMORY_SIZE (1024 * 1024)
#define SIM_MAX_TIME 10000000

#define SIM_RAND_SEED 0x01371730UL

#define SIM_MAX_ALLOC_SIZE (100 * 1024)
#define SIM_MAX_DELAY (5)

static unsigned long int memory[SIM_MEMORY_SIZE / sizeof(unsigned long int)];
static buddy_allocator_t *allocator;

typedef struct sim_data {
	int mark;
	int expire;
	int size;
	struct sim_data *next;
	unsigned char data[];
} sim_data_t;

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

int main(int argc, char **argv)
{
	int mark;
	int new_size;
	int new_delay;
	sim_data_t *datum;
	sim_data_t *sim_data[SIM_MAX_DELAY];
	int sim_data_mark;
	int outstanding = 0;

	printf("simulator testing of allocator: %lu overhead %1.2f%%\n", buddy_sizeof_metadata(SIM_MEMORY_SIZE, BUDDY_MIN_LEAF_SIZE), ((buddy_sizeof_metadata(SIM_MEMORY_SIZE, BUDDY_MIN_LEAF_SIZE) * 1.0) / SIM_MEMORY_SIZE) * 100);
	allocator = buddy_create(memory, SIM_MEMORY_SIZE);
	buddy_dump_info(allocator);

	srand(SIM_RAND_SEED + time(0));
	memset(sim_data, 0, sizeof(sim_data));

	/* Run the simulation */
	for (mark = 0; mark < SIM_MAX_TIME; ++mark) {

		new_size = sizeof(sim_data_t) + (rand() % (SIM_MAX_ALLOC_SIZE - sizeof(sim_data_t)));
		new_delay = (rand() % SIM_MAX_DELAY);

		sim_data_mark = mark % SIM_MAX_DELAY;

		datum = buddy_alloc(allocator, new_size);
		if (datum) {
			++outstanding;
			datum->mark = mark;
			datum->expire = mark + new_delay;
			datum->size = new_size;
			datum->next = sim_data[(sim_data_mark + new_delay) % SIM_MAX_DELAY];
			sim_data[(sim_data_mark + new_delay) % SIM_MAX_DELAY] = datum;
		} else
			printf("fail at %u for size %d with %zu available and largest %zu\n", mark, new_size, buddy_available(allocator), buddy_largest_available(allocator));

		while ((datum = sim_data[sim_data_mark]) != 0) {
			sim_data[sim_data_mark] = sim_data[sim_data_mark]->next;
			buddy_free(allocator, datum);
			--outstanding;
		}
	}

	for (int i = 0; i < SIM_MAX_DELAY; ++i) {
		while ((datum = sim_data[i]) != 0) {
			sim_data[i] = sim_data[i]->next;
			buddy_free(allocator, datum);
			--outstanding;
		}
	}

	printf("lost: %zu bytes, outstanding: %d\n", buddy_used(allocator), outstanding);

	return 0;
}
