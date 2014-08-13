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

/*
 * This file implements sub-command "undo".
 */

#include "partrt.h"

#include <getopt.h>
#include <string.h>
#include <errno.h>

static const char *option_settings_file = PARTRT_SETTINGS_FILE;
static int option_settings_file_specified = 0;

static void usage_create(void)
{
	puts("partrt [options] undo [cmd-options]\n"
	     "\n"
	     "Undo partitioning done by create sub-command."
	     "\n"
	     "cmd-options:\n"
	     "-h          Show this help text and exit.\n"
	     "-s <file>   Use <file> as restore file rather than " PARTRT_SETTINGS_FILE "\n"
		);

	exit(0);
}

static void restore_from_file(void)
{
	FILE * const file = fopen(option_settings_file, "r");
	int nr_matches;
	char *name;
	char *value;

	if (file == NULL) {
		if (!option_settings_file_specified
			&& (errno == ENOENT)) {
			info("%s: File does not exist, values will not be restored",
				option_settings_file);
			return;
		} else {
			fail("%s: Failed fopen(): %s",
				option_settings_file, strerror(errno));
		}
	}

	nr_matches = fscanf(file, "partrt_settings: %ms ", &value);
	if (nr_matches < 0)
		fail("%s: Failed fscanf(): %s",
			option_settings_file, strerror(errno));
	if (nr_matches != 1)
		fail("%s: Not a partrt settings files");

	info("Restoring settings made %s", value);

	for (nr_matches = fscanf(file, " %ms=%m[^\n] ", &name, &value);
	     nr_matches == 2;
	     nr_matches = fscanf(file, " %ms=%m[^\n] ", &name, &value)) {
		FILE * const settings_file = fopen(name, "w");
		size_t bytes_to_write = strlen(value);
		size_t bytes_written = 0;

		if (settings_file == NULL)
			fail("%s: Failed fopen(): %s",
				name, strerror(errno));
		do {
			bytes_written = fwrite(&value[bytes_written],
					bytes_to_write,
					1,
					settings_file);
			bytes_to_write -= bytes_written;
		} while (bytes_written != 0);

		if (ferror(settings_file))
			fail("%s: Failed fwrite(): %s",
				name, strerror(errno));

		info("%s = '%s'", name, value);

		if (fclose(settings_file) == EOF)
			fail("%s: Failed fclose(): %s",
				name, strerror(errno));

		free(name);
		free(value);
	}

	if (fclose(file) == EOF)
		fail("%s: Failed fclose(): %s",
			option_settings_file, strerror(errno));
}

int cmd_undo(int argc, char *argv[])
{
	static const struct option long_options[] = {
		{ "help",                no_argument,       NULL, 'h' },
		{ "settings-file",       required_argument, NULL, 's' },
		{ NULL,                  0,                 NULL, '\0'}
	};
	static const char short_options[] = "+hs:";
	int c;

	while ((c = getopt_long(argc, argv, short_options, long_options,
				NULL)) != -1) {
		switch (c) {
		case 'h':
			usage_create();
		case 's':
			option_settings_file = optarg;
			option_settings_file_specified = 1;
			break;
		case '?':
			exit(1);
		default:
			fail("Internal error: '-%c': Switch accepted but not implemented",
				c);
		}
	}

	restore_from_file();
	cpuset_partition_unlink();

	return 0;
}
