#include <errno.h>
#include <sys/stat.h>
#include "stat.h"
#include "inode.h"
#include "file.h"
#include "allocator.h"
#include "dentry.h"
#include "superblock.h"
#include "globals.h"
#include "debug.h"

int
pfs_getattr(const char *path, struct stat *stat)
{
        block_t inode_block;
        struct inode inode;

        lock();

        debug("pfs_getattr(): path: \"%s\"\n",path);

        inode_block=get_inode_block(path);

        if (!inode_block)
        {
                unlock();
                return -ENOENT;
        }

        read_inode(inode_block,&inode);

        stat->st_ino=inode_block;
        stat->st_blksize=BLOCK_SIZE;
        stat->st_nlink=1;

        stat->st_blocks=inode.blocks;
        stat->st_mode=0777;

        switch (inode.type)
        {
        case INODE_DIR: 
        {
                stat->st_size=inode.blocks*BLOCK_SIZE;
                stat->st_mode|=S_IFDIR; 
                break;
        }
        case INODE_RFILE: 
        {
                stat->st_size=inode.size;
                debug("pfs_getattr(): st_size: %d\n",inode.size);
                debug("pfs_getattr(): inode: triple_ind_block: %d\n",
                      inode.triple_ind_block);
                stat->st_mode|=S_IFREG; 
                break;
        }
        }

        debug("pfs_getattr(): st_size: %d\n",(int)stat->st_size);

        unlock();

        return 0;
}

int
pfs_statfs(const char *unused, struct statvfs *stats)
{
        lock();
        
        stats->f_bsize=BLOCK_SIZE;
        stats->f_frsize=stats->f_bsize;
        stats->f_namemax=MAX_FILENAME_LENGTH-1;

        debug("pfs_statfs(): files: %d\n",get_inode_number());
        debug("pfs_statfs(): blocks: %d\n",get_block_number());
        debug("pfs_statfs(): free: %d\n",get_free_blocks_count());

        stats->f_files=get_inode_number();
        stats->f_blocks=get_block_number();
        
        stats->f_bfree=get_free_blocks_count();
        stats->f_ffree=stats->f_bfree;
        stats->f_favail=stats->f_ffree;
        stats->f_bavail=stats->f_favail;

        unlock();

        return 0;
}
