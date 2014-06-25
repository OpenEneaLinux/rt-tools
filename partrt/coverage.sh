#!/bin/sh -eu

#
# Run this script from a cmake build directory, after having built everything.
# This script will run "make test", and present code coverage based on the
# execution.
#

err () {
    printf "%s" "$*" > /dev/stderr

    exit 1
}

[ -f CMakeCache.txt ] || err Run this script from a cmake build directory.

lcov --directory . --zerocounters
make test
lcov --directory . --capture --output-file coverage.info
genhtml coverage.info

echo "Use 'firefox -safe-mode index.html' to view results"
