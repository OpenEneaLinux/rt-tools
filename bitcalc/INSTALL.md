How to install bitcalc
======================

To build cmake, make sure you have the following:
 - CMake, 2.6 or newer
 - Make, 3.81 or newer
 - C compiler toolchain that CMake supports

Instructions below assumes that:
    <srcdir> is the directory where you found this file
    <builddir> is the directory where the build is done

Lines starting with $ (dollar) character are lines to be entered at the shell
command line, excluding the $ character.

Explaining install path
-----------------------
The install path consists of the following parts:
<DESTDIR>/<CMAKE_INSTALL_PREFIX>/<DESTINATION>/<file name>

The <DESTDIR> part is specified when building using "make install". If not
specified, this defaults to nothing, i.e. the <CMAKE_INSTALL_PREFIX> will
determine where to install to.
The <CMAKE_INSTALL_PREFIX> is specified when generating make files using the
cmake command. If not specified, this defaults to "/usr/local"
The <DESTINATION> is determined by the CMake scripts and depends on the file
type. For executables, this is "/bin", for man pages "/man" and so on.

If running cmake and make without arguments, the install path will be
"/usr/local/bin" for executables, "/usr/local/man" for man pages.

More details for each part of the install path is given in the instructions
below.

How to build
------------
$ mkdir -p <builddir>
$ cd <builddir>
$ cmake <srcdir>
$ make

Add "-DCMAKE_BUILD_TYPE=Debug" to the cmake command line if you want debug
symbols.

Add "-DCMAKE_INSTALL_PREFIX:PATH=<path>" to the cmake command line to specify
an install prefix to replace the default "/usr/local".

How to install
--------------
First follow instructions for how to build, then:

$ make install

Add "DESTDIR=<path>" to specify where to install. Defaults to nothing.

How to run unit tests
---------------------
After building, it is possible to run unit tests:

$ make test

Note that the "test" make target will not detect any changes made, so you must
run "make all" before "make test" in order to test the new code. You can use
following command to ensure this:

$ make all test

If tests fail, check the log:

$ cat Testing/Temporary/LastTest.log

