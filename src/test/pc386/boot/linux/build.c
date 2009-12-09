/*
 * build.c
 *	System build utility
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
 * The code in this file was originally based on software written and GPL'd for
 * the Linux OS by Linus Torvalds.
 *
 * Glues the various bootstrap, kernel and boot-time service images
 * together into one file that can be booted.  We do a certain amount of
 * messing about with the endian format of things as this utility is
 * executed on the host and not the target system.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "bootconst.h"

/*
 * Data type designed to ease translations between longs, shorts and chars 
 */
typedef union {
	long l;
	short s[2];
	char b[4];
} conv;

/*
 * intel_short:
 *	Convert from default short format to Intel's little endian format
 */
short intel_short(short l)
{
	conv t;
  
	t.b[0] = l & 0xff;
	l >>= 8;
	t.b[1] = l & 0xff;
	l >>= 8;
	return t.s[0];
}								

/*
 * Display a particular failure message and then terminate 
 */
void die(char *str1, char *str2)
{
	fprintf(stderr, "%s: '%s'\n", str1, str2);
	exit(1);
}

/*
 * Display the utility's usage line and then terminate 
 */
void usage(void)
{
	fprintf(stderr, "usage: build <bootsector-file> <setup-file> <kernel-file> [> image]\n");
	exit(1);
}

/*
 * Handle the parsing of the real-mode code.
 */
void parse_real_mode_code(char *fname, int fsize, int boot)
{
	int i, c, t = 0;
	FILE *fd;
	char buf[1024];

	if ((fd = fopen(fname, "rb")) == NULL) {
		die("unable to open", fname);
	}

	for (i = 0; (c = fread(buf, 1, sizeof buf, fd)) > 0; i += c) {
		if (fwrite(buf, 1, c, stdout) != c) {
			die("write call failed on", "stdout");
		}
		t += c;
	}

	if (c != 0) {
		die("file error in", fname);
	}

	if (t > fsize) {
		die("wrong file size for", fname);
	}

	if (t != fsize) {
		for (i = t; i < fsize; i++) {
			fputc(0, stdout);
		}
	}

	fprintf(stderr, "'%s' is %d Bytes (using %d Bytes)\n",
		fname, t, fsize);

	if ((boot) && ((*(unsigned short *)(buf + 510))
		        != (unsigned short)intel_short(0xAA55))) {
		die("boot block hasn't got boot flag (0xAA55) in", fname);
	}

	fclose(fd);
}

/*
 * Main entry point 
 */
int main(int argc, char **argv)
{
	int i;
	int fsize = 0;
	char buf[1024];
	FILE *kfile;

	if (argc != 4) {
		usage();
	}  
  
	for (i = 0; i < sizeof buf; i++) {
		buf[i] = 0;
	}

	parse_real_mode_code(argv[1], SECTOR_SIZE, 1);
	parse_real_mode_code(argv[2], SECTOR_SIZE * SETUP_SECS, 0);

	if ((kfile = fopen(argv[3], "rb")) == NULL) {
		die("unable to open binary image file", argv[3]);
	}

	/*
	 * Copy the rest of the file.
	 */
	i = fgetc(kfile);
	do {
		fputc(i, stdout);
		i = fgetc(kfile);
		fsize++;
	} while (i != EOF);
  
	if (fseek(stdout, (SECTOR_SIZE - 4), SEEK_SET) != 0) {
		die("unable to seek back in bootsector", argv[1]);
	}

	fsize += (SECTOR_SIZE - 1);
	fsize /= SECTOR_SIZE;
	buf[0] = (fsize & 0xff);
	buf[1] = ((fsize >> 8) & 0xff);
	fwrite(buf, 1, 2, stdout);
	fprintf(stderr, "system image is %d sectors\n\n", fsize);

	fclose(kfile);
	fflush(stdout);

	return 0;
}
