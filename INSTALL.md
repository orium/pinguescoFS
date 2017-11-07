# Install

The simplest way to compile this package is:

  1. `cd` to the directory containing the package's source code and type
     `./configure` to configure the package for your system.
     
     Running `configure` takes awhile.  While running, it prints some
     messages telling which features it is checking for.

  2. Type `make` to compile the package.

  3. Type `make install` to install the programs and any data files and
     documentation.

  4. You can remove the program binaries and object files from the
     source code directory by typing `make clean`.  To also remove the
     files that `configure` created (so you can compile the package for
     a different kind of computer), type `make distclean`.  There is
     also a `make maintainer-clean' target, but that is intended mainly
     for the package's developers.  If you use it, you may have to get
     all sorts of other programs in order to regenerate files that came
     with the distribution.
