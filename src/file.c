#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>
#include "allocator.h"
#include "superblock.h"
#include "file.h"
#include "inode.h"
#include "dir.h"
#include "dentry.h"
#include "ind_block.h"
#include "globals.h"
#include "disk.h"
#include "open_files.h"
#include "debug.h"

/* calculates the number of blocks needed given a size in bytes*/
#define needed_blocks(b) ((b)/BLOCK_SIZE+!!(b%BLOCK_SIZE))

/* calculates the number of the last block given a size in bytes*/
#define last_block(b) (needed_blocks(b)-1)

static int get_final_ind_block(struct inode *inode, int block_no,
                               int *index, struct ind_block *ind_block, 
                               block_t *ind_block_block);
static int create_ind_blocks_recursive(block_t *ind_block_block, int *left, 
                                       int ind_level,
                                       int *allocated_blocks,
                                       int first_block_path[4],
                                       int depth,
                                       bool first);
static int expand(struct inode *inode, int new_blocks, int *ready_new_blocks);
static int shrink(struct inode *inode, int new_blocks);
static void zero_last_block(struct inode *inode, int offset);
static void free_blocks_recursive(block_t *ind_block_block, int ind_level, 
                                  int *freed_blocks, bool first, int depth,
                                  int last_block_path[4]);

block_t
get_inode_block(const char const *filepath)
{
        char path[strlen(filepath)+1];
        char *file;
        bool first=true;
        block_t inode_block;

        lock();

        strcpy(path,filepath);

        inode_block=ROOT_INODE_BLOCK;

        while ((file=strtok(first ? path : NULL,"/")) != NULL)
        {
                struct inode inode;
                struct dentry *dentry;

                read_inode(inode_block,&inode);
                inode_block=0;

                if (inode.type != INODE_DIR)
                        break;

                dentry=search_dentry(&inode,file);

                if (!dentry)
                        break;
                
                inode_block=dentry->inode;
                free(dentry);

                first=false;
        }

        unlock();

        return inode_block;
}

int
pfs_access(const char *path, int mode)
{
        lock();

        debug("pfs_access(): path: \"%s\"\n",path);

        if (!get_inode_block(path))
        {
                unlock();
                return -ENOENT;
        }

        unlock();

        return 0;
}

int
create_file(struct inode *dir_inode, block_t dir_inode_block,
            const char *filename, block_t inode)
{
        struct dir_iterator it;
        bool created=false;

        lock();

        assert(strlen(filename) < MAX_FILENAME_LENGTH);

        debug("create_file(): filename: %s, inode = %d\n",filename,inode);

        dir_iterator_init_dtable(&it,dir_inode->dtable);

        while (!dir_iterator_end(&it) && !created)
        {
                struct dentry *dentry;

                dentry=dir_iterator_get(&it);

                if (dentry_free(dentry)) 
                {
                        strcpy(dentry->name,filename);
                        dentry->inode=inode;
                        write_dentry_table(it.dtable_block,&it.dtable);
                        created=true;
                }
                else
                        dir_iterator_next(&it);
        }

        if (!created)
        {
                struct dentry_table last_dtable;
                struct dentry_table new_dtable;
                block_t last_dtable_block;
                block_t new_dtable_block;
                
                new_dtable_block=alloc_block();
                
                if (!new_dtable_block)
                {
                        unlock();
                        return -1;
                }

                last_dtable=it.dtable;                
                last_dtable_block=it.dtable_block;

                last_dtable.next=new_dtable_block;

                init_dtable(&new_dtable,filename,inode);
                
                write_dentry_table(new_dtable_block,&new_dtable);
                write_dentry_table(last_dtable_block,&last_dtable);

                dir_inode->blocks++;
                debug("create_file(): inc block number: %d\n",dir_inode->blocks);
                write_inode(dir_inode_block,dir_inode);
        }
        
        unlock();

        return 0;
}

int
pfs_create(const char *path, mode_t mode, struct fuse_file_info *file_info)
{
        struct inode inode;
        struct inode parent_inode;
        struct ind_block ind_block;
        block_t inode_block;
        block_t parent_inode_block;
        block_t ind_block_block;
        char *parent_path;
        char *filename;
        int z;

        lock();

        debug("pfs_create(): path: \"%s\"\n",path);

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

        parent_inode_block=get_inode_block(parent_path);

        if (!parent_inode_block)
        {
                unlock();
                return -ENOENT;
        }

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

        ind_block_block=alloc_block();

        if (!ind_block_block)
        {
                free_block(inode_block);
                unlock();
                return -ENOSPC;
        }

        inode.type=INODE_RFILE;
        inode.blocks=2;
        inode.size=0;
        inode.single_ind_block=ind_block_block;
        inode.double_ind_block=0;
        inode.triple_ind_block=0;

        init_empty_ind_block(&ind_block);
        write_ind_block(ind_block_block,&ind_block);

        write_inode(inode_block,&inode);

        z=create_file(&parent_inode,parent_inode_block,filename,inode_block);

        if (z)
        {
                free_block(inode_block);
                free_block(ind_block_block);
                unlock();
                return -ENOSPC;
        }

        inc_inode_number();

        z=pfs_open(path,file_info);

        unlock();

        return z;
}

int
pfs_open(const char *path, struct fuse_file_info *file_info)
{
        struct open_file *of;
        block_t inode_block;
        int z;

        lock();

        debug("pfs_open(): path: \"%s\"\n",path);

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

        read_inode(inode_block,&of->inode);

        if (of->inode.type != INODE_RFILE)
        {
                free(of);
                unlock();
                return -ENOTDIR;
        }

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

int
pfs_flush(const char *path, struct fuse_file_info *file_info)
{
        assert(file_info->fh);
        /* nothing to do */
        return 0;
}

int
pfs_release(const char *path, struct fuse_file_info *file_info)
{
        int z;

        lock();

        debug("pfs_release(): path: \"%s\"\n",path);

        z=pfs_flush(path,file_info);

        if (z)
                return z;

        assert(file_info->fh);

        close_file(((struct open_file *)file_info->fh)->block);

        free((void *)file_info->fh);

        unlock();

        return 0;
}

int
pfs_unlink(const char *path)
{
        struct inode inode;
        struct inode parent_inode;
        block_t parent_block;
        block_t inode_block;
        char *filename;
        char *parent_path;

        lock();
        
        debug("pfs_unlink(): path: \"%s\"\n",path);

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

        if (inode.type == INODE_DIR)
        {
                int z;
                z=pfs_rmdir(path);
                unlock();
                return z;
        }

        if (is_file_opened(inode_block))
        {
                unlock();
                return -EBUSY;
        }

        remove_dentry(&parent_inode,parent_block,filename);

        free_block(inode_block);

        free_ind_block(inode.single_ind_block,SINGLE_INDIRECTION);
        free_ind_block(inode.double_ind_block,DOUBLE_INDIRECTION);
        free_ind_block(inode.triple_ind_block,TRIPLE_INDIRECTION);

        dec_inode_number();

        unlock();

        return 0;
}

static int
get_final_ind_block(struct inode *inode, int block_no,
                    int *index, struct ind_block *ind_block,
                    block_t *ind_block_block)
{
        int path[4];
        int z;

        if (block_no < 0)
                return -1;

        z=get_data_block_path(path,block_no);

        if (z)
                return -1;

        switch (path[0])
        {
        case SINGLE_INDIRECTION: 
        {
                if (!inode->single_ind_block)
                        return -1;
                read_ind_block(inode->single_ind_block,
                               ind_block); 
                *ind_block_block=inode->single_ind_block;
                break;
        }
        case DOUBLE_INDIRECTION: 
        {
                if (!inode->double_ind_block)
                        return -1;
                read_ind_block(inode->double_ind_block,
                               ind_block); 
                break;
        }
        case TRIPLE_INDIRECTION: 
        {
                if (!inode->triple_ind_block)
                        return -1;
                read_ind_block(inode->triple_ind_block,
                               ind_block); 
                break;
        }
        default: assert(false);
        }

        for (int i=1; i < path[0]; i++)
        {
                if (!ind_block->blocks[path[i]])
                        return -1;
                *ind_block_block=ind_block->blocks[path[i]];
                read_ind_block(ind_block->blocks[path[i]],ind_block);
        }

        *index=path[path[0]];

        return 0;
}

int
pfs_read(const char *path, char *buf, size_t buf_size,
         off_t offset, struct fuse_file_info *file_info)
{
        char block[BLOCK_SIZE];
        struct inode *inode;
        struct ind_block ind_block;
        int index;
        int read=0;
        bool eof;

        lock();

        debug("pfs_read(): path=\"%s\"\n",path);
        debug("pfs_read(): buf_size=%d\n",(int)buf_size);
        debug("pfs_read(): offset=%d\n",(int)offset);

        assert(file_info->fh);

        inode=&((struct open_file *)file_info->fh)->inode;

        eof=false;
        index=INDIRECT_BLOCKS;

        while (!eof && read < buf_size)
        {
                int this_read;

                if (offset >= inode->size)
                {
                        eof=true;
                        continue;
                }

                /* got to read next indirect block */
                if (index >= INDIRECT_BLOCKS)
                {
                        block_t unused;
                        int z __attribute__((unused));

                        debug("pfs_read(): getting final ind_block for"
                                  " block %d\n",(int)offset/BLOCK_SIZE);

                        z=get_final_ind_block(inode,offset/BLOCK_SIZE,
                                              &index,&ind_block,&unused);

                        assert(!z);
                }

                assert(index < INDIRECT_BLOCKS);

                if (!ind_block.blocks[index])
                {
                        /* this means that the block is only zeros */
                        memset(block,0,sizeof(block));

                        debug("pfs_read(): read zero block\n");
                }
                else
                {
                        debug("pfs_read(): read non-zero block\n");

                        seek(devfd,block_addr(ind_block.blocks[index]));
                        cread(devfd,block,sizeof(block));
                }

                /* if the offset doesn't start at the beginning of a block 
                 * we can read no more than this
                 */
                this_read=BLOCK_SIZE-offset%BLOCK_SIZE;

                /* if the users requests less than we can read
                 * we only read what was requested
                 */
                if (buf_size-read < this_read)
                        this_read=buf_size-read;

                /* if we reach end-of-file we only read what is possible
                 */
                if (offset+this_read > inode->size)
                        this_read=inode->size-offset;

                memcpy(buf+read,block+offset%BLOCK_SIZE,this_read);
                read+=this_read;
                offset+=this_read;

                index++;
        }

        assert(read <= buf_size);

        unlock();

        return read;
}

static int
create_ind_blocks_recursive(block_t *ind_block_block, int *left, int ind_level,
                            int *allocated_blocks,
                            int first_block_path[4],
                            int depth,
                            bool first)
{
        struct ind_block ind_block;
        bool changed=false;
        int init;
        
        if (!*left)
                return 0;

        if (!ind_level)
        {
                /* this is a new data block */

                (*left)--;

                debug("create_ind_blocks_recursive(): decremented left\n");
                
                return 0;
        }

        if (!*ind_block_block)
        {
                *ind_block_block=alloc_block();

                if (!*ind_block_block)
                        return -1;
                
                (*allocated_blocks)++;

                init_empty_ind_block(&ind_block);

                debug("create_ind_blocks_recursive(): allocated block\n");

                changed=true;
        }
        else
                read_ind_block(*ind_block_block,&ind_block);

        init=first ? first_block_path[depth] : 0;

        for (int i=init; i < INDIRECT_BLOCKS && *left; i++)
        {
                block_t last_value=ind_block.blocks[i];
                
                create_ind_blocks_recursive(&ind_block.blocks[i],
                                            left,ind_level-1,
                                            allocated_blocks,
                                            first_block_path,
                                            depth+1,first);

                first=false;

                if (last_value != ind_block.blocks[i])
                        changed=true;
        }

        if (changed)
                write_ind_block(*ind_block_block,&ind_block);

        return 0;
}

/* Note: the inode structure is not stored here */
/* FIXME: In some cases the expansion is done with no reason.
 * However it should only take linear time on the number
 * of adicional blocks.
 *
 * This could be solved by maintaing the number of data block
 * ready to be used in the inode.
 */
static int
expand(struct inode *inode, int new_blocks, int *ready_new_blocks)
{
        int left;
        int allocated_blocks=0;
        int z;
        int first_block;
        int first_block_path[4];
        bool first;

        debug("expand(): new_blocks: %d\n",new_blocks);

        /* the remaining of the last block size already contains zeros,
         * so there is no need to write them
         */
        left=new_blocks-needed_blocks(inode->size);

        first_block=last_block(inode->size)+1;

        debug("expand(): left: %d\n",left);
        debug("expand(): first_block: %d\n",first_block);

        z=get_data_block_path(first_block_path,first_block);

        if (z)
                return -1;

        first=true;

        switch (first_block_path[0])
        {
        case SINGLE_INDIRECTION: 
                z=create_ind_blocks_recursive(&inode->single_ind_block,&left,
                                              SINGLE_INDIRECTION,
                                              &allocated_blocks,
                                              first_block_path,1,
                                              first);
                first=false;
                /* fall through */
        case DOUBLE_INDIRECTION: 
                z+=create_ind_blocks_recursive(&inode->double_ind_block,&left,
                                               DOUBLE_INDIRECTION,
                                               &allocated_blocks,
                                               first_block_path,1,
                                               first);
                first=false;
                /* fall through */
        case TRIPLE_INDIRECTION: 
                z+=create_ind_blocks_recursive(&inode->triple_ind_block,&left,
                                               TRIPLE_INDIRECTION,
                                               &allocated_blocks,
                                               first_block_path,1,
                                               first);
                first=false;
        }

        inode->blocks+=allocated_blocks;

        if (ready_new_blocks)
                *ready_new_blocks=new_blocks-left;

        if (z)
                return -1;

        debug("expand(): allocated_blocks: %d\n",allocated_blocks);

        return left != 0;
}

int
pfs_write(const char *path, const char *buf, size_t buf_size, off_t offset,
          struct fuse_file_info *file_info)
{
        char block[BLOCK_SIZE];
        struct inode *inode;
        struct ind_block ind_block;
        block_t inode_block;
        block_t ind_block_block;
        int written=0;
        int index;
        bool changed_ind_block;

        lock();

        debug("pfs_write(): path=\"%s\"\n",path);
        debug("pfs_write(): buf_size=%d\n",(int)buf_size);
        debug("pfs_write(): offset=%d\n",(int)offset);

        assert(file_info->fh);

        inode=&((struct open_file *)file_info->fh)->inode;
        inode_block=((struct open_file *)file_info->fh)->block;

        debug("pfs_write(): inode_block=%d\n",inode_block);

        if (needed_blocks(offset+buf_size) > needed_blocks(inode->size))
        {
                int real_new_blocks;

                expand(inode,needed_blocks(offset+buf_size),&real_new_blocks);

                debug("pfs_write(): expanded to %d\n",real_new_blocks);
        }

        index=INDIRECT_BLOCKS;
        changed_ind_block=false;

        while (written < buf_size)
        {
                int this_write;

                /* got to read next indirect block */
                if (index >= INDIRECT_BLOCKS)
                {
                        int z;

                        if (changed_ind_block)
                                write_ind_block(ind_block_block,&ind_block);

                        changed_ind_block=false;

                        z=get_final_ind_block(inode,offset/BLOCK_SIZE,
                                              &index,&ind_block,
                                              &ind_block_block);

                        if (z)
                                break;
                }

                if (!ind_block.blocks[index])
                {
                        debug("pfs_write(): Creating new data block\n");

                        /* this represents a block of zeros */
                        
                        ind_block.blocks[index]=alloc_block();

                        if (!ind_block.blocks[index])
                                break;

                        inode->blocks++;

                        changed_ind_block=true;

                        memset(block,0,sizeof(block));
                }
                else
                {
                        seek(devfd,block_addr(ind_block.blocks[index]));
                        cread(devfd,block,sizeof(block));
                }

                /* if the offset doesn't start at the beginning of a block 
                 * we can write no more than this
                 */
                this_write=BLOCK_SIZE-offset%BLOCK_SIZE;

                /* if the users requests less than we can write
                 * we only write what was requested
                 */                
                if (buf_size-written < this_write)
                        this_write=buf_size-written;

                memcpy(block+offset%BLOCK_SIZE,buf+written,this_write);

                written+=this_write;
                offset+=this_write;

                seek(devfd,block_addr(ind_block.blocks[index]));
                cwrite(devfd,block,sizeof(block));

                index++;

                assert(written <= buf_size);
        }

        if (changed_ind_block)
                write_ind_block(ind_block_block,&ind_block);

        if (offset > inode->size)
                inode->size=offset;

        write_inode(inode_block,inode);

        if (!written && buf_size)
        {
                unlock();
                return -ENOSPC;
        }

        unlock();

        return written;
}

static void
free_blocks_recursive(block_t *ind_block_block, int ind_level, 
                      int *freed_blocks, bool first, int depth,
                      int last_block_path[4])
{
        struct ind_block ind_block;
        bool need_restore;
        int init;

        if (!ind_level)
        {
                if (*ind_block_block)
                {
                        free_block(*ind_block_block);
                        *ind_block_block=0;
                        (*freed_blocks)++;

                        debug("free_blocks_recursive(): freeing"
                                  " final data block\n");
                }

                return;
        }

        if (!*ind_block_block)
                return;

        read_ind_block(*ind_block_block,&ind_block);

        /* only the first indirect blocks need to be saved */
        need_restore=first;

        init=first ? last_block_path[depth] : 0;

        for (int i=init; i < INDIRECT_BLOCKS; i++)
        {
                free_blocks_recursive(&ind_block.blocks[i],
                                      ind_level-1,freed_blocks,
                                      first,depth+1,last_block_path);
                first=false;
        }

        if (need_restore)
                write_ind_block(*ind_block_block,&ind_block);
        else
        {
                free_block(*ind_block_block);
                *ind_block_block=0;
                (*freed_blocks)++;

                debug("free_blocks_recursive(): freeing"
                          " indirect block of level %d\n",ind_level);
        }
}

static int
shrink(struct inode *inode, int new_blocks)
{
        int last_block;
        int last_block_path[4];
        bool first;
        int freed_blocks;
        int z;

        last_block=last_block(inode->size)+1;

        debug("shrink(): last_block: %d\n",last_block);

        debug("shrink(): new_blocks: %d\n",new_blocks);

        z=get_data_block_path(last_block_path,last_block);

        if (z)
                return -1;

        first=true;
        
        switch (last_block_path[0])
        {
        case SINGLE_INDIRECTION: 
                free_blocks_recursive(&inode->single_ind_block,
                                      SINGLE_INDIRECTION,&freed_blocks,first,
                                      1,last_block_path);
                first=false;
                /* fall through */
        case DOUBLE_INDIRECTION: 
                free_blocks_recursive(&inode->double_ind_block,
                                      DOUBLE_INDIRECTION,&freed_blocks,first,
                                      1,last_block_path);
                first=false;
                /* fall through */
        case TRIPLE_INDIRECTION: 
                free_blocks_recursive(&inode->triple_ind_block,
                                      TRIPLE_INDIRECTION,&freed_blocks,first,
                                      1,last_block_path);
                first=false;
        }

        inode->blocks-=freed_blocks;

        return 0;
}

/* zeroes last block bytes from offset to the end
 *
 * Unused bytes from the last block must be zeroed to
 * allow a future expansion to have the correct
 * behaviour
 */
static void
zero_last_block(struct inode *inode, int offset)
{
        char block[BLOCK_SIZE];
        struct ind_block ind_block;
        int index;
        int z;
        block_t ind_block_block;

        assert(0 <= offset && offset < BLOCK_SIZE);

        z=get_final_ind_block(inode,last_block(inode->size),
                              &index,&ind_block,
                              &ind_block_block);
        
        if (z)
                return;

        debug("zero_last_block(): offset: %d\n",offset);

        if (!ind_block.blocks[index])
                return;

        seek(devfd,block_addr(ind_block.blocks[index]));
        cread(devfd,block,sizeof(block));

        memset(block+offset,0,BLOCK_SIZE-offset);

        seek(devfd,block_addr(ind_block.blocks[index]));
        cwrite(devfd,block,sizeof(block));
}

int
pfs_truncate(const char *path, off_t offset)
{
        struct inode inode;
        block_t inode_block;
        uint32_t new_size;

        lock();

        debug("pfs_truncate(): path: \"%s\"\n",path);
        debug("pfs_truncate(): offset: %d\n",(int)offset);

        inode_block=get_inode_block(path);

        if (!inode_block)
        {
                unlock();
                return -ENOENT;
        }

        read_inode(inode_block,&inode);

        if (inode.type != INODE_RFILE)
        {
                unlock();
                return -ENOTDIR;
        }

        new_size=offset;

        if (needed_blocks(offset) > needed_blocks(inode.size))
        {
                int z;
                int ready_new_blocks;

                debug("pfs_truncate(): expanding\n");

                z=expand(&inode,needed_blocks(offset),&ready_new_blocks);

                debug("pfs_truncate(): expanded returned %d\n",z);

                if (z && ready_new_blocks*BLOCK_SIZE < offset)
                        new_size=ready_new_blocks*BLOCK_SIZE;
        }
        else if (needed_blocks(offset) < needed_blocks(inode.size))
        {
                debug("pfs_truncate(): shrinking\n");

                inode.size=new_size;

                shrink(&inode,needed_blocks(offset));

                debug("pfs_truncate(): single: %d\n",inode.single_ind_block);
                debug("pfs_truncate(): double: %d\n",inode.double_ind_block);
                debug("pfs_truncate(): triple: %d\n",inode.triple_ind_block);

                zero_last_block(&inode,offset%BLOCK_SIZE);
        }
        else
        {
                if (offset < inode.size)
                        zero_last_block(&inode,offset%BLOCK_SIZE);
                
                debug("pfs_truncate(): The number of blocks is ok\n");
        }

        inode.size=new_size;

        debug("pfs_truncate(): new size: %d\n",inode.size);

        write_inode(inode_block,&inode);

        unlock();

        return 0;        
}
