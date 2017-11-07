bin_PROGRAMS=formatter

formatter_SOURCES=formatter.c \
		 ../src/disk.c

noinst_HEADERS=

#Set the include path found by configure
INCLUDES=$(all_includes)

#Set the library search path.
formatter_LDFLAGS=$(all_libraries) 

clean-local:
	rm -f *~
