#include "partrt.h"

#include <stdlib.h>
#include <limits.h>
#include <sys/sysinfo.h>

/*
 * CPU mask handling functions.
 */

int cpumask_nr_cpus(void)
{
	static int nr_cpus;

	if (nr_cpus == 0) {
		nr_cpus = get_nprocs_conf();
		if (nr_cpus <= 0)
			fail("Could not determine number of CPUs");
		info("%d CPUs configured in kernel", nr_cpus);
	}

	return nr_cpus;
}

cpu_set_t *cpumask_alloc_zero(void)
{
	cpu_set_t * const set = CPU_ALLOC(cpumask_nr_cpus());
	if (set == NULL)
		fail("Out of memory allocating CPU mask for %d CPUs",
			cpumask_nr_cpus());

	CPU_ZERO_S(CPU_ALLOC_SIZE(cpumask_nr_cpus()), set);

	return set;
}

void cpumask_free(cpu_set_t *set)
{
	CPU_FREE(set);
}

void cpumask_set(int cpu, cpu_set_t *set)
{
	if (cpu >= cpumask_nr_cpus())
		fail("Internal error: Illegal use of cpumask_set(%d,...): Only %d CPUs in the set",
			cpu, cpumask_nr_cpus());
	CPU_SET_S(cpu, CPU_ALLOC_SIZE(cpumask_nr_cpus()), set);
}

cpu_set_t *cpumask_alloc_set(int cpu)
{
	cpu_set_t *set = cpumask_alloc_zero();

	cpumask_set(cpu, set);

	return set;
}

int cpumask_isset(int cpu, const cpu_set_t *set)
{
	if (cpu > cpumask_nr_cpus())
		return 0;
	return CPU_ISSET_S(cpu, CPU_ALLOC_SIZE(cpumask_nr_cpus()), set);
}

size_t cpumask_alloc_size(void)
{
	return CPU_ALLOC_SIZE(cpumask_nr_cpus());
}

size_t cpumask_cpu_count(const cpu_set_t *set)
{
	return CPU_COUNT_S(CPU_ALLOC_SIZE(cpumask_nr_cpus()), set);
}

char *cpumask_hex(const cpu_set_t *set)
{
	char *curr;
	int cpu = 0;
	const int nr_cpus = cpumask_nr_cpus();
	const size_t str_size = 1 /* NUL */ + ((nr_cpus+3) / 4) /* round up */;
	char *str = malloc(str_size);

	if (str == NULL)
		fail("Out of memory allocating %zu bytes", str_size);

	curr = str + str_size - 1;
	*curr = '\0';

	while (cpu < nr_cpus) {
		char ch = 0;
		int bit;
		for (bit = 0; bit < 4; bit++, cpu++) {
			if (cpumask_isset(cpu, set))
				ch |= (1 << bit);
		}
		curr--;
		if (ch > 9)
			*curr = 'a' + (ch - 10);
		else
			*curr = '0' + ch;
	}

	return str;
}

static int cpumask_list_write(size_t curr_idx, size_t size_alloced,
			int first_cpu, int last_cpu, char *str)
{
	int status;

	if (first_cpu == last_cpu)
		status = snprintf(str + curr_idx, size_alloced - curr_idx,
				"%s%d", (curr_idx > 0) ? "," : "", first_cpu);
	else
		status = snprintf(str + curr_idx, size_alloced - curr_idx,
				"%s%d-%d", (curr_idx > 0) ? "," : "",
				first_cpu, last_cpu);

	if (status < 0)
		fail("cpumask_list_write(): Internal error: snprintf() returned error");

	return status;
}

char *cpumask_list(const cpu_set_t *set)
{
	int curr_idx = 0;
	int size_alloced = 0;
	char *str = NULL;
	const int nr_cpus = cpumask_nr_cpus();
	int cpu;
	int first_cpu = -1;
	int last_cpu = -1;

	for (cpu = 0; cpu <= nr_cpus; cpu++) {
		int bytes_needed;

		if (first_cpu == -1) {
			if (cpumask_isset(cpu, set))
				first_cpu = cpu;
			continue;
		}

		if (cpumask_isset(cpu, set))
			continue;

		last_cpu = cpu - 1;
		bytes_needed = cpumask_list_write(
			curr_idx, size_alloced, first_cpu, last_cpu, str);
		if (bytes_needed >= (size_alloced - curr_idx)) {
			str = realloc(str, size_alloced + bytes_needed + 1);
			if (str == NULL)
				fail("Out of memory allocating %d bytes",
					size_alloced + bytes_needed + 1);
			size_alloced += bytes_needed + 1;
			bytes_needed = cpumask_list_write(
				curr_idx, size_alloced, first_cpu,
				last_cpu, str);

			if (bytes_needed >= (size_alloced - curr_idx))
				fail("cpumask_list(): Internal error: Did not allocate enough. bytes_needed=%d, size_alloced=%d, curr_idx=%d",
					bytes_needed, size_alloced, curr_idx);
		}

		curr_idx += bytes_needed;
		first_cpu = -1;
	}

	if (str == NULL) {
		str = malloc(1);
		if (str == NULL)
			fail("Out of memory allocating 2 bytes");
		str[0] = '\0';
	}
	return str;
}

cpu_set_t *cpumask_alloc_from_list(const char *list)
{
	const char * const original_list = list;
	cpu_set_t *set = cpumask_alloc_zero();

	while (*list != '\0') {
		int bit;
		char *endptr;
		long range_first;
		long range_last;

		range_first = strtol(list, &endptr, 0);
		if (endptr == list)
			fail("%s: Malformed CPU list", original_list);
		if (range_first > INT_MAX)
			fail("%s: %ld is out of range", original_list,
				range_first);
		list = endptr;
		if (*list == '-') {
			list++;
			range_last = strtol(list, &endptr, 0);
			if (endptr == list)
				fail("%s: Malformed CPU list", original_list);
			if (range_last > INT_MAX)
				fail("%s: %ld is out of range",
					original_list, range_last);
			list = endptr;
		} else {
			range_last = range_first;
		}

		/* Set all bits in range */
		for (bit = range_first; bit <= range_last; bit++)
			cpumask_set(bit, set);

		if (*list == ',')
			list++;
	}

	return set;
}

cpu_set_t *cpumask_alloc_complement(const cpu_set_t *set)
{
	cpu_set_t * const comp_set = cpumask_alloc_zero();
	const int nr_cpus = cpumask_nr_cpus();
	int cpu;

	for (cpu = 0; cpu < nr_cpus; cpu++) {
		if (! cpumask_isset(cpu, set))
			cpumask_set(cpu, comp_set);
	}

	return comp_set;
}

static cpu_set_t *alloc_from_mask(const char *mask, char ignore_char)
{
	const int nr_cpus = cpumask_nr_cpus();
	cpu_set_t * const set = cpumask_alloc_zero();
	const char *curr;
	int bit = 0;

	if (strncmp(mask, "0x", 2) == 0)
		mask += 2;

	for (curr = mask + strlen(mask) - 1;
	     curr >= mask;
	     curr--) {
		int val;
		const int starting_bit = bit;

		if ((ignore_char != '\0') && (*curr == ignore_char))
			continue;

		if ((*curr >= '0') && (*curr <= '9'))
			val = *curr - '0';
		else if ((*curr >= 'a') && (*curr <= 'f'))
			val = *curr - 'a' + 10;
		else if ((*curr >= 'A') && (*curr <= 'F'))
			val = *curr - 'A' + 10;
		else
			fail("%s: Character '%c' is not legal in hexadecimal mask", mask, *curr);

		for (; bit < (starting_bit + 4); bit++) {
			if (val & (1 << (bit - starting_bit))) {
				if (bit >= nr_cpus)
					fail("%s: Illegal mask, only %d cpus detected",
						mask, nr_cpus);
				cpumask_set(bit, set);
			}
		}
	}

	return set;
}

cpu_set_t *cpumask_alloc_from_mask(const char *mask)
{
	return alloc_from_mask(mask, '\0');
}

cpu_set_t *cpumask_alloc_from_u32_list(const char *mlist)
{
	return alloc_from_mask(mlist, ',');
}
