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
 * This file implement helper functions to deal with files in a virtual file
 * system, e.g. sysfs or procfs.
 */

#include "partrt.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

/* #define VERBOSE_DEBUG */

#define PATH_DOT "."

char *file_fd_to_path_alloc(int fd)
{
	struct stat st;
	char *path = NULL;
	ssize_t name_size;
	char *proc_path = NULL;

	if (fd == AT_FDCWD) {
		path = strdup(PATH_DOT);
		if (path == NULL)
			fail("Out of memory allocating %zu bytes",
				sizeof (PATH_DOT));
		return path;
	}

	if (asprintf(&proc_path, "/proc/self/fd/%d", fd) == -1)
		goto do_return;

	do {
		if (lstat(proc_path, &st) == -1) {
			asprintf(&path, "<Could not translate file descriptor to file name: %s: Failed stat(): %s>",
				proc_path, strerror(errno));
			goto do_return;
		}

		path = malloc(st.st_size + 1);
		if (path == NULL)
			goto do_return;

		name_size = readlink(proc_path, path, st.st_size + 1);
		if (name_size == -1) {
			asprintf(&path, "<Could not translate file descriptor to file name: %s: Failed readlink(): %s>",
					proc_path, strerror(errno));
			goto do_return;
		}
	} while (name_size > st.st_size);

	/* readlink() does not append NUL character */
	path[name_size] = '\0';

do_return:
	free(proc_path);
	return path;
}

/*
 * TODO: lseek() does not work to determine file size!
 *       Use a page sized static buffer to read file into. Not thread safe,
 *       but will require less memory. This application does not need threads.
 */
char *file_read_alloc(int dirfd, const char *file)
{
	const int fd = openat(dirfd, file, O_RDONLY);
	char *buf;
	ssize_t bytes_read;
	static char read_cache[4096];

	if (fd == -1)
		fail("%s/%s: Failed openat(): %s",
			file_fd_to_path_alloc(dirfd), file, strerror(errno));

	bytes_read = read(fd, read_cache, sizeof (read_cache));

	if (bytes_read == -1)
		fail("%s: Failed read(): %s",
			file_fd_to_path_alloc(fd), strerror(errno));

	if (close(fd) == -1)
		fail("%s: Failed close(): %s",
			file_fd_to_path_alloc(fd), strerror(errno));

	/* Remove trailing newline if any */
	if (bytes_read > 0) {
		if (read_cache[bytes_read - 1] == '\n')
			bytes_read--;
	}
	read_cache[bytes_read] = '\0';

	buf = malloc(bytes_read + 1);
	if (buf == NULL)
		fail("Out of memory allocating %zu bytes", bytes_read + 1);

	memcpy(buf, read_cache, bytes_read + 1);

#ifdef VERBOSE_DEBUG
	debug("%s: Returned '%s'", __func__, buf);
#endif
	return buf;
}

static void save_old_content(int fd_root, const char *file_name, FILE *dest)
{
	char * const buf = file_read_alloc(fd_root, file_name);
	char *file_path;

	asprintf(&file_path, "%s/%s", file_fd_to_path_alloc(fd_root),
		file_name);
	if (file_path == NULL)
		fail("Out of memory");

	if (fprintf(dest, "%s=%s\n", file_path, buf) == EOF)
		fail("%s: Failed fprintf(): %s", file_path, strerror(errno));

	debug("%s: %s=%s\n", __func__, file_path, buf);

	free(file_path);
}

int file_try_write(int fd_root, const char *file_name,
		const char *value, FILE *value_log)
{
	const int fd = openat(fd_root, file_name,
			(value_log == NULL) ? O_WRONLY : O_RDWR);
	ssize_t bytes_to_write = strlen(value);

	if (fd < 0)
		return errno;

	if (value_log != NULL)
		save_old_content(fd_root, file_name, value_log);

	do {
		const ssize_t bytes_written = write(fd, value, bytes_to_write);
		if (bytes_written == -1)
			return errno;

		/* Zero bytes written should normally not occur for regular
		 * files, but the standard does not prohibit this. It might
		 * be worthwhile trying again, but since this probably never
		 * happens, fail instead of implementing retry counter. */
		if (bytes_written == 0)
			fail("%s: Failed write(): Zero bytes written",
				file_fd_to_path_alloc(fd),
				bytes_written,
				bytes_to_write);
		bytes_to_write -= bytes_written;
	} while (bytes_to_write > 0);

	if (close(fd) != 0)
		fail("%s/%s: Failed closing file descriptor: %s",
			file_fd_to_path_alloc(fd_root),
			file_name,
			strerror(errno));

	debug("%s/%s = %s", file_fd_to_path_alloc(fd_root), file_name, value);

	return 0;
}

void file_write(int fd_root, const char *file_name,
		const char *value, FILE *value_log)
{
	const int result = file_try_write(fd_root, file_name, value, value_log);

	if (result != 0)
		fail("%s/%s: Failed to write to file: %s",
			file_fd_to_path_alloc(fd_root),
			file_name,
			strerror(result));
}

char *file_pid_to_name_alloc(pid_t pid)
{
	char *file_name;
	FILE *file;
	char *buf;
	int result;

	asprintf(&file_name, "/proc/%d/status", pid);
	file = fopen(file_name, "r");

	result = fscanf(file, "Name: %ms", &buf);
	if (result < 0)
		fail("%s: Failed fscanf(): %s",
			file_name, strerror(errno));
	if (result != 1)
		fail("%s: Failed fscanf(): Count not find 'Name:'",
			file_name);
	free(file_name);
	if (fclose(file) == EOF)
		fail("%s: Failed fclose(): %s",
			file_name, strerror(errno));
	return buf;
}

struct file_iterator_t
{
	int fd;
	DIR *dir;
	mode_t mode;
};

struct file_iterator_t *file_iterator_init(int fd_dir, const char *dir_name,
	mode_t mode)
{
	struct file_iterator_t * const iterator =
		checked_malloc(sizeof (struct file_iterator_t));

	iterator->mode = mode;

	iterator->fd = openat(fd_dir, dir_name, O_DIRECTORY | O_RDONLY);

	if (iterator->fd == -1)
		fail("%s/%s: Failed openat(): %s",
			file_fd_to_path_alloc(fd_dir), dir_name,
			strerror(errno));

	iterator->dir = fdopendir(iterator->fd);
	if (iterator->dir == NULL)
		fail("%s/%s: Failed fdopendir(): %s",
			file_fd_to_path_alloc(fd_dir), dir_name,
			strerror(errno));

	return iterator;
}

const char *file_iterator_next(struct file_iterator_t *iterator)
{
	const struct dirent * dirent;
	int old_errno = errno;
	struct stat stat;

	errno = 0;

	do {
		dirent = readdir(iterator->dir);
		if ((dirent == NULL) && (errno != 0))
			fail("%s: Failed readdir(): %s",
				file_fd_to_path_alloc(iterator->fd),
				strerror(errno));

		if (dirent == NULL) {
			errno = old_errno;
			return NULL;
		}

		if (strcmp(dirent->d_name, ".") == 0)
			continue;

		if (strcmp(dirent->d_name, "..") == 0)
			continue;

		if (fstatat(iterator->fd, dirent->d_name, &stat, 0) == -1)
			fail("%s/%s: Failed stat(): %s",
				file_fd_to_path_alloc(iterator->fd),
				dirent->d_name,
				strerror(errno));

	} while ((stat.st_mode & S_IFMT) != iterator->mode);

	errno = old_errno;

	return dirent->d_name;
}

void file_iterator_close(struct file_iterator_t *iterator)
{
	if (closedir(iterator->dir) == -1)
		fail("%s: Failed closedir(): %s",
			file_fd_to_path_alloc(iterator->fd), strerror(errno));

	iterator->fd = -1;
	iterator->dir = NULL;

	free(iterator);
}
