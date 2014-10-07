Linux Real-Time Tools
=====================

This repository contains a collection of tools for achieving real-time
performance in Linux. It is currently focused on CPU isolation using
cgroups/cpuset.

For information about license, see LICENSE file.

The following tools are available:

partrt        Partition the CPUs into two sets: One set for real-time
              applications and one set for the rest. The goal for this tool is
              to achive tickless execution on the real-time CPU set.
              See man page found in "doc" sub-directory for more information.

count_ticks   Counts number of ticks that occurs when executing one or several
              shell commands. Uses ftrace for this.

bitcalc       Bit calculator, helper application for partrt script.

Installing
----------
Make a new build directory. If this build directory is <build root> and the
directory where this file is found is called <src root>, do the following
to install:

cd <build root>
cmake <src root> -DCMAKE_INSTALL_PREFIX:PATH=<prefix>
make install DESTDIR=<destdir>

The installation directory will be <destdir>/<prefix>/bin for binaries,
and <destdir>/<prefix>/share/man for man pages.

If -DCMAKE_INSTALL_PREFIX:PATH is not given, then the default value is
/usr/local. The default value for DESTDIR is the empty string.

Contributing
------------

Send pull requests, patches, comments or questions to

     openenealinux@lists.openenealinux.org

When sending single patches, please using something like:

     git send-email -1 --to openenealinux@lists.openenealinux.org --subject-prefix=rt-tools][PATCH --compose

or, if this is re-send of patch having a version number 2:

     git send-email -1 --to openenealinux@lists.openenealinux.org --subject-prefix=rt-tools][PATCH v2 --compose

For multiple patches, "-1" is increased with how many patches from the top to
be sent.

Maintainer
----------

Mats Liljegren <mats.liljegren@enea.com>
