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

#define _GNU_SOURCE

#include "../src/common.c"
#include "../src/bitmap.c"
#include <check.h>

/* Run prior to each test case */
void checked_setup()
{
	option_verbose = 0;
}

/* Run after each test case */
void checked_teardown()
{
}

/*
 * Checks that bitmap_alloc_zero() actually zeroes the bit field.
 * Checks that bitmap_set() sets correct bits according to bitmap_isset().
 */
START_TEST(test_bitmap_set)
{
	static const int first_bit_set = 0;
	static const int last_bit_set = 4002;
	struct bitmap_t * const set = bitmap_alloc_zero();
	int bit;

	info("%s: Test case entry", __func__);

	/* Assure the bit field starts out being zeroed */
	for (bit = 0; (unsigned) bit < (set->size_bits + 5); bit++)
		ck_assert_int_eq(0, bitmap_isset(bit, set));

	/* Set bits within range */
	for (bit = first_bit_set; bit <= last_bit_set; bit++)
		bitmap_set(bit, set);

	/* Check that only the bits within the range are set */
	for (bit = 0; (unsigned) bit < (set->size_bits + 5); bit++)
		ck_assert_msg(
			(((bit >= first_bit_set) && (bit <= last_bit_set))
				? 1 : 0) == bitmap_isset(bit, set),
			"bit %d, first_bit_set %d, last_bit_set %d",
			bit, first_bit_set, last_bit_set);

	/* Cleanup */
	bitmap_free(set);

	info("%s: Test case exit", __func__);
}
END_TEST

/*
 * Checks that bitmap_alloc_set() sets the correct bit.
 */
START_TEST(test_bitmap_alloc_set)
{
	static const int bit_set = 25;
	int bit;

	/* Allocate bit field with one bit set */
	struct bitmap_t * const set = bitmap_alloc_set(bit_set);

	info("%s: Test case entry", __func__);

	/* Verify that correct bit was set */
	for (bit = 0; (unsigned) bit < (set->size_bits + 5); bit++)
		ck_assert_msg(((bit == bit_set) ? 1 : 0) ==
			bitmap_isset(bit, set),
			"bit %d, bit_set %d",
			bit, bit_set);

	/* Cleanup */
	bitmap_free(set);

	info("%s: Test case exit", __func__);
}
END_TEST

/*
 * Check bitmap_alloc_from_list() and bitmap_hex().
 */
START_TEST(test_bitmap_hex_from_list)
{
	static const char in_hex[] = "79a";
	struct bitmap_t *set;
	char *returned_hex;

	info("%s: Test case entry", __func__);

	set = bitmap_alloc_from_list("1,7-10,3-4");
	returned_hex = bitmap_hex(set);

	ck_assert_msg(strcmp(in_hex, returned_hex) == 0,
		"in_hex='%s' returned_hex='%s'", in_hex, returned_hex);

	bitmap_free(set);
	free(returned_hex);

	info("%s: Test case exit", __func__);
}
END_TEST

/*
 * Check bitmap_alloc_complement() function.
 */
START_TEST(test_bitmap_alloc_complement)
{
	static const char in_hex[] = "65";
	struct bitmap_t *set;
	struct bitmap_t *comp_set;
	char *returned_hex;

	info("%s: Test case entry", __func__);

	set = bitmap_alloc_from_list("1,7-10,3-4");
	comp_set = bitmap_alloc_complement(set);
	returned_hex = bitmap_hex(comp_set);

	ck_assert_msg(strcmp(in_hex, returned_hex) == 0,
		"in_hex='%s' returned_hex='%s'", in_hex, returned_hex);

	bitmap_free(set);
	bitmap_free(comp_set);
	free(returned_hex);

	info("%s: Test case exit", __func__);
}
END_TEST

static void try_alloc_from_mask(const char *in_mask, const char *out_mask)
{
	struct bitmap_t * const set = bitmap_alloc_from_mask(in_mask);
	char * const returned_mask = bitmap_hex(set);

	ck_assert_msg(strcmp(returned_mask, out_mask) == 0,
		"in_mask='%s' out_mask='%s' returned_mask='%s'",
		in_mask, out_mask, returned_mask);

	free(returned_mask);
}

START_TEST(test_bitmap_alloc_from_mask_1)
{
	info("%s: Test case entry", __func__);

	try_alloc_from_mask("0xf01F", "f01f");

	info("%s: Test case exit", __func__);
}
END_TEST


START_TEST(test_bitmap_alloc_from_mask_2)
{
	info("%s: Test case entry", __func__);

	try_alloc_from_mask("2Ee0", "2ee0");

	info("%s: Test case exit", __func__);
}
END_TEST

START_TEST(test_bitmap_alloc_from_u32_list)
{
	const char *mlist = "00000000,00000000,00000000,00000000,00000000,00000000,00000000,00000003";
	struct bitmap_t *set;

	info("%s: Test case entry", __func__);

	set = bitmap_alloc_from_u32_list(mlist);

	ck_assert(bitmap_isset(0, set));
	ck_assert(bitmap_isset(1, set));

	bitmap_free(set);

	info("%s: Test case exit", __func__);
}
END_TEST

START_TEST(test_bitmap_list_1)
{
	static const char list[] = "1-2,4-7,9";
	struct bitmap_t * const set = bitmap_alloc_from_list(list);
	char *returned_list = bitmap_list(set);

	info("%s: Test case entry", __func__);

	ck_assert_msg(strcmp(list, returned_list) == 0,
		"list='%s' returned_list='%s'",
		list, returned_list);

	bitmap_free(set);
	free(returned_list);

	info("%s: Test case exit", __func__);
}
END_TEST

START_TEST(test_bitmap_list_2)
{
	char *list;
	struct bitmap_t *set;
	char *returned_list;

	info("%s: Test case entry", __func__);

	asprintf(&list, "%d", 63);
	ck_assert(list != NULL);
	set = bitmap_alloc_from_list(list);
	returned_list = bitmap_list(set);
	ck_assert(returned_list != NULL);

	ck_assert_msg(strcmp(list, returned_list) == 0,
		"list='%s' returned_list='%s'",
		list, returned_list);

	bitmap_free(set);
	free(returned_list);
	free(list);

	info("%s: Test case exit", __func__);
}
END_TEST

START_TEST(test_bitmap_bit_count)
{
	struct bitmap_t * const set = bitmap_alloc_zero();
	int count;

	info("%s: Test case entry", __func__);

	count = bitmap_bit_count(set);
	ck_assert_msg(count == 0, "count=%d, expected 0", count);

	bitmap_set(0, set);
	count = bitmap_bit_count(set);
	ck_assert_msg(count == 1,
		"count=%d, expected 1", count);

	bitmap_set(64, set);
	count = bitmap_bit_count(set);
	ck_assert_msg(count == 2,
		"count=%d, expected 2", count);

	bitmap_set(127, set);
	count = bitmap_bit_count(set);
	ck_assert_msg(count == 3,
		"count=%d, expected 3", count);

	bitmap_free(set);

	info("%s: Test case exit", __func__);
}
END_TEST

Suite *
suite_bitmap(void)
{
	Suite *s = suite_create("bitmap");

	TCase *tc_core = tcase_create("Core");
	tcase_add_checked_fixture(tc_core, checked_setup, checked_teardown);
	tcase_add_test(tc_core, test_bitmap_set);
	tcase_add_test(tc_core, test_bitmap_alloc_set);
	tcase_add_test(tc_core, test_bitmap_hex_from_list);
	tcase_add_test(tc_core, test_bitmap_alloc_complement);
	tcase_add_test(tc_core, test_bitmap_alloc_from_mask_1);
	tcase_add_test(tc_core, test_bitmap_alloc_from_mask_2);
	tcase_add_test(tc_core, test_bitmap_alloc_from_u32_list);
	tcase_add_test(tc_core, test_bitmap_list_1);
	tcase_add_test(tc_core, test_bitmap_list_2);
	tcase_add_test(tc_core, test_bitmap_bit_count);
	suite_add_tcase(s, tc_core);

	return s;
}

int main(int argc, char *argv[])
{
	int number_failed;
	Suite *s = suite_bitmap();
	SRunner *sr = srunner_create(s);

	(void) argc;
	(void) argv;

	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
