#define _GNU_SOURCE

#include <sched.h>

#ifdef HAVE_LTTNG
#  include <lttng/tracef.h>
#else
#  define tracef(...)
#endif

/*
 * partrt.c
 */
extern const char *nrt_partition;
extern const char *rt_partition;
extern void fail(const char *format, ...);
extern void info(const char *format, ...);
extern void debug(const char *format, ...);
extern unsigned long long option_to_ul(const char *str, unsigned long min,
				unsigned long max, const char *errprefix);

/*
 * partrt_create.c
 */

extern int cmd_create(int argc, char *argv[]);

/*
 * partrt_cpumask.c
 */

extern int cpuset_nr_cpus(void);
extern cpu_set_t *cpuset_alloc_zero(void);
extern void cpuset_free(cpu_set_t *set);
extern void cpuset_set(int cpu, cpu_set_t *set);
extern cpu_set_t *cpuset_alloc_set(int cpu);
extern int cpuset_isset(int cpu, const cpu_set_t *set);
extern size_t cpuset_alloc_size(void);
extern const char *cpuset_hex(const cpu_set_t *set);
extern cpu_set_t *cpuset_from_list(const char *list);
