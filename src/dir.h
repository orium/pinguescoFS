#ifndef DIR_H__
#define DIR_H__

#include <stdbool.h>
#include <fuse.h>
#include "dentry.h"
#include "inode.h"
#include "dentry.h"

struct dir_iterator
{
        struct dentry_table dtable;
        int index;
        block_t dtable_block;
        block_t prev_dtable_block;
};

extern void dir_iterator_init_inode(struct dir_iterator *it, 
                                    struct inode *dir_inode);
extern void dir_iterator_init_dtable(struct dir_iterator *it, 
                                     block_t root_dtable_block);
extern void dir_iterator_init(struct dir_iterator *it, block_t dir_inode_block);
extern void dir_iterator_next(struct dir_iterator *it);
extern struct dentry *dir_iterator_get(struct dir_iterator *it);
extern bool dir_iterator_end(struct dir_iterator *it);

extern struct dentry *search_dentry(struct inode *inode, char *file);
extern int search_dentry_context(struct inode *inode, char *file,
                                 block_t *dtable_block, block_t *prev_dtable_block,
                                 struct dentry_table *dtable, int *index);

extern int pfs_opendir(const char *path, struct fuse_file_info *file_info);
extern int pfs_readdir(const char *path, void *buf, 
                       fuse_fill_dir_t filler, off_t offset,
                       struct fuse_file_info *file_info);
extern int pfs_releasedir(const char *path, struct fuse_file_info *file_info);
extern int pfs_mkdir(const char *path, mode_t mode);
extern int pfs_rename(const char  *oldpath, const char *newpath);
extern int pfs_rmdir(const char  *path);

extern block_t remove_dentry(struct inode *inode, block_t inode_block, 
                             char *filename);


#endif
