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
#  define TRACEF(fmt, ...) tracef("Trace: " __FILE__ ":" STRSTR(__LINE__) ": %s(): " fmt, __func__, ##__VA_ARGS__)
#else
#  define TRACEF(...)
#endif

/*******************************************************************************
 * partrt_common.c
 */
extern const char *nrt_partition;
extern const char *rt_partition;
extern void std_fail(const char *format, ...);
extern void std_info(const char *format, ...);
extern void std_debug(const char *format, ...);
extern void *checked_malloc(size_t size);
extern unsigned long long option_to_ul(const char *str, unsigned long min,
				unsigned long max, const char *errprefix);

#ifdef HAVE_LTTNG

#  define fail(fmt, ...)			\
	do {					\
	    tracef("Fail: "  __FILE__ ":" STRSTR(__LINE__) ": %s(): " fmt, __func__, ##__VA_ARGS__); \
	    std_fail(fmt "\n", ##__VA_ARGS__);	\
	} while (0)

#  define info(fmt, ...)			\
	do {					\
	    tracef("Info: "  __FILE__ ":" STRSTR(__LINE__) ": %s(): " fmt, __func__, ##__VA_ARGS__); \
	    std_info(fmt "\n", ##__VA_ARGS__);	\
	} while (0)

#  define debug(fmt, ...)			\
	do {					\
	    tracef("Debug: "  __FILE__ ":" STRSTR(__LINE__) ": %s(): " fmt, __func__, ##__VA_ARGS__); \
	    std_debug(fmt "\n", ##__VA_ARGS__);	\
	} while (0)

#else

#  define fail(...) std_fail(__VA_ARGS__)
#  define info(...) std_info(__VA_ARGS__)
#  define debug(...) std_debug(__VA_ARGS__)

#endif

/*******************************************************************************
 * partrt_create.c
 */

extern int cmd_create(int argc, char *argv[]);

/*******************************************************************************
 * partrt_cpumask.c
 */

extern int cpumask_nr_cpus(void);
extern cpu_set_t *cpumask_alloc_zero(void);
extern void cpumask_free(cpu_set_t *set);
extern void cpumask_set(int cpu, cpu_set_t *set);
extern cpu_set_t *cpumask_alloc_set(int cpu);
extern int cpumask_isset(int cpu, const cpu_set_t *set);
extern size_t cpumask_alloc_size(void);
extern size_t cpumask_cpu_count(const cpu_set_t *set);
extern char *cpumask_hex(const cpu_set_t *set);
extern char *cpumask_list(const cpu_set_t *set);
extern cpu_set_t *cpumask_alloc_from_list(const char *list);
extern cpu_set_t *cpumask_alloc_complement(const cpu_set_t *set);
extern cpu_set_t *cpumask_alloc_from_mask(const char *mask);

/*
 * Create a cpu_set_t from a string containing a list of hexadecimal
 * unsigned 32-bit values separated by commas. This format is used by some
 * files in the sysfs.
 */
extern cpu_set_t *cpumask_alloc_from_u32_list(const char *mlist);

/*******************************************************************************
 * partrt_cpuset.c
 */

enum CpufsPartition {
	partition_rt = 0,
	partition_nrt = 1
};

/*
 * Check whether partrt has already created its directories.
 */
extern int cpuset_is_empty(void);

/*
 * Write value into file named file_name in the directory given by partition.
 * Will log the old value by appending "<file>=<value>" line to file given
 * by value_log.
 *
 * partition - Specifies in which directory the file should be searched for.
 * file_name - Name of file to write to.
 * value     - Value to be written.
 * value_log - If non-NULL, refers to a file to which a line of the format
 *             "<file>=<value>" is to be appended to.
 */
extern void cpuset_write(enum CpufsPartition partition, const char *file_name,
			const char *value, FILE *value_log);


/*******************************************************************************
 * partrt_file.c
 */

extern char *file_fd_to_path_alloc(int fd);
extern char *file_read_alloc(int dirfd, const char *file);

