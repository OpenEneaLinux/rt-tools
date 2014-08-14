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

static const char * const partition_name[] = { "", "rt", "nrt", NULL };
static int enable_create = 0;

static void logged_mount(const char *source, const char *target,
			const char *fstype, const char *options)
{
	if (mount(source, target, fstype, 0, options) != 0)
		fail("%s: Error mounting device '%s' as file system type '%s' with options '%s'",
			target, source, fstype, options);
	info("%s: Mounted device '%s' as file system type '%s' with options '%s'",
		target, source, fstype, options);
}

static void logged_mkdir(const char *path)
{
	if (mkdir(path, 0777) != 0)
		fail("%s: Failed creating directory: %s",
			path, strerror(errno));
	info("%s: Created directory", path);
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
		fail("%s: Could not open directory for reading: %s",
			file_fd_to_path_alloc(fd), strerror(errno));

	while ((d = readdir(dir)) != NULL) {
		if ((strcmp(d->d_name, ".") != 0)
			&& (strcmp(d->d_name, "..") != 0)) {
			if (string_in_list(d->d_name, entries)
				|| (*entries == NULL)) {
				char *fd_path = file_fd_to_path_alloc(fd);
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
		fail("%s: Failed closing directory: %s",
			file_fd_to_path_alloc(fd), strerror(errno));

	if (close(stat_fd) != 0)
		fail("%s: Failed closing descriptor: %s",
			file_fd_to_path_alloc(fd), strerror(errno));

	return success;
}

/*
 * Return true (1) if directory path is empty, or false (0) otherwise.
 */
static int dir_is_empty(const char *dirname)
{
	const int fd = open(dirname, O_RDONLY | O_DIRECTORY);

	if (fd < 0)
		fail("%s: Failed open directory for reading: %s",
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
		fail("%s: Failed open directory for reading: %s",
			file_fd_to_path_alloc(fd), strerror(errno));

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
		fail("%s: Failed opening for read: %s",
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
			fail("Could not find cpuset file system support in the kernel");
		else
			fail("%s: Error when reading file: %s",
				PROCFS_FILESYSTEMS, strerror(errno));
	}

	if (fclose(file) != 0)
		fail("%s: Failed closing file handle: %s",
			PROCFS_FILESYSTEMS, strerror(errno));

	/*
	 * Determine how much has been mounted.
	 */
	file = fopen(PROCFS_MOUNTS, "r");
	if (file == NULL)
		fail("%s: Failed opening for read: %s",
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
			fail("Could not find cpuset support in the kernel");
		else
			fail("%s: Error when reading file: %s",
				PROCFS_MOUNTS, strerror(errno));
	}

	if (fclose(file) != 0)
		fail("%s: Failed closing file handle: %s",
			PROCFS_MOUNTS, strerror(errno));

	if (!enable_create && (!found_cgroup || !found_cpuset))
		fail("cpuset virtual file system not mounted, terminating");

	/*
	 * Perform any missing mounts
	 */

	if (!found_cgroup) {
		/* Only mount if dir is empty, or else files will
		 * disappear causing unexpected results. */
		if (!dir_is_empty(CGROUP_ROOT))
			fail("%s: Directory not empty, mount aborted",
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
		fail("%s: Unable open path as a directory reference: %s",
			CGROUP_CPUSET_ROOT, strerror(errno));

	errno = saved_errno;

	return fd;
}

static int partition_fd[3] = { -1, -1, -1};

int cpuset_partition_root(enum CpufsPartition partition)
{
	int fd_root;

	if (partition_fd[partition] != -1)
		return partition_fd[partition];

	if (partition == partition_root) {
		partition_fd[partition] = cpuset_root();
		return partition_fd[partition];
	}

	fd_root = cpuset_root();

	if (enable_create) {
		if (mkdirat(fd_root, partition_name[partition], 0777)
			!= 0)
			fail("%s/%s: Failed creating directory: %s",
				CGROUP_CPUSET_ROOT,
				partition_name[partition],
				strerror(errno));
	}
	partition_fd[partition] = openat(fd_root, partition_name[partition],
		O_PATH | O_DIRECTORY);
	if (partition_fd[partition] < 0) {
		if (!enable_create)
			info("%s/%s: Partition not found, and create not requested. Ignoring.",
			CGROUP_CPUSET_ROOT,
			partition_name[partition]);
		else
			fail("%s/%s: Failed open(): %s",
			CGROUP_CPUSET_ROOT,
			partition_name[partition],
			strerror(errno));
	}
	return partition_fd[partition];
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
	file_write(cpuset_partition_root(partition), file_name, value,
		value_log);
}

int cpuset_try_write(enum CpufsPartition partition, const char *file_name,
		const char *value, FILE *value_log)
{
	return file_try_write(cpuset_partition_root(partition), file_name,
			value, value_log);
}

char *cpuset_read_alloc(enum CpufsPartition partition, const char *file)
{
	return file_read_alloc(cpuset_partition_root(partition), file);
}

void cpuset_partition_unlink(void)
{
	const int root = cpuset_root();
	int idx;
	char *root_name = file_fd_to_path_alloc(root);

	info("Removing partitions");

	for (idx = partition_rt; idx <= partition_nrt; idx++) {
		debug("Considering %s/%s", root_name, partition_name[idx]);

		if (faccessat(root, partition_name[idx], F_OK, 0) == 0) {
			if (unlinkat(root, partition_name[idx], AT_REMOVEDIR)
				== -1)
				fail("%s/%s: Failed unlink(): %s",
					file_fd_to_path_alloc(root),
					partition_name[idx],
					strerror(errno));
			info("Removed partition %s", partition_name[idx]);
		}

		if ((partition_fd[idx] != -1)
			&& (close(partition_fd[idx]) == -1))
			fail("%s/%s: Failed close(): %s",
				file_fd_to_path_alloc(root),
				partition_name[idx],
				strerror(errno));

		partition_fd[idx] = -1;
	}
}

const char *cpuset_partition_name(enum CpufsPartition partition)
{
	if (partition == partition_root)
		return "<root>";

	return partition_name[partition];
}

void cpuset_move_task(pid_t pid, enum CpufsPartition partition)
{
	char *pid_str;
	char * const file_name = file_pid_to_name_alloc(pid);

	asprintf(&pid_str, "%u", pid);

	if (pid_str == NULL)
		fail("Out of memory");

	if (cpuset_try_write(partition, "tasks", pid_str, NULL) != 0)
		info("%u (%s): Failed move to %s",
			pid, file_name, cpuset_partition_name(partition));

	free(pid_str);
	free(file_name);
}

void cpuset_move_all_tasks(enum CpufsPartition from, enum CpufsPartition to)
{
	char * const buf = cpuset_read_alloc(from, "tasks");
	char *task;

	info("Moving all tasks from partition %s to %s",
		cpuset_partition_name(from), cpuset_partition_name(to));

	for (task = strtok(buf, " \n\r\t");
	     task != NULL;
	     task = strtok(NULL, " \n\r\t")) {
		char *endptr;
		pid_t task_pid = strtoul(task, &endptr, 0);
		if ((task[0] == '\0') || (endptr[0] != '\0'))
			fail("Error reading tasks file: %s: Illegal task PID",
				task);

		cpuset_move_task(task_pid, to);
	}

	free(buf);
}

void cpuset_set_create(int enable)
{
	enable_create = enable;
}
