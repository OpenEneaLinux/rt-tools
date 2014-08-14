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
 * This file implements sub-command "create".
 */

#include "partrt.h"

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

static int dry_run = 0;

static int disable_numa_affinity = 1;
static int migrate_bwq = 1;
static int disable_machine_check = 1;
static int defer_ticks = 1;
static int restart_hotplug = 1;
static int disable_watchdog = 1;

static struct bitmap_t *rt_set = NULL;
static struct bitmap_t *nrt_set = NULL;
static struct bitmap_t *possible_numa_set = NULL;
static struct bitmap_t *rt_numa_set = NULL;
static struct bitmap_t *nrt_numa_set = NULL;

/* These are cached values from *_set.
 * *_mask was retrieved using bitmap_hex(), and *_list was retrieved using
 * bitmap_list(). */
static const char *rt_mask = NULL;
static const char *nrt_mask = NULL;
static const char *rt_list = NULL;
static const char *nrt_list = NULL;
static const char *rt_numa_list = NULL;
static const char *nrt_numa_list = NULL;

static void usage_create(void)
{
	puts("partrt [options] create [cmd-options] [cpumask]\n"
	     "\n"
	     "Using prtrt with the create command will divide the available CPUs\n"
	     "into two partitions. One partition 'rt' (default name) and one\n"
	     "partition 'nrt' (default name). partrt will try to move sources of\n"
	     "jitter from the 'rt' CPUs to the 'nrt' CPUs.\n"
	     "\n"
	     "The old environment will be saved in /tmp/partrt_env\n"
	     "\n"
	     "[cpumask]:   cpumask that specifies CPUs for the real time partition.\n"
	     "Not needed if the -n flag is passed.\n"
	     "\n"
	     "cmd-options:\n"
	     "-a            Disable writeback workqueue NUMA affinity\n"
	     "-b            Do not migrate block workqueue when creating a new\n"
	     "              partition\n"
	     "-c            Do not disable machine check (x86)\n"
	     "-d            Do not defer ticks when creating a new partition\n"
	     "-h            Show this help text and exit.\n"
	     "-n <node>     Use NUMA topology to configure the partitions. The CPUs\n"
	     "              and memory that belong to NUMA <node> will be exclusive\n"
	     "              to the RT partition. This flag omits the [cpumask]\n"
	     "              parameter.\n"
	     "-m            Do not delay vmstat housekeeping when creating a\n"
	     "              new partition\n"
	     "-r            Do not restart hotplug CPUs when creating a new\n"
	     "              partition\n"
	     "-t            Do not disable real-time throttling when creating a\n"
	     "              new partition\n"
	     "-w            Do not disable watchdog timer when creating a new\n"
	     "              partition\n"
	     "-C <cpu list> List of CPU ranges to be included in RT partition\n"
		);

	exit(0);
}

static void irq_set_affinity(const char *cpu_mask)
{
	const int fd = open("/proc/irq", O_PATH | O_DIRECTORY);
	struct file_iterator_t *iterator;
	const char *name;

	info("Setting affinity mask 0x%s to all interrupt vectors",
		cpu_mask);

	if (fd == -1)
		fail("/proc/irq: Failed open(): %s", strerror(errno));

	iterator = file_iterator_init(fd, ".", S_IFDIR);

	file_write(fd, "default_smp_affinity", cpu_mask, NULL);

	for (name = file_iterator_next(iterator);
	     name != NULL;
	     name = file_iterator_next(iterator)) {
		char *path;

		asprintf(&path, "%s/smp_affinity", name);
		if (path == NULL)
			fail("Out of memory");
		if (file_try_write(fd, path, cpu_mask, NULL) != 0) {
			char *fd_name = file_fd_to_path_alloc(fd);
			info("%s/%s = %s  -- Failed, ignoring",
				fd_name, path, cpu_mask);
		}
	}
}

int cmd_create(int argc, char *argv[])
{

	static const struct option long_options[] = {
		{ "keep-numa-affinity",  no_argument,       NULL, 'a' },
		{ "disable-migrate-bwq", no_argument,       NULL, 'b' },
		{ "keep-machine-check",  no_argument,       NULL, 'c' },
		{ "no-defer-ticks",      no_argument,       NULL, 'd' },
		{ "help",                no_argument,       NULL, 'h' },
		{ "numa",                required_argument, NULL, 'n' },
		{ "no-restart-hotplug",  no_argument,       NULL, 'r' },
		{ "keep-watchdog",       no_argument,       NULL, 'w' },
		{ "cpu",                 required_argument, NULL, 'C' },
		{ "dry-run",             no_argument,       NULL, 'D' },
		{ NULL,                  0,                 NULL, '\0'}
	};
	static const char short_options[] = "+abcdhn:rtwC:D";
	int c;
	int bit;
	FILE *log_file = NULL;
	const time_t current_time = time(NULL);

	cpuset_set_create(1);

	while ((c = getopt_long(argc, argv, short_options, long_options,
				NULL)) != -1) {
		switch (c) {
		case 'h':
			usage_create();
		case 'a':
			disable_numa_affinity = 0;
			break;
		case 'b':
			migrate_bwq = 0;
			break;
		case 'c':
			disable_machine_check = 0;
			break;
		case 'd':
			defer_ticks = 0;
			break;
		case 'n':
			if (rt_numa_set != NULL)
				bitmap_free(rt_numa_set);

			rt_numa_set = bitmap_alloc_from_list(optarg, 0);
			break;
		case 'r':
			restart_hotplug = 0;
			break;
		case 'w':
			disable_watchdog = 0;
			break;
		case 'C':
			rt_set = bitmap_alloc_from_list(optarg, nr_cpus());
			break;
		case 'D':
			dry_run = 1;
			break;
		case '?':
			exit(1);
		default:
			fail("Internal error: '-%c': Switch accepted but not implemented",
				c);
		}
	}

	if (rt_numa_set != NULL) {
		char * const all_numa_nodes = file_read_alloc(
			AT_FDCWD, "/sys/devices/system/node/possible");

		if (rt_set != NULL)
			fail("partrt create: Specified both CPU list (-C/--cpu) and numa partition (-n/--numa), these options are mutually exclusive");

		possible_numa_set = bitmap_alloc_from_list(all_numa_nodes, 0);
		nrt_numa_set = bitmap_alloc_filter_out(possible_numa_set, rt_numa_set);

		rt_numa_list = bitmap_list(rt_numa_set);
		nrt_numa_list = bitmap_list(nrt_numa_set);

		free(all_numa_nodes);
	}

	if (rt_set == NULL) {
		if (rt_numa_set != NULL) {
			char *file_name;
			char *buf;
			asprintf(&file_name, "/sys/devices/system/node/node%s/cpumap", rt_numa_list);
			buf = file_read_alloc(AT_FDCWD, file_name);
			rt_set = bitmap_alloc_from_u32_list(buf, nr_cpus());
			free(buf);
			free(file_name);
		} else {
			if (optind >= argc)
				fail("partrt create: No CPU configured for RT partition, nothing to do");
			rt_set = bitmap_alloc_from_mask(argv[optind], nr_cpus());
			optind++;
		}
	}

	if (optind < argc)
		fail("partrt create: '%s': Too many parameters given. Use 'partrt create --help' for help.", argv[optind]);

	nrt_set = bitmap_alloc_complement(rt_set);

	rt_mask = bitmap_hex(rt_set);
	nrt_mask = bitmap_hex(nrt_set);
	rt_list = bitmap_list(rt_set);
	nrt_list = bitmap_list(nrt_set);

	info("RT partition : mask: %s, list: %s", rt_mask, rt_list);
	info("nRT partition: mask: %s, list: %s", nrt_mask, nrt_list);

	if (bitmap_bit_count(rt_set) < 1)
		fail("partrt create: RT partition contains no CPUs");

	if (bitmap_bit_count(nrt_set) < 1)
		fail("partrt create: NRT partition contains no CPUs");

	if (!cpuset_is_empty())
		fail("partrt create: There are already cpusets/partitions in the system, remove them first with 'partrt undo'");

	/* Disable load balancing in root partition, or else it will not be
	 * possible to disable load balancing in RT partition. */
	cpuset_write(partition_root, "cpuset.sched_load_balance", "0", NULL);

	/* Set interrupt affinity to NRT partition */
	irq_set_affinity(nrt_mask);

	/* Restart CPUs in RT partition to force timers to migrate */
	if (restart_hotplug)
		for (bit = bitmap_next_bit(-1, rt_set);
		     bit != -1;
		     bit = bitmap_next_bit(bit, rt_set)) {
			char *buf;

			info("Restarting CPU %d", bit);
			asprintf(&buf, "/sys/devices/system/cpu/cpu%d/online",
				bit);
			if (buf == NULL)
				fail("Out of memory");
			file_write(AT_FDCWD, buf, "0", NULL);
			file_write(AT_FDCWD, buf, "1", NULL);
			free(buf);
		}

	/*
	 * Configure RT partition
	 */

	/* Allocate CPUs */
	cpuset_write(partition_rt, "cpuset.cpus", rt_list, NULL);

	/* Make CPU list exclusive to RT partition */
	cpuset_write(partition_rt, "cpuset.cpu_exclusive", "1", NULL);

	/* Handle NUMA */
	if (rt_numa_set != NULL) {
		cpuset_write(partition_rt, "cpuset.mems", rt_numa_list, NULL);
		cpuset_write(partition_rt, "cpuset.mem_exclusive", "1", NULL);
	} else{
		cpuset_write(partition_rt, "cpuset.mems", "0", NULL);
	}

	/* Disable load balance in RT partition */
	cpuset_write(partition_rt, "cpuset.sched_load_balance", "0", NULL);

	/*
	 * Configure nRT partition
	 */

	/* Handle NUMA */
	if (rt_numa_set != NULL)
		cpuset_write(partition_nrt, "cpuset.mems", nrt_numa_list, NULL);
	else
		cpuset_write(partition_nrt, "cpuset.mems", "0", NULL);

	/* Allocate CPUs */
	cpuset_write(partition_nrt, "cpuset.cpus", nrt_list, NULL);

	/* Move all tasks/processes from root partition to NRT */
	cpuset_move_all_tasks(partition_root, partition_nrt);

	/* Enable load balancing in NRT partition */
	cpuset_write(partition_nrt, "cpuset.sched_load_balance", "1", NULL);

	/*
	 * Tweak kernel parameters to get better real-time characteristics
	 */

	/* Open settings log file, which can be used to restore settings */
	log_file = fopen(PARTRT_SETTINGS_FILE, "w");
	if (log_file == NULL)
		fail("%s: Failed open(): %s",
			PARTRT_SETTINGS_FILE, strerror(errno));

	fprintf(log_file, "%s", ctime(&current_time));

	/* Try to avoid 1 second ticks, currently relies on a separate patch */
	if (defer_ticks)
		file_write(AT_FDCWD, "/sys/kernel/debug/sched_tick_max_deferment", "-1", log_file);

	/* Disable NUMA affinity */
	if (disable_numa_affinity)
		file_write(AT_FDCWD, "/sys/bus/workqueue/devices/writeback/numa", "0", log_file);

	/* Disable kernel watchdog */
	if (disable_watchdog)
		file_write(AT_FDCWD, "/proc/sys/kernel/watchdog", "0", log_file);

	/* Move block device writeback workqueues */
	if (migrate_bwq)
		file_write(AT_FDCWD, "/sys/bus/workqueue/devices/writeback/cpumask", nrt_mask, log_file);

	/* Disable machine check. Writing 0 to machinecheck0/check_interall
	 * will siable it for all CPUs */
	if (disable_machine_check)
		file_write(AT_FDCWD, "/sys/devices/system/machinecheck/machinecheck0/check_interval", "0", log_file);

	info("System was successfylly divided into following partitions:");
	info("Isolated CPUs    : %s", rt_list);
	info("Non-isolated CPUs: %s", nrt_list);

	fflush(stdout);

	return 0;
}


/*
 * TODO:
 * - Introduce command line options to specify that isolcpus or nohz_full
 *   kernel parameter should be used as CPU list.
 * - Rename NRT to GP?
 */
