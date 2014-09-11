/*
 * Copyright (c) 2013,2014 by Enea Software AB
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
	     "  and    1 2 and      1 & 2\n"
	     "  xor    1 2 xor      1 ^ 2\n"
	     "\n"
	     "Options:\n"
	     "-v, verbose           Produce informational message to stderr. Can be given"
	     "                      multiple times for more verbosity.\n"
	     "-V, version           Show version information and exit.\n"
	     "-h, help              Print this help text and exit.\n"
	     "-f, --file=<script>   Execute file <script>, '-' means stdin.\n"
	     "\n"
	     "Example:\n"
	     "   bitcalc '#1-2,4-5 #2-4 xor'\n"
	     "   echo '#1-2,4-5 #2-4 xor' | bitcalc --file=-\n"
		);

	exit(0);
}

static void version(void)
{
	printf("bitcalc %d.%d\n"
		"\n"
		"Copyright (C) 2014 by Enea Software AB.\n"
		"This is free software; see the source for copying conditions.  There is NO\n"
		"warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE,\n"
		"to the extent permitted by law.\n"
		,
		bitcalc_VERSION_MAJOR, bitcalc_VERSION_MINOR
		);

	exit(0);
}

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
		bitmap_stack = checked_realloc(bitmap_stack, BITMAP_STACK_GROW_SIZE
			* sizeof (*bitmap_stack));
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

static void execute_binary_operator(
	const char *name,
	struct bitmap_t * (*func)(struct bitmap_t *first, struct bitmap_t *second)
	)
{
	struct bitmap_t *first;
	struct bitmap_t *second;
	struct bitmap_t *result;

	parse_scope = name;
	debug("%s: Identified as %s", name);

	if (bitmap_stack_depth < 2)
		fail("Need two values, but %zu available", bitmap_stack_depth);

	first = pop_bitmap();
	second = pop_bitmap();
	result = func(first, second);
	bitmap_free(first);
	bitmap_free(second);
	push_bitmap(result);
}

static void execute_token(const char *token)
{
	if (*token == '#') {
		/* Token contains ",", assume it is a list of u32
		 * hexadecimal values */
		parse_scope = "list";
		push_bitmap(bitmap_alloc_from_list(&token[1]));
	} else if (*token == '%') {
		parse_scope = "u32 list";
		push_bitmap(bitmap_alloc_from_u32_list(&token[1]));
	} else if (strcmp(token, "and") == 0) {
		execute_binary_operator("binary operator 'and'",
					bitmap_and);
	} else if (strcmp(token, "xor") == 0) {
		execute_binary_operator("binary operator 'xor'",
					bitmap_xor);
	} else {
		parse_scope = "mask";
		debug("%s: Identified as %s", token, parse_scope);
		push_bitmap(bitmap_alloc_from_mask(token));
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
	     token != NULL;
	     token = strtok(NULL, " \n\r\t"))
		execute_token(token);

	free(str);
}

static void execute_stream(FILE *stream)
{
	char *token;
	int nr_matches;

	for (nr_matches = fscanf(stream, "%ms ", &token);
	     nr_matches == 1;
	     nr_matches = fscanf(stream, "%ms ", &token)) {
		execute_token(token);
		free(token);
	}
}

int main(int argc, char *argv[])
{
	static const struct option long_options[] = {
		{ "help",    no_argument,       NULL, 'h' },
		{ "verbose", no_argument,       NULL, 'v' },
		{ "version", no_argument,       NULL, 'V' },
		{ "file",    required_argument, NULL, 'f' },
		{ "format",  required_argument, NULL, 'F' },
		{ NULL,      0,                 NULL, '\0'}
	};
	static const char short_options[] = "hvVf:F:";
	int c;
	struct bitmap_t *item;
	FILE *stream;
	int first;

	debug("bitcalc: Execution begins");

	while ((c = getopt_long(argc, argv, short_options, long_options,
				NULL)) != -1) {
		switch(c) {
		case 'h':
			usage();
		case 'V':
			version();
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
					fail("%s: Error opening file for reading: %s",
						optarg, strerror(errno));
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
				fail("%s: %s is an unknown format", argv[optind], optarg);
			break;
		case '?':
			exit(1);
		default:
			fail("Internal error: '-%c': Switch accepted but not implemented\n",
				c);
		}
	}

	while (optind < argc) execute_string(argv[optind++]);

	debug("Calculations finished successfully, %zu item%s in stack", bitmap_stack_depth,
		(bitmap_stack_depth == 1) ? "" : "s");
	first = 1;
	for (item = pop_bitmap(); item != NULL; item = pop_bitmap()) {
		char * const bitmap = bitmap_str(item);
		printf("%s%s", first ? "" : " ", bitmap);
		bitmap_free(item);
		first = 0;
	}

	putchar('\n');

	assert(bitmap_stack_depth == 0);
}

