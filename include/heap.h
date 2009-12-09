/*
 * heap.h
 *
 * Copyright (C) 2000 David J. Hudson <dave@humbug.demon.co.uk>
 *
 * This file is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You can redistribute this file and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software Foundation;
 * either version 2 of the License, or (at your discretion) any later version.
 * See the accompanying file "copying-gpl.txt" for more details.
 *
 * As a special exception to the GPL, permission is granted for additional
 * uses of the text contained in this file.  See the accompanying file
 * "copying-liquorice.txt" for details.
 */

/*
 * Debug configuration.
 */
#define HEAP_DEBUG 1

/*
 * Magic numbers used if we're debugging.
 */
#define MEMORY_HOLE_MAGIC 0x4d48
#define MEMORY_BLOCK_MAGIC 0x4d42

/*
 * Structure used to create a list of memory holes (i.e. free memory blocks).
 */
struct memory_hole {
	addr_t mh_size;			/* Size of the hole including this structure */
	struct memory_hole *mh_next;	/* Pointer to the next hole in memory */
};

/*
 * Structure used to prefix any allocated block of memory.
 */
struct memory_block {
	addr_t mb_size;			/* Size of the block including this structure */
};

/*
 * Union of the two possible structures.  We don't really use this union
 * directly, but it exists in practice and we need to be able to determine
 * the size of it.
 *
 * IMPORTANT NOTE: the initial parts of memory_hole and memory_block structures
 * are identical!  We rely on this characteristic later!
 */
union memory_union {
	struct memory_hole mu_hole;
	struct memory_block mu_block;
};

/*
 * Heap allocation functions.
 */
extern void *heap_alloc(addr_t size);
extern void heap_free(void *block);
extern addr_t heap_get_free(void);
extern int heap_dump_stats(struct memory_hole *mbuf, int max);
extern void heap_add(addr_t addr, addr_t sz);
