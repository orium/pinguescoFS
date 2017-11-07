#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "../src/superblock.h"
#include "../src/allocator.h"
#include "../src/disk.h"
#include "../src/inode.h"

#define ROOT_DENTRY_BLOCK (ROOT_INODE_BLOCK+1)
#define FIRST_FREE_BLOCK  (ROOT_INODE_BLOCK+2)

static void usage(FILE *, char *);
int main(int, char **);

static void
usage(FILE *out, char *argv0)
{
        fprintf(out,"Usage: %s <dev>\n",argv0);
}

uint32_t
block_number(int fd)
{
        off_t offset;
        uint32_t blocks;

        offset=lseek(fd,0,SEEK_END);
        blocks=offset/BLOCK_SIZE;

        if (blocks > MAX_BLOCKS)
                blocks=MAX_BLOCKS;

        return blocks;
}

void
write_superblock(int fd)
{
        magic_t m=MAGIC_NO;
        block_t first_free_block=FIRST_FREE_BLOCK;
        uint32_t inodes=0;
        uint32_t blocks=block_number(fd);

        seek(fd,0);
        
        cwrite(fd,&m,sizeof(m));
        cwrite(fd,&first_free_block,sizeof(first_free_block));
        cwrite(fd,&blocks,sizeof(blocks));
        cwrite(fd,&inodes,sizeof(inodes));
}

void
write_fat(int fd)
{
        block_t *fat;
        block_t next;
        int i;
        uint32_t blocks=block_number(fd);

        fat=malloc(FAT_ENTRIES*sizeof(block_t));

        if (!fat)
        {
                fprintf(stderr,"Memory exhausted\n");
                exit(-1);
        }

        memset(fat,0,FAT_ENTRIES*sizeof(block_t));

        next=FIRST_FREE_BLOCK+1;
        
        for (; i < blocks-1; i++)
                fat[i]=next++;

        /* last free pointer points to null */

        seek(fd,block_addr(FAT_INITIAL_BLOCK));
        cwrite(fd,fat,FAT_ENTRIES*sizeof(block_t));

        free(fat);
}

void
write_root_inode(int fd)
{
        inode_type_t itype=INODE_DIR;
        block_t dentry_block=ROOT_DENTRY_BLOCK;
        uint32_t two=2;

        seek(fd,block_addr(ROOT_INODE_BLOCK));

        cwrite(fd,&itype,sizeof(itype));
        cwrite(fd,&dentry_block,sizeof(dentry_block));
        cwrite(fd,&two,sizeof(two));
}

void
write_root_dentry(int fd)
{
        uint8_t zeros[BLOCK_SIZE];

        seek(fd,block_addr(ROOT_DENTRY_BLOCK));
        
        memset(zeros,0,sizeof(zeros));

        cwrite(fd,zeros,sizeof(zeros));
}

int
main(int argc, char **argv)
{
        int fd;
        char *filename;
        uint32_t blocks;

        if (argc != 2)
        {
                usage(stderr,argv[0]);
                return -1;
        }

        filename=argv[1];

        fd=open(filename,O_RDWR);

        if (fd < 0)
        {
                perror("open()");
                return -1;
        }

        blocks=block_number(fd);

        if (blocks <= FAT_INITIAL_BLOCK+FAT_BLOCKS)
        {
                fprintf(stderr,"error: The device is too small\n");
                close(fd);
                return -1;
        }

        write_superblock(fd);
        write_fat(fd);
        write_root_inode(fd);
        write_root_dentry(fd);

        close(fd);

        return 0;
}
