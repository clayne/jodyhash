/*
 * Jody Bruchon hashing function command-line utility
 *
 * Copyright (C) 2014, 2015 by Jody Bruchon <jody@jodybruchon.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include "jody_hash.h"

#define BSIZE 4096

int main(int argc, char **argv)
{
	unsigned char blk[BSIZE];
	char name[PATH_MAX];
	size_t i;
	FILE *fp;
	hash_t hash = 0;

	if (argc > 2) goto error_argc;

	/* Read from stdin */
	if (argc == 1 || !strcmp("-", argv[1])) {
		fp = stdin;
		strncpy(name, "(stdin)", PATH_MAX);
	} else {
		fp = fopen(argv[1], "r");
		strncpy(name, argv[1], PATH_MAX);
	}

	if (!fp) goto error_open;

	while ((i = fread(blk, 1, BSIZE, fp))) {
		if (ferror(fp)) goto error_read;
		hash = jody_block_hash((hash_t *)blk, hash, i);
		if (feof(fp)) break;
	}
	printf("%016lx\n", hash);
	fclose(fp);

	exit(EXIT_SUCCESS);

error_open:
	fprintf(stderr, "error: cannot open: %s\n", name);
	exit(EXIT_FAILURE);
error_argc:
	fprintf(stderr, "error: only specify one file name (none or '-' to read from stdin)\n");
	exit(EXIT_FAILURE);
error_read:
	fprintf(stderr, "error reading file: %s\n", name);
	exit(EXIT_FAILURE);
}

