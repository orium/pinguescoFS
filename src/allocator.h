#ifndef ALLOCATOR_H__
#define ALLOCATOR_H__

#include <stdint.h>

#define GB (1024*1024*1024)

/* number of bytes of a block*/
#define BLOCK_SIZE 1024

/* maximum blocks the fs can have */
#define MAX_BLOCKS ((1ULL<<32)/BLOCK_SIZE)

/* number of entries in the FAT table */
#define FAT_ENTRIES (4*(GB/BLOCK_SIZE))

/* fat table size in bytes */
#define FAT_SIZE (sizeof(block_t)*FAT_ENTRIES)

/* number of blocks the FAT table takes on disk */
#define FAT_BLOCKS (FAT_SIZE/BLOCK_SIZE)

#define FAT_INITIAL_BLOCK 1

/* translates block indexes to disk addresses */
#define block_addr(b) ((b)*BLOCK_SIZE)

typedef uint32_t block_t;

extern int init_fat(void);
extern block_t alloc_block(void);
extern void free_block(block_t block);
extern uint32_t get_free_blocks_count(void);
extern void synch_fat(void);
extern void free_fat(void);

#endif
