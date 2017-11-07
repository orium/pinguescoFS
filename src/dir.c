#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>
#include "dir.h"
#include "inode.h"
#include "file.h"
#include "dentry.h"
#include "globals.h"
#include "superblock.h"
#include "open_files.h"
#include "debug.h"

static bool is_dir_empty(struct inode *inode);
static bool is_dtable_free(struct dentry_table *dtable);
static void remove_dtable_free(struct dentry_table *dtable, block_t previous);

void
dir_iterator_init_inode(struct dir_iterator *it, struct inode *dir_inode)
{
        assert(dir_inode->type == INODE_DIR);
        dir_iterator_init_dtable(it,dir_inode->dtable);
}

void
dir_iterator_init_dtable(struct dir_iterator *it, block_t root_dtable_block)
{
        it->prev_dtable_block=0;
        it->dtable_block=root_dtable_block;
        it->index=0;

        read_dentry_table(root_dtable_block,&it->dtable);
}

void
dir_iterator_init(struct dir_iterator *it, block_t dir_inode_block)
{
        struct inode inode;
        
        lock();

        read_inode(dir_inode_block,&inode);

        dir_iterator_init_inode(it,&inode);

        unlock();
}

void
dir_iterator_next(struct dir_iterator *it)
{
        assert(!dir_iterator_end(it));

        it->index++;

        if (it->index >= DENTRY_TABLE_ENTRIES
            && !dir_iterator_end(it))
        {
                assert(it->dtable.next);

                it->index=0;

                it->prev_dtable_block=it->dtable_block;
                it->dtable_block=it->dtable.next;
                read_dentry_table(it->dtable_block,&it->dtable);
        }
}

struct dentry *
dir_iterator_get(struct dir_iterator *it)
{
        return &it->dtable.file[it->index];
}

bool
dir_iterator_end(struct dir_iterator *it)
{
        return !it->dtable.next && it->index >= DENTRY_TABLE_ENTRIES;
}

/* returns the dentry of file `file' in the directory represented by
 * inode `inode'.
 *
 * pre: strcmp(file,"")
 */
struct dentry *
search_dentry(struct inode *inode, char *file)
{
        int index;
        block_t unused;
        struct dentry_table dtable;
        int z;
        struct dentry *r;

        z=search_dentry_context(inode,file,&unused,&unused,&dtable,&index);

        if (z)
                return NULL;

        r=malloc(sizeof(*r));

        if (!r)
                return NULL;

        memcpy(r,&dtable.file[index],sizeof(*r));

        return r;
}

/* Same as search_dentry() but sets the dtable_block, dtable and index */
int
search_dentry_context(struct inode *inode, char *file,
                      block_t *dtable_block, block_t *prev_dtable_block,
                      struct dentry_table *dtable, int *index)
{
        struct dir_iterator it;
        bool found;

        lock();

        assert(file[0]);
        assert(inode->type == INODE_DIR);

        found=false;

        dir_iterator_init_inode(&it,inode);

        while (!dir_iterator_end(&it) && !found)
        {
                struct dentry *dentry;

                dentry=dir_iterator_get(&it);

                 if (!strcmp(dentry->name,file)) 
                 {
                         *prev_dtable_block=it.prev_dtable_block;
                         *dtable_block=it.dtable_block;
                         *dtable=it.dtable; 
                         *index=it.index;
                         found=true;
                 }
                 else
                         dir_iterator_next(&it);
        }

        unlock();

        return !found;
}

int 
pfs_opendir(const char *path, struct fuse_file_info *file_info)
{
        struct open_file *of;
        block_t inode_block;
        int z;

        lock();

        debug("pfs_opendir(): path: \"%s\"\n",path);
        
        of=malloc(sizeof(*of));

        if (!of)
        {
                unlock();
                return -ENOMEM;
        }

        inode_block=get_inode_block(path);

        if (!inode_block)
        {
                free(of);
                unlock();
                return -ENOENT;
        }

        debug("pfs_opendir(): inode block: %d\n",inode_block);

        read_inode(inode_block,&of->inode);

        if (of->inode.type != INODE_DIR)
        {
                free(of);
                unlock();
                return -ENOTDIR;
        }

        debug("pfs_opendir(): inode type: %d\n",of->inode.type);
        debug("pfs_opendir(): dentry block: %d\n",of->inode.dtable);

        of->block=inode_block;

        assert(sizeof(file_info->fh) >= sizeof(of));
        file_info->fh=(typeof(file_info->fh))of;

        z=open_file(inode_block);

        if (z)
        {
                free(of);
                unlock();
                return -ENOMEM;
        }

        unlock();

        return 0;
}

/* TODO: use partial reading instead of reading the whole directory */
int 
pfs_readdir(const char *path, void *buf,
            fuse_fill_dir_t filler, off_t offset,
            struct fuse_file_info *file_info)
{
        struct inode *inode;
        struct dir_iterator it;

        lock();

        /* the directory could have been deleted */
        if (!get_inode_block(path))
        {
                unlock();
                return -ENOENT;
        }

        debug("pfs_readdir(): path: \"%s\"\n",path);

        assert(file_info->fh);

        inode=&((struct open_file *)file_info->fh)->inode;
        
        filler(buf,".",NULL,0);
        filler(buf,"..",NULL,0);

        dir_iterator_init_inode(&it,inode);

        while (!dir_iterator_end(&it))
        {
                struct dentry *dentry;

                dentry=dir_iterator_get(&it);

                if (!dentry_free(dentry))
                        filler(buf,dentry->name,NULL,0);

                dir_iterator_next(&it);
        }

        unlock();

        return 0;
}

int
pfs_releasedir(const char *path, struct fuse_file_info *file_info)
{
        lock();

        debug("pfs_releasedir(): path: \"%s\"\n",path);

        assert(file_info->fh);

        close_file(((struct open_file *)file_info->fh)->block);

        free((void *)file_info->fh);

        unlock();

        return 0;
}

int
pfs_mkdir(const char const *path, mode_t mode)
{
        struct inode parent_inode;
        struct inode new_inode;
        struct dentry_table new_dtable;
        block_t parent_inode_block;
        block_t inode_block;
        block_t dentry_block;
        char *parent_path;
        char *filename;
        int z;

        lock();

        debug("pfs_mkdir(): path: \"%s\"\n",path);

        if (get_inode_block(path))
        {
                unlock();
                return -EEXIST;
        }

        parent_path=dname(path);
        filename=bname(path);

        if (strlen(filename) >= MAX_FILENAME_LENGTH)
        {
                unlock();
                return -ENAMETOOLONG;
        }

        debug("pfs_mkdir(): base filename: \"%s\"\n",filename);
        debug("pfs_mkdir(): parent path: \"%s\"\n",parent_path);

        parent_inode_block=get_inode_block(parent_path);

        if (!parent_inode_block)
        {
                unlock();
                return -ENOENT;
        }

        debug("pfs_mkdir(): parent inode block: %d\n",parent_inode_block);

        read_inode(parent_inode_block,&parent_inode);

        if (parent_inode.type != INODE_DIR)
        {
                unlock();
                return -ENOTDIR;
        }

        inode_block=alloc_block(); 

        if (!inode_block)
        {
                unlock();
                return -ENOSPC;
        }
       
        dentry_block=alloc_block();

        if (!dentry_block)
        {
                free_block(inode_block);
                unlock();
                return -ENOSPC;
        }

        debug("pfs_mkdir(): new inode block: %d\n",inode_block);
        debug("pfs_mkdir(): new dentry block: %d\n",dentry_block);

        new_inode.type=INODE_DIR;
        new_inode.dtable=dentry_block;
        new_inode.blocks=2;

        init_emtpy_dtable(&new_dtable);

        write_inode(inode_block,&new_inode);
        write_dentry_table(dentry_block,&new_dtable);

        z=create_file(&parent_inode,parent_inode_block,filename,inode_block);

        if (z)
        {
                free_block(inode_block);
                free_block(dentry_block);
                unlock();
                return -ENOSPC;
        }

        inc_inode_number();

        unlock();

        return 0;
}

static bool
is_dtable_free(struct dentry_table *dtable)
{
        for (int i=0; i < DENTRY_TABLE_ENTRIES; i++)
                if (!dentry_free(&dtable->file[i]))
                        return false;

        return true;
}

static void
remove_dtable_free(struct dentry_table *dtable, block_t previous)
{
        struct dentry_table prev_dtable;
        block_t dtable_block;

        lock();

        read_dentry_table(previous,&prev_dtable);

        dtable_block=prev_dtable.next;

        debug("freeing one unused dtable at block %d\n",dtable_block);

        prev_dtable.next=dtable->next;

        write_dentry_table(previous,&prev_dtable);

        free_block(dtable_block);

        unlock();
}

/* returns the inode number of the removed file */
block_t
remove_dentry(struct inode *inode, block_t inode_block, char *filename)
{
        struct dentry_table dtable;
        block_t dtable_block;
        block_t prev_dtable_block; /* will be 0 if no previous dtable exists */
        int index;
        int z;

        lock();
        
        z=search_dentry_context(inode,filename,&dtable_block,
                                &prev_dtable_block,&dtable,&index);

        if (z)
        {
                unlock();
                return 0;
        }

        dtable.file[index].name[0]='\0';
        
        write_dentry_table(dtable_block,&dtable);

        if (prev_dtable_block /* we never free the first dtable */
            && is_dtable_free(&dtable))
        {
                remove_dtable_free(&dtable,prev_dtable_block);
                inode->blocks--;
                write_inode(inode_block,inode);
        }

        unlock();

        return dtable.file[index].inode;
}

static bool 
is_dir_empty(struct inode *inode)
{
        struct dentry_table dtable;

        assert(inode->type == INODE_DIR);
        assert(inode->dtable);

        read_dentry_table(inode->dtable,&dtable);

        /* We only need to check the first dtable since emtpy directories
         * will always have one dtable.
         */

        debug("is_dir_empty(): dtable.next: %d\n",dtable.next);

        return !dtable.next && is_dtable_free(&dtable);
}

int
pfs_rmdir(const char *path)
{
        struct inode inode;
        struct inode parent_inode;
        block_t parent_block;
        block_t inode_block;
        char *filename;
        char *parent_path;

        lock();
        
        debug("pfs_rmdir(): path: \"%s\"\n",path);

        parent_path=dname(path);
        filename=bname(path);

        parent_block=get_inode_block(parent_path);
        
        if (!parent_block)
        {
                unlock();
                return -ENOENT;
        }

        read_inode(parent_block,&parent_inode);

        if (parent_inode.type != INODE_DIR)
        {
                unlock();
                return -ENOTDIR;
        }

        inode_block=get_inode_block(path);

        if (!inode_block)
        {
                unlock();
                return -ENOENT;
        }

        read_inode(inode_block,&inode);

        if (inode.type != INODE_DIR)
        {
                unlock();
                return -ENOTDIR;
        }

        if (!is_dir_empty(&inode))
        {
                unlock();
                return -ENOTEMPTY;
        }

        remove_dentry(&parent_inode,parent_block,filename);

        free_block(inode_block);
        free_block(inode.dtable);

        dec_inode_number();

        unlock();

        return 0;
}

int
pfs_rename(const char *oldpath, const char *newpath)
{
        struct inode new_parent_inode;
        struct inode old_parent_inode;
        block_t new_parent_block;
        block_t old_parent_block;
        block_t file_inode;
        char *old_filename;
        char *old_parent_path;
        char *new_filename;
        char *new_parent_path;
        int z;

        lock();
        
        debug("pfs_rename(): oldpath: \"%s\"\n",oldpath);
        debug("pfs_rename(): newpath: \"%s\"\n",newpath);

        old_parent_path=dname(oldpath);
        old_filename=bname(oldpath);
        new_parent_path=dname(newpath);
        new_filename=bname(newpath);

        /* the EINVAL error is handled by the VFS or fuse :) */

        if (get_inode_block(newpath))
        {
                unlock();
                return -EEXIST;
        }

        old_parent_block=get_inode_block(old_parent_path);
        
        if (!old_parent_block)
        {
                unlock();
                return -ENOENT;
        }

        read_inode(old_parent_block,&old_parent_inode);

        if (old_parent_inode.type != INODE_DIR)
        {
                unlock();
                return -ENOTDIR;
        }

        new_parent_block=get_inode_block(new_parent_path);
        
        if (!new_parent_block)
        {
                unlock();
                return -ENOENT;
        }

        read_inode(new_parent_block,&new_parent_inode);

        if (new_parent_inode.type != INODE_DIR)
        {
                unlock();
                return -ENOTDIR;
        }

        if (strlen(new_filename) >= MAX_FILENAME_LENGTH)
        {
                unlock();
                return -ENAMETOOLONG;
        }

        file_inode=get_inode_block(oldpath);

        if (!file_inode)
        {
                unlock();
                return -ENOENT;
        }

        z=create_file(&new_parent_inode,new_parent_block,new_filename,file_inode);

        if (z)
        {
                unlock();
                return -ENOSPC;
        }

        remove_dentry(&old_parent_inode,old_parent_block,old_filename);

        unlock();

        return 0;
}
