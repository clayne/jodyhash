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
	__m128i *aligned_data, *aligned_data_e;
	__m128i v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12;
	__m128i vec_const, vec_ror2;
#endif


	/* Don't bother trying to hash a zero-length block */
	if (count == 0) return hash;

#ifndef NOVEC
	/* Use SSE2 if possible */
	vec_constant.v64[0]      = JODY_HASH_CONSTANT;
	vec_constant.v64[1]      = JODY_HASH_CONSTANT;
	vec_constant_ror2.v64[0] = JODY_HASH_CONSTANT_ROR2;
	vec_constant_ror2.v64[1] = JODY_HASH_CONSTANT_ROR2;
	vec_const = _mm_load_si128(&vec_constant.v128);
	vec_ror2  = _mm_load_si128(&vec_constant_ror2.v128);
	if (count > 63) {
		/* Only handle 64-byte sized chunks and leave the rest */
		vec_allocsize =  count & 0xffffffffffffffc0U;
		aligned_data_e = (__m128i *)aligned_alloc(32, vec_allocsize);
		aligned_data  = (__m128i *)aligned_alloc(32, vec_allocsize);
		if (!aligned_data_e || !aligned_data) goto oom;
		memcpy(aligned_data, data, vec_allocsize);
		len = vec_allocsize / 16; // sizeof(__m128i)

		/* Constants preload */
		for (size_t i = 0; i < len; i += 4) {
			v1  = _mm_load_si128(&aligned_data[i]);
			v3  = _mm_load_si128(&aligned_data[i]);
			v4  = _mm_load_si128(&aligned_data[i+1]);
			v6  = _mm_load_si128(&aligned_data[i+1]);
			v7  = _mm_load_si128(&aligned_data[i+2]);
			v9  = _mm_load_si128(&aligned_data[i+2]);
			v10 = _mm_load_si128(&aligned_data[i+3]);
			v12 = _mm_load_si128(&aligned_data[i+3]);
			/* "element2" gets RORed */
			v1  = _mm_srli_epi64(v1, JODY_HASH_SHIFT);
			v2  = _mm_slli_epi64(v3, (64 - JODY_HASH_SHIFT));
			v1  = _mm_or_si128(v1, v2);
			v1  = _mm_xor_si128(v1, vec_ror2);  // XOR against the ROR2 constant
			v4  = _mm_srli_epi64(v4, JODY_HASH_SHIFT);  // Repeat for all vectors
			v5  = _mm_slli_epi64(v6, (64 - JODY_HASH_SHIFT));
			v4  = _mm_or_si128(v4, v5);
			v4  = _mm_xor_si128(v4, vec_ror2);
			v7  = _mm_srli_epi64(v7, JODY_HASH_SHIFT);
			v8  = _mm_slli_epi64(v9, (64 - JODY_HASH_SHIFT));
			v7  = _mm_or_si128(v7, v8);
			v7  = _mm_xor_si128(v7, vec_ror2);
			v10 = _mm_srli_epi64(v10, JODY_HASH_SHIFT);
			v11 = _mm_slli_epi64(v12, (64 - JODY_HASH_SHIFT));
			v10 = _mm_or_si128(v10, v11);
			v10 = _mm_xor_si128(v10, vec_ror2);
			/* Add the constant to "element" */
			v3  = _mm_add_epi64(v3,  vec_const);
			v6  = _mm_add_epi64(v6,  vec_const);
			v9  = _mm_add_epi64(v9,  vec_const);
			v12 = _mm_add_epi64(v12, vec_const);
#ifdef USE_SSE2
			/* Store everything */
			_mm_store_si128(&aligned_data[i], v1);
			_mm_store_si128(&aligned_data_e[i], v3);
			_mm_store_si128(&aligned_data[i+1], v4);
			_mm_store_si128(&aligned_data_e[i+1], v6);
			_mm_store_si128(&aligned_data[i+2], v7);
			_mm_store_si128(&aligned_data_e[i+2], v9);
			_mm_store_si128(&aligned_data[i+3], v10);
			_mm_store_si128(&aligned_data_e[i+3], v12);

			/* Perform the rest of the hash normally */
			uint64_t *ep1 = (uint64_t *)(aligned_data_e) + (i << 1);
			uint64_t *ep2 = (uint64_t *)(aligned_data) + (i << 1);

			for (size_t j = 0; j < 8; j++) {
				element = *(ep1 + j);
				element2 = *(ep2 + j);
				hash += element;
				hash ^= element2;
				hash = ROL2(hash);
				hash += element;
			}
#endif
#ifdef USE_SSE4
			for (size_t j = 0; j < 8; j++) {
				switch (j) {
					default:
					case 0:
						element  = (uint64_t)_mm_extract_epi64(v3, 0);
						element2 = (uint64_t)_mm_extract_epi64(v1, 0);
						break;
					case 1:
						element  = (uint64_t)_mm_extract_epi64(v3, 1);
						element2 = (uint64_t)_mm_extract_epi64(v1, 1);
						break;
					case 2:
						element  = (uint64_t)_mm_extract_epi64(v6, 0);
						element2 = (uint64_t)_mm_extract_epi64(v4, 0);
						break;
					case 3:
						element  = (uint64_t)_mm_extract_epi64(v6, 1);
						element2 = (uint64_t)_mm_extract_epi64(v4, 1);
						break;
					case 4:
						element  = (uint64_t)_mm_extract_epi64(v9, 0);
						element2 = (uint64_t)_mm_extract_epi64(v7, 0);
						break;
					case 5:
						element  = (uint64_t)_mm_extract_epi64(v9, 1);
						element2 = (uint64_t)_mm_extract_epi64(v7, 1);
						break;
					case 6:
						element  = (uint64_t)_mm_extract_epi64(v12, 0);
						element2 = (uint64_t)_mm_extract_epi64(v10, 0);
						break;
					case 7:
						element  = (uint64_t)_mm_extract_epi64(v12, 1);
						element2 = (uint64_t)_mm_extract_epi64(v10, 1);
						break;
				}
				hash += element; hash ^= element2; hash = ROL2(hash); hash += element;
			}
#endif
		}

		free(aligned_data_e); free(aligned_data);
		data += vec_allocsize / sizeof(jodyhash_t);
		len = (count - vec_allocsize) / sizeof(jodyhash_t);
	} else {
		len = count / sizeof(jodyhash_t);
	}
#else
	len = count / sizeof(jodyhash_t);
#endif /* NOVEC */

	/* Handle tails or everything */
	for (; len > 0; len--) {
		element = *data;
		element2 = ROR(element);
		element2 ^= s_constant;
		element += JODY_HASH_CONSTANT;
		hash += element;
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
