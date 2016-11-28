/*
 * Jody Bruchon hashing function command-line utility
 *
 * Copyright (C) 2014-2016 by Jody Bruchon <jody@jodybruchon.com>
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

#ifndef BSIZE
#define BSIZE 131072
#endif

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
	//intmax_t bytes = 0;

#ifdef UNICODE
	static wchar_t wname[PATH_MAX];
	/* Create a UTF-8 **argv from the wide version */
	static char **argv;
	argv = (char **)malloc(sizeof(char *) * argc);
	if (!argv) goto error_oom;
	widearg_to_argv(argc, wargv, argv);
#endif /* UNICODE */

	if (argc == 2 && !strcmp("-v", argv[1])) {
		fprintf(stderr, "Jody Bruchon's hashing utility %s (%s) [%d bit width]\n", VER, VERDATE, JODY_HASH_WIDTH);
		exit(EXIT_SUCCESS);
	}
	if (argc == 2 && !strcmp("-h", argv[1])) {
		fprintf(stderr, "Jody Bruchon's hashing utility %s (%s) [%d bit width]\n", VER, VERDATE, JODY_HASH_WIDTH);
		fprintf(stderr, "usage: %s [-s] [file_to_hash]\n", argv[0]);
		fprintf(stderr, "Specifying no name or '-' as the name reads from stdin\n");
		fprintf(stderr, "   -s   Output in md5sum style instead of bare hashes\n");
		exit(EXIT_FAILURE);
	}

	if (argc > 2 && !strcmp("-s", argv[1])) {
		outmode = 1;
		argnum++;
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

		if (!fp) goto error_open;

		while ((i = fread((void *)blk, 1, BSIZE, fp))) {
			if (ferror(fp)) goto error_read;
			//bytes += i;
			hash = jody_block_hash(blk, hash, i);
			if (feof(fp)) break;
		}
#if JODY_HASH_WIDTH == 64
		printf("%016" PRIx64, hash);
#endif
#if JODY_HASH_WIDTH == 32
		printf("%08" PRIx32, hash);
#endif
#if JODY_HASH_WIDTH == 16
		printf("%04" PRIx16, hash);
#endif
#ifdef UNICODE
		_setmode(_fileno(stdout), _O_U16TEXT);
		if (outmode) wprintf(L" *%S\n", wargv[argnum]);
		else wprintf(L"\n");
		_setmode(_fileno(stdout), _O_TEXT);
#else
		if (outmode) printf(" *%s\n", name);
		else printf("\n");
#endif /* UNICODE */
		//fprintf(stderr, "processed %jd bytes\n", bytes);
		fclose(fp);
		argnum++;
	} while (argnum < argc);

	exit(EXIT_SUCCESS);

error_open:
	fprintf(stderr, "error: cannot open: ");
	ERR(wname, name);
	exit(EXIT_FAILURE);
error_read:
	fprintf(stderr, "error reading file: ");
	ERR(wname, name);
	exit(EXIT_FAILURE);
#ifdef UNICODE
error_oom:
	fprintf(stderr, "out of memory\n");
	exit(EXIT_FAILURE);
error_mb2wc:
	fprintf(stderr, "fatal: MultiByteToWideChar failed\n");
	exit(EXIT_FAILURE);
#endif
}

