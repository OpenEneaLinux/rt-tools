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
START_TEST(test_cpuset_nr_cpus_fail_1)
{
	configured_nr_cpus = -1;
	cpuset_nr_cpus(); /* Should fail with exit(EXIT_FAILURE) */
}
END_TEST

START_TEST(test_cpuset_nr_cpus_fail_2)
{
	configured_nr_cpus = 0;
	cpuset_nr_cpus(); /* Should fail with exit(EXIT_FAILURE) */
}
END_TEST

START_TEST(test_cpuset_nr_cpus_success)
{
	configured_nr_cpus = TESTED_NR_CPUS;
	ck_assert_int_eq(TESTED_NR_CPUS, cpuset_nr_cpus());
}
END_TEST

/*
 * Checks that cpuset_alloc_zero() actually zeroes the bit field.
 * Checks that cpuset_set() sets correct bits according to cpuset_isset().
 */
START_TEST(test_cpuset_set)
{
	static const int first_bit_set = 1000;
	static const int last_bit_set = 1002;
	const int nr_cpus = cpuset_nr_cpus();
	cpu_set_t * const set = cpuset_alloc_zero();
	int cpu;

	/* Assure the bit field starts out being zeroed */
	for (cpu = 0; cpu < (nr_cpus + 5); cpu++)
		ck_assert_int_eq(0, cpuset_isset(cpu, set));

	/* Set bits within range */
	for (cpu = first_bit_set; cpu <= last_bit_set; cpu++)
		cpuset_set(cpu, set);

	/* Check that only the bits within the range are set */
	for (cpu = 0; cpu < (nr_cpus + 5); cpu++)
		ck_assert_int_eq(
			((cpu >= first_bit_set) && (cpu <= last_bit_set))
			? 1 : 0,
			cpuset_isset(cpu, set));

	/* Cleanup */
	cpuset_free(set);
}
END_TEST

/*
 * Checks that cpuset_alloc_set() sets the correct bit.
 */
START_TEST(test_cpuset_alloc_set)
{
	const int nr_cpus = cpuset_nr_cpus();
	static const int bit_set = 25;
	int cpu;

	/* Allocate bit field with one bit set */
	cpu_set_t * const set = cpuset_alloc_set(bit_set);

	/* Verify that correct bit was set */
	for (cpu = 0; cpu < (nr_cpus + 5); cpu++)
		ck_assert_int_eq((cpu == bit_set) ? 1 : 0,
				cpuset_isset(cpu, set));

	/* Cleanup */
	cpuset_free(set);
}
END_TEST

/*
 * Checks that cpuset_alloc_set() fails for illegal cpu values.
 */
START_TEST(test_cpuset_alloc_set_illegal)
{
	static const int bit_set = TESTED_NR_CPUS + 1;

	/* Allocate bit field with one bit set */
	cpuset_alloc_set(bit_set);

	/* Expecting exit(EXIT_FAILURE) here */
}
END_TEST

/*
 * Check cpuset_alloc_from_list() and cpuset_hex().
 */
START_TEST(test_cpuset_hex_from_list)
{
	static const char in_hex[] = "79a";
	cpu_set_t *set;
	const char *returned_hex;

	configured_nr_cpus = 11;
	set = cpuset_alloc_from_list("1,7-10,3-4");
	returned_hex = cpuset_hex(set);

	ck_assert_msg(strcmp(in_hex, returned_hex) == 0,
		"in_hex='%s' returned_hex='%s'", in_hex, returned_hex);

	cpuset_free(set);
}
END_TEST

/*
 * Check cpuset_alloc_complement() function.
 */
START_TEST(test_cpuset_alloc_complement)
{
	static const char in_hex[] = "f865";
	cpu_set_t *set;
	cpu_set_t *comp_set;
	const char *returned_hex;

	configured_nr_cpus = 16;
	set = cpuset_alloc_from_list("1,7-10,3-4");
	comp_set = cpuset_alloc_complement(set);
	returned_hex = cpuset_hex(comp_set);

	ck_assert_msg(strcmp(in_hex, returned_hex) == 0,
		"in_hex='%s' returned_hex='%s'", in_hex, returned_hex);

	cpuset_free(set);
	cpuset_free(comp_set);
}
END_TEST

Suite *
suite_cpumask(void)
{
	Suite *s = suite_create("partrt_cpumask");

	TCase *tc_core = tcase_create("Core");
	tcase_add_exit_test(tc_core, test_cpuset_nr_cpus_fail_1, EXIT_FAILURE);
	tcase_add_exit_test(tc_core, test_cpuset_nr_cpus_fail_2, EXIT_FAILURE);
	tcase_add_test(tc_core, test_cpuset_nr_cpus_success);
	tcase_add_test(tc_core, test_cpuset_set);
	tcase_add_exit_test(tc_core, test_cpuset_alloc_set_illegal,
			EXIT_FAILURE);
	tcase_add_test(tc_core, test_cpuset_alloc_set);
	tcase_add_test(tc_core, test_cpuset_hex_from_list);
	tcase_add_test(tc_core, test_cpuset_alloc_complement);
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
