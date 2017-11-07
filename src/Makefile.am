bin_PROGRAMS=pinguescofs

pinguescofs_SOURCES=pinguescofs.c \
                    globals.c     \
		    allocator.c   \
		    disk.c        \
		    superblock.c  \
		    dentry.c      \
		    dir.c         \
		    file.c        \
		    inode.c       \
		    stat.c        \
	            ind_block.c   \
		    open_files.c

noinst_HEADERS=globals.h    \
	       allocator.h  \
	       disk.h       \
	       superblock.h \
	       dentry.h     \
	       debug.h      \
	       inode.h      \
	       dir.h        \
	       file.h       \
	       stat.h       \
	       ind_block.h  \
	       open_files.h

# Set the include path found by configure
INCLUDES=$(all_includes)

# Set the library search path.
pinguescofs_LDFLAGS=$(all_libraries) 

clean-local:
	rm -f *~
