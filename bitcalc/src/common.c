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

#include "common.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/sysinfo.h>

int option_verbose = 0;
const char *parse_scope = NULL;

void std_fail(const char *format, ...)
{
	va_list va;

	if (parse_scope != NULL)
		fprintf(stderr, "Error while parsing %s: ", parse_scope);
	else
		fprintf(stderr, "Error: ");
	va_start(va, format);
	vfprintf(stderr, format, va);
	va_end(va);

	exit(EXIT_FAILURE);
}

void std_info(const char *format, ...)
{
	va_list va;

	if (option_verbose < 1) return;

	if (parse_scope != NULL)
		fprintf(stderr, "Parsing %s: ", parse_scope);

	va_start(va, format);
	vfprintf(stderr, format, va);
	va_end(va);
}

void std_debug(const char *format, ...)
{
	va_list va;

	if (option_verbose < 2) return;

	if (parse_scope != NULL)
		fprintf(stderr, "Parsing %s: ", parse_scope);

	va_start(va, format);
	vfprintf(stderr, format, va);
	va_end(va);
}

void *checked_malloc(size_t size)
{
	void *mem = malloc(size);
	if (mem == NULL)
		fail("Out of memory allocating %zu bytes\n", size);

	memset(mem, 0, size);

	return mem;
}

void *checked_realloc(void *old_alloc, size_t new_size)
{
	void *mem = realloc(old_alloc, new_size);
	if (mem == NULL)
		fail("Out of memory allocating %zu bytes\n", new_size);
	return mem;
}

