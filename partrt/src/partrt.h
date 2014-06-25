#define _GNU_SOURCE

#include <sched.h>

#define STR(x) #x
#define STRSTR(x) STR(x)

/*
 * For those believing in printf debugging, lttng is an alternative. Use
 * TRACEF() macro like a printf() statement to do the debugging. See
 * README files for more details.
 */
#ifdef HAVE_LTTNG
#  include <lttng/tracef.h>
#  define TRACEF(fmt, ...) tracef(__FILE__ ":" STRSTR(__LINE__) ": %s(): " fmt, __func__, ##__VA_ARGS__)
#else
#  define TRACEF(...)
#endif

/*
 * partrt_common.c
 */
extern const char *nrt_partition;
extern const char *rt_partition;
extern void fail(const char *format, ...);
extern void info(const char *format, ...);
extern void debug(const char *format, ...);
extern void *checked_malloc(size_t size);
extern unsigned long long option_to_ul(const char *str, unsigned long min,
				unsigned long max, const char *errprefix);

/*
 * partrt_create.c
 */

extern int cmd_create(int argc, char *argv[]);

/*
 * partrt_cpumask.c
 */

extern int cpumask_nr_cpus(void);
extern cpu_set_t *cpumask_alloc_zero(void);
extern void cpumask_free(cpu_set_t *set);
extern void cpumask_set(int cpu, cpu_set_t *set);
extern cpu_set_t *cpumask_alloc_set(int cpu);
extern int cpumask_isset(int cpu, const cpu_set_t *set);
extern size_t cpumask_alloc_size(void);
extern char *cpumask_hex(const cpu_set_t *set);
extern char *cpumask_list(const cpu_set_t *set);
extern cpu_set_t *cpumask_alloc_from_list(const char *list);
extern cpu_set_t *cpumask_alloc_complement(const cpu_set_t *set);
extern cpu_set_t *cpumask_alloc_from_mask(const char *mask);

/*
 * partrt_cpuset.c
 */

enum CpufsPartition {
	partition_rt = 0,
	partition_nrt = 1
};

extern int cpuset_is_empty(void);
extern void cpuset_write(enum CpufsPartition partition, const char *file_name,
			const char *value);
