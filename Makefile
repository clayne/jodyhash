CC=gcc
#CFLAGS=-Os -flto -ffunction-sections -fdata-sections -fno-unwind-tables -fno-asynchronous-unwind-tables
CFLAGS=-O2
#CFLAGS=-Og -g3
BUILD_CFLAGS = -std=gnu99 -I. -D_FILE_OFFSET_BITS=64 -fstrict-aliasing -pipe
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

# MinGW needs this for printf() conversions to work
ifeq ($(OS), Windows_NT)
	WIN_CFLAGS += -D__USE_MINGW_ANSI_STDIO=1 -municode
	EXT=.exe
endif

all: jodyhash

benchmark: jody_hash.o benchmark.o
	$(CC) -c benchmark.c $(BUILD_CFLAGS) $(CFLAGS) $(CFLAGS_EXTRA) -o benchmark.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(BUILD_CFLAGS) $(CFLAGS_EXTRA) -o benchmark jody_hash.o benchmark.o
	./benchmark 1000000

jodyhash: jody_hash.o utility.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(BUILD_CFLAGS) $(WIN_CFLAGS) $(CFLAGS_EXTRA) -o jodyhash jody_hash.o utility.o

.c.o:
	$(CC) -c $(BUILD_CFLAGS) $(CFLAGS) $(WIN_CFLAGS) $(CFLAGS_EXTRA) $<

stripped: jodyhash
	strip jodyhash$(EXT)
clean:
	rm -f *.o *~ .*un~ benchmark jodyhash debug.log *.?.gz

distclean:
	rm -f *.o *~ .*un~ benchmark jodyhash debug.log *.?.gz *.pkg.tar.*

install: all
	install -D -o root -g root -m 0755 -s jodyhash $(DESTDIR)/$(bindir)/jodyhash

package:
	+./chroot_build.sh
