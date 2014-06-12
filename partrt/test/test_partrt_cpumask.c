#include "../src/partrt_cpumask.c"
#include <check.h>

START_TEST(test_cpuset_nr_cpus)
{
	const int nr_cpus = cpuset_nr_cpus();

	ck_assert_int_gt(nr_cpus, 0);
}
END_TEST

START_TEST(test_cpuset_1)
{
	const int nr_cpus = cpuset_nr_cpus();

	cpu_set_t * const set = cpuset_alloc_zero();
	cpu_set_t * const set1 = cpuset_alloc_set(0);
	int cpu;

	for (cpu = 0; cpu < (nr_cpus + 5); cpu++)
		ck_assert_int_eq(0, cpuset_isset(cpu, set));

	cpuset_set(0, set);

	for (cpu = 0; cpu < (nr_cpus + 5); cpu++)
		ck_assert_int_eq((cpu == 0) ? 1 : 0, cpuset_isset(cpu, set));

	for (cpu = 0; cpu < (nr_cpus + 5); cpu++)
		ck_assert_int_eq((cpu == 0) ? 1 : 0, cpuset_isset(cpu, set1));

	cpuset_free(set);
}
END_TEST

Suite *
suite_cpumask(void)
{
	Suite *s = suite_create("partrt_cpumask");

	TCase *tc_core = tcase_create("Core");
	tcase_add_test(tc_core, test_cpuset_nr_cpus);
	tcase_add_test(tc_core, test_cpuset_1);
	suite_add_tcase(s, tc_core);

	return s;
}

int main(int argc, char *argv[])
{
	int number_failed;
	Suite *s = suite_cpumask();
	SRunner *sr = srunner_create(s);

	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
