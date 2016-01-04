
Buddy Allocator Math:
=========

	memory_size = size in bytes of power of 2 memory area
	memory_address = address of memory aligned on a boundary of it size
	pointer_size = sizeof(void *)
	x * (2 ^ y) ==> x << y
	x / (2 ^ y) ==> x >> y
	number_bits =  8 * sizeof(pointer_size)
	bits_base = log2(number_of_bits)
	leaf_size = 2 * pointer_size
	total_levels = log2(memory_size)
	max_levels = log2(memory_size) - log2(leaf_size)
	max_indexes = 
	    (2 ^ max_levels) - 1 => 
	    (1 << max_levels) - 1 or (total_size / leaf_size) - 1
	block_indexes = ((max_indexes / number_of_bits) / 2) + 1
	total_size = 
	    2 ^ total_levels =>
	    (2 ^ max_level) * leaf_size
	size_of_level(level) = 
	    total_size / (2 ^ level) => 
	    total_size >> level
	max_blocks_in_level(level) = 
	    2 ^ level => 
	    1 << level
	index_of(level) = 
	    (2 ^ level) - 1 => 
	    (1 << level) - 1
	index_offset_in_level(ptr, level) = 
	    (ptr - memory_address) / size_of_level(level) =>
	    (ptr - memory_address) / ((2 ^ total_levels) / 2 ^ level) =>
	    (ptr - memory_address) / 2 ^ (total_levels - level) =>
	    (ptr - memory_address) >> (total_levels - level)
	index(ptr, level) = index_of(level) + index_offset_in_level(ptr, level)
	address_from_level_offset(level_offset, level) = 
	    level_offset * size_of_level(level) + memory_address =>
	    level_offset * ((2 ^ total_levels) / (2 ^ level) + memory_address =>
	    level_offset * (2 ^ (total_levels - level)) + memory_address =>
	    level_offset << (total_levels - level) + memory_address
	address_from_level(index, level) = (index - index_of(level)) * size_of(level) + memory_address;
	address_from_index(index) = address_from_level(index, floor(log2(index + 1)))

