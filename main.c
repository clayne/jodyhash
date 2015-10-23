/*
 * Jody Bruchon hashing function command-line utility
 *
 * Copyright (C) 2014, 2015 by Jody Bruchon <jody@jodybruchon.com>
 * Released under the MIT License (see LICENSE for details)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include "jody_hash.h"
#include "version.h"

#define BSIZE 65536

int main(int argc, char **argv)
{
	hash_t blk[(BSIZE / sizeof(hash_t))];
	char name[PATH_MAX];
	size_t i;
	FILE *fp;
	hash_t hash = 0;

	if (argc > 2) goto error_argc;

	if (argc == 2 && !strcmp("-v", argv[1])) {
		fprintf(stderr, "Jody Bruchon's hashing utility %s (%s)\n", VER, VERDATE);
		exit(EXIT_SUCCESS);
	}
	if (argc == 2 && !strcmp("-h", argv[1])) {
		fprintf(stderr, "Jody Bruchon's hashing utility %s (%s)\n", VER, VERDATE);
		fprintf(stderr, "usage: %s [file_to_hash]\n", argv[0]);
		fprintf(stderr, "Specifying no name or '-' as the name reads from stdin\n");
		exit(EXIT_FAILURE);
	}

	/* Read from stdin */
	if (argc == 1 || !strcmp("-", argv[1])) {
		fp = stdin;
		strncpy(name, "(stdin)", PATH_MAX);
	} else {
		fp = fopen(argv[1], "r");
		strncpy(name, argv[1], PATH_MAX);
	}

	if (!fp) goto error_open;

	while ((i = fread((void *)blk, 1, BSIZE, fp))) {
		if (ferror(fp)) goto error_read;
		hash = jody_block_hash(blk, hash, i);
		if (feof(fp)) break;
	}
	printf("%016" PRIx64 "\n", hash);
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

