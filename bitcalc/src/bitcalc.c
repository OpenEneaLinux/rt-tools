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

#define _GNU_SOURCE

#include "common.h"
#include "bitmap.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

#define BITMAP_STACK_GROW_SIZE 10

enum bitmap_format_t {
	format_mask,
	format_list,
	format_u32list
};

static enum bitmap_format_t display_format = format_mask;

static struct bitmap_t **bitmap_stack = NULL;
static size_t bitmap_stack_depth = 0;
static size_t bitmap_stack_depth_max = 0;

static char *bitmap_str(const struct bitmap_t *set)
{
	char *str;

	switch (display_format) {
	case format_mask:
		str = bitmap_hex(set);
		break;
	case format_list:
		str = bitmap_list(set);
		break;
	case format_u32list:
		str = bitmap_u32list(set);
		break;
	}

	return str;
}

static void push_bitmap(struct bitmap_t *entry)
{
	if (option_verbose > 2) {
		char *mask = bitmap_str(entry);
		debug("Pushing bitmap: %s\n", mask);
		free(mask);
	}

	if (bitmap_stack_depth_max <= bitmap_stack_depth) {
		bitmap_stack =
		    checked_realloc(bitmap_stack,
				    BITMAP_STACK_GROW_SIZE *
				    sizeof(*bitmap_stack));
		bitmap_stack_depth_max += BITMAP_STACK_GROW_SIZE;
	}

	bitmap_stack[bitmap_stack_depth] = entry;
	bitmap_stack_depth++;
}

static struct bitmap_t *pop_bitmap(void)
{
	if (bitmap_stack_depth == 0)
		return NULL;

	bitmap_stack_depth--;

	if (option_verbose > 2) {
		char *mask = bitmap_str(bitmap_stack[bitmap_stack_depth]);
		debug("Popping bitmap: %s\n", mask);
		free(mask);
	}

	return bitmap_stack[bitmap_stack_depth];
}

static void execute_void_unary_operator(
	const char *token,
	const char *name,
	void (*func) (struct bitmap_t * first)
    )
{
	struct bitmap_t *first;

	parse_scope = name;
	debug("%s: Identified as %s", token, name);

	if (bitmap_stack_depth < 1)
		fail("Need one value, but none available");

	first = pop_bitmap();
	func(first);
	bitmap_free(first);
}

static void execute_binary_operator(
	const char *token,
	const char *name,
	struct bitmap_t *(*func) (struct bitmap_t *first,
				struct bitmap_t *second)
    )
{
	struct bitmap_t *first;
	struct bitmap_t *second;
	struct bitmap_t *result;

	parse_scope = name;
	debug("%s: Identified as %s", token, name);

	if (bitmap_stack_depth < 2)
		fail("Need two values, but %zu available", bitmap_stack_depth);

	first = pop_bitmap();
	second = pop_bitmap();
	result = func(first, second);
	bitmap_free(first);
	bitmap_free(second);
	push_bitmap(result);
}

static size_t str_to_int_hex(const char *value)
{
	char *check;
	const unsigned long val = strtoul(value, &check, 16);

	if (value == NULL)
		fail("Expected unsigned integer value, got null pointer");

	if (value[0] == '\0')
		fail("Expected unsigned integer value, got empty string");

	if (check[0] != '\0')
		fail("'%s': Not a valid unsigned integer value", value);

	return val;
}

static void print_bitmap_bit_count(struct bitmap_t *set)
{
	const size_t count = bitmap_bit_count(set);
	printf("%zu", count);
}

static void execute_token(const char *token)
{
	if (*token == '#') {
		/* Token contains ",", assume it is a list of u32
		 * hexadecimal values */
		parse_scope = "list";
		debug("%s: Identified as %s", token, parse_scope);
		push_bitmap(bitmap_alloc_from_list(&token[1]));
	} else if (*token == '&') {
		parse_scope = "nr bits";
		debug("%s: Identified as %s", token, parse_scope);
		push_bitmap(bitmap_alloc_nr_bits(str_to_int_hex(&token[1])));
	} else if (strcmp(token, "and") == 0) {
		execute_binary_operator(token, "binary operator 'and'",
					bitmap_and);
	} else if (strcmp(token, "xor") == 0) {
		execute_binary_operator(token, "binary operator 'xor'",
					bitmap_xor);
	} else if (strcmp(token, "print-bit-count") == 0) {
		execute_void_unary_operator(token,
					"unary operator 'print-bit-count'",
					print_bitmap_bit_count);
	} else {
		parse_scope = "mask or u32 list";
		debug("%s: Identified as %s", token, parse_scope);
		push_bitmap(bitmap_alloc_from_u32_list(token));
	}

	parse_scope = NULL;
}

static void execute_string(const char *const_str)
{
	char *str = strdup(const_str);
	char *token;

	if (str == NULL)
		fail("Out of memory, aborting");

	for (token = strtok(str, " \n\r\t");
	     token != NULL; token = strtok(NULL, " \n\r\t"))
		execute_token(token);

	free(str);
}

static void execute_stream(FILE * stream)
{
	char *token;
	int nr_matches;

	for (nr_matches = fscanf(stream, "%ms ", &token);
	     nr_matches == 1; nr_matches = fscanf(stream, "%ms ", &token)) {
		execute_token(token);
		free(token);
	}
}

/*****************************************************************************
 * This is the man page using POD text format.
 *
 * For syntax description:
 *     http://perldoc.perl.org/perlpod.html

=head1 NAME

bitcalc - bitmask calculator

=head1 SYNOPSIS

bitcalc -F <format> | --format <format> | -f <file> | --file <file> | <script> ...

=head1 DESCRIPTION

bitcalc is a bit calculator which supports unlimited sized bit masks.
It supports the following output formats:

B<mask> where B<mask> is a hexadecimal masks optionally prefixed with
"0x", and that optionally has "," in it. The latter is to support the
bitmask format found in some files in the sysfs virtual file system.
Examples:
    ff30
    0xff30
    ff305050,10404020

#B<list> where B<list> is a comma separated ranges of bits. Examples:
    #1
    #1,5,3
    #0-5,9-15,7

&B<nr bits> where B<nr bits> is the number of bits starting with bit 0
that should be set. The most common use case would be to create a mask
out of the number of bits. Example:
    &13

The scripts are written in postfix notation, which means that all
parameters are given first, then the operator. For a binary operator,
there are two arguments, and for a unary operator there is one
argument. The operator will concume the number of arguments it needs,
leaving any extra on the stack, and place the resulting mask in their
place.

=head1 OPTIONS

B<bitcalc> will execute each option as it appears on the command line, so
the order they are given is important. This makes it possible to mix
scripts in files and scripts given on the ommand line.

Note however that only the last --format or -F will take effect.

B<-v,--verbose>
       Produce informational message to stderr

B<-V,--version>
       Show version information and exit

B<-h, --help>
       Show help text and exit

B<-f, --file=FILE>
       Execute commands from file, or from stdin if FILE is '-'

B<-F, --format=FORMAT>
       Set output format for bitmasks. Must be one of: 'mask', 'list', 'u32list'.
       Default: 'list'

B<SCRIPT> Execute commands given on the command line. Note that you need to enclose the script code in '' (single quote characters).

=head1 EXAMPLE

B<bitcalc '#0-3 #4-6 xor'>

Does the equivalent of "0xf ^ 0x70" in C programming language.

B<bitcalc '7f f and 3 xor'>

Does the equivalent of "(0x7f & 0xf) xor 0x3)" in C programming language.

B<bitcalc>

When calling bitcalc with no input, it will remain silent.
Nothing in, nothing out. This is deliberate, since it can be useful in
scripts where you might or might not provide a bitmask. If you do not
provide one, you do not expect one to be returned.

B<echo 0xff | bitcalc 0xff00 --file=- xor>

Shows how to mix --file argument with command line arguments.

B<echo xor | bitcalc 0xfe 0xfe00 -vv --file=->

Enable debug message when handling the --file argument.

=head1 AUTHOR

Mats Liljegren, Enea Software AB

=head1 REPORTING BUGS

Report bugs to openenealinux@lists.openenealinux.org

=head1 COPYRIGHT

Copyright (c) 2014 by Enea Software AB
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of Enea Software AB nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=cut

*****************************************************************************/

static void usage(void)
{
	puts("bitcalc - Perform bit calculations on the shell command line\n"
	     "Usage:\n"
	     "bitcalc [options] <script>...\n"
	     "\n"
	     "Performs bit calculations on the expression provided as script. The result will\n"
	     "printed to stdout. If an error occurs the error message is printed to stderr\n"
	     "and a non-zero return code is given.\n"
	     "\n"
	     "The script contains a list of tokens, where a token is either a constant or a\n"
	     "binary operator. Constants can be of two types, lists or masks. A list starts\n"
	     "with hash character '#', and then a comma separate list of ranges. A mask is\n"
	     "simply a hexadecimal value, optionally prefixed with 0x.\n"
	     "The following binary operators are supported, the script syntax and corresponding\n"
	     "C syntax:\n"
	     "  bitwise and:    1 2 and      1 & 2\n"
	     "  bitwise xor:    1 2 xor      1 ^ 2\n"
	     "\n"
	     "Options:\n"
	     "-V, version           Show version information and exit.\n"
	     "-h, help              Print this help text and exit.\n"
	     "-v, verbose           Produce informational message to stderr. Can be given"
	     "                      multiple times for more verbosity.\n"
	     "-f, --file=<script>   Execute file <script>, '-' means stdin.\n"
	     "-F, --format=<format> Set output format. One of 'mask', 'list', \n"
	     "                      and 'u32list'.\n"
	     "                      Default: 'mask'\n"
	     "\n"
	     "Example:\n"
	     "   bitcalc '#1-2,4-5 #2-4 xor'\n"
	     "   echo '#1-2,4-5 #2-4 xor' | bitcalc --file=-\n");
}

static void version(void)
{
	printf("bitcalc %d.%d\n"
	       "\n"
	       "Copyright (C) 2014 by Enea Software AB.\n"
	       "This is free software; see the source for copying conditions.  There is NO\n"
	       "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE,\n"
	       "to the extent permitted by law.\n",
	       bitcalc_VERSION_MAJOR, bitcalc_VERSION_MINOR);
}

int main(int argc, char *argv[])
{
	static const struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"verbose", no_argument, NULL, 'v'},
		{"version", no_argument, NULL, 'V'},
		{"file", required_argument, NULL, 'f'},
		{"format", required_argument, NULL, 'F'},
		{NULL, 0, NULL, '\0'}
	};
	static const char short_options[] = "-hvVf:F:";
	int c;
	struct bitmap_t *item;
	FILE *stream;
	int first;

	while ((c = getopt_long(argc, argv, short_options, long_options,
				NULL)) != -1) {
		switch (c) {
		case 'h':
			usage();
			return 0;
		case 'V':
			version();
			return 0;
		case 'v':
			option_verbose++;
			break;
		case 'f':
			if (strcmp(optarg, "-") == 0) {
				debug("<stdin>: Executing script");
				execute_stream(stdin);
			} else {
				debug("%s: Executing script", optarg);
				stream = fopen(optarg, "r");
				if (stream == NULL)
					fail("%s: Error opening file for reading: %s", optarg, strerror(errno));
				execute_stream(stream);
				if (fclose(stream) == -1)
					fail("%s: Error closing stream: %s",
					     optarg, strerror(errno));
			}
			break;
		case 'F':
			if (strcmp(optarg, "mask") == 0)
				display_format = format_mask;
			else if (strcmp(optarg, "list") == 0)
				display_format = format_list;
			else if (strcmp(optarg, "u32list") == 0)
				display_format = format_u32list;
			else
				fail("%s: %s is an unknown format",
				     argv[optind], optarg);
			break;
		case 1:
			execute_string(optarg);
			break;
		case '?':
			exit(1);
		default:
			fail("Internal error: '-%c': Switch accepted but not implemented\n", c);
		}
	}

	if (optind < argc)
		fail("%s: Unexpected argument", argv[optind]);

	debug("Calculations finished successfully, %zu item%s in stack",
	      bitmap_stack_depth, (bitmap_stack_depth == 1) ? "" : "s");
	first = 1;
	for (item = pop_bitmap(); item != NULL; item = pop_bitmap()) {
		char *const bitmap = bitmap_str(item);
		printf("%s%s", first ? "" : " ", bitmap);
		bitmap_free(item);
		first = 0;
	}

	if (first == 0)
		putchar('\n');

	assert(bitmap_stack_depth == 0);

	return 0;
}
