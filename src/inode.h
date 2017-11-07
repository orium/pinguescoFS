#ifndef INODE_H__
#define INODE_H__

#include <stdint.h>
#include "allocator.h"

#define ROOT_INODE_BLOCK (FAT_BLOCKS+1)

typedef uint8_t inode_type_t;

#define INODE_DIR   ((inode_type_t)0)
#define INODE_RFILE ((inode_type_t)1)

struct inode
{
        inode_type_t type;

        /* used when type == INODE_DIR */
        block_t dtable; 

        /* number of blocks of this file, including inode */
        uint32_t blocks;

        /* used when type == INODE_RFILE */
        uint32_t size;
        block_t single_ind_block;
        block_t double_ind_block;
        block_t triple_ind_block;
};

extern void read_inode(block_t block, struct inode *inode);
extern void write_inode(block_t block, struct inode const *inode);

#endif
