#include "partrt.h"

#include <stdlib.h>
#include <limits.h>
#include <sys/sysinfo.h>

/*
 * CPU mask handling functions.
 */

int cpuset_nr_cpus(void)
{
	static int nr_cpus;

	if (nr_cpus == 0) {
		nr_cpus = get_nprocs_conf();
		if (nr_cpus <= 0)
			fail("Could not determine number of CPUs\n");
		info("%d CPUs configured in kernel\n", nr_cpus);
	}

	return nr_cpus;
}

cpu_set_t *cpuset_alloc_zero(void)
{
	cpu_set_t * const set = CPU_ALLOC(cpuset_nr_cpus());
	if (set == NULL)
		fail("Out of memory allocating CPU mask for %d CPUs\n",
			cpuset_nr_cpus());

	CPU_ZERO_S(CPU_ALLOC_SIZE(cpuset_nr_cpus()), set);

	return set;
}

void cpuset_free(cpu_set_t *set)
{
	CPU_FREE(set);
}

void cpuset_set(int cpu, cpu_set_t *set)
{
	if (cpu >= cpuset_nr_cpus())
		fail("Internal error: Illegal use of cpuset_set(%d,...): Only %d CPUs in the set\n",
			cpu, cpuset_nr_cpus());
	CPU_SET_S(cpu, CPU_ALLOC_SIZE(cpuset_nr_cpus()), set);
}

cpu_set_t *cpuset_alloc_set(int cpu)
{
	cpu_set_t *set = cpuset_alloc_zero();

	cpuset_set(cpu, set);

	return set;
}

int cpuset_isset(int cpu, const cpu_set_t *set)
{
	if (cpu > cpuset_nr_cpus())
		return 0;
	return CPU_ISSET_S(cpu, CPU_ALLOC_SIZE(cpuset_nr_cpus()), set);
}

size_t cpuset_alloc_size(void)
{
	return CPU_ALLOC_SIZE(cpuset_nr_cpus());
}

const char *cpuset_hex(const cpu_set_t *set)
{
	static char *str;
	char *curr;
	int cpu = 0;
	const int nr_cpus = cpuset_nr_cpus();
	const size_t str_size = 1 /* NUL */ + ((nr_cpus+3) / 4) /* round up */;

	if (str == NULL)
		str = malloc(str_size);

	if (str == NULL)
		fail("Out of memory allocating %zu bytes\n", str_size);

	curr = str + str_size - 1;
	*curr = '\0';

	while (cpu < nr_cpus) {
		char ch = 0;
		int bit;
		for (bit = 0; bit < 4; bit++, cpu++) {
			if (cpuset_isset(cpu, set))
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

cpu_set_t *cpuset_alloc_from_list(const char *list)
{
	const char * const original_list = list;
	cpu_set_t *set = cpuset_alloc_zero();

	while (*list != '\0') {
		int bit;
		char *endptr;
		long range_first;
		long range_last;

		range_first = strtol(list, &endptr, 0);
		if (endptr == list)
			fail("%s: Malformed CPU list\n", original_list);
		if (range_first > INT_MAX)
			fail("%s: %ld is out of range\n", original_list,
				range_first);
		list = endptr;
		if (*list == '-') {
			list++;
			range_last = strtol(list, &endptr, 0);
			if (endptr == list)
				fail("%s: Malformed CPU list\n", original_list);
			if (range_last > INT_MAX)
				fail("%s: %ld is out of range\n",
					original_list, range_last);
			list = endptr;
		} else {
			range_last = range_first;
		}

		/* Set all bits in range */
		for (bit = range_first; bit <= range_last; bit++)
			cpuset_set(bit, set);

		if (*list == ',')
			list++;
	}

	return set;
}

cpu_set_t *cpuset_alloc_complement(const cpu_set_t *set)
{
	cpu_set_t * const comp_set = cpuset_alloc_zero();
	const int nr_cpus = cpuset_nr_cpus();
	int cpu;

	for (cpu = 0; cpu < nr_cpus; cpu++) {
		if (! cpuset_isset(cpu, set))
			cpuset_set(cpu, comp_set);
	}

	return comp_set;
}

cpu_set_t *cpuset_alloc_from_mask(const char *mask)
{
	const int nr_cpus = cpuset_nr_cpus();
	cpu_set_t * const set = cpuset_alloc_zero();
	const char *curr;
	int bit = 0;

	if (strncmp(mask, "0x", 2) == 0) {
		TRACEF("Skipped 0x");
		mask += 2;
	}

	for (curr = mask + strlen(mask) - 1;
	     curr >= mask;
	     curr--) {
		int val;
		const int starting_bit = bit;

		if ((*curr >= '0') && (*curr <= '9'))
			val = *curr - '0';
		else if ((*curr >= 'a') && (*curr <= 'f'))
			val = *curr - 'a' + 10;
		else if ((*curr >= 'A') && (*curr <= 'F'))
			val = *curr - 'A' + 10;
		else
			fail("%s: Character '%c' is not legal in hexadecimal mask\n", mask, *curr);

		for (; bit < (starting_bit + 4); bit++) {
			if (val & (1 << (bit - starting_bit))) {
				if (bit >= nr_cpus)
					fail("%s: Illegal mask, only %d cpus detected\n",
						mask, nr_cpus);
				cpuset_set(bit, set);
			}
		}
	}

	return set;
}
