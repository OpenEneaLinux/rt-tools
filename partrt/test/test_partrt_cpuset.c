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
