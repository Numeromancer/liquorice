/*
 * avr/string.h
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
 * Some of the code in this file is derived from the avr-libc library written
 * by Marek Michalkiewicz <marekm@linux.org.pl>
 */

/*
 * strlen()
 *	Determine the length of a string.
 */
extern inline addr_t strlen(const char *s)
{
	char *t;
		
	asm volatile ("\n"
			"L_hi%=:\n\t"
			"ld __tmp_reg__, %a0+\n\t"
			"tst __tmp_reg__\n\t"
			"brne L_hi%=\n\t"
			: "=e" (t)
			: "0" (s));
	return (--t) - s;
}

/*
 * strcpy()
 *	Copy one string to another.
 */
extern inline char *strcpy(char *dest, const char *src)
{
	void *a1, *a2;
	asm volatile ("\n"
			"L_ho%=:\n\t"
			"ld __tmp_reg__, %a1+\n\t"
			"st %a0+, __tmp_reg__\n\t"
			"tst __tmp_reg__\n\t"
			"brne L_ho%=\n\t"
			: "=e" (a1), "=e" (a2)
			: "0" (dest), "1" (src));

	return dest;
}

/*
 * strncpy()
 *	Copy one string to another, but to a maximum number of characters.
 */
extern inline char *strncpy(char *dest, const char *src, addr_t n)
{
	void *a1, *a2;
	addr_t a3;

	if (n) {
		asm volatile ("\n"
				"L_ho%=:\n\t"
				"ld __tmp_reg__, %a1+\n\t"
				"st %a0+, __tmp_reg__\n\t"
				"sbiw %2, 1\n\t"
				"breq L_hi%=\n\t"
				"tst __tmp_reg__\n\t"
				"brne L_ho%=\n"
				"\n"
				"L_hi%=:\n\t"
				: "=e" (a1), "=e" (a2), "=w" (a3)
				: "2" (n), "0" (dest), "1" (src));
	}

	return dest;
}

/*
 * strcat()
 *	Append one string to another
 */
extern inline char *strcat(char *dest, const char *src)
{
	void *a1, *a2;
	
	asm volatile ("\n"
			"L_hi%=:\n\t"
			"ld __tmp_reg__, %a1+\n\t"
			"tst __tmp_reg__\n\t"
			"brne L_hi%=\n\t"
			"sbiw %1, 1\n"
			"\n"
			"L_ho%=:\n\t"
			"ld __tmp_reg__, %a0+\n\t"
			"st %a1+, __tmp_reg__\n\t"
			"tst __tmp_reg__\n\t"
			"brne L_ho%=\n\t"
			: "=e" (a1), "=e" (a2)
			: "1" (dest), "0" (src));

	return dest;
}

/*
 * strncat()
 *	Append one string to another, but to a maximum number of characters.
 */
extern inline char *strncat(char *dest, const char *src, addr_t n)
{
	void *a1, *a2;
	addr_t a3;

	if (n) {
		asm volatile ("\n"
				"L_hi%=:\n\t"
				"ld __tmp_reg__, %a1+\n\t"
				"tst __tmp_reg__\n\t"
				"brne L_hi%=\n\t"
				"sbiw %1,1\n"
				"\n"
				"L_ho%=:\n\t"
				"ld __tmp_reg__, %a0+\n\t"
				"st %a1+, __tmp_reg__\n\t"
				"sbiw %2, 1\n\t"
				"breq L_hh%=\n\t"
				"tst __tmp_reg__\n\t"
				"brne L_ho%=\n"
				"L_hh%=:\n\t"
				: "=e" (a1), "=e" (a2), "=w" (a3)
				: "1" (dest), "0" (src), "2" (n));
  	}
  	
	return dest;
}


/*
 * strcmp()
 *	Compare one string with another
 */
extern inline s8_t strcmp(const char *s1, const char *s2)
{
	void *a1, *a2;
	s8_t res;

	asm volatile ("cmp_loop%=:\n\t"
			"ld __tmp_reg__, %a2+\n\t"
			"ld %0, %a1+\n\t"
			"sub %0, __tmp_reg__\n\t"
			"brne cmp_out%=\n\t"
			"tst __tmp_reg__\n\t"
			"brne cmp_loop%=\n"
			"\n"
			"cmp_out%=:\n\t"
			"brcc cmp_end%=\n\t"
			"ldi %0, 0xff\n"
			"\n"
			"cmp_end%=:\n\t"
			: "=d" (res), "=e" (a1), "=e" (a2)
			: "1" (s1), "2" (s2));

	return res;
}

/*
 * strncmp()
 *	Compare up to N characters of two strings.
 */
extern inline s8_t strncmp(const char *s1, const char *s2, addr_t n)
{
	void *a1, *a2;
	addr_t a3;
	s8_t res;

	if (n) {
		asm volatile ("cmp_loop%=:\n\t"
				"ld __tmp_reg__, %a2+\n\t"
				"ld %0, %a1+\n\t"
				"sub %0, __tmp_reg__\n\t"
				"brne cmp_end%=\n\t"
				"sbiw %3, 1\n\t"
				"breq cmp_out%=\n\t"
				"tst __tmp_reg__\n\t"
				"brne cmp_loop%=\n"
				"\n"
				"cmp_out%=:\n\t"
				"brcc cmp_end%=\n\t"
				"ldi %0, 0xff\n"
				"\n"
				"cmp_end%=:\n\t"
				: "=d" (res), "=e"(a1), "=e"(a2), "=w"(a3)
				: "1" (s1), "2" (s2), "3" (n));
	}

	return res;
}

/*
 * strchr()
 *	Find the first occurrence of a given character in a string.
 */
extern inline char *strchr(const char *s, fast_s8_t c)
{
	asm volatile ("\n"
			"L_hi%=:\n\t"
			"ld __tmp_reg__, %a0+\n\t"
			"cp __tmp_reg__, %2\n\t"
			"breq L_ho%=\n\t"
			"tst __tmp_reg__\n\t"
			"brne L_hi%=\n\t"
			"ldi %A0, 1\n\t"
			"ldi %B0, 0\n"
			"\n"
			"L_ho%=:\n\t"
			"sbiw %0, 1"
			: "=e" (s)
			: "0" (s), "r" (c));
	
	return (char *)s;
}


/*
 * strrchr()
 *	Find the last occurrence of a given character in a string.
 */
extern inline char *strrchr(const char *src, fast_s8_t c)
{
	void *d;
	char *ret;

	asm volatile ("\n"
			"L_hi%=:\n\t"
			"ld __tmp_reg__, %a0+\n\t"
			"cp __tmp_reg__, %2\n\t"
			"brne L_ho%=\n\t"
			"mov %A1, %A0\n\t"
			"mov %B1, %B0\n"
			"\n"
			"L_ho%=:\n\t"
			"tst __tmp_reg__\n\t"
			"brne L_hi%=\n\t"
			: "=e" (d), "=r" (ret)
			: "r" (c), "1" (1L), "0" (src));

	return --ret;
}
