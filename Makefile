# Copyright (c) 2013,2014 by Enea Software AB
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of the <organization> nor the
#       names of its contributors may be used to endorse or promote products
#       derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#################
# Setup variables
#################


# This code must be before any includes
SRC_ROOT := $(abspath $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST)))))

# Check that SRC_ROOT exists
ifeq ($(wildcard $(SRC_ROOT)),)
$(error Could not determine source root! Has current working directory become stale?)
endif

# Default values for optional parameters.
# These are given to make it possible to check for unintentional use of
# uninitialized variables.
VERBOSE ?=
INSTALLDIR ?= $(CURDIR)/install

# If verbose mode is selected, change the shell to be executed for each command
# so that information is printed before the command is executed.
.SILENT:

# This assignment will make sure we get something better than default.
SHELL := $(shell which bash)
ifeq ($(VERBOSE),yes)
OLD_SHELL := $(SHELL)
SHELL = $(warning $(if $@,Building,Running shell command) $@$(if $<, (from $<))$(if $?, ($? newer)))$(OLD_SHELL) -x
$(info SRC_ROOT=$(SRC_ROOT))
endif

# Files to be copied to install dir
INSTALL_FILES.bin := partrt list2mask count_ticks
INSTALL_FILES.doc := man1/partrt.1

# The following targets are not files
.PHONY: all install clean

# Specify this target first so it becomes default
all: $(addprefix $(INSTALLDIR)/bin/,$(INSTALL_FILES.bin)) $(addprefix $(INSTALLDIR)/man/,$(INSTALL_FILES.doc))

$(INSTALLDIR)/bin:
	mkdir -p $@

install: $(INSTALL_FILES.bin) all $(INSTALLDIR)/bin

$(INSTALLDIR)/bin/%: $(SRC_ROOT)/bin/%
	echo "Installing $(@F)"
	install -D $< $@

install_doc: $(INSTALL_FILES.bin)

$(INSTALLDIR)/man/%: $(SRC_ROOT)/doc/%
	echo "Installing $(@F)"
	install -D $< $@

clean:
