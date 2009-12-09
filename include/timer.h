/*
 * timer.h
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

#define TICK_RATE 100

#define LDAV_FSHIFT 11			/* Fixed point shift */
#define LDAV_1 (1L << LDAV_FSHIFT)
#define LDAV_VALS 3
#define LDAV_TICKS (TICK_RATE * 2)
#define LDAV_EXP_1 1981			/* 1.0 / exp(2 secs / 1 minute) in fixed point */
#define LDAV_EXP_5 2034			/* 1.0 / exp(2 secs / 5 minutes) in fixed point */
#define LDAV_EXP_15 2043		/* 1.0 / exp(2 secs / 15 minutes) in fixed point */

/*
 * Function declarations
 */
extern void timer_init(void);
extern u32_t timer_get_jiffies(void);
extern void timer_dump_stats(u32_t *ldavgs, int max);
