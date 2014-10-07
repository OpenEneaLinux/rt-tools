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

#include "common.h"
#include "bitmap.h"

#include <stdlib.h>
#include <limits.h>
#include <sys/sysinfo.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#ifndef NDEBUG
#include <ctype.h>
#endif

#define MAX(a,b) ((a) > (b)) ? (a) : (b)

/*
 * Dynamic sized bitmap.
 */
struct bitmap_t {
	/* Allocated size of bitmap */
	size_t size_u32;

	/* Highest bit that has been set to a value */
	size_t size_bits;

	/* The malloc'ed bitmap */
	uint32_t *map;
};

struct bitmap_t *bitmap_alloc_zero(void)
{
	struct bitmap_t *const set = checked_malloc(sizeof(struct bitmap_t));

	set->size_bits = 0;
	set->size_u32 = 0;

	set->map = NULL;

	return set;
}

void bitmap_free(struct bitmap_t *set)
{
	set->size_u32 = 0;
	set->size_bits = 0;
	free(set);
}

static void bitmap_set_bit(size_t bit, int value, struct bitmap_t *set)
{
	if (bit >= set->size_bits) {
		set->size_bits = bit + 1;
		if ((set->size_bits / 32) >= set->size_u32) {
			const size_t old_size_u32 = set->size_u32;

			set->size_u32 = (set->size_bits / 32) + 1;

			set->map =
			    checked_realloc(set->map,
					    sizeof(uint32_t) * set->size_u32);
			memset(&set->map[old_size_u32], 0,
			       (set->size_u32 - old_size_u32)
			       * sizeof(uint32_t));
		}
	}

	if (value)
		set->map[bit / 32] |= ((uint32_t) 1 << (bit % 32));
	else
		set->map[bit / 32] &= ~((uint32_t) 1 << (bit % 32));
}

struct bitmap_t *bitmap_alloc_set(size_t bit)
{
	struct bitmap_t *set = bitmap_alloc_zero();

	bitmap_set_bit(bit, 1, set);

	return set;
}

struct bitmap_t *bitmap_alloc_nr_bits(size_t nr_bits)
{
	size_t bit;
	struct bitmap_t *set;

	if (nr_bits == 0)
		return bitmap_alloc_zero();

	bit = nr_bits - 1;

	set = bitmap_alloc_set(bit);

	while (bit > 0) {
		bit--;
		bitmap_set_bit(bit, 1, set);
	}

	return set;
}

int bitmap_isset(size_t bit, const struct bitmap_t *set)
{
	if (bit >= set->size_bits)
		return 0;

	return ((set->map[bit / 32] & (uint32_t)(1 << (bit % 32))) == 0)
		? 0 : 1;
}

size_t bitmap_bit_count(const struct bitmap_t * set)
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
	size_t bit = 0;
	const size_t nr_bits = set->size_bits;
	const size_t str_size =
	    1 /* NUL */  + ((nr_bits + 3) / 4) /* round up */ ;
	char *str = checked_malloc(str_size);

	curr = str + str_size - 1;
	*curr = '\0';

	while (bit < nr_bits) {
		char ch = 0;
		int nibble_bit;
		for (nibble_bit = 0; nibble_bit < 4; nibble_bit++, bit++) {
			if (bitmap_isset(bit, set))
				ch |= (char)(1 << nibble_bit);
		}
		curr--;
		if (ch > 9)
			*curr = (char)('a' + (ch - 10));
		else
			*curr = (char)('0' + ch);
	}

	return str;
}

static size_t bitmap_list_write(size_t curr_idx, size_t size_alloced,
			     size_t first_bit, size_t last_bit, char *str)
{
	int status;

	if (first_bit == last_bit)
		status = snprintf(str + curr_idx, size_alloced - curr_idx,
				  "%s%zu", (curr_idx > 0) ? "," : "", first_bit);
	else
		status = snprintf(str + curr_idx, size_alloced - curr_idx,
				  "%s%zu-%zu", (curr_idx > 0) ? "," : "",
				  first_bit, last_bit);

	if (status < 0)
		fail("%s: Internal error: snprintf() returned error", __func__);

	return (size_t) status;
}

char *bitmap_list(const struct bitmap_t *set)
{
	size_t curr_idx = 0;
	size_t size_alloced = 0;
	char *str = NULL;
	const size_t nr_bits = set->size_bits;
	size_t bit;
	size_t first_bit_set = 0;
	size_t first_bit = 0;
	size_t last_bit;

	for (bit = 0; bit <= nr_bits; bit++) {
		size_t bytes_needed;

		if (!first_bit_set) {
			if (bitmap_isset(bit, set)) {
				first_bit = bit;
				first_bit_set = 1;
			}
			continue;
		}

		if (bitmap_isset(bit, set))
			continue;

		last_bit = bit - 1;
		bytes_needed =
		    bitmap_list_write(curr_idx, size_alloced, first_bit,
				      last_bit, str);
		if (bytes_needed >= (size_alloced - curr_idx)) {
			str =
			    checked_realloc(str,
					    size_alloced + bytes_needed + 1);
			size_alloced += bytes_needed + 1;
			bytes_needed =
			    bitmap_list_write(curr_idx, size_alloced, first_bit,
					      last_bit, str);

			assert(bytes_needed < (size_alloced - curr_idx));
		}

		curr_idx += bytes_needed;
		first_bit_set = 0;
	}

	if (str == NULL) {
		str = checked_malloc(1);
		str[0] = '\0';
	}
	return str;
}

char *bitmap_u32list(const struct bitmap_t *set)
{
	size_t bit;
	char *str = checked_malloc(set->size_bits + (set->size_bits / 32) + 1);
	size_t str_idx = 0;
	char ch = 0;
	int has_written = 0;

	if (set->size_bits == 0) {
		*str = '\0';
		return str;
	}

	/* Iterate backwards using bit as iterator */
	bit = set->size_bits;
	do {
		char nibble;

		bit--;

		nibble = bit % 4;

		if (bitmap_isset(bit, set))
			ch |= (char)(1 << nibble);

		/* Finished this nibble, write the character and start new nibble */
		if (nibble == 0) {
			if (ch < 10)
				str[str_idx] = (char)( '0' + ch);
			else
				str[str_idx] = (char)('a' + ch - 10);

			assert(isalnum(str[str_idx]));
			str_idx++;
			ch = 0;
			has_written = 1;
		}

		/* Write , at 32 bit interval, but avoid starting or ending with , */
		if (has_written && (bit % 32 == 0) && (bit != 0))
			str[str_idx++] = ',';
	} while (bit > 0);

	str[str_idx] = '\0';

	return str;
}

struct bitmap_t *bitmap_alloc_from_list(const char *list)
{
	const char *const original_list = list;
	struct bitmap_t *set = bitmap_alloc_zero();

	while (*list != '\0') {
		size_t bit;
		char *endptr;
		size_t range_first;
		size_t range_last;

		range_first = strtoul(list, &endptr, 0);
		if (endptr == list)
			fail("%s: Malformed bitmap", original_list);
		list = endptr;
		if (*list == '-') {
			list++;
			range_last = strtoul(list, &endptr, 0);
			if (endptr == list)
				fail("%s: Malformed bitmap", original_list);
			list = endptr;
		} else {
			range_last = range_first;
		}

		if (range_first > range_last) {
			const size_t range_tmp = range_first;
			range_first = range_last;
			range_last = range_tmp;
		}

		/* Set all bits in range */
		for (bit = range_first; bit <= range_last; bit++)
			bitmap_set_bit(bit, 1, set);

		if (*list == ',')
			list++;
	}

	return set;
}

struct bitmap_t *bitmap_alloc_from_u32_list(const char *mask)
{
	struct bitmap_t *const set = bitmap_alloc_zero();
	const char *curr;
	size_t bit = 0;

	if (strncmp(mask, "0x", 2) == 0)
		mask += 2;

	for (curr = mask + strlen(mask) - 1; curr >= mask; curr--) {
		int val;
		const size_t starting_bit = bit;

		if (*curr == ',')
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
			bitmap_set_bit(bit,
				(val & (1 << (bit - starting_bit))) != 0,
				set);
		}
	}

	return set;
}

struct bitmap_t *bitmap_and(struct bitmap_t *first, struct bitmap_t *second)
{
	const size_t nr_bits = MAX(first->size_bits, second->size_bits);
	struct bitmap_t *result = bitmap_alloc_zero();
	size_t bit;

	for (bit = 0; bit < nr_bits; bit++)
		bitmap_set_bit(bit,
			       bitmap_isset(bit, first) & bitmap_isset(bit,
								       second),
			       result);

	return result;
}

struct bitmap_t *bitmap_xor(struct bitmap_t *first, struct bitmap_t *second)
{
	const size_t nr_bits = MAX(first->size_bits, second->size_bits);
	struct bitmap_t *result = bitmap_alloc_zero();
	size_t bit;

	for (bit = 0; bit < nr_bits; bit++)
		bitmap_set_bit(bit,
			       bitmap_isset(bit, first) ^ bitmap_isset(bit,
								       second),
			       result);

	return result;
}
