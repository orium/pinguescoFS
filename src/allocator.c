#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include "allocator.h"
#include "superblock.h"
#include "globals.h"
#include "disk.h"
#include "debug.h"

struct fat
{
        block_t *next; /* table of next pointers */
        uint32_t free_blocks;
};

static struct fat fat;

static void read_fat(struct fat *);
static uint32_t count_free_blocks(void);

static void
read_fat(struct fat *fat)
{
        lock();

        seek(devfd,block_addr(FAT_INITIAL_BLOCK));
        cread(devfd,fat->next,FAT_SIZE);

        unlock();
}

uint32_t
get_free_blocks_count(void)
{
        uint32_t r;

        lock();

        r=fat.free_blocks;

        unlock();

        return r;
}

static uint32_t
count_free_blocks(void)
{
        uint32_t c=0;
        block_t b;

        lock();

        for (b=get_free_list_head(); b; b=fat.next[b])
                c++;

        unlock();

        return c;
}

int
init_fat(void)
{
        lock();
        fat.next=malloc(FAT_SIZE);

        if (!fat.next)
        {
                unlock();
                return -1;
        }

        read_fat(&fat);

        fat.free_blocks=count_free_blocks();

        unlock();

        return 0;
}

/* returns the block number of the new block, or 0 if none is free */
block_t
alloc_block(void)
{
        block_t block;

        lock();
        
        block=get_free_list_head();

        if (!block)
        {
                debug("alloc_block(): no more free blocks!\n");
                unlock();
                return 0;
        }

        set_free_list_head(fat.next[block]);

        debug("alloc_block(): alloced block %d, next %d\n",
              block,fat.next[block]);

        fat.free_blocks--;

        unlock();

        return block;
}

void
free_block(block_t block)
{
        block_t oldhead;

        lock();

        assert(block > FAT_BLOCKS);

        oldhead=get_free_list_head();

        fat.next[block]=oldhead;
        
        set_free_list_head(block);

        fat.free_blocks++;

        unlock();
}

void
synch_fat(void)
{
        lock();

        seek(devfd,block_addr(FAT_INITIAL_BLOCK));
        cwrite(devfd,fat.next,FAT_SIZE);

        unlock();
}

void
free_fat(void)
{
        free(fat.next);
}
