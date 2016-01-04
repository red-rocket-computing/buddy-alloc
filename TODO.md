- Add support for specific min allocations
- Add support for max allocations, effectively pre-splitting of blocks with ceiling on min level when combining blocks
- Add support for discontious memory when using max allocations setup
- Add support for power of 2 aligned allocations using the follow idea:
    Allocate block of the size of alignments
    Split block to nearest power of 2 of the requested size
    Add the remainder to the free block lists
 - Use intptr_ instead of unsigned long int
 