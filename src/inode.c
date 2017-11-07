#include <string.h>
#include "globals.h"
#include "inode.h"
#include "disk.h"
#include "debug.h"

void
read_inode(block_t block, struct inode *inode)
{
        uint8_t b[sizeof(struct inode)];
        int offset;

        lock();

        debug("read_inode(): block: %d\n",block);

        seek(devfd,block_addr(block));

        cread(devfd,b,sizeof(b));

        offset=0;
        inode->type=b[offset];
        offset+=1;
        memcpy(&inode->dtable,b+offset,sizeof(inode->dtable));
        offset+=sizeof(inode->dtable);
        memcpy(&inode->blocks,b+offset,sizeof(inode->blocks));
        offset+=sizeof(inode->blocks);
        memcpy(&inode->size,b+offset,sizeof(inode->size));
        offset+=sizeof(inode->size);
        memcpy(&inode->single_ind_block,b+offset,
               sizeof(inode->single_ind_block));
        offset+=sizeof(inode->single_ind_block);
        memcpy(&inode->double_ind_block,b+offset,
               sizeof(inode->double_ind_block));
        offset+=sizeof(inode->double_ind_block);
        memcpy(&inode->triple_ind_block,b+offset,
               sizeof(inode->triple_ind_block));
        offset+=sizeof(inode->triple_ind_block);
        
        unlock();
}

void
write_inode(block_t block, struct inode const *inode)
{
        uint8_t b[sizeof(struct inode)];
        int offset;

        lock();

        debug("write_inode(): block: %d\n",block);

        /* just to avoid valgrind warning */
        memset(b,0,sizeof(b));

        offset=0;
        b[offset]=inode->type;
        offset+=1;
        memcpy(b+offset,&inode->dtable,sizeof(inode->dtable));
        offset+=sizeof(inode->dtable);
        memcpy(b+offset,&inode->blocks,sizeof(inode->blocks));
        offset+=sizeof(inode->blocks);
        memcpy(b+offset,&inode->size,sizeof(inode->size));
        offset+=sizeof(inode->size);
        memcpy(b+offset,&inode->single_ind_block,
               sizeof(inode->single_ind_block));
        offset+=sizeof(inode->single_ind_block);
        memcpy(b+offset,&inode->double_ind_block,
               sizeof(inode->double_ind_block));
        offset+=sizeof(inode->double_ind_block);
        memcpy(b+offset,&inode->triple_ind_block,
               sizeof(inode->triple_ind_block));
        offset+=sizeof(inode->triple_ind_block);

        seek(devfd,block_addr(block));
        cwrite(devfd,b,sizeof(b));

        unlock();
}
