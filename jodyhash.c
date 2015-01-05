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
#include <fcntl.h>
#include "jody_hash.h"

#define BSIZE 4096

int main(int argc, char **argv)
{
	unsigned char blk[BSIZE];
	int i;
	hash_t hash = 0;

	while ((i = fread(blk, 1, BSIZE, stdin))) {
		if (ferror(stdin)) goto error_read;
		hash = jody_block_hash((hash_t *)blk, hash, i);
	}
	printf("%016lx\n", hash);

	exit(EXIT_SUCCESS);

error_read:
	fprintf(stderr, "Error reading file %s\n", "stdin");
	exit(EXIT_FAILURE);
}

