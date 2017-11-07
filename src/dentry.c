#include <string.h>
#include <assert.h>
#include "debug.h"
#include "dentry.h"
#include "disk.h"
#include "globals.h"

void
read_dentry_table(block_t block, struct dentry_table *dtable)
{
        uint8_t b[BLOCK_SIZE];

        lock();

        debug("read_dentry_table(): block: %d\n",block);

        seek(devfd,block_addr(block));

        cread(devfd,b,BLOCK_SIZE);

        for (int i=0; i < DENTRY_TABLE_ENTRIES; i++)
        {
                memcpy(dtable->file[i].name,
                       b+DENTRY_SIZE*i,
                       MAX_FILENAME_LENGTH);
                memcpy(&dtable->file[i].inode,
                       b+DENTRY_SIZE*i+MAX_FILENAME_LENGTH,
                       sizeof(dtable->file[i].inode));
        }

        memcpy(&dtable->next,
               b+DENTRY_TABLE_NEXT_POINTER_OFFSET,
               sizeof(dtable->next));

        unlock();
}

void
write_dentry_table(block_t block, struct dentry_table const *dtable)
{
        uint8_t b[BLOCK_SIZE];

        lock();

        debug("write_dentry_table(): block: %d\n",block);

        for (int i=0; i < DENTRY_TABLE_ENTRIES; i++)
        {
                memcpy(b+DENTRY_SIZE*i,
                       dtable->file[i].name,
                       MAX_FILENAME_LENGTH);
                memcpy(b+DENTRY_SIZE*i+MAX_FILENAME_LENGTH,
                       &dtable->file[i].inode,
                       sizeof(dtable->file[i].inode));
        }

        memcpy(b+DENTRY_TABLE_NEXT_POINTER_OFFSET,
               &dtable->next,
               sizeof(dtable->next));

        seek(devfd,block_addr(block));
        cwrite(devfd,b,BLOCK_SIZE);

        unlock();
}

void
init_emtpy_dtable(struct dentry_table *dtable)
{
        memset(dtable,0,sizeof(*dtable));
}

void
init_dtable(struct dentry_table *dtable, const char *filename, block_t inode)
{
        assert(strlen(filename) < MAX_FILENAME_LENGTH);

        init_emtpy_dtable(dtable);
        strcpy(dtable->file[0].name,filename);
        dtable->file[0].inode=inode;
}
