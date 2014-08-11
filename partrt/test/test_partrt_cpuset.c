#include "../src/partrt_cpuset.c"
#include <check.h>

void unchecked_setup()
{
	info("Running setup\n");
}

void unchecked_teardown()
{
	info("Running teardown\n");
	cpuset_partition_unlink();
}

START_TEST(test_cpuset_is_empty)
{
	ck_assert_int_eq(1, cpuset_is_empty());
}
END_TEST

START_TEST(test_cpuset_write)
{
	static const char * const test_str = "1";
	static const char * const compare_str = "/sys/fs/cgroup/cpuset/nrt/cpuset.cpus=1\n";
	char logged_value[64];
	FILE * const log = tmpfile();

	cpuset_write(partition_nrt, "cpuset.cpus", test_str, NULL);
	cpuset_write(partition_nrt, "cpuset.cpus", test_str, log);

	rewind(log);

	ck_assert(fgets(logged_value, sizeof (logged_value), log) != NULL);

	ck_assert_msg(strcmp(logged_value, compare_str) == 0,
		"logged_value: '%s', compare_str: '%s'",
		logged_value, compare_str);
}
END_TEST

Suite *
suite_cpuset(void)
{
	Suite *s = suite_create("partrt_cpuset");

	TCase *tc_core = tcase_create("Core");
	tcase_add_unchecked_fixture(tc_core, unchecked_setup, unchecked_teardown);
	tcase_add_test(tc_core, test_cpuset_is_empty);
	tcase_add_test(tc_core, test_cpuset_write);
	suite_add_tcase(s, tc_core);

	return s;
}

int main(int argc, char *argv[])
{
	int number_failed;
	Suite *s = suite_cpuset();
	SRunner *sr = srunner_create(s);

	(void) argc;
	(void) argv;

	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
