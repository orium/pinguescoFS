#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include "debug.h"
#include "superblock.h"
#include "allocator.h"
#include "disk.h"
#include "globals.h"

#define MAGIC_SIZE sizeof(magic_t)

struct superblock
{
        block_t free_list_head;
        uint32_t blocks;
        uint32_t inodes;
} superblock;

static int check_magic(void);

static int
check_magic(void)
{
        magic_t m;
        int r;

        lock();

        assert(sizeof(m) == sizeof(MAGIC_NO));

        seek(devfd,0);
        cread(devfd,&m,sizeof(m));

        debug("check_magic(): read magic 0x%x\n",m);

        r=m != MAGIC_NO;

        unlock();

        return r;
}

int
init_superblock(void)
{
        lock();

        if (check_magic())
        {
                unlock();
                return -1;
        }

        seek(devfd,MAGIC_SIZE);
        cread(devfd,&superblock.free_list_head,
              sizeof(superblock.free_list_head));
        cread(devfd,&superblock.blocks,sizeof(superblock.blocks));
        cread(devfd,&superblock.inodes,sizeof(superblock.inodes));
        
        unlock();

        return 0;
}

block_t
get_free_list_head(void)
{
        block_t r;

        lock();

        r=superblock.free_list_head;

        unlock();

        return r;
}

void
set_free_list_head(block_t block)
{
        lock();
        superblock.free_list_head=block;
        unlock();
}

uint32_t
get_block_number(void)
{
        uint32_t r;

        lock();

        r=superblock.blocks;

        unlock();

        return r;
}

uint32_t
get_inode_number(void)
{
        uint32_t r;

        lock();

        r=superblock.inodes;

        unlock();

        return r;
}

void
inc_inode_number(void)
{
        lock();
        superblock.inodes++;
        unlock();
}

void
dec_inode_number(void)
{
        lock();
        superblock.inodes--;
        unlock();
}

void
synch_superblock(void)
{
        lock();

        seek(devfd,sizeof(magic_t));
        cwrite(devfd,&superblock.free_list_head,
               sizeof(superblock.free_list_head));
        cwrite(devfd,&superblock.blocks,sizeof(superblock.blocks));
        cwrite(devfd,&superblock.inodes,sizeof(superblock.inodes));
        
        unlock();
}
