/* Jody Bruchon's fast hashing function
 *
 * This function was written to generate a fast hash that also has a
 * fairly low collision rate. The collision rate is much higher than
 * a secure hash algorithm, but the calculation is drastically simpler
 * and faster.
 *
 * Copyright (C) 2014-2023 by Jody Bruchon <jody@jodybruchon.com>
 * Released under The MIT License
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform_intrin.h"
#include "jody_hash.h"

/* DO NOT modify shifts/contants unless you know what
 * you're doing. They were chosen after lots of testing.
 * Changes will likely cause lots of hash collisions. */
#ifndef JODY_HASH_SHIFT
#define JODY_HASH_SHIFT 14
#endif

/* The constant value's purpose is to cause each byte in the
 * jodyhash_t word to have a positionally dependent variation.
 * It is injected into the calculation to prevent a string of
 * identical bytes from easily producing an identical hash. */

/* The tail mask table is used for block sizes that are
 * indivisible by the width of a jodyhash_t. It is ANDed with the
 * final jodyhash_t-sized element to zero out data in the buffer
 * that is not part of the data to be hashed. */

/* Set hash parameters based on requested hash width */
#if JODY_HASH_WIDTH == 64
#ifndef JODY_HASH_CONSTANT
#define JODY_HASH_CONSTANT 0xf20596b93bd1a710ULL
#endif
static const jodyhash_t tail_mask[] = {
	0x0000000000000000,
	0x00000000000000ff,
	0x000000000000ffff,
	0x0000000000ffffff,
	0x00000000ffffffff,
	0x000000ffffffffff,
	0x0000ffffffffffff,
	0x00ffffffffffffff,
	0xffffffffffffffff
};
#endif /* JODY_HASH_WIDTH == 64 */
#if JODY_HASH_WIDTH == 32
#ifndef JODY_HASH_CONSTANT
#define JODY_HASH_CONSTANT 0xa682a37eU
#endif
static const jodyhash_t tail_mask[] = {
	0x00000000,
	0x000000ff,
	0x0000ffff,
	0x00ffffff,
	0xffffffff
};
#endif /* JODY_HASH_WIDTH == 32 */
#if JODY_HASH_WIDTH == 16
#ifndef JODY_HASH_CONSTANT
#define JODY_HASH_CONSTANT 0x1f5bU
#endif
static const jodyhash_t tail_mask[] = {
	0x0000,
	0x00ff,
	0xffff
};
#endif /* JODY_HASH_WIDTH == 16 */

/* Double-length shift for double-rotation optimization */
#define JH_SHIFT2 ((JODY_HASH_SHIFT * 2) - (((JODY_HASH_SHIFT * 2) > JODY_HASH_WIDTH) * JODY_HASH_WIDTH))

/* Macros for bitwise rotation */
#define ROL(a)  (jodyhash_t)((a << JODY_HASH_SHIFT) | (a >> ((sizeof(jodyhash_t) * 8) - JODY_HASH_SHIFT)))
#define ROR(a)  (jodyhash_t)((a >> JODY_HASH_SHIFT) | (a << ((sizeof(jodyhash_t) * 8) - JODY_HASH_SHIFT)))
#define ROL2(a) (jodyhash_t)(a << JH_SHIFT2 | (a >> ((sizeof(jodyhash_t) * 8) - JH_SHIFT2)))
#define ROR2(a) (jodyhash_t)(a >> JH_SHIFT2 | (a << ((sizeof(jodyhash_t) * 8) - JH_SHIFT2)))

/* Maximum size of vectorized data block */
#define MAX_VEC_BLOCK 32768

/* Hash a block of arbitrary size; must be divisible by sizeof(jodyhash_t)
 * The first block should pass a start_hash of zero.
 * All blocks after the first should pass start_hash as the value
 * returned by the last call to this function. This allows hashing
 * of any amount of data. If data is not divisible by the size of
 * jodyhash_t, it is MANDATORY that the caller provide a data buffer
 * which is divisible by sizeof(jodyhash_t). */
extern jodyhash_t jody_block_hash(const jodyhash_t * restrict data,
		const jodyhash_t start_hash, const size_t count)
{
	const jodyhash_t s_constant = ROR2(JODY_HASH_CONSTANT);
	union UINT128 {
		__m128i  v128;
		uint64_t v64[2];
	};
	union UINT128 vec_constant;
	jodyhash_t hash = start_hash;
	jodyhash_t element, element2, partial_constant;
	size_t len;
	size_t vec_allocsize;
	int vec_constant_built = 0;
	__m128i *aligned_data;
	__m128i *aligned_data_clean;
	__m128i vec_i, vec_j;


	/* Don't bother trying to hash a zero-length block */
	if (count == 0) return hash;

	/* Use SSE2 if possible */
	if (count > 31) {
		if (!vec_constant_built) {
//			vec_constant.v64[0] = JODY_HASH_CONSTANT;
//			vec_constant.v64[1] = JODY_HASH_CONSTANT;
			vec_constant.v64[0] = 0x1000000000000001;
			vec_constant.v64[1] = 0x1000000000000001;
			vec_constant_built = 1;
		}
		vec_allocsize = (MAX_VEC_BLOCK > count) ? count & 0xfffffff0U : MAX_VEC_BLOCK;
		fprintf(stderr, "vec_allocsize = %ld for length %ld\n", vec_allocsize, count);
		aligned_data       = (__m128i *)aligned_alloc(32, vec_allocsize);
		aligned_data_clean = (__m128i *)aligned_alloc(32, vec_allocsize);
		if (!aligned_data || !aligned_data_clean) goto oom;
		fprintf(stderr, "aligned_data       at ptr 0x%p\n", (void *)aligned_data);
		fprintf(stderr, "aligned_data_clean at ptr 0x%p\n", (void *)aligned_data_clean);
		memcpy(aligned_data, data, vec_allocsize);
		memcpy(aligned_data_clean, aligned_data, vec_allocsize);
		len = count / sizeof(__m128i);

		for (size_t i = 0; i < len; i++) {
			fprintf(stderr, "\npointers %lu: 0x%p, 0x%p\n", i, (void *)(aligned_data + i), (void *)(((uint64_t *)aligned_data) + (i << 1) + 1));
			fprintf(stderr, "              |              ||              |\n");
			fprintf(stderr, "data start: 0x%016lx%016lx\n", *(uint64_t *)(aligned_data + i), *(((uint64_t *)aligned_data) + (i << 1) + 1));
			fprintf(stderr, "constant  : 0x%016lx%016lx\n", vec_constant.v64[0], vec_constant.v64[1]);
			vec_i = _mm_loadu_si128(&aligned_data[i]);
			vec_j = _mm_loadu_si128(&vec_constant.v128);
			vec_i = _mm_add_epi64(vec_i, vec_j);
			_mm_storeu_si128(&aligned_data[i], vec_i);
			fprintf(stderr, "data end  : 0x%016lx%016lx\n", *(uint64_t *)(aligned_data + i), *(((uint64_t *)aligned_data) + (i << 1) + 1));
	//	hash += element;
	//	hash += partial_constant;
	//	or, ROR element, hash XOR element, ROL2 hash
		hash = ROL(hash);
		hash ^= element;
		hash = ROL(hash);
		hash ^= partial_constant;
		hash += element;
		data++;
		}

		exit(EXIT_FAILURE);
	} else {
		len = count / sizeof(jodyhash_t);
	}

	for (; len > 0; len--) {
		element = *data;
		element2 = ROR(element);
		element2 ^= s_constant;
		hash += element;
		hash += JODY_HASH_CONSTANT;
		hash ^= element2;
		hash = ROL2(hash);
		hash += element;
		data++;
	}

	/* Handle data tail (for blocks indivisible by sizeof(jodyhash_t)) */
	len = count & (sizeof(jodyhash_t) - 1);
	if (len) {
		partial_constant = JODY_HASH_CONSTANT & tail_mask[len];
		element = *data & tail_mask[len];
		hash += element;
		hash += partial_constant;
		hash = ROL(hash);
		hash ^= element;
		hash = ROL(hash);
		hash ^= partial_constant;
		hash += element;
	}

	return hash;
oom:
	fprintf(stderr, "out of memory\n");
	exit(EXIT_FAILURE);
}
