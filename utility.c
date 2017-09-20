/*
 * Jody Bruchon hashing function command-line utility
 *
 * Copyright (C) 2014-2017 by Jody Bruchon <jody@jodybruchon.com>
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

/* Detect Windows and modify as needed */
#if defined _WIN32 || defined __CYGWIN__
 #define ON_WINDOWS 1
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
 #include <io.h>
 /* Output end of error string with Win32 Unicode as needed */
 #define ERR(a,b) { _setmode(_fileno(stderr), _O_U16TEXT); \
		 fwprintf(stderr, L"%S\n", a); \
		 _setmode(_fileno(stderr), _O_TEXT); }
#else
 #define ERR(a,b) fprintf(stderr, "%s\n", b);
#endif

#if JODY_HASH_WIDTH == 64
#define PRINTHASH(a) printf("%016" PRIx64, a)
#endif
#if JODY_HASH_WIDTH == 32
#define PRINTHASH(a) printf("%08" PRIx32, hash)
#endif
#if JODY_HASH_WIDTH == 16
#define PRINTHASH(a) printf("%04" PRIx16, hash)
#endif

#ifndef BSIZE
#define BSIZE 32768
#endif

static int error = EXIT_SUCCESS;
static char *progname;

static void usage(void)
{
	fprintf(stderr, "Jody Bruchon's hashing utility %s (%s) [%d bit width]\n", VER, VERDATE, JODY_HASH_WIDTH);
	fprintf(stderr, "usage: %s [-b|s|l] [file_to_hash]\n", progname);
	fprintf(stderr, "Specifying no name or '-' as the name reads from stdin\n");
	fprintf(stderr, "  -b|-s  Output in md5sum binary style instead of bare hashes\n");
	fprintf(stderr, "  -l     Generate a hash for each text input line\n");
	fprintf(stderr, "  -L     Same as -l but also prints hashed text after the hash\n");
	exit(error);
}

#ifdef UNICODE
/* Copy Windows wide character arguments to UTF-8 */
static void widearg_to_argv(int argc, wchar_t **wargv, char **argv)
{
	char temp[PATH_MAX + 1];
	int len;

	if (!argv) goto error_bad_argv;
	for (int counter = 0; counter < argc; counter++) {
		len = WideCharToMultiByte(CP_UTF8, 0, wargv[counter],
				-1, (LPSTR)&temp, PATH_MAX * 2, NULL, NULL);
		if (len < 1) goto error_wc2mb;

		argv[counter] = (char *)malloc((size_t)len + 1);
		if (!argv[counter]) goto error_oom;
		strncpy(argv[counter], temp, (size_t)len + 1);
	}
	return;

error_bad_argv:
	fprintf(stderr, "fatal: bad argv pointer\n");
	exit(EXIT_FAILURE);
error_wc2mb:
	fprintf(stderr, "fatal: WideCharToMultiByte failed\n");
	exit(EXIT_FAILURE);
error_oom:
	fprintf(stderr, "out of memory\n");
	exit(EXIT_FAILURE);
}


/* Unicode on Windows requires wmain() and the -municode compiler switch */
int wmain(int argc, wchar_t **wargv)
#else
int main(int argc, char **argv)
#endif /* UNICODE */
{
	static hash_t blk[(BSIZE / sizeof(hash_t))];
	static char name[PATH_MAX];
	static size_t i;
	static FILE *fp;
	static hash_t hash;
	static int argnum = 1;
	static int outmode = 0;
	static int read_err = 0;
	//intmax_t bytes = 0;

#ifdef UNICODE
	static wchar_t wname[PATH_MAX];
	/* Create a UTF-8 **argv from the wide version */
	static char **argv;
	argv = (char **)malloc(sizeof(char *) * argc);
	if (!argv) goto error_oom;
	widearg_to_argv(argc, wargv, argv);
#endif /* UNICODE */

	progname = argv[0];

	/* Process options */
	if (argc > 1) {
		if (!strcmp("-v", argv[1])) {
			fprintf(stderr, "Jody Bruchon's hashing utility %s (%s) [%d bit width]\n", VER, VERDATE, JODY_HASH_WIDTH);
			exit(EXIT_SUCCESS);
		}
		if (!strcmp("-h", argv[1])) usage();
	}
	if (argc > 2) {
		if (!strcmp("-s", argv[1]) || !strcmp("-b", argv[1])) outmode = 1;
		if (!strcmp("-l", argv[1])) outmode = 2;
		if (!strcmp("-L", argv[1])) outmode = 3;
		if (outmode > 0 || !strcmp("--", argv[1])) argnum++;
	}

	do {
		hash = 0;
		/* Read from stdin */
		if (argc == 1 || !strcmp("-", argv[argnum])) {
			strncpy(name, "-", PATH_MAX);
#ifdef ON_WINDOWS
			_setmode(_fileno(stdin), _O_BINARY);
#endif
			fp = stdin;
		} else {
			strncpy(name, argv[argnum], PATH_MAX);
#ifdef UNICODE
			fp = _wfopen(wargv[argnum], L"rbS");
			if (!MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, PATH_MAX)) goto error_mb2wc;
#else
			fp = fopen(argv[argnum], "rb");
#endif /* UNICODE */
		}

		if (!fp) {
			fprintf(stderr, "error: cannot open: ");
			ERR(wname, name);
			error = EXIT_FAILURE;
			argnum++;
			continue;
		}

		/* Line-by-line hashing with -l/-L */
		if (outmode == 2 || outmode == 3) {
			while (fgets((char *)blk, BSIZE, fp) != NULL) {
				hash = 0;
				if (ferror(fp)) {
					fprintf(stderr, "error reading file: ");
					ERR(wname, name);
					error = EXIT_FAILURE; read_err = 1;
					break;
				}
				/* Skip empty lines */
				i = strlen((char *)blk);
				if (i < 2 || *(char *)blk == '\n') continue;
				/* Strip \r\n and \n and \r newlines */
				if (((char *)blk)[i - 2] == '\r') ((char *)blk)[i - 2] = '\0';
				else ((char *)blk)[i - 1] = '\0';

				hash = jody_block_hash(blk, hash, i - 1);

				PRINTHASH(hash);
				if (outmode == 3) printf(" '%s'\n", (char *)blk);
				else printf("\n");

				if (feof(fp)) break;
			}
			goto close_file;
		}

		while ((i = fread((void *)blk, 1, BSIZE, fp))) {
			if (ferror(fp)) {
				fprintf(stderr, "error reading file: ");
				ERR(wname, name);
				error = EXIT_FAILURE; read_err = 1;
				break;
			}
			//bytes += i;
			hash = jody_block_hash(blk, hash, i);
			if (feof(fp)) break;
		}

		/* Loop without result on read errors */
		if (read_err == 1) {
			read_err = 0;
			goto close_file;
		}

		PRINTHASH(hash);

#ifdef UNICODE
		_setmode(_fileno(stdout), _O_U16TEXT);
		if (outmode == 1) wprintf(L" *%S\n", wargv[argnum]);
		else wprintf(L"\n");
		_setmode(_fileno(stdout), _O_TEXT);
#else
		if (outmode == 1) printf(" *%s\n", name);
		else printf("\n");
#endif /* UNICODE */
		//fprintf(stderr, "processed %jd bytes\n", bytes);
close_file:
		fclose(fp);
		argnum++;
	} while (argnum < argc);

	exit(error);

#ifdef UNICODE
error_oom:
	fprintf(stderr, "out of memory\n");
	exit(EXIT_FAILURE);
error_mb2wc:
	fprintf(stderr, "fatal: MultiByteToWideChar failed\n");
	exit(EXIT_FAILURE);
#endif
}

