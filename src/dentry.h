#ifndef DENTRY_H__
#define DENTRY_H__

#include "allocator.h"

#define MAX_FILENAME_LENGTH 64 /* including the nul*/

#define DENTRY_SIZE (MAX_FILENAME_LENGTH+sizeof(block_t))

/* number of files per dentry_table */
#define DENTRY_TABLE_ENTRIES ((BLOCK_SIZE-sizeof(block_t))/DENTRY_SIZE)

#define DENTRY_TABLE_NEXT_POINTER_OFFSET (DENTRY_TABLE_ENTRIES*DENTRY_SIZE)

#define dentry_free(dentry) ((dentry)->name[0] == '\0')

struct dentry
{
        char name[MAX_FILENAME_LENGTH];
        block_t inode;
};

struct dentry_table
{
        struct dentry file[DENTRY_TABLE_ENTRIES];
        block_t next;
};

extern void init_emtpy_dtable(struct dentry_table *dtable);
extern void init_dtable(struct dentry_table *dtable, const char *filename, 
                        block_t inode);
extern void read_dentry_table(block_t block, 
                              struct dentry_table *dtable);
extern void write_dentry_table(block_t block, 
                               struct dentry_table const *dtable);

#endif
