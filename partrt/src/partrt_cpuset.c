/*
 * Copyright (c) 2013,2014 by Enea Software AB
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
 * This file implements a function interface towards the Linux kernel cpuset.
 */

#include "partrt.h"

#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define PROCFS_FILESYSTEMS "/proc/filesystems"
#define PROCFS_MOUNTS      "/proc/mounts"

#define CGROUP_CPUSET_ROOT "/sys/fs/cgroup/cpuset"
#define CGROUP_ROOT        "/sys/fs/cgroup"

static const char * const partition_name[] = { "rt", "nrt", NULL };

char *fd_to_path_alloc(int fd)
{
	struct stat st;
	char *path = NULL;
	ssize_t name_size;
	char *proc_path = NULL;

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

do_return:
	free(proc_path);
	return path;
}

static void logged_mount(const char *source, const char *target,
			const char *fstype, const char *options)
{
	if (mount(source, target, fstype, 0, options) != 0)
		fail("%s: Error mounting device '%s' as file system type '%s' with options '%s'\n",
			target, source, fstype, options);
	info("%s: Mounted device '%s' as file system type '%s' with options '%s'\n",
		target, source, fstype, options);
}

static void logged_mkdir(const char *path)
{
	if (mkdir(path, 0777) != 0)
		fail("%s: Failed creating directory: %s\n",
			path, strerror(errno));
	info("%s: Created directory\n", path);
}

static int string_in_list(const char *str, const char * const *list)
{
	for (; *list != NULL; list++)
		if (strcmp(str, *list) == 0)
			return 1;

	return 0;
}


/*
 * fd - Opened directory file descriptor, will be closed by this function.
 * entries - NULL terminated list of pointers to strings, where each string
 *           is a name of a directory entry that is expected to _not_ be found.
 */
static int dir_is_empty_helper(int fd, const char * const *entries)
{
	struct dirent *d;
	const int stat_fd = dup(fd);
	DIR * const dir = fdopendir(fd);
	int success = 1;

	if (dir == NULL)
		fail("%s: Could not open directory for reading: %s\n",
			fd_to_path_alloc(fd), strerror(errno));

	while ((d = readdir(dir)) != NULL) {
		if ((strcmp(d->d_name, ".") != 0)
			&& (strcmp(d->d_name, "..") != 0)) {
			if (string_in_list(d->d_name, entries)
				|| (*entries == NULL)) {
				char *fd_path = fd_to_path_alloc(fd);
				TRACEF("%s: Directory not empty, found '%s'",
					fd_path,
					d->d_name);
				free(fd_path);
				success = 0;
				break;
			}
		}
	}

	if (closedir(dir) != 0)
		fail("%s: Failed closing directory: %s\n",
			fd_to_path_alloc(fd), strerror(errno));

	if (close(stat_fd) != 0)
		fail("%s: Failed closing descriptor: %s\n",
			fd_to_path_alloc(fd), strerror(errno));

	return success;
}

/*
 * Return true (1) if directory path is empty, or false (0) otherwise.
 */
static int dir_is_empty(const char *dirname)
{
	const int fd = open(dirname, O_RDONLY | O_DIRECTORY);

	if (fd < 0)
		fail("%s: Failed open directory for reading: %s\n",
			dirname, strerror(errno));

	return dir_is_empty_helper(fd, NULL);
}

/*
 * Return true (1) if dirctory fd is empty, or false (0) otherwise.
 */
static int fddir_is_empty(int fd, const char * const *entries)
{
	/* fd might be opened with O_PATH, so a dup() will not do. */
	const int dirfd = openat(fd, ".", O_RDONLY | O_DIRECTORY);

	if (dirfd < 0)
		fail("%s: Failed open directory for reading: %s\n",
			fd_to_path_alloc(fd), strerror(errno));

	/* dirfd is closed by called function. */
	return dir_is_empty_helper(dirfd, entries);
}

#define FS_NAME_SIZE 128

/*
 * Returns an opened file descriptor for cpuset root directory.
 * This descriptor is intended to be used in <func>at() calls as the
 * "dirfd" argument.
 */
static int cpuset_root(void)
{
	static int fd = -1;
	FILE *file;
	const int saved_errno = errno;
	char match[4][FS_NAME_SIZE];
	int nr_matches;
	int found_cpuset = 0;
	int found_cgroup = 0;

	if (fd != -1)
		return fd;

	file = fopen(PROCFS_FILESYSTEMS, "r");
	if (file == NULL)
		fail("%s: Failed opening for read: %s\n",
			PROCFS_FILESYSTEMS, strerror(errno));

	/* Make sure cpuset exists as a file system type */
	errno = 0;
	while ((nr_matches = fscanf(file, "%" STRSTR(FS_NAME_SIZE) "s %" STRSTR(FS_NAME_SIZE) "s ", match[0], match[1])) > 0) {
		/*
		 * File syntax: [dev]\t[fs]
		 * Since dev is optional, only compare the last match.
		 */
		if (strcmp(match[nr_matches - 1], "cpuset") == 0)
			break;
	}

	if (nr_matches <= 0) {
		if (errno == 0)
			fail("Could not find cpuset file system support in the kernel\n");
		else
			fail("%s: Error when reading file: %s\n",
				PROCFS_FILESYSTEMS, strerror(errno));
	}

	if (fclose(file) != 0)
		fail("%s: Failed closing file handle: %s\n",
			PROCFS_FILESYSTEMS, strerror(errno));

	/*
	 * Determine how much has been mounted.
	 */
	file = fopen(PROCFS_MOUNTS, "r");
	if (file == NULL)
		fail("%s: Failed opening for read: %s\n",
			PROCFS_MOUNTS, strerror(errno));

	errno = 0;
	while ((nr_matches = fscanf(file, "%" STRSTR(FS_NAME_SIZE) "s %" STRSTR(FS_NAME_SIZE) "s %" STRSTR(FS_NAME_SIZE) "s %" STRSTR(FS_NAME_SIZE) "s %*[^\n] ", match[0], match[1], match[2], match[3])) > 0) {
		if (nr_matches != 4)
			/* Not enough information found for this entry */
			continue;

		if ((strcmp(match[2], "cgroup") == 0)
			&& (strcmp(match[1], CGROUP_CPUSET_ROOT) == 0)) {
			found_cpuset = 1;
			found_cgroup = 1;
			break;
		}

		if ((strcmp(match[2], "cgroup") == 0)
			&& (strcmp(match[1], CGROUP_ROOT) == 0))
			found_cgroup = 1;
	}

	if (nr_matches <= 0) {
		if (errno == 0)
			fail("Could not find cpuset support in the kernel\n");
		else
			fail("%s: Error when reading file: %s\n",
				PROCFS_MOUNTS, strerror(errno));
	}

	if (fclose(file) != 0)
		fail("%s: Failed closing file handle: %s\n",
			PROCFS_MOUNTS, strerror(errno));

	/*
	 * Perform any missing mounts
	 */

	if (!found_cgroup) {
		/* Only mount if dir is empty, or else files will
		 * disappear causing unexpected results. */
		if (!dir_is_empty(CGROUP_ROOT))
			fail("%s: Directory not empty, mount aborted\n",
				CGROUP_ROOT);

		/* mount -t tmpfs cgroup_root /sys/fs/cgroup */
		logged_mount("cgroup_root", CGROUP_ROOT, "tmpfs", "");
	}

	if (!found_cpuset) {
		logged_mkdir(CGROUP_CPUSET_ROOT);

		/* mount -t cgroup cpuset -ocpuset /sys/fs/cgroup/cpuset */
		logged_mount("cpuset", CGROUP_CPUSET_ROOT, "cpuset", "cpuset");
	}

	fd = open(CGROUP_CPUSET_ROOT, O_PATH | O_DIRECTORY);
	if (fd < 0)
		fail("%s: Unable open path as a directory reference: %s\n",
			CGROUP_CPUSET_ROOT, strerror(errno));

	errno = saved_errno;

	return fd;
}

static int partition_fd[2] = { -1, -1};

static int cpuset_partition_root(enum CpufsPartition partition)
{
	int fd_root;

	if (partition_fd[partition] != -1)
		return partition_fd[partition];

	fd_root = cpuset_root();
	if (mkdirat(fd_root, partition_name[partition], 0777)
		!= 0)
		fail("%s/%s: Failed creating directory: %s\n",
			CGROUP_CPUSET_ROOT,
			partition_name[partition],
			strerror(errno));
	partition_fd[partition] = openat(fd_root, partition_name[partition],
		O_PATH | O_DIRECTORY);
	if (partition_fd[partition] < 0)
		fail("%s/%s: Failed opening directory as a reference: %s\n",
			CGROUP_CPUSET_ROOT,
			partition_name[partition],
			strerror(errno));
	return partition_fd[partition];
}

static void save_old_content(int fd, FILE *dest)
{
	FILE * const stream = fdopen(dup(fd), "r");
	char *buf;
	size_t str_len;
	off_t file_size;

	if (stream == NULL)
		fail("%s: Failed fdopen(): %s\n",
			fd_to_path_alloc(fd),
			strerror(errno));

	/* Seek to end of file to determine file size */
	file_size = lseek(fd, SEEK_END, 0);

	/* Rewind file */
	lseek(fd, SEEK_SET, 0);

	buf = malloc(file_size + 1);
	if (buf == NULL)
		fail("Out of memory allocating %zu bytes\n",
			 + 1);

	if (fgets(buf, file_size + 1, stream) == NULL) {
		if (ferror(stream))
			fail("%s: Failed fgets(): %s\n",
				fd_to_path_alloc(fd),
				strerror(errno));
		else
			fail("%s: Failed fgets(): File unexpectedly empty\n",
				fd_to_path_alloc(fd));
	}

	/* Remove trailing newline, if there is one */
	str_len = strlen(buf);
	if (buf[str_len-1] == '\n')
		buf[str_len-1] = '\0';

	if (fputs(buf, dest) == EOF)
		fail("%s: Failed fputs(): %s\n",
			fd_to_path_alloc(fileno(dest)),
			strerror(errno));

	if (fclose(stream) == EOF)
		fail("%s: Failed fclose(): %s\n",
			fd_to_path_alloc(fd),
			strerror(errno));
}

/*
 * Public functions
 * For descriptions of these functions look in partrt.h
 */

int cpuset_is_empty(void)
{
	return fddir_is_empty(cpuset_root(), partition_name);
}

void cpuset_write(enum CpufsPartition partition, const char *file_name,
		const char *value, FILE *value_log)
{
	const int fd_root = cpuset_partition_root(partition);
	const int fd = openat(fd_root, file_name,
			(value_log == NULL) ? O_WRONLY : O_RDWR);
	const ssize_t bytes_to_write = strlen(value);
	ssize_t bytes_written;

	TRACEF("Writing '%s' to %s partition\n",
		value,
		(partition == partition_rt) ? "rt" : "nrt");

	info("fd=%d\n", fd);

	if (fd < 0)
		fail("%s/%s/%s: Failed to open file: %s\n",
			CGROUP_CPUSET_ROOT,
			partition_name[partition],
			file_name,
			strerror(errno));

	if (value_log != NULL)
		save_old_content(fd, value_log);

	bytes_written = write(fd, value, bytes_to_write);
	if (bytes_written != bytes_to_write) {
		if (bytes_written == -1)
			fail("%s: Failed to write to file: %s\n",
				fd_to_path_alloc(fd),
				strerror(errno));
		else
			fail("%s: Failed to write to file: %zd bytes written, expected %zd bytes\n",
				fd_to_path_alloc(fd),
				bytes_written,
				bytes_to_write);
	}

	if (close(fd) != 0)
		fail("%s/%s/%s: Failed closing file descriptor: %s\n",
			CGROUP_CPUSET_ROOT,
			partition_name[partition],
			file_name,
			strerror(errno));
}

void cpuset_partition_unlink(void)
{
	const int root = cpuset_root();
	int idx;

	for (idx = 0; idx < 2; idx++) {
		if ((faccessat(root, partition_name[idx], F_OK, 0) == 0) &&
			(unlinkat(root, partition_name[idx], AT_REMOVEDIR)
				== -1))
			fail("%s/%s: Failed unlink(): %s\n",
				fd_to_path_alloc(root),
				partition_name[idx],
				strerror(errno));

		if ((partition_fd[idx] != -1)
			&& (close(partition_fd[idx]) == -1))
			fail("%s/%s: Failed close(): %s",
				fd_to_path_alloc(root),
				partition_name[idx],
				strerror(errno));

		partition_fd[idx] = -1;
	}
}
