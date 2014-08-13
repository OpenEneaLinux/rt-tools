/*
 * Copyright (c) 2014 by Enea Software AB
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Enea Software AB nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This file implements generic bitmap functions.
 */

#include "partrt.h"

#include <stdlib.h>
#include <limits.h>
#include <sys/sysinfo.h>

/* #define VERBOSE_DEBUG */

struct bitmap_t {
	size_t size_u32;
	size_t size_bits;
	size_t max_size_bits;
	uint32_t *map;
};

struct bitmap_t *bitmap_alloc_zero(size_t max_size_bits)
{
	struct bitmap_t * const set = checked_malloc(sizeof (struct bitmap_t));

	set->size_bits = max_size_bits;
	set->size_u32 = (set->size_bits / 32) + 1;
	set->max_size_bits = max_size_bits;

	set->map = checked_malloc(sizeof (uint32_t) * set->size_u32);
	memset(set->map, 0, sizeof (uint32_t) * set->size_u32);

#ifdef VERBOSE_DEBUG
	debug("%s(): Returning %p", __func__, set);
#endif

	return set;
}

void bitmap_free(struct bitmap_t *set)
{
#ifdef VERBOSE_DEBUG
	debug("%s(%p)", __func__, set);
#endif
	set->size_u32 = 0;
	set->size_bits = 0;
	free(set);
}

void bitmap_set(int bit, struct bitmap_t *set)
{
	if (bit < 0)
		fail("%s: Illegal bit index %d", __func__, bit);

	if ((set->max_size_bits > 0) && ((unsigned) bit >= set->max_size_bits))
		fail("%s: Bit %d exceeds max size", __func__, bit);

	if ((unsigned) bit >= set->size_bits) {
		set->size_bits = bit + 1;
		if ((set->size_bits / 32) > set->size_u32) {
			const size_t old_size_u32 = set->size_u32;

			set->size_u32 = (set->size_bits / 32) + 1;
			set->map = checked_realloc(
				set->map, sizeof (uint32_t) * set->size_u32);
			memset(&set->map[old_size_u32], 0,
				(set->size_u32 - old_size_u32)
				* sizeof (uint32_t));
		}
	}

	set->map[bit / 32] |= ((uint32_t) 1 << (bit % 32));

#ifdef VERBOSE_DEBUG
	debug("%s(%d,%p): size u32: %zu, size bits: %zu, bit in u32: %d, map[%d]: 0x%x",
		__func__, bit, set, set->size_u32, set->size_bits,
		bit % 32, bit/32, set->map[bit / 32]);
#endif
}

struct bitmap_t *bitmap_alloc_set(int bit, size_t max_size_bits)
{
	struct bitmap_t *set = bitmap_alloc_zero(max_size_bits);

	if (bit < 0)
		fail("%s: Illegal bit index %d", __func__, bit);

	bitmap_set(bit, set);

	return set;
}

int bitmap_isset(int bit, const struct bitmap_t *set)
{
	if (bit < 0)
		fail("%s: Illegal bit index %d", __func__, bit);

	if ((unsigned) bit >= set->size_bits)
		return 0;

#ifdef VERBOSE_DEBUG
	debug("%s(%d,%p): size u32: %zu, size bits: %zu, map[%d]: 0x%x, value %d",
		__func__, bit, set, set->size_u32, set->size_bits,
		bit/32, set->map[bit / 32],
		(set->map[bit / 32] & ((uint32_t) 1 << (bit % 32))) == 0 ? 0 : 1
);
#endif

	return (set->map[bit / 32] & (1 << (bit % 32))) == 0 ? 0 : 1;
}

size_t bitmap_bit_count(const struct bitmap_t *set)
{
	size_t nr_bits = 0;
	size_t bit;

	for (bit = 0; bit < set->size_bits; bit++)
		if (bitmap_isset(bit, set))
			nr_bits++;

	return nr_bits;
}

char *bitmap_hex(const struct bitmap_t *set)
{
	char *curr;
	int bit = 0;
	const int nr_bits = set->size_bits;
	const size_t str_size = 1 /* NUL */ + ((nr_bits+3) / 4) /* round up */;
	char *str = checked_malloc(str_size);

	curr = str + str_size - 1;
	*curr = '\0';

	while (bit < nr_bits) {
		char ch = 0;
		int nibble_bit;
		for (nibble_bit = 0; nibble_bit < 4; nibble_bit++, bit++) {
			if (bitmap_isset(bit, set))
				ch |= (1 << nibble_bit);
		}
		curr--;
		if (ch > 9)
			*curr = 'a' + (ch - 10);
		else
			*curr = '0' + ch;
	}

	return str;
}

static int bitmap_list_write(size_t curr_idx, size_t size_alloced,
			int first_bit, int last_bit, char *str)
{
	int status;

	if (first_bit == last_bit)
		status = snprintf(str + curr_idx, size_alloced - curr_idx,
				"%s%d", (curr_idx > 0) ? "," : "", first_bit);
	else
		status = snprintf(str + curr_idx, size_alloced - curr_idx,
				"%s%d-%d", (curr_idx > 0) ? "," : "",
				first_bit, last_bit);

	if (status < 0)
		fail("%s: Internal error: snprintf() returned error", __func__);

	return status;
}

char *bitmap_list(const struct bitmap_t *set)
{
	int curr_idx = 0;
	int size_alloced = 0;
	char *str = NULL;
	const int nr_bits = set->size_bits;
	int bit;
	int first_bit = -1;
	int last_bit = -1;

	for (bit = 0; bit <= nr_bits; bit++) {
		int bytes_needed;

		if (first_bit == -1) {
			if (bitmap_isset(bit, set))
				first_bit = bit;
			continue;
		}

		if (bitmap_isset(bit, set))
			continue;

		last_bit = bit - 1;
		bytes_needed = bitmap_list_write(
			curr_idx, size_alloced, first_bit, last_bit, str);
		if (bytes_needed >= (size_alloced - curr_idx)) {
			str = checked_realloc(
				str, size_alloced + bytes_needed + 1);
			size_alloced += bytes_needed + 1;
			bytes_needed = bitmap_list_write(
				curr_idx, size_alloced, first_bit,
				last_bit, str);

			if (bytes_needed >= (size_alloced - curr_idx))
				fail("bitmap_list(): Internal error: Did not allocate enough. bytes_needed=%d, size_alloced=%d, curr_idx=%d",
					bytes_needed, size_alloced, curr_idx);
		}

		curr_idx += bytes_needed;
		first_bit = -1;
	}

	if (str == NULL) {
		str = checked_malloc(1);
		str[0] = '\0';
	}
	return str;
}

struct bitmap_t *bitmap_alloc_from_list(const char *list, size_t max_size_bits)
{
	const char * const original_list = list;
	struct bitmap_t *set = bitmap_alloc_zero(max_size_bits);

	while (*list != '\0') {
		int bit;
		char *endptr;
		long range_first;
		long range_last;

		range_first = strtol(list, &endptr, 0);
		if (endptr == list)
			fail("%s: Malformed bitmap", original_list);
		if (range_first > INT_MAX)
			fail("%s: %ld is out of range", original_list,
				range_first);
		list = endptr;
		if (*list == '-') {
			list++;
			range_last = strtol(list, &endptr, 0);
			if (endptr == list)
				fail("%s: Malformed bitmap", original_list);
			if (range_last > INT_MAX)
				fail("%s: %ld is out of range",
					original_list, range_last);
			list = endptr;
		} else {
			range_last = range_first;
		}

		/* Set all bits in range */
		for (bit = range_first; bit <= range_last; bit++)
			bitmap_set(bit, set);

		if (*list == ',')
			list++;
	}

	return set;
}

struct bitmap_t *bitmap_alloc_complement(const struct bitmap_t *set)
{
	struct bitmap_t * const comp_set = bitmap_alloc_zero(set->max_size_bits);
	const int nr_bits = set->size_bits;
	int bit;

	for (bit = 0; bit < nr_bits; bit++) {
		if (! bitmap_isset(bit, set))
			bitmap_set(bit, comp_set);
	}

	return comp_set;
}

static struct bitmap_t *alloc_from_mask(const char *mask, char ignore_char,
	size_t max_size_bits)
{
	struct bitmap_t * const set = bitmap_alloc_zero(max_size_bits);
	const char *curr;
	int bit = 0;

	if (strncmp(mask, "0x", 2) == 0)
		mask += 2;

	for (curr = mask + strlen(mask) - 1;
	     curr >= mask;
	     curr--) {
		int val;
		const int starting_bit = bit;

		if ((ignore_char != '\0') && (*curr == ignore_char))
			continue;

		if ((*curr >= '0') && (*curr <= '9'))
			val = *curr - '0';
		else if ((*curr >= 'a') && (*curr <= 'f'))
			val = *curr - 'a' + 10;
		else if ((*curr >= 'A') && (*curr <= 'F'))
			val = *curr - 'A' + 10;
		else
			fail("%s: Character '%c' is not legal in hexadecimal mask", mask, *curr);

		for (; bit < (starting_bit + 4); bit++) {
			if (val & (1 << (bit - starting_bit)))
				bitmap_set(bit, set);
		}
	}

	return set;
}

struct bitmap_t *bitmap_alloc_from_mask(const char *mask, size_t max_size_bits)
{
	return alloc_from_mask(mask, '\0', max_size_bits);
}

struct bitmap_t *bitmap_alloc_from_u32_list(const char *mlist,
					size_t max_size_bits)
{
	return alloc_from_mask(mlist, ',', max_size_bits);
}
