#ifndef FILE_H__
#define FILE_H__

#include <fuse.h>
#include <string.h>
#include <libgen.h>
#include "allocator.h"
#include "dentry.h"
#include "inode.h"

#define dname(filepath) ({                                        \
                        char *f=alloca(strlen(filepath)+1);       \
                        f[0]='\0'; /* avoid valgrind warning */   \
                        strcpy(f,filepath);                       \
                        dirname(f);                               \
                })

#define bname(filepath) ({                                        \
                        char *f=alloca(strlen(filepath)+1);       \
                        f[0]='\0'; /* avoid valgrind warning */   \
                        strcpy(f,filepath);                       \
                        basename(f);                              \
                })

struct open_file
{
        struct inode inode;
        block_t block;
};

extern block_t get_inode_block(const char const *filepath);
extern int pfs_access(const char *path, int mode);
extern int create_file(struct inode *dir_inode, block_t dir_inode_block,
                       const char *filename, block_t inode);

extern int pfs_create(const char *path, mode_t mode, 
                      struct fuse_file_info *file_info);
extern int pfs_open(const char *path, struct fuse_file_info *file_info);
extern int pfs_flush(const char *path, struct fuse_file_info *file_info);
extern int pfs_release(const char *path, struct fuse_file_info *file_info);
extern int pfs_unlink(const char *path);

extern int pfs_read(const char *path, char *buf, size_t buf_size,
                    off_t offset, struct fuse_file_info *file_info);
extern int pfs_write(const char *path, const char *buf, size_t buf_size, 
                     off_t offset, struct fuse_file_info *file_info);

extern int pfs_truncate(const char *path, off_t offset);

#endif
