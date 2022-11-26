#!/bin/sh

[ -z "$JODYHASH" ] && JODYHASH=./jodyhash
[ ! -x "$JODYHASH" ] && echo "Please build the program first, or set env JODYHASH=/path/to/jodyhash_program" && exit 128

TESTDIR=testdata
FILE1="1_million_numbers.txt"
FILE2="all_english_words_lcuc.txt"
TF1="$TESTDIR/$FILE1"
TF2="$TESTDIR/$FILE2"
GF1="$TESTDIR/hash_$FILE1"
GF2="$TESTDIR/hash_$FILE2"

GOOD1=$(cat "$GF1")
GOOD2=$(cat "$GF2")
HASH1=$($JODYHASH "$TF1")
HASH2=$($JODYHASH "$TF2")

ERR=0

[ -z "$GOOD1" ] && echo "ERROR: Read hash from '$GF1' FAILED" && exit 127
[ -z "$GOOD2" ] && echo "ERROR: Read hash from '$GF2' FAILED" && exit 126
[ -z "$HASH1" ] && echo "ERROR: Hashing file '$TF1' FAILED" && exit 125
[ -z "$HASH2" ] && echo "ERROR: Hashing file '$TF2' FAILED" && exit 124

if [ "$HASH1" != "$GOOD1" ]; then echo "Hash FAILED: $TF1"; ERR=1; else echo "Hash PASSED: $TF1"; fi
if [ "$HASH2" != "$GOOD2" ]; then echo "Hash FAILED: $TF2"; ERR=2; else echo "Hash PASSED: $TF2"; fi

exit $ERR
