/*
 * heap.c
 *	Heap (memory) allocation routines.
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
 *
 * The strategy used within this memory allocator is to try and cause as
 * little waste as possible.  This can make things a little slower than
 * would be ideal, but does give the best chance of things keeping running.
 */
#include "types.h"
#include "memory.h"
#include "debug.h"
#include "context.h"
#include "heap.h"

/*
 * Lock used to ensure that we don't run into any memory corruption problems,
 * but without causing any trouble with interrupt latencies either.
 */
static struct lock heap_lock = {0, 0x08, 0xff};

/*
 * How much memory is there free?
 */
static addr_t free_ram = 0;

/*
 * Pointer to the first memory hole.
 */
struct memory_hole *first_hole = NULL;

/*
 * heap_alloc()
 *	Allocate a block of memory.
 */
void *heap_alloc(addr_t size)
{
	struct memory_hole *mh, **mhprev;
	struct memory_hole *use = NULL;
        struct memory_hole **useprev = NULL;
        void *block = NULL;
        int required;
        fast_u16_t *magic;

	if (DEBUG) {
#if defined(MEGA103) || defined (AT90S8515)
		if (size > 0x0c00) {
#else
		if (size > 0x8000) {
#endif
			debug_stop();
			do {
				debug_print_pstr("\fheap_alloc: large sz: ");
				debug_print_addr(size);
				debug_wait_button();
				debug_stack_trace();
			} while (debug_cycle());
		}
	}

        /*
         * All allocations need to be "memory_block" bytes larger than the
         * amount requested by our caller.  They also need to be large enough
         * that they can contain a "memory_hole" and any magic values used in
         * debugging (for when the block gets freed and becomes an isolated
         * hole).
         */
        required = size + sizeof(struct memory_block) + (HEAP_DEBUG ? sizeof(fast_u16_t) : 0);
	if (required < (sizeof(struct memory_hole) + (HEAP_DEBUG ? sizeof(fast_u16_t) : 0))) {
		required = sizeof(struct memory_hole) + (HEAP_DEBUG ? sizeof(fast_u16_t) : 0);
	}
	
	spinlock_lock(&heap_lock);

	/*
	 * Scan the list of all available memory holes and find the smallest
	 * one that meets our requirement.  We have an early out from this scan
	 * if we find a hole that *exactly* matches our needs.
	 */
	mh = first_hole;
	mhprev = &first_hole;
	while (mh) {
        	if (mh->mh_size == required) {
        		use = mh;
        		useprev = mhprev;
        		break;
        	} else if (mh->mh_size > required) {
        		if (!use || (use->mh_size > mh->mh_size)) {
        			use = mh;
        			useprev = mhprev;
        		}
        	}
			
		mhprev = &mh->mh_next;
		mh = mh->mh_next;
	}

        /*
         * Did we find any space available?  If yes, then remove a chunk of it
         * and, if we can, release any of what's left as a new hole.  If we can't
         * release any then allocate more than was requested and remove this
         * hole from the hole list.
         */
        if (use) {
        	struct memory_block *mb;
        	
        	if ((use->mh_size - required) > (sizeof(union memory_union) + 1 + (HEAP_DEBUG ? sizeof(fast_u16_t) : 0))) {
        		struct memory_hole *new_hole = (struct memory_hole *)((addr_t)use + required);
        		new_hole->mh_size = use->mh_size - required;
        		new_hole->mh_next = use->mh_next;
        		*useprev = new_hole;
			if (HEAP_DEBUG) {
				magic = (fast_u16_t *)(new_hole + 1);
				*magic = MEMORY_HOLE_MAGIC;
			}
        	} else {
        		required = use->mh_size;
        		*useprev = use->mh_next;
        	}

        	mb = (struct memory_block *)use;
        	mb->mb_size = required;
		magic = (fast_u16_t *)(mb + 1);
		if (HEAP_DEBUG) {
			*magic++ = MEMORY_BLOCK_MAGIC;
		}
        	
        	block = (void *)magic;
	
		free_ram -= required;
	} else if (DEBUG) {
		debug_stop();
		do {
			debug_print_pstr("\fheap_alloc: NULL pointer");
			debug_wait_button();
			debug_stack_trace();
		} while (debug_cycle());
        }

	spinlock_unlock(&heap_lock);
	
	return block;
}

/*
 * heap_free()
 *	Release a block of memory.
 */
void heap_free(void *block)
{
	struct memory_block *mb;
        struct memory_hole *mh, **mhprev;
        struct memory_hole *new_hole;
        fast_u16_t *magic;
                        	
	magic = block;
        if (HEAP_DEBUG) {
        	magic--;
		if (*magic != MEMORY_BLOCK_MAGIC) {
			debug_stop();
			do {
				debug_print_pstr("\fheap_free: non-memory-blk: ");
				debug_print_addr((addr_t)block);
				debug_wait_button();
				debug_stack_trace();
			} while (debug_cycle());
		}
        }
		
	mb = ((struct memory_block *)magic) - 1;
		
	spinlock_lock(&heap_lock);

	free_ram += mb->mb_size;
		
	/*
	 * Convert our block into a hole.
	 */
       	new_hole = (struct memory_hole *)mb;
	if (HEAP_DEBUG) {
		magic = (fast_u16_t *)(new_hole + 1);
		*magic = MEMORY_HOLE_MAGIC;
	}
	
	/*
	 * Stroll through the hole list and see if this newly freed block can
	 * be merged with anything else to form a larger space.  Whatever
	 * happens, we still ensure that the list is ordered lowest-addressed
	 * -hole first through to highest-addressed-hole last.
	 */
        mh = first_hole;
        mhprev = &first_hole;
        while (mh) {
        	if (((addr_t)mh + mh->mh_size) == (addr_t)mb) {
        		mh->mh_size += mb->mb_size;
        		if (((addr_t)mh + mh->mh_size) == (addr_t)mh->mh_next) {
        			mh->mh_size += mh->mh_next->mh_size;
        			mh->mh_next = mh->mh_next->mh_next;
        		}
       			break;
        	}

		if ((addr_t)mh > (addr_t)mb) {
        		*mhprev = new_hole;
        		if (((addr_t)new_hole + new_hole->mh_size) == (addr_t)mh) {
        			new_hole->mh_size += mh->mh_size;
        			new_hole->mh_next = mh->mh_next;
        		} else {
	        		new_hole->mh_next = mh;
        		}
       			break;
        	}
        	
        	mhprev = &mh->mh_next;
        	mh = mh->mh_next;
        }

	if (!mh) {
		new_hole->mh_next = mh;
		*mhprev = new_hole;
	}

	spinlock_unlock(&heap_lock);
}

/*
 * heap_get_free()
 *	Return the amount of heap space that's still available.
 */
addr_t heap_get_free(void)
{
	addr_t ret;
	
	spinlock_lock(&heap_lock);
	ret = free_ram;
	spinlock_unlock(&heap_lock);

	return ret;
}

/*
 * heap_dump_stats()
 */
int heap_dump_stats(struct memory_hole *mbuf, int max)
{
	struct memory_hole *mh;
        int ct = 0;
		
	spinlock_lock(&heap_lock);

	mh = first_hole;
	while (mh && (ct < max)) {
		mbuf->mh_next = mh;
		mbuf->mh_size = mh->mh_size;
		mbuf++;
		ct++;
		mh = mh->mh_next;
	}
	
	spinlock_unlock(&heap_lock);

	return ct;
}

/*
 * heap_add()
 *	Add a region of memory to the free heap.
 *
 * Adding new space to the heap is just a case of fooling the heap_free()
 * function into believing that the new space was previously allocated.  All we
 * have to do is forge an "alloc"'d block!
 */
void heap_add(addr_t addr, addr_t sz)
{
	struct memory_block *mb;
        fast_u16_t *magic;

	mb = (struct memory_block *)addr;
	mb->mb_size = sz;
        magic = (fast_u16_t *)(mb + 1);
	
	if (HEAP_DEBUG) {
		*magic++ = MEMORY_BLOCK_MAGIC;
	}
	
	heap_free(magic);
}
