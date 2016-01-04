Buddy Allocator
===============

Copyright 2015, Red Rocket Computing, LLC, stephen@redrocketcomputing.com

Overview
--------

This an implementation Knuth's "buddy" system for dynamic memory allocation 
(Knuth. The Art of Computer Programming, Volume 1, Fundamental Algorithms,
Section 2.5C).  

The allocator manages free block using intrusive linked list structures with
one list head for every available block size. Block splits and buddy states
are tracked using binary bit trees.

The library support both internal and external storage of the allocator 
metadata and provides both a standard unsized "free" API as well as an 
accelerated "release" API for use when the size of the block to be free
is known.

License
-------

To facilate the use of this library in statically linked open and closed source 
embedded systems, this library is licensed under the terms of the 
[Mozilla Public License, V2.0.](http://mozilla.org/MPL/2.0)

Acknowledgments
---------------

This implementation of Knuth's Buddy Allocator closely follows the most 
excellent ideas posted by 
[Niklas](https://www.blogger.com/profile/10055379994557504977) in his post
[Allocation Adventures 3: The Buddy Allocator](http://bitsquid.blogspot.com/2015/08/allocation-adventures-3-buddy-allocator.html)

Niklas primer on memory allocation metadata management is fantastic and has 
also been included in the doc directory. I took the liberty of converting the
blog post to markdown and attaching a copyright attributing the writeup him.

*Niklas, if you have a problem with this, let me know and I will remove it 
markdown version from my project.  Just reach out!*

Dependencies
------------

The allocator requires a compatible compiler supporting the following built-ins:

	__builtin_clzl /* Count lead zeros long */
	__builtin_ctzl /* Count trailing zeros long */

and is known to work with recent version of gcc and clang.

Building The Allocator
----------------------

For the default release version (-O3)

	CD=path/to/project
	make

To build a debug (-O0 -pg -g) version of the library:

	CD=path/to/project
	make V=1 BUILD_TYPE=debug
	
The library can be found at 

	path/to/project/BUILD_TYPE/libbuddy-alloc.a

The build location can be adjusted with OUTPUT_ROOT which must be an absolute path:

	make OUTPUT_ROOT=/some/directory

To cross compile use the CROSS_COMPILE variable:

	make CROSS_COMPILE=arm-none-eabi-

Other build options can be adjusted in Makefile.common, Makefile.debug and Makefile.release.

Using The Allocator
-------------------

To create an 1MB buddy allocator with internal metadata:

	#include <stdio.h>
	#include <stdlib.h>
	#include <buddy-alloc.h>
	
	#define ALLOCATOR_SIZE 1048576
	
	unsigned long int memory_region[ALLOCATOR_SIZE / sizeof(unsigned long int)];
	
	int main(int argc, char **argv)
	{
		buddy_allocator_t *allocator;
		void *ptr;
		
		/* The return address is always equal to the provide memory region */
		allocator = buddy_create(memory_region, ALLOCATOR_SIZE);
	
		/* This will allocate a block 16K in size */
		ptr = buddy_alloc(allocator, 13773);
		if(!ptr) {
			perror("allocation failed");
			exit(1);
		}
	
		/* This will determine the size of the memory block, by searching the
		 free index at each level, somewhat slower */
		buddy_free(allocator, ptr);
	
		/* This will allocate a block 16K in size */
		ptr = buddy_alloc(allocator, 13773);
		if(!ptr) {
			perror("allocation failed");
			exit(1);
		}
	
		/* This frees the memory block, using the provided size, a little
		 faster as the block level can be derived from the size */
		buddy_release(allocator, ptr, 13773);
	
		return 0;
	}
