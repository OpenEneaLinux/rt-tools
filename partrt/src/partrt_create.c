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

static int dry_run = 0;

static int disable_numa_affinity = 1;
static int migrate_bwq = 1;
static int disable_machine_check = 1;
static int defer_ticks = 1;
static int numa_partition = 0;
static const char *rt_numa_node = "0";
/* static const char *nrt_numa_nodes = "0"; */
static int restart_hotplug = 1;
static int disable_watchdog = 1;
static struct bitmap_t *rt_set = NULL;
static struct bitmap_t *nrt_set = NULL;
static const char *rt_mask = NULL;
static const char *nrt_mask = NULL;
static const char *rt_list = NULL;
static const char *nrt_list = NULL;

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

#if 0
static move_task(pid_t pid, const char *partition)
{
	const char * const report_name =
		(partition[0] == '\0') ? "root" : partition;

	cpuset_write();
}
#endif

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
			numa_partition = 1;
			rt_numa_node = optarg;
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

	if (numa_partition && (rt_set != NULL))
		fail("partrt create: Specified both CPU list (-C/--cpu) and numa partition (-n/--numa), these options are mutually exclusive");

	if (rt_set == NULL) {
		if (numa_partition) {
			char *file_name;
			char *buf;
			asprintf(&file_name, "/sys/devices/system/node/node%s/cpumap", rt_numa_node);
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

	/*
	 * Configure RT partition
	 */

	/* Allocate CPUs */
	cpuset_write(partition_rt, "cpuset.cpus", rt_list, NULL);

	/* Make CPU list exclusive to RT partition */
	cpuset_write(partition_rt, "cpuset.cpu_exclusive", "1", NULL);

	/* Handle NUMA */
	if (numa_partition) {
		cpuset_write(partition_rt, "cpuset.mems", rt_numa_node, NULL);
		cpuset_write(partition_rt, "cpuset.mem_exclusive", "1", NULL);
	} else{
		cpuset_write(partition_rt, "cpuset.mems", "0", NULL);
	}

	/*
	 * Configure nRT partition
	 */

	

	/* TODO: Insert implementation here */
	


	fflush(stdout);

	return 0;
}


/*
 * TODO:
 * - Introduce command line options to specify that isolcpus or nohz_full
 *   kernel parameter should be used as CPU list.
 * - Rename NRT to GP?
 */
