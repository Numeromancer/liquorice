/*
 * membuf.h
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
 */

/*
 * Memory buffer layout.
 */
struct membuf {
	ref_t mb_refs;			/* Reference count */
	void (*mb_free)(void *);	/* Function to free up the membuf's contents */
};

/*
 * Prototypes.
 */
extern void *membuf_alloc(addr_t size, void (*mfree)(void *));
extern void membuf_ref(void *buf);
extern ref_t membuf_deref(void *buf);
extern ref_t membuf_get_refs(void *buf);
extern void membuf_init(void);
