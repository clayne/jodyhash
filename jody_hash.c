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

/* DO NOT modify shifts/contants unless you know what you're doing. They were
 * chosen after lots of testing. Changes will likely cause lots of hash
 * collisions. The vectorized versions also use constants that have this value
 * "baked in" which must be updated before using them. */
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
#define JODY_HASH_CONSTANT           0xf20596b93bd1a710ULL
#define JODY_HASH_CONSTANT_ROR2      0xbd1a710f20596b93ULL
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
	jodyhash_t hash = start_hash;
	jodyhash_t element, element2, partial_constant;
	size_t len;
#ifndef NOVEC
	union UINT128 {
		__m128i  v128;
		uint64_t v64[2];
	};
	union UINT128 vec_constant, vec_constant_ror2;
	size_t vec_allocsize;
	int vec_constant_built = 0;
	__m128i *aligned_data;
	__m128i *aligned_data_e;
	__m128i vec1, vec2, vec3, vec_const, vec_ror2;
#endif


	/* Don't bother trying to hash a zero-length block */
	if (count == 0) return hash;

#ifndef NOVEC
	/* Use SSE2 if possible */
	if (count > 15) {
		if (!vec_constant_built) {
			vec_constant.v64[0]      = JODY_HASH_CONSTANT;
			vec_constant.v64[1]      = JODY_HASH_CONSTANT;
			vec_constant_ror2.v64[0] = JODY_HASH_CONSTANT_ROR2;
			vec_constant_ror2.v64[1] = JODY_HASH_CONSTANT_ROR2;
			vec_constant_built = 1;
		}
		/* Only handle 16-byte sized chunks and leave the rest */
		vec_allocsize =  count & 0xfffffffffffffff0U;
//fprintf(stderr, "vec_allocsize = %ld for length %ld\n", vec_allocsize, count);
		aligned_data_e = (__m128i *)aligned_alloc(32, vec_allocsize);
		aligned_data  = (__m128i *)aligned_alloc(32, vec_allocsize);
		if (!aligned_data_e || !aligned_data) goto oom;
//fprintf(stderr, "aligned_data_e at ptr 0x%p\n", (void *)aligned_data_e);
//fprintf(stderr, "aligned_data   at ptr 0x%p\n", (void *)aligned_data);
		memcpy(aligned_data, data, vec_allocsize);
//fprintf(stderr, "data %p\n", (const void *)data);
//OK	element = *data;
		len = count / 16; // sizeof(__m128i)
//fprintf(stderr, "len start: %lu\n", len);

		/* Constants preload */
		vec_const = _mm_load_si128(&vec_constant.v128);
		vec_ror2  = _mm_load_si128(&vec_constant_ror2.v128);
		for (size_t i = 0; i < len; i++) {
			vec1 = _mm_load_si128(&aligned_data[i]);
			/* "element2" - OR copies register, then ROR by ORing partial shifts together */
			vec2 = _mm_setzero_si128();
			vec3 = _mm_or_si128(vec1, vec2); // Need second copy for later add phase
			vec1 = _mm_or_si128(vec1, vec2);
//_mm_store_si128(&aligned_data[i], vec1); _mm_store_si128(&aligned_data_e[i], vec3);
//fprintf(stderr, "dataE copy1: %016lx %016lx\n", *(uint64_t *)(aligned_data_e + i),   *(((uint64_t *)aligned_data_e) + (i << 1) + 1));
//fprintf(stderr, "data  copy1: %016lx %016lx\n", *(uint64_t *)(aligned_data + i),   *(((uint64_t *)aligned_data) + (i << 1) + 1));
			vec1 = _mm_srli_epi64(vec1, JODY_HASH_SHIFT);
			vec2 = _mm_slli_epi64(vec3, (64 - JODY_HASH_SHIFT));
//_mm_store_si128(&aligned_data[i], vec1); _mm_store_si128(&aligned_data_e[i], vec2);
//fprintf(stderr, "data  shft?: %016lx %016lx\n", *(uint64_t *)(aligned_data + i),   *(((uint64_t *)aligned_data) + (i << 1) + 1));
			vec1 = _mm_or_si128(vec1, vec2);
//_mm_store_si128(&aligned_data[i], vec1); _mm_store_si128(&aligned_data_e[i], vec2);
//fprintf(stderr, "dataE shft?: %016lx %016lx\n", *(uint64_t *)(aligned_data_e + i),   *(((uint64_t *)aligned_data_e) + (i << 1) + 1));
//fprintf(stderr, "data  shft2: %016lx %016lx\n", *(uint64_t *)(aligned_data + i),   *(((uint64_t *)aligned_data) + (i << 1) + 1));
//OK	element2 = ROR(element);
			/* XOR against the ROR2 constant */
			vec1 = _mm_xor_si128(vec1, vec_ror2);
			_mm_store_si128(&aligned_data[i], vec1);
//_mm_store_si128(&aligned_data_e[i], vec_ror2);
//fprintf(stderr, "data  const: %016lx %016lx\n", *(uint64_t *)(aligned_data_e + i), *(((uint64_t *)aligned_data_e) + (i << 1) + 1));
//_mm_store_si128(&aligned_data[i], vec1); _mm_store_si128(&aligned_data_e[i], vec3);
//fprintf(stderr, "data  XOR 3: %016lx %016lx\n", *(uint64_t *)(aligned_data + i),   *(((uint64_t *)aligned_data) + (i << 1) + 1));
//OK	element2 ^= s_constant;
			/* Add the constant to "element" */
			vec3 = _mm_add_epi64(vec3, vec_const);
			_mm_store_si128(&aligned_data_e[i], vec3);
//OK	hash += JODY_HASH_CONSTANT; - done by pre-adding to data
//fprintf(stderr, "dataE ADD 4: %016lx %016lx\n", *(uint64_t *)(aligned_data_e + i), *(((uint64_t *)aligned_data_e) + (i << 1) + 1));
		}

		len = count / sizeof(__m128i) * 2;  // reset for second loop

//fprintf(stderr, "len phas2: %lu\n", len);
		for (size_t i = 0; i < len; i++) {
			element = *((uint64_t *)aligned_data_e + i);
			element2 = *((uint64_t *)aligned_data + i);
//fprintf(stderr, "SIMD x [%ld]: %016lx %016lx\n", i, element, element2);
			hash += element;
			hash ^= element2;
			hash = ROL2(hash);
			hash += element;
//fprintf(stderr, "hash 5 [%ld]: %016lx\n", len, hash);
		}
		free(aligned_data_e); free(aligned_data);
		data += vec_allocsize / sizeof(jodyhash_t);
		len = (count - vec_allocsize) / sizeof(jodyhash_t);
//fprintf(stderr, "data = %p, vec_allocsize %lu, len %lu\n", (const void *)data, vec_allocsize, len);
	} else {
		len = count / sizeof(jodyhash_t);
	}
#else
	len = count / sizeof(jodyhash_t);
#endif /* NOVEC */

	/* Handle tails or everything */
	for (; len > 0; len--) {
		element = *data;
//fprintf(stderr, "copy 1 [%ld]: %016lx EMPTY\n", len, element);
		element2 = ROR(element);
//fprintf(stderr, "shft 2 [%ld]: %016lx %016lx\n", len, 0, element2);
		element2 ^= s_constant;
//fprintf(stderr, "XOR  3 [%ld]: %016lx %016lx\n", len, 0, element2);
		element += JODY_HASH_CONSTANT;
//fprintf(stderr, "ADD  4 [%ld]: %016lx %016lx\n", len, element, 0);
		hash += element;
		hash ^= element2;
		hash = ROL2(hash);
		hash += element;
//fprintf(stderr, "hash 5![%ld]: %016lx\n", len, hash);
		data++;
	}

	/* Handle data tail (for blocks indivisible by sizeof(jodyhash_t)) */
	len = count & (sizeof(jodyhash_t) - 1);
	if (len) {
		partial_constant = JODY_HASH_CONSTANT & tail_mask[len];
		element = *data & tail_mask[len];
		hash += partial_constant;
		hash += element;
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
