/*
 * i386/string.h
 *
 * These functions are based on those found in the Linux kernel - Linus
 * Torvalds notes: "consider these trivial functions to be PD".  If this is
 * good enough for Linus then it's good enough for me :-) - DJH.
 */

/*
 * strlen()
 *	Determine the length of a string.
 */
extern inline addr_t strlen(const char *s)
{
	int d0;
	register addr_t res;

	asm volatile ("repne\n\t"
			"scasb\n\t"
			"notl %0\n\t"
			"decl %0\n\t"
			: "=c" (res), "=&D" (d0)
			: "1" (s), "a" (0), "0" (0xffffffff));
	
	return res;
}

/*
 * strcpy()
 *	Copy one string to another.
 */
extern inline char *strcpy(char *dest, const char *src)
{
	int d0, d1, d2;
	
	asm volatile ("1:\t"
			"lodsb\n\t"
			"stosb\n\t"
			"testb %%al, %%al\n\t"
			"jne 1b\n\t"
			: "=&S" (d0), "=&D" (d1), "=&a" (d2)
			: "0" (src), "1" (dest) : "memory");
	
	return dest;
}

/*
 * strncpy()
 *	Copy one string to another, but to a maximum number of characters.
 */
extern inline char *strncpy(char *dest, const char *src, addr_t n)
{
	int d0, d1, d2, d3;
	
	asm volatile ("1:\t"
			"decl %2\n\t"
			"js 2f\n\t"
			"lodsb\n\t"
			"stosb\n\t"
			"testb %%al, %%al\n\t"
			"jne 1b\n\t"
			"rep\n\t"
			"stosb\n"
			"2:\t"
			: "=&S" (d0), "=&D" (d1), "=&c" (d2), "=&a" (d3)
			:"0" (src), "1" (dest), "2" (n)
			: "memory");
	
	return dest;
}

/*
 * strcat()
 *	Append one string to another
 */
extern inline char *strcat(char *dest, const char *src)
{
	int d0, d1, d2, d3;

	asm volatile ("repne\n\t"
			"scasb\n\t"
			"decl %1\n"
			"1:\t"
			"lodsb\n\t"
			"stosb\n\t"
			"testb %%al, %%al\n\t"
			"jne 1b\n\t"
			: "=&S" (d0), "=&D" (d1), "=&a" (d2), "=&c" (d3)
			: "0" (src), "1" (dest), "2" (0), "3" (0xffffffff)
			: "memory");

	return dest;
}

/*
 * strncat()
 *	Append one string to another, but to a maximum number of characters.
 */
extern inline char *strncat(char *dest, const char *src, addr_t n)
{
	int d0, d1, d2, d3;
	
	asm volatile ("repne\n\t"
			"scasb\n\t"
			"decl %1\n\t"
			"movl %8, %3\n"
			"1:\t"
			"decl %3\n\t"
			"js 2f\n\t"
			"lodsb\n\t"
			"stosb\n\t"
			"testb %%al, %%al\n\t"
			"jne 1b\n"
			"2:\t"
			"xorl %2, %2\n\t"
			"stosb\n\t"
			: "=&S" (d0), "=&D" (d1), "=&a" (d2), "=&c" (d3)
			: "0" (src), "1" (dest), "2" (0), "3" (0xffffffff), "g" (n)
			: "memory");
	
	return dest;
}


/*
 * strcmp()
 *	Compare one string with another
 */
extern inline s8_t strcmp(const char *s1, const char *s2)
{
	int d0, d1;
	register int res;
	
	asm volatile ("1:\t"
			"lodsb\n\t"
			"scasb\n\t"
			"jne 2f\n\t"
			"testb %%al, %%al\n\t"
			"jne 1b\n\t"
			"xorl %%eax, %%eax\n\t"
			"jmp 3f\n"
			"2:\t"
			"sbbl %%eax, %%eax\n\t"
			"orb $1, %%al\n"
			"3:\t"
			: "=a" (res), "=&S" (d0), "=&D" (d1)
			: "1" (s1), "2" (s2));
	
	return res;
}

/*
 * strncmp()
 *	Compare up to N characters of two strings.
 */
extern inline s8_t strncmp(const char *s1, const char *s2, addr_t n)
{
	register int res;
	int d0, d1, d2;
	
	asm volatile ("1:\t"
			"decl %3\n\t"
			"js 2f\n\t"
			"lodsb\n\t"
			"scasb\n\t"
			"jne 3f\n\t"
			"testb %%al, %%al\n\t"
			"jne 1b\n"
			"2:\t"
			"xorl %%eax, %%eax\n\t"
			"jmp 4f\n"
			"3:\t"
			"sbbl %%eax, %%eax\n\t"
			"orb $1, %%al\n"
			"4:"
			: "=a" (res), "=&S" (d0), "=&D" (d1), "=&c" (d2)
			: "1" (s1), "2" (s2), "3" (n));
	
	return res;
}

/*
 * strchr()
 *	Find the first occurrence of a given character in a string.
 */
extern inline char *strchr(const char *s, fast_s8_t c)
{
	int d0;
	register char *res;
	
	asm volatile ("movb %%al, %%ah\n"
			"1:\t"
			"lodsb\n\t"
			"cmpb %%ah, %%al\n\t"
			"je 2f\n\t"
			"testb %%al, %%al\n\t"
			"jne 1b\n\t"
			"movl $1, %1\n"
			"2:\t"
			"movl %1, %0\n\t"
			"decl %0\n\t"
			: "=a" (res), "=&S" (d0)
			: "1" (s), "0" (c));

	return res;
}


/*
 * strrchr()
 *	Find the last occurrence of a given character in a string.
 */
extern inline char *strrchr(const char *s, fast_s8_t c)
{
	int d0, d1;
	register char *res;
	
	asm volatile("movb %%al, %%ah\n"
			"1:\t"
			"lodsb\n\t"
			"cmpb %%ah, %%al\n\t"
			"jne 2f\n\t"
			"leal -1(%%esi), %0\n"
			"2:\t"
			"testb %%al, %%al\n\t"
			"jne 1b\n\t"
			: "=g" (res), "=&S" (d0), "=&a" (d1)
			: "0" (0), "1" (s), "2" (c));
	
	return res;
}
