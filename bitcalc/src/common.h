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

#ifndef BITCALC_H
#define BITCALC_H

#include <sys/types.h>

#ifdef HAVE_LTTNG
#  include <lttng/tracef.h>
#endif

#define STR(x) #x
#define STRSTR(x) STR(x)

extern int option_verbose;
extern const char *parse_scope;

extern void std_fail(const char *format, ...);
extern void std_info(const char *format, ...);
extern void std_debug(const char *format, ...);
extern void *checked_malloc(size_t size);
extern void *checked_realloc(void *old_alloc, size_t new_size);

#define DO_LOG(func, fmt, ...) \
	do { \
		if (option_verbose > 1) \
			func(__FILE__ ":" STRSTR(__LINE__) ": %s(): " fmt "\n", __func__, ##__VA_ARGS__); \
		else \
			func(fmt "\n", ##__VA_ARGS__); \
	} while (0)

#ifdef HAVE_LTTNG

#  define fail(fmt, ...)                        \
        do {                                    \
            tracef("Fail: "  __FILE__ ":" STRSTR(__LINE__) ": %s(): " fmt, __func__, ##__VA_ARGS__); \
	    DO_LOG(std_fail, fmt, ##__VA_ARGS__); \
        } while (0)

#  define info(fmt, ...)                        \
        do {                                    \
            tracef("Info: "  __FILE__ ":" STRSTR(__LINE__) ": %s(): " fmt, __func__, ##__VA_ARGS__); \
            DO_LOG(std_info, fmt, ##__VA_ARGS__); \
        } while (0)

#  define debug(fmt, ...)                       \
        do {                                    \
            tracef("Debug: " __FILE__ ":" STRSTR(__LINE__) ": %s(): " fmt, __func__, ##__VA_ARGS__); \
            DO_LOG(std_debug, fmt, ##__VA_ARGS__); \
        } while (0)

#else

#  define fail(fmt, ...) DO_LOG(std_fail, fmt, ##__VA_ARGS__)
#  define info(...) DO_LOG(std_info, fmt, ##__VA_ARGS__)
#  define debug(...) DO_LOG(std_debug, fmt, ##__VA_ARGS__)

#endif
#endif

