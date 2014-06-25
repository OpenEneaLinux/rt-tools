#include "../src/partrt_cpumask.c"
#include <check.h>

#define TESTED_NR_CPUS 1025

static int configured_nr_cpus = TESTED_NR_CPUS;

int get_nprocs_conf(void)
{
	return configured_nr_cpus;
}

/*
 * Check that nr_cpus() is caching the number of CPUs, since the implementation
 * depends on the fact that all calls will return the same value. But only if
 * get_nprocs_conf() returns success (> 0).
 */
START_TEST(test_cpumask_nr_cpus_fail_1)
{
	configured_nr_cpus = -1;
	cpumask_nr_cpus(); /* Should fail with exit(EXIT_FAILURE) */
}
END_TEST

START_TEST(test_cpumask_nr_cpus_fail_2)
{
	configured_nr_cpus = 0;
	cpumask_nr_cpus(); /* Should fail with exit(EXIT_FAILURE) */
}
END_TEST

START_TEST(test_cpumask_nr_cpus_success)
{
	configured_nr_cpus = TESTED_NR_CPUS;
	ck_assert_int_eq(TESTED_NR_CPUS, cpumask_nr_cpus());
}
END_TEST

/*
 * Checks that cpumask_alloc_zero() actually zeroes the bit field.
 * Checks that cpumask_set() sets correct bits according to cpumask_isset().
 */
START_TEST(test_cpumask_set)
{
	static const int first_bit_set = 1000;
	static const int last_bit_set = 1002;
	const int nr_cpus = cpumask_nr_cpus();
	cpu_set_t * const set = cpumask_alloc_zero();
	int cpu;

	/* Assure the bit field starts out being zeroed */
	for (cpu = 0; cpu < (nr_cpus + 5); cpu++)
		ck_assert_int_eq(0, cpumask_isset(cpu, set));

	/* Set bits within range */
	for (cpu = first_bit_set; cpu <= last_bit_set; cpu++)
		cpumask_set(cpu, set);

	/* Check that only the bits within the range are set */
	for (cpu = 0; cpu < (nr_cpus + 5); cpu++)
		ck_assert_int_eq(
			((cpu >= first_bit_set) && (cpu <= last_bit_set))
			? 1 : 0,
			cpumask_isset(cpu, set));

	/* Cleanup */
	cpumask_free(set);
}
END_TEST

/*
 * Checks that cpumask_alloc_set() sets the correct bit.
 */
START_TEST(test_cpumask_alloc_set)
{
	const int nr_cpus = cpumask_nr_cpus();
	static const int bit_set = 25;
	int cpu;

	/* Allocate bit field with one bit set */
	cpu_set_t * const set = cpumask_alloc_set(bit_set);

	/* Verify that correct bit was set */
	for (cpu = 0; cpu < (nr_cpus + 5); cpu++)
		ck_assert_int_eq((cpu == bit_set) ? 1 : 0,
				cpumask_isset(cpu, set));

	/* Cleanup */
	cpumask_free(set);
}
END_TEST

/*
 * Checks that cpumask_alloc_set() fails for illegal cpu values.
 */
START_TEST(test_cpumask_alloc_set_illegal)
{
	static const int bit_set = TESTED_NR_CPUS + 1;

	/* Allocate bit field with one bit set */
	cpumask_alloc_set(bit_set);

	/* Expecting exit(EXIT_FAILURE) here */
}
END_TEST

/*
 * Check cpumask_alloc_from_list() and cpumask_hex().
 */
START_TEST(test_cpumask_hex_from_list)
{
	static const char in_hex[] = "79a";
	cpu_set_t *set;
	char *returned_hex;

	configured_nr_cpus = 11;
	set = cpumask_alloc_from_list("1,7-10,3-4");
	returned_hex = cpumask_hex(set);

	ck_assert_msg(strcmp(in_hex, returned_hex) == 0,
		"in_hex='%s' returned_hex='%s'", in_hex, returned_hex);

	cpumask_free(set);
	free(returned_hex);
}
END_TEST

/*
 * Check cpumask_alloc_complement() function.
 */
START_TEST(test_cpumask_alloc_complement)
{
	static const char in_hex[] = "f865";
	cpu_set_t *set;
	cpu_set_t *comp_set;
	char *returned_hex;

	configured_nr_cpus = 16;
	set = cpumask_alloc_from_list("1,7-10,3-4");
	comp_set = cpumask_alloc_complement(set);
	returned_hex = cpumask_hex(comp_set);

	ck_assert_msg(strcmp(in_hex, returned_hex) == 0,
		"in_hex='%s' returned_hex='%s'", in_hex, returned_hex);

	cpumask_free(set);
	cpumask_free(comp_set);
	free(returned_hex);
}
END_TEST

static void try_alloc_from_mask(const char *in_mask, const char *out_mask)
{
	cpu_set_t * const set = cpumask_alloc_from_mask(in_mask);
	char * const returned_mask = cpumask_hex(set);

	ck_assert_msg(strcmp(returned_mask, out_mask) == 0,
		"in_mask='%s' out_mask='%s' returned_mask='%s'",
		in_mask, out_mask, returned_mask);

	free(returned_mask);
}

START_TEST(test_cpumask_alloc_from_mask_1)
{
	configured_nr_cpus = 16;
	try_alloc_from_mask("0xf01F", "f01f");
}
END_TEST


START_TEST(test_cpumask_alloc_from_mask_2)
{
	configured_nr_cpus = 14;
	try_alloc_from_mask("2Ee0", "2ee0");
}
END_TEST

START_TEST(test_cpumask_list)
{
	static const char list[] = "1-2,4-7,9";
	cpu_set_t * const set = cpumask_alloc_from_list(list);
	char *returned_list = cpumask_list(set);

	ck_assert_msg(strcmp(list, returned_list) == 0,
		"list='%s' returned_list='%s'",
		list, returned_list);

	cpumask_free(set);
	free(returned_list);
}
END_TEST

Suite *
suite_cpumask(void)
{
	Suite *s = suite_create("partrt_cpumask");

	TCase *tc_core = tcase_create("Core");
	tcase_add_exit_test(tc_core, test_cpumask_nr_cpus_fail_1, EXIT_FAILURE);
	tcase_add_exit_test(tc_core, test_cpumask_nr_cpus_fail_2, EXIT_FAILURE);
	tcase_add_test(tc_core, test_cpumask_nr_cpus_success);
	tcase_add_test(tc_core, test_cpumask_set);
	tcase_add_exit_test(tc_core, test_cpumask_alloc_set_illegal,
			EXIT_FAILURE);
	tcase_add_test(tc_core, test_cpumask_alloc_set);
	tcase_add_test(tc_core, test_cpumask_hex_from_list);
	tcase_add_test(tc_core, test_cpumask_alloc_complement);
	tcase_add_test(tc_core, test_cpumask_alloc_from_mask_1);
	tcase_add_test(tc_core, test_cpumask_alloc_from_mask_2);
	tcase_add_test(tc_core, test_cpumask_list);
	suite_add_tcase(s, tc_core);

	return s;
}

int main(int argc, char *argv[])
{
	int number_failed;
	Suite *s = suite_cpumask();
	SRunner *sr = srunner_create(s);

	(void) argc;
	(void) argv;

	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
