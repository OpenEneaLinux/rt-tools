#include "partrt.h"

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

static int disable_numa_affinity = 1;
static int migrate_bwq = 1;
static int disable_machine_check = 1;
static int defer_ticks = 1;
static int numa_partition = 0;
static int numa_node = 0;
static int restart_hotplug = 1;
static int disable_watchdog = 1;
static cpu_set_t *rt_set = NULL;
static cpu_set_t *nrt_set = NULL;
static const char *rt_mask = NULL;
static const char *nrt_mask = NULL;

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
	     "              partition"
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
		{ NULL,                  0,                 NULL, '\0'}
	};
	static const char short_options[] = "+abcdhn:rtwC:";
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
			numa_node = (int) option_to_ul(optarg, 0, INT_MAX,
						       "--numa");
			break;
		case 'r':
			restart_hotplug = 0;
			break;
		case 'w':
			disable_watchdog = 0;
			break;
		case 'C':
			rt_set = cpuset_alloc_from_list(optarg);
			break;
		case '?':
			exit(1);
		default:
			fail("Internal error: '-%c': Switch accepted but not implemented\n",
				c);
		}
	}

	if (rt_set == NULL) {
		if (optind >= argc)
			fail("partrt create: No CPU configured for RT partition, nothing to do\n");
		rt_set = cpuset_alloc_from_mask(argv[optind]);
		optind++;
	}

	if (optind < argc)
		fail("partrt create: Too many parameters given. Use 'partrt create --help' for help.\n");

	nrt_set = cpuset_alloc_complement(rt_set);

	rt_mask = cpuset_hex(rt_set);
	nrt_mask = cpuset_hex(nrt_set);

	info("RT partition : %s\n", rt_mask);
	info("nRT partition: %s\n", nrt_mask);

	/* Insert implementation here */
	


	fflush(stdout);

	return 0;
}
