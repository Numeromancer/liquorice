/*
 * membuf.c
 *	Memory buffer support.
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
 * The idea behind memory buffers is to try and provide a reference counting
 * mechanism suitable for garbage collecting dynamically-allocated memory
 * blocks.  We provide a way to allocate a user-required block of memory but
 * add a few extra bytes to each allocation.  These form a header for the block
 * with a reference count and pointers to functions to provide optional
 * user-level behaviour when the block is referenced, dereferenced or finally
 * freed.
 *
 * When we allocate a membuf it's reference count is set to one.  Any time
 * anyone creates a copy-reference for use at a later time, it should get a
 * "ref" (adding one to the count).  When a copy-reference or the original
 * reference are no longer required it should get a "deref" (removing one from
 * the count).  When the count hits zero the membuf gets released (freed).
 *
 * A major advantage of this concept is that it makes it easy to check that the
 * software is behaving in a rational manner.  We can do an easy check for
 * possible memory leaks by simply counting that the number of alloc and ref
 * operations matches the number of deref operations!
 *
 * One thing to be aware of - don't try and statically declare a membuf; it
 * won't work!  membufs are strictly dynamically allocated.
 */
#include "types.h"
#include "memory.h"
#include "debug.h"
#include "context.h"
#include "heap.h"
#include "membuf.h"

/*
 * Lock used to ensure that we don't run into any memory corruption problems,
 * but without causing any trouble with interrupt latencies either.
 */
static struct lock membuf_lock;

/*
 * membuf_alloc()
 */
void *membuf_alloc(addr_t size, void (*mfree)(void *))
{
	struct membuf *mb;
	
	mb = (struct membuf *)heap_alloc(sizeof(struct membuf) + size);
	
	mb->mb_refs = 1;
        mb->mb_free = mfree;
		
	return (void *)(mb + 1);
}

/*
 * membuf_ref()
 *	Increase the reference count on a membuf.
 */
void membuf_ref(void *buf)
{
	struct membuf *mb;
	
	mb = ((struct membuf *)buf) - 1;
	
	spinlock_lock(&membuf_lock);
	
	mb->mb_refs++;
	
	spinlock_unlock(&membuf_lock);
}

/*
 * membuf_deref()
 *	Decrease the reference count on a membuf.  If zeroed, then free it!
 */
ref_t membuf_deref(void *buf)
{
	struct membuf *mb;
        ref_t res;
			
	mb = ((struct membuf *)buf) - 1;
	
	spinlock_lock(&membuf_lock);
	
	mb->mb_refs--;
	res = mb->mb_refs;
	
	if (mb->mb_refs != 0) {
		spinlock_unlock(&membuf_lock);
		return res;
	}

	spinlock_unlock(&membuf_lock);
			
	if (mb->mb_free) {
		mb->mb_free(buf);
	}
	
	heap_free(mb);

	return 0;
}

/*
 * membuf_get_refs()
 *	Get the reference count on a membuf.
 */
ref_t membuf_get_refs(void *buf)
{
	ref_t res;
	struct membuf *mb;
	
	mb = ((struct membuf *)buf) - 1;
	
	spinlock_lock(&membuf_lock);
	
	res = mb->mb_refs;

	spinlock_unlock(&membuf_lock);

	return res;
}

/*
 * membuf_init()
 *	Initialize the membuf handling.
 */
void membuf_init(void)
{
	spinlock_init(&membuf_lock, 0x08);
}
