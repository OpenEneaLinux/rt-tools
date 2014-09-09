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

static void usage(void)
{
	puts("bitcalc - Perform bit calculations on the shell command line\n"
	     "Usage:\n"
	     "bitcalc [options] <script>\n"
	     "\n"
	     "Performs bit calculations on the expression provided as script. The result will\n"
	     "printed to stdout. If an error occurs the error message is printed to stderr\n"
	     "and a non-zero return code is given.\n"
	     "\n"
	     "Options:\n"
	     "-v           Produce informational message to stderr\n"
	     "-V           Show version information and exit.\n"
	     "-h           Print this help text and exit.\n"
	     "\n"
	     "Example:\n"
		);

	exit(0);
}

static void version(void)
{
	printf("bitcalc %d.%d\n"
		"\n"
		"Copyright (C) 2014 by Enea Software AB.\n",
		bitcalc_VERSION_MAJOR, bitcalc_VERSION_MINOR
		);

	exit(0);
}

static struct bitmap_t **bitmap_stack = NULL;
static size_t bitmap_stack_depth = 0;
static size_t bitmap_stack_depth_max = 0;

#define BITMAP_STACK_GROW_SIZE 10

static void push_bitmap(struct bitmap_t *entry)
{
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

	return bitmap_stack[bitmap_stack_depth];
}

static void execute_token(const char *token)
{
	struct bitmap_t *first;
	struct bitmap_t *second;
	struct bitmap_t *result;

	if (*token == '#') {
		/* Token contains ",", assume it is a list of u32
		 * hexadecimal values */
		debug("%s: Identified as list", token);
		push_bitmap(bitmap_alloc_from_list(&token[1], 0));
	} else if (strcmp(token, "and") == 0) {
		/* And operator, uses two elements from the stack */
		debug("%s: Identified as 'and' operator", token);
		if (bitmap_stack_depth < 2)
			fail("Operator 'and': Need two values");

		first = pop_bitmap();
		second = pop_bitmap();
		result = bitmap_and(first, second);
		bitmap_free(first);
		bitmap_free(second);
		push_bitmap(result);
	} else if (strcmp(token, "xor") == 0) {
		/* Xor operator, uses two elements from the stack */
		debug("%s: Identified as 'xor' operator", token);
		if (bitmap_stack_depth < 2)
			fail("Operator 'xor': Need two values");

		first = pop_bitmap();
		second = pop_bitmap();
		result = bitmap_xor(first, second);
		bitmap_free(first);
		bitmap_free(second);
		push_bitmap(result);
	} else {
		debug("%s: Identified as mask", token);
		push_bitmap(bitmap_alloc_from_mask(token, 0));
	}
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
	     nr_matches = fscanf(stream, "%ms ", &token))
		execute_token(token);
}

int main(int argc, char *argv[])
{
	static const struct option long_options[] = {
		{ "help",    no_argument,       NULL, 'h' },
		{ "verbose", no_argument,       NULL, 'v' },
		{ "version", no_argument,       NULL, 'V' },
		{ "file",    required_argument, NULL, 'f' },
		{ NULL,      0,                 NULL, '\0'}
	};
	static const char short_options[] = "hvVf";
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
		case '?':
			exit(1);
		default:
			fail("Internal error: '-%c': Switch accepted but not implemented\n",
				c);
		}
	}

	execute_string(argv[optind]);

	first = 1;
	for (item = pop_bitmap(); item != NULL; item = pop_bitmap()) {
		char * const bitmap = bitmap_hex(item);
		printf("%s%s", first ? "" : " ", bitmap);
		bitmap_free(item);
		first = 0;
	}
}
