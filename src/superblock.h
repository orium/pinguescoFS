#ifndef SUPERBLOCK_H__
#define SUPERBLOCK_H__

#include "allocator.h"

typedef uint32_t magic_t;

#define MAGIC_NO ((magic_t)0xbadebabe)

extern int init_superblock(void);
extern block_t get_free_list_head(void);
extern uint32_t get_block_number(void);
extern uint32_t get_inode_number(void);
extern void inc_inode_number(void);
extern void dec_inode_number(void);
extern void set_free_list_head(block_t block);
extern void synch_superblock(void);

#endif
