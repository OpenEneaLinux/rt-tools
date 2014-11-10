#ifndef BITMAP_H
#define BITMAP_H

/*******************************************************************************
 * bitcalc_bitset.c
 */

struct bitmap_t;

/*
 * Convert bitmap to string
 */

/* Return a malloc'ed string with the bitmap presented with hexadecimal
 * characters. */
extern char *bitmap_hex(const struct bitmap_t *set);

/* Return a malloc'ed string with the bitmap presented as a comma separated
 * list of bit ranges. */
extern char *bitmap_list(const struct bitmap_t *set);

/* Return a malloc'ed string with the bitmap presented as a space separated
 * list of bit indexes. */
extern char *bitmap_array(const struct bitmap_t *set);

/* Return a malloc'ed string with comma separated unsigned 32 bit bit masks
 * presented with hexadecimal characters. */
extern char *bitmap_u32list(const struct bitmap_t *set);

/*
 * Allocate a new bitmap
 */

/* Allocate an empty bit mask */
extern struct bitmap_t *bitmap_alloc_zero(void);

/* Allocate a bit mask with a single bit set */
extern struct bitmap_t *bitmap_alloc_set(size_t bit);

/* Allocate a bit mask with the first nr_bits set */
extern struct bitmap_t *bitmap_alloc_nr_bits(size_t nr_bits);

/* Allocate a bit mask with the bits set as described by the string.
 * The string is a comma separated list of ranges. */
extern struct bitmap_t *bitmap_alloc_from_list(const char *list);

/* Allocate a bitmap which is the result of and'ing first and second
 * bit masks together. */
extern struct bitmap_t *bitmap_and(struct bitmap_t *first,
				   struct bitmap_t *second);


/* Allocate a bitmap which is the result of xor'ing first and second
 * bit masks together. */
extern struct bitmap_t *bitmap_xor(struct bitmap_t *first,
				   struct bitmap_t *second);

/* Create a bitmap_t from a string containing a list of hexadecimal
 * unsigned 32-bit values separated by commas. This format is used by some
 * files in the sysfs. */
extern struct bitmap_t *bitmap_alloc_from_u32_list(const char *mlist);

/*
 * Bitmap interrogation
 */

/* Return 1 if bit is set in bit mask, 0 otherwise. */
extern int bitmap_isset(size_t bit, const struct bitmap_t *set);

/* Return the number of bits set in bit mask. */
extern size_t bitmap_bit_count(const struct bitmap_t *set);

/*
 * Modify bitmap
 */

extern void bitmap_free(struct bitmap_t *set);

#endif
