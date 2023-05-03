#!/bin/sh

make clean && make -j && mv jodyhash.exe jh_simd.exe
make clean && make -j CFLAGS_EXTRA=-DNO_SIMD && mv jodyhash.exe jh_nosimd.exe

[ -z "$X" ] && X=60
while [ $X -lt 129 ]
	do
	echo $X:
	H1=$(printf "%0${X}s" "" | ./jh_nosimd.exe -)
	H2=$(printf "%0${X}s" "" | ./jh_simd.exe)
	echo -e "$H1\n$H2"
	[ "$H1" != "$H2" ] && echo "---------------------------WRONG"
	X=$((X+1))
done
