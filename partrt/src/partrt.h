#define _GNU_SOURCE

#include <sched.h>

#define STR(x) #x
#define STRSTR(x) STR(x)

/*
 * For those believing in printf debugging, lttng is an alternative. Use
 * TRACEF() macro like a printf() statement to do the debugging. Then use
 * the following commands:
 * make
 * lttng create
 * lttng enable-event -u -a
 * lttng start
 * make test
 * lttng stop
 * lttng view
 *
 * Use "lttng destroy" before starting over, or else the trace log will contain
 * old entries as well as new ones.
 */
#ifdef HAVE_LTTNG
#  include <lttng/tracef.h>
#  define TRACEF(fmt, ...) tracef(__FILE__ ":" STRSTR(__LINE__) ": %s(): " fmt, __func__, ##__VA_ARGS__)
#else
#  define TRACEF(...)
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
extern char *cpuset_hex(const cpu_set_t *set);
extern char *cpuset_list(const cpu_set_t *set);
extern cpu_set_t *cpuset_alloc_from_list(const char *list);
extern cpu_set_t *cpuset_alloc_complement(const cpu_set_t *set);
extern cpu_set_t *cpuset_alloc_from_mask(const char *mask);
