CC=gcc
#CFLAGS=-Os -flto -ffunction-sections -fdata-sections -fno-unwind-tables -fno-asynchronous-unwind-tables
CFLAGS=-O2
#CFLAGS=-Og -g3
BUILD_CFLAGS=-std=gnu99 -I. -D_FILE_OFFSET_BITS=64 -pipe -Wall -pedantic
#LDFLAGS=-s -Wl,--gc-sections
#LDFLAGS=

prefix=/usr
exec_prefix=${prefix}
bindir=${exec_prefix}/bin
mandir=${prefix}/man
datarootdir=${prefix}/share
datadir=${datarootdir}
sysconfdir=${prefix}/etc

all: jodyhash

jodyhash: jody_hash.o main.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(BUILD_CFLAGS) -o jodyhash jody_hash.o main.o

.c.o:
	$(CC) -c $(BUILD_CFLAGS) $(FUSE_CFLAGS) $(CFLAGS) $<

clean:
	rm -f *.o *~ .*un~ jodyhash debug.log *.?.gz

distclean:
	rm -f *.o *~ .*un~ jodyhash debug.log *.?.gz *.pkg.tar.*

install: all
	install -D -o root -g root -m 0755 -s jodyhash $(DESTDIR)/$(bindir)/jodyhash

package:
	+./chroot_build.sh
