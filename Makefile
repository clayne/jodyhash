# jodyhash Makefile

CC=gcc
#CFLAGS=-Os -flto -ffunction-sections -fdata-sections -fno-unwind-tables -fno-asynchronous-unwind-tables
CFLAGS=-O2 -g
#CFLAGS=-Og -g3
BUILD_CFLAGS = -std=gnu11 -I. -D_FILE_OFFSET_BITS=64 -fstrict-aliasing -pipe
BUILD_CFLAGS += -Wall -Wextra -Wwrite-strings -Wcast-align -Wstrict-aliasing -pedantic -Wstrict-overflow -Wstrict-prototypes -Wpointer-arith -Wundef
BUILD_CFLAGS += -Wshadow -Wfloat-equal -Wstrict-overflow=5 -Waggregate-return -Wcast-qual -Wswitch-default -Wswitch-enum -Wunreachable-code -Wformat=2 -Winit-self -Wconversion
#LDFLAGS=-s -Wl,--gc-sections
#LDFLAGS=

prefix=/usr
exec_prefix=${prefix}
bindir=${exec_prefix}/bin
mandir=${prefix}/man
datarootdir=${prefix}/share
datadir=${datarootdir}
sysconfdir=${prefix}/etc

ifeq ($(OS), Windows_NT)
# MinGW needs this for printf() conversions to work
	WIN_CFLAGS += -D__USE_MINGW_ANSI_STDIO=1 -municode
	EXT=.exe
# Windows resources for Windows XP and NT6+
	ifeq ($(UNAME_S), MINGW32_NT-5.1)
		OBJS += winres_xp.o
	else
		OBJS += winres.o
	endif
endif

# SIMD SSE2/AVX2 implementations may need these extra flags
ifdef NO_SIMD
BUILD_CFLAGS += -DNO_SIMD -DNO_SSE2 -DNO_AVX2
else
SIMD_OBJS += jody_hash_simd.o
ifdef NO_SSE2
BUILD_CFLAGS += -DNO_SSE2
else
SIMD_OBJS += jody_hash_sse2.o
endif
ifdef NO_AVX2
BUILD_CFLAGS += -DNO_AVX2
else
SIMD_OBJS += jody_hash_avx2.o
endif
endif

ifdef PERFBENCHMARK
BUILD_CFLAGS += -DPERFBENCHMARK
endif

all: jodyhash

benchmark: jody_hash.o benchmark.o
	$(CC) -c benchmark.c $(BUILD_CFLAGS) $(CFLAGS) $(CFLAGS_EXTRA) -o benchmark.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(BUILD_CFLAGS) $(CFLAGS_EXTRA) -o benchmark jody_hash.o benchmark.o
	./benchmark 1000000

jodyhash: jody_hash.o utility.o $(OBJS) $(SIMD_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(BUILD_CFLAGS) $(WIN_CFLAGS) $(CFLAGS_EXTRA) -o jodyhash jody_hash.o utility.o $(OBJS) $(SIMD_OBJS)

jody_hash_simd.o:
	$(CC) $(CFLAGS) $(BUILD_CFLAGS) $(WIN_CFLAGS) $(CFLAGS_EXTRA) -mavx2 -msse2 -c -o jody_hash_simd.o jody_hash_simd.c

jody_hash_avx2.o: jody_hash_simd.o
	$(CC) $(CFLAGS) $(BUILD_CFLAGS) $(WIN_CFLAGS) $(CFLAGS_EXTRA) -mavx2 -c -o jody_hash_avx2.o jody_hash_avx2.c

jody_hash_sse2.o: jody_hash_simd.o
	$(CC) $(CFLAGS) $(BUILD_CFLAGS) $(WIN_CFLAGS) $(CFLAGS_EXTRA) -msse2 -c -o jody_hash_sse2.o jody_hash_sse2.c

.c.o:
	$(CC) -c $(BUILD_CFLAGS) $(CFLAGS) $(WIN_CFLAGS) $(CFLAGS_EXTRA) $<

winres.o: winres.rc winres.manifest.xml
	./tune_winres.sh
	windres winres.rc winres.o

winres_xp.o: winres_xp.rc
	./tune_winres.sh
	windres winres_xp.rc winres_xp.o

stripped: jodyhash
	strip jodyhash$(EXT)

test: jodyhash
	./test.sh

clean:
	rm -f *.o *~ .*un~ benchmark jodyhash$(SUFFIX) debug.log *.?.gz

distclean: clean
	rm -f *.pkg.tar.* *.zip
	rm -rf jodyhash-*-*/

install: all
	install -D -o root -g root -m 0755 -s jodyhash $(DESTDIR)/$(bindir)/jodyhash

chrootpackage:
	+./chroot_build.sh

package:
	+./generate_packages.sh
