#!/bin/sh

# Generate package folders with variant builds

# Number of parallel make processes
test -z "$PM" && PM=12

NAME="jodyhash"

VER="$(cat version.h | grep '#define VER "' | cut -d\" -f2)"
echo "Program version: $VER"

TA=__NONE__
PKGTYPE=gz

UNAME_S="$(uname -s | tr '[:upper:]' '[:lower:]')"
UNAME_P="$(uname -p)"
UNAME_M="$(uname -m)"

# Detect macOS
if [ "$UNAME_S" = "darwin" ]
	then
	PKGTYPE=zip
	TA=mac32
	test "$UNAME_M" = "x86_64" && TA=mac64
fi

# Detect Power Macs under macOS
if [[ "$UNAME_P" = "Power Macintosh" || "$UNAME_P" = "powerpc" ]]
	then
	PKGTYPE=zip
	TA=macppc32
	test "$(sysctl hw.cpu64bit_capable)" = "hw.cpu64bit_capable: 1" && TA=macppc64
fi

# Detect Linux
if [ "$UNAME_S" = "linux" ]
	then
	TA="linux-$UNAME_M"
	[ ! -z "$ARCH" ] && TA="linux-$ARCH"
	PKGTYPE=xz
fi

# Fall through - assume Windows
if [ "$TA" = "__NONE__" ]
	then
	PKGTYPE=zip
	TGT=$(gcc -v 2>&1 | grep Target | cut -d\  -f2- | cut -d- -f1)
	test "$TGT" = "i686" && TA=win32
	test "$TGT" = "x86_64" && TA=win64
	test "$UNAME_S" = "MINGW32_NT-5.1" && TA=winxp
	EXT=".exe"
fi

echo "Target architecture: $TA"
test "$TA" = "__NONE__" && echo "Failed to detect system type" && exit 1
PKGNAME="${NAME}-${VER}-$TA"

echo "Generating package for: $PKGNAME"
mkdir -p "$PKGNAME"
test ! -d "$PKGNAME" && echo "Can't create directory for package" && exit 1
cp CHANGES README LICENSE $PKGNAME/
E1=1; E2=1; E3=1
make clean && make -j$PM stripped && cp $NAME$EXT $PKGNAME/$NAME$EXT && E1=0
make clean && make -j$PM CFLAGS_EXTRA="-DJODY_HASH_WIDTH=32" stripped && cp $NAME$EXT $PKGNAME/${NAME}32$EXT && E2=0
make clean && make -j$PM CFLAGS_EXTRA="-DJODY_HASH_WIDTH=16" stripped && cp $NAME$EXT $PKGNAME/${NAME}16$EXT && E3=0
make clean
test $((E1 + E2 + E3)) -gt 0 && echo "Error building packages; aborting." && exit 1
# Make a fat binary on macOS x86_64 if possible
if [ "$TA" = "mac64" ] && ld -v 2>&1 | grep -q 'archs:.*i386'
	then
	E1=1; E2=1; E3=1
	BITS=32
	make clean && make -j$PM CFLAGS_EXTRA="-m32 " stripped && cp $NAME$EXT $PKGNAME/$NAME$EXT$BITS && E1=0
	make clean && make -j$PM CFLAGS_EXTRA="-m32 -DJODY_HASH_WIDTH=32" stripped && cp $NAME$EXT $PKGNAME/${NAME}32$EXT$BITS && E2=0
	make clean && make -j$PM CFLAGS_EXTRA="-m32 -DJODY_HASH_WIDTH=32" stripped && cp $NAME$EXT $PKGNAME/${NAME}16$EXT$BITS && E3=0
	make clean
	test $((E1 + E2 + E3)) -gt 0 && echo "Error building packages; aborting." && exit 1
	test "$E1" = "0" && lipo -create -output $PKGNAME/$NAME_temp $PKGNAME/$NAME$EXT$BITS $PKGNAME/$NAME$EXT && mv $PKGNAME/$NAME_temp $PKGNAME/$NAME$EXT
	test "$E2" = "0" && lipo -create -output $PKGNAME/$NAME_temp $PKGNAME/${NAME}32$EXT$BITS $PKGNAME/$NAME32$EXT && mv $PKGNAME/$NAME_temp $PKGNAME/$NAME32$EXT
	test "$E3" = "0" && lipo -create -output $PKGNAME/$NAME_temp $PKGNAME/${NAME}16$EXT$BITS $PKGNAME/$NAME16$EXT && mv $PKGNAME/$NAME_temp $PKGNAME/$NAME16$EXT
	rm -f $PKGNAME/$NAME$EXT$BITS $PKGNAME/${NAME}32$EXT$BITS $PKGNAME/${NAME}16$EXT$BITS
fi
test "$PKGTYPE" = "zip" && zip -9r $PKGNAME.zip $PKGNAME/
test "$PKGTYPE" = "gz"  && tar -c $PKGNAME/ | gzip -9 > $PKGNAME.pkg.tar.gz
test "$PKGTYPE" = "xz"  && tar -c $PKGNAME/ | xz -e > $PKGNAME.pkg.tar.xz
echo "Package generation complete."
