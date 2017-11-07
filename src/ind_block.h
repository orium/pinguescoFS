#ifndef IND_BLOCK_H__
#define IND_BLOCK_H__

#include "allocator.h"

/* number of pointers int a indirect block. */
#define INDIRECT_BLOCKS (BLOCK_SIZE/sizeof(block_t))

#define SINGLE_INDIRECTION 1
#define DOUBLE_INDIRECTION 2
#define TRIPLE_INDIRECTION 3

#define SIND_MAX INDIRECT_BLOCKS
#define DIND_MAX (SIND_MAX+INDIRECT_BLOCKS*INDIRECT_BLOCKS)
#define TIND_MAX (DIND_MAX+INDIRECT_BLOCKS*INDIRECT_BLOCKS*INDIRECT_BLOCKS)

struct ind_block
{
        block_t blocks[INDIRECT_BLOCKS];
};

extern void read_ind_block(block_t block, struct ind_block *ind_block);
extern void write_ind_block(block_t block, struct ind_block const *ind_block);

extern void init_empty_ind_block(struct ind_block *ind_block);

extern void free_ind_block(block_t block, int ind_level);

/* computes that path from the inode to the data block /block_no/ */
extern int get_data_block_path(int *path, int block_no);

#endif
