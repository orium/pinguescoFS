#include <string.h>
#include <assert.h>
#include "disk.h"
#include "globals.h"
#include "ind_block.h"

void
read_ind_block(block_t block, struct ind_block *ind_block)
{
        lock();

        seek(devfd,block_addr(block));

        cread(devfd,ind_block->blocks,sizeof(ind_block->blocks));

        unlock();
}

void
write_ind_block(block_t block, struct ind_block const *ind_block)
{
        lock();

        seek(devfd,block_addr(block));

        cwrite(devfd,ind_block->blocks,sizeof(ind_block->blocks));

        unlock();
}

void
init_empty_ind_block(struct ind_block *ind_block)
{
        memset(ind_block,0,sizeof(*ind_block));
}

void
free_ind_block(block_t block, int ind_level)
{
        struct ind_block ind_block;

        lock();

        if (block)
        {
                if (ind_level)
                        read_ind_block(block,&ind_block);
                free_block(block);
        }

        if (!block || !ind_level)
        {
                unlock();
                return;
        }

        for (int i=0; i < INDIRECT_BLOCKS; i++)
                free_ind_block(ind_block.blocks[i],ind_level-1);

        unlock();
}

int
get_data_block_path(int *path, int block_no)
{
        int i;

        memset(path,0,sizeof(*path)*4);

        if (block_no < SIND_MAX)
        {
                path[0]=SINGLE_INDIRECTION;
                i=1;
        }
        else if (block_no < DIND_MAX)
        {
                path[0]=DOUBLE_INDIRECTION;
                block_no-=SIND_MAX;
                i=2;
        }
        else if (block_no < TIND_MAX)
        {
                path[0]=TRIPLE_INDIRECTION;
                block_no-=DIND_MAX;
                i=3;
        }
        else
                return -1; /* block_no too large */

        while (block_no)
        {
                assert(0 <= i && i <= 3);
                path[i]=block_no%INDIRECT_BLOCKS;
                block_no/=INDIRECT_BLOCKS;
                i--;
        }

        return 0;
}
