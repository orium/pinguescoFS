#define FUSE_USE_VERSION 26

#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fuse.h>
#include <stdint.h>
#include <assert.h>
#include "globals.h"
#include "superblock.h"
#include "allocator.h"
#include "dir.h"
#include "file.h"
#include "stat.h"
#include "open_files.h"

static int open_device(const char const *);
static void usage(FILE *, char *);
void pfs_fsync(void);
void pfs_destroy(void *);
int main(int, char **);

struct fuse_operations pfs_op=
{
	.mkdir=pfs_mkdir,
	.readdir=pfs_readdir,
	.opendir=pfs_opendir,
	.releasedir=pfs_releasedir,
	.destroy=pfs_destroy,
        .getattr=pfs_getattr,
	.access=pfs_access,
	.rmdir=pfs_rmdir,
	.rename=pfs_rename,
	.statfs=pfs_statfs,
        .create=pfs_create,
	.flush=pfs_flush,
	.release=pfs_release,
        .open=pfs_open,
	.unlink=pfs_unlink,
	.read=pfs_read,
	.write=pfs_write,
	.truncate=pfs_truncate,
};

static int
open_device(const char const *dev)
{
        devfd=open(dev,O_RDWR);

        if (devfd < 0)
        {
                perror("open()");
                return -1;
        }

        return 0;
}

static void
usage(FILE *out, char *argv0)
{
        fprintf(stderr,"Usage: %s [OPTIONS] <mountpoint> <device>\n",argv0);  
}

void
pfs_sync(void)
{
        lock();
        synch_fat();
        synch_superblock();
        unlock();
}

void
pfs_destroy(void *nothing)
{
        lock();
        pfs_sync();
        close(devfd);

        free_fat();

        free_open_files();

        unlock();

        pthread_mutex_destroy(&mutex);
}

int 
main(int argc, char **argv)
{
        pthread_mutexattr_t attr; 
        char *device;
        int z;

        if (argc < 3)
        {
                usage(stderr,argv[0]);
                return -1;
        }

        argc--;
        device=argv[argc];
        argv[argc]=NULL;

        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&mutex, &attr); 
        
        z=open_device(device);
        
        if (z)
        {
                pthread_mutex_destroy(&mutex);
                fprintf(stderr,"error: Error opening device!\n");
                return -1;
        }

        z=init_superblock();

        if (z)
        {
                pthread_mutex_destroy(&mutex);
                fprintf(stderr,"error: Error reading superblock!\n");
                return -1;
        }

        z=init_fat();

        if (z)
        {
                pthread_mutex_destroy(&mutex);
                fprintf(stderr,"error: Error initializing fat!\n");
                return -1;
        }

        init_open_files();

	return fuse_main(argc,argv,&pfs_op,NULL);
}
