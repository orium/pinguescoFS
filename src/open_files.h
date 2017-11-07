#ifndef OPEN_FILES_H__
#define OPEN_FILES_H__

#include <stdbool.h>
#include "allocator.h"

extern void init_open_files(void);
extern void free_open_files(void);

extern int open_file(block_t inode_no);
extern void close_file(block_t inode_no);
extern bool is_file_opened(block_t inode_no);

#endif
