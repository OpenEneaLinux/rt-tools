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

#define open(...) mockup_open(__VA_ARGS__)
#define openat(...) mockup_openat(__VA_ARGS__)
#define close(...) mockup_close(__VA_ARGS__)
#define read(...) mockup_read(__VA_ARGS__)
#define write(...) mockup_write(__VA_ARGS__)
#define mkdirat(...) mockup_mkdirat(__VA_ARGS__)
#define fstat(...) mockup_fstat(__VA_ARGS__)
#define fstatat(...) mockup_fstatat(__VA_ARGS__)
#define stat(...) mockup_stat(__VA_ARGS__)
#define lstat(...) mockup_lstat(__VA_ARGS__)
#define fdopendir(...) mockup_fdopendir(__VA_ARGS__)
#define closedir(...) mockup_closedir(__VA_ARGS__)
#define readdir(...) mockup_readdir(__VA_ARGS__)
#define fdopendir(...) mockup_fdopendir(__VA_ARGS__)
#define closedir(...) mockup_closedir(__VA_ARGS__)
#define readdir(...) mockup_readdir(__VA_ARGS__)
#define dup(...) mockup_dup(__VA_ARGS__)

#include "../src/partrt_common.c"
#include "../src/partrt_file.c"
#include "../src/partrt_cpuset.c"
#include <check.h>

#define NR_ELEMS(array) (sizeof (array) / sizeof (*(array)))

/*
 * Mocked up functions
 */

int mkdirat(int dirfd, const char *pathname, mode_t mode)
{
	(void) dirfd;
	(void) pathname;
	(void) mode;

	return 0;
}

/* Describes file content for a file descriptor. This struct is registered
 * using mockup_file_instruction_register(). It is used in open() to return
 * success for given file name, in read() to return read data and in write
 * to assert that the expected writes are done. */
struct FileInstruction
{
	const char *name;
	int dirfd;        /* Use AT_FDCWD for open() */
	const char *buf;
	size_t buf_size;
	const struct stat *stat_buf;
};

struct FileInstructionNode
{
	const struct FileInstruction *node;
	struct FileInstructionNode *next;
} *mockup_file_instruction_head = NULL;

struct FileOpened
{
	int fd;
	int open_flags;
	size_t pos;
	struct FileInstructionNode *instruction;
	struct FileOpened *next;
} *mockup_file_opened_head = NULL;

#define FILE_INSTRUCTION_REGISTER_ARRAY(array) mockup_file_instruction_register(array, NR_ELEMS(array))

void mockup_file_instruction_register_one(const struct FileInstruction *add)
{
	struct FileInstructionNode * const node = malloc(sizeof (struct FileInstructionNode));
	struct FileInstructionNode *curr;

	ck_assert_msg(node != NULL, "Out of memory allocating %zu bytes",
		sizeof (struct FileInstructionNode));
	node->node = add;
	node->next = NULL;

	if (mockup_file_instruction_head == NULL)
		mockup_file_instruction_head = node;
	else {
		for (curr = mockup_file_instruction_head;
		     curr->next != NULL;
		     curr = curr->next);
		curr->next = node;
	}
}

void mockup_file_instruction_register(const struct FileInstruction *add, size_t nr_instructions)
{
	size_t idx;

	for (idx = 0; idx < nr_instructions; idx++)
		mockup_file_instruction_register_one(add++);
}

struct FileInstructionNode *mockup_file_instruction_find(int dirfd, const char *path)
{
	struct FileInstructionNode *node;
	
	for (node = mockup_file_instruction_head;
	     node != NULL;
	     node = node->next)
		if ((dirfd == dirfd) && (strcmp(node->node->name, path) == 0)) 
			break;

	return node;
}

int mockup_file_opened_add(int open_flags, struct FileInstructionNode *node)
{
	static int next_fd = 1000;
	struct FileOpened *new = malloc(sizeof (struct FileOpened));
	struct FileOpened *ptr;

	ck_assert_msg(new != NULL, "Out of memory allocatng %zu bytes",
		sizeof (struct FileOpened));

	new->fd = next_fd++;
	new->open_flags = open_flags;
	new->pos = 0;
	new->instruction = node;
	new->next = NULL;
	if (mockup_file_opened_head == NULL)
		mockup_file_opened_head = new;
	else {
		for (ptr = mockup_file_opened_head;
		     ptr->next != NULL;
		     ptr = ptr->next);
		ptr->next = new;
	}

	return new->fd;
}

int mockup_file_opened_remove(int fd)
{
	struct FileOpened *ptr;
	struct FileOpened **prev = &mockup_file_opened_head;

	for (ptr = mockup_file_opened_head;
	     ptr != NULL;
	     ptr = ptr->next) {
		if (ptr->fd == fd) {
			*prev = ptr->next;
			return 0;
		}
		prev = &ptr->next;
	}

	return -1;
}

struct FileOpened *mockup_file_opened_find(int fd)
{
	struct FileOpened *ptr;

	for (ptr = mockup_file_opened_head;
	     ptr != NULL;
	     ptr = ptr->next) {
		if (ptr->fd == fd)
			return ptr;
	}

	return NULL;
}

void print_open_flags_helper(int *first, const char *text, FILE *output)
{
	if (*first)
		fputc('|', output);
	else
		*first = 1;

	fputs(text, output);
}

void print_open_flags(int flags, FILE *output)
{
	int first = 0;

	if (flags & O_APPEND)
		print_open_flags_helper(&first, "O_APPEND", output);

	if (flags & O_ASYNC)
		print_open_flags_helper(&first, "O_ASYNC", output);
	
	if (flags & O_CLOEXEC)
		print_open_flags_helper(&first, "O_CLOEXEC", output);
	
	if (flags & O_CREAT)
		print_open_flags_helper(&first, "O_CREAT", output);
	
	if (flags & O_DIRECT)
		print_open_flags_helper(&first, "O_DIRECT", output);
	
	if (flags & O_DIRECTORY)
		print_open_flags_helper(&first, "O_DIRECTORY", output);
	
	if (flags & O_EXCL)
		print_open_flags_helper(&first, "O_EXCL", output);
	
	if (flags & O_LARGEFILE)
		print_open_flags_helper(&first, "O_LARGEFILE", output);
	
	if (flags & O_NOATIME)
		print_open_flags_helper(&first, "O_NOATIME", output);

	if (flags & O_NOCTTY)
		print_open_flags_helper(&first, "O_NOCTTY", output);

	if (flags & O_NOFOLLOW)
		print_open_flags_helper(&first, "O_NOFOLLOW", output);

	if (flags & O_NONBLOCK)
		print_open_flags_helper(&first, "O_NONBLOCK", output);

	if (flags & O_PATH)
		print_open_flags_helper(&first, "O_PATH", output);

	if (flags & O_SYNC)
		print_open_flags_helper(&first, "O_SYNC", output);
	
	if (flags & O_TRUNC)
		print_open_flags_helper(&first, "O_TRUNC", output);
}

int mockup_open(const char *pathname, int flags, ...)
{
	mode_t mode = 0;

	if (flags & O_CREAT) {
		va_list va;
		va_start(va, flags);
		mode = va_arg(va, mode_t);
		va_end(va);
		return mockup_openat(AT_FDCWD, pathname, flags, mode);
	}
	return mockup_openat(AT_FDCWD, pathname, flags);
}

typedef struct {
	int append;
	int fd;
} FILE;

FILE *mockup_fopen(const char *path, const char *mode)
{
	int open_flags = 0;
	int open_mode = 0666;
	int append;
	int fd;
	FILE *file = NULL;

	fprintf(stderr, "mockup: fopen('%s', '%s') => ",
		path, mode);

	if (strcmp(mode, "r") == 0)
		open_flags = O_RDONLY;
	else if (strcmp(mode, "r+") == 0)
		open_flags = O_RDWR;
	else if (strcmp(mode, "w") == 0)
		open_flags = O_WRONLY | O_TRUNC | O_CREAT;
	else if (strcmp(mode, "w+") == 0)
		open_flags = O_RDWR | O_TRUNC | O_CREAT;
	else if (strcmp(mode, "a") = 0) {
		append = 1;
		open_flags = O_WRONLY | O_CREAT;
	} else if (strcmp(mode, "a+") = 0) {
		append = 1;
		open_flags = O_RDWR | O_CREAT;
	} else {
		errno = EINVAL;
		fputs("-1 (EINVAL)\n", stderr);
		return -1;
	}

	fd = mockup_open(path, open_flags, open_mode);
	if (fd == -1) {
		fprintf(stderr, "-1 (%s)\n", strerror(errno));
		return -1;
	}

	file = malloc(sizeof (FILE));
	ck_assert(file != NULL);
	file->fd = fd;
	file->append = append;

	return file;
}

int mockup_openat(int dirfd, const char *pathname, int flags, ...)
{
	struct FileInstructionNode *node;
	va_list va;
	mode_t mode = 0;
	int fd = -1;

	if (flags & O_CREAT) {
		va_start(va, flags);
		mode = va_arg(va, mode_t);
		va_end(va);
	}

	node = mockup_file_instruction_find(dirfd, pathname);
	if (node != NULL)
		fd = mockup_file_opened_add(flags, node);

	fputs("mockup: openat(", stderr);
	if (dirfd == AT_FDCWD)
		fputs("AT_FDCWD", stderr);
	else
		fprintf(stderr, "%d", dirfd);
	fprintf(stderr, ", '%s', ", pathname);
	print_open_flags(flags, stderr);
	if (flags & O_CREAT)
		fprintf(stderr, ", %o) => %d\n", mode, fd);
	else
		fprintf(stderr, ") => %d\n", fd);

	if (fd == -1) {
		errno = ENOENT;
		return -1;
	}

	return fd;
}

int mockup_close(int fd)
{
	fprintf(stderr, "mockup: close(%d) => ", fd);
	if (mockup_file_opened_remove(fd) == -1) {
		fputs("-1 (EBADF)\n", stderr);
		errno = EBADF;
		return -1;
	}

	fputs("0\n", stderr);
	return 0;
}

int mockup_dup(int fd)
{
	const struct FileOpened * const file;
	int new_fd = -1;

	file = mockup_file_opened_find(fd);

	if (file != NULL) {
		new_fd = mockup_file_opened_add(file->open_flags, file->instruction);
		fprintf(stderr, "mockup: dup(%d) => %d\n", fd, new_fd);
	} else {
		errno = EBADF;
		fprintf(stderr, "mockup: dup(%d) => %d (%s)\n",
			fd, new_fd, strerror(errno));
	}

	return new_fd;
}

ssize_t mockup_read(int fd, void *buf, size_t count)
{
	struct FileOpened * const file = mockup_file_opened_find(fd);
	const size_t old_count = count;

	fprintf(stderr, "mockup: read(%d, %p, %zu) => %zu\n",
		fd, buf, old_count, count);

	if (file == NULL) {
		errno = EBADF;
		return -1;
	} 

	if (count > (file->pos + file->instruction->node->buf_size))
		count = file->instruction->node->buf_size - file->pos;

	memcpy(buf, file->instruction->node->buf, count);
	file->pos += count;

	return count;
}

ssize_t mockup_write(int fd, const void *buf, size_t count)
{
	struct FileOpened * const file = mockup_file_opened_find(fd);

	fprintf(stderr, "mockup write(%d, %p, %zu) => ",
		fd, buf, count);

	if (file == NULL) {
		fputs("-1 (EBADF)\n", stderr);
		errno = EBADF;
		return -1;
	}

	if (count > (file->pos + file->instruction->node->buf_size))
		count = file->instruction->node->buf_size - file->pos;

	fprintf(stderr, "%zu\n", count);

	ck_assert(memcmp(buf, file->instruction->node->buf, count) == 0);
	file->pos += count;

	return count;
}

int mockup_fstat(int fd, struct stat *buf)
{
	const struct FileOpened * const file = mockup_file_opened_find(fd);

	fprintf(stderr, "mockup: fstat(%d, %p) => ", fd, buf);

	if (file != NULL) {
		fputs("0\n", stderr);
		memcpy(buf, &file->instruction->node->stat_buf, sizeof (struct stat));
		return 0;
	}

	fputs("-1 (EBADF)\n", stderr);
	errno = EBADF;
	return -1;
}

int mockup_fstatat(int dirfd, const char *pathname, struct stat *buf, int flags)
{
	const struct FileInstructionNode * const node =
		mockup_file_instruction_find(dirfd, pathname);

	fprintf(stderr, "mockup: fstatat(%d, '%s', %p, %d) => ",
	        dirfd, pathname, buf, flags);

	if (node != NULL) {
		memcpy(buf, &node->node->stat_buf, sizeof (struct stat));
		fputs("0\n", stderr);
		return 0;
	}

	fputs("-1 (EBADF)\n", stderr);
	errno = EBADF;
	return -1;
}

int mockup_stat(const char *path, struct stat *buf)
{
	return mockup_fstatat(AT_FDCWD, path, buf, 0);
}

int mockup_lstat(const char *path, struct stat *buf)
{
	return mockup_fstatat(AT_FDCWD, path, buf, AT_SYMLINK_NOFOLLOW);
}

struct __dirstream { int fd; };

DIR *mockup_fdopendir(int fd)
{
	DIR *dirp = malloc(sizeof (int));

	ck_assert(dirp != NULL);
	dirp->fd = fd;
	fprintf(stderr, "mockup: fdopendir(%d) => %p\n",
		fd, dirp);
	return dirp;	
}

struct dirent *mockup_readdir(DIR *dirp)
{
	(void) dirp;
	fprintf(stderr, "mockup: readdir(%p) => NULL\n", dirp);
	return NULL;
}

int closedir(DIR *dirp)
{
	int result = mockup_close(dirp->fd);
	dirp->fd = -1;
	free(dirp);
	fprintf(stderr, "mockup: closedir(%p) => %d (%s)\n",
		dirp, result, (result == -1) ? strerror(errno) : "");
	return result;
}

static const struct stat generic_dir_stat = {
	.st_mode = 0555 | S_IFDIR
};

static const struct FileInstruction cpuset_dir[] = {
	{ "/sys/fs/cgroup/cpuset", AT_FDCWD, "", 0, &generic_dir_stat },
	{ ".", 1000, "", 0, &generic_dir_stat },
	{ "nrt", 1000, "", 0, &generic_dir_stat }
};

void unchecked_setup()
{
	fputs("Running unchecked setup\n", stderr);
}

void unchecked_teardown()
{
	fputs("Running unchecked teardown\n", stderr);
	mockup_file_instruction_register(cpuset_dir, NR_ELEMS(cpuset_dir));
	cpuset_partition_unlink();
}

/* Run prior to each test case */
void checked_setup()
{
	fputs("Running checked setup\n", stderr);
	option_verbose = 1;
	option_debug = 1;
}

/* Run after each test case */
void checked_teardown()
{
	fputs("Running checked teardown\n", stderr);
}

START_TEST(test_cpuset_is_empty)
{
	fprintf(stderr, "Running testcase %s\n", __func__);
	mockup_file_instruction_register(cpuset_dir, NR_ELEMS(cpuset_dir));
	ck_assert_int_eq(1, cpuset_is_empty());
}
END_TEST

START_TEST(test_cpuset_write)
{
	static const char * const test_str = "1";
	static const char * const compare_str = "/sys/fs/cgroup/cpuset/nrt/cpuset.cpus=1\n";
	char logged_value[64];
	FILE * const log = tmpfile();

	fprintf(stderr, "Running testcase %s\n", __func__);
	mockup_file_instruction_register(cpuset_dir, NR_ELEMS(cpuset_dir));
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
	tcase_add_checked_fixture(tc_core, checked_setup, checked_teardown);
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
