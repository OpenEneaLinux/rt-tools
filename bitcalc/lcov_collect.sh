#!/bin/sh -eu

cd ..
lcov --directory . --capture --output-file coverage.info.new
if [ -f coverage.info ]; then
    lcov --directory . --add-tracefile coverage.info --add-tracefile coverage.info.new --output-file coverage.info
    rm coverage.info.new
else
    mv coverage.info.new coverage.info
fi
