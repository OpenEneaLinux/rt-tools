#ifndef BITMAP_H
#define BITMAP_H

/*******************************************************************************
 * bitcalc_cpuset.c
 */

struct bitmap_t;

extern struct bitmap_t *bitmap_alloc_zero();
extern void bitmap_free(struct bitmap_t *set);
extern void bitmap_set(int cpu, struct bitmap_t *set);
extern struct bitmap_t *bitmap_alloc_set(int cpu);
extern int bitmap_isset(int cpu, const struct bitmap_t *set);
extern size_t bitmap_bit_count(const struct bitmap_t *set);
extern char *bitmap_hex(const struct bitmap_t *set);
extern char *bitmap_list(const struct bitmap_t *set);
extern struct bitmap_t *bitmap_alloc_from_list(const char *list);
extern struct bitmap_t *bitmap_alloc_complement(const struct bitmap_t *set);
extern struct bitmap_t *bitmap_alloc_from_mask(const char *mask);
extern struct bitmap_t *bitmap_alloc_filter_out(
	const struct bitmap_t *base, const struct bitmap_t *filter);

/*
 * Create a bitmap_t from a string containing a list of hexadecimal
 * unsigned 32-bit values separated by commas. This format is used by some
 * files in the sysfs.
 */
extern struct bitmap_t *bitmap_alloc_from_u32_list(const char *mlist);
extern int bitmap_next_bit(int previous_bit, const struct bitmap_t *set);

/* Bitwise oepration */
extern struct bitmap_t *bitmap_and(struct bitmap_t *first, struct bitmap_t *second);
extern struct bitmap_t *bitmap_xor(struct bitmap_t *first, struct bitmap_t *second);

#endif