#define _GNU_SOURCE
  
#include <stdlib.h>
#include <search.h>
#include <stddef.h>
#include <globals.h>
#include <assert.h>
#include "open_files.h"
#include "debug.h"

void *tree;

struct open_file 
{
        block_t inode;
        int count;
};

void tree_node_free(void *of);
int cmp(const void *p1, const void *p2);

void
tree_node_free(void *of)
{
        free(of);
}

int
cmp(const void *p1, const void *p2)
{
        const struct open_file *of1=(const struct open_file *)p1;
        const struct open_file *of2=(const struct open_file *)p2;

        debug("%d %d => %d\n",of1->inode,of2->inode,
                  (int)(((int64_t)of1->inode)-((int64_t)of2->inode)));

        return (int)(((int64_t)of1->inode)-((int64_t)of2->inode));
}

void
init_open_files(void)
{
        tree=NULL;
}

void
free_open_files(void)
{
        tdestroy(tree,tree_node_free);
}

int
open_file(block_t inode_no)
{
        struct open_file needle = {
                .inode=inode_no,
                .count=0,
        };
        struct open_file **of;

        lock();

        of=tfind(&needle,&tree,cmp);

        if (of)
        {
                assert((*of)->inode == inode_no);
                assert((*of)->count > 0);

                (*of)->count++;
        }
        else
        {
                struct open_file *nof;

                nof=malloc(sizeof(*nof));

                if (!nof)
                {
                        unlock();
                        return -1;
                }

                nof->inode=inode_no;
                nof->count=1;

                of=tsearch(nof,&tree,cmp);

                if (!of)
                {
                        unlock();
                        return -1;
                }

                assert((*of)->inode == inode_no);
        }

        debug("open_file(): inode %d has %d opens\n",inode_no,(*of)->count);

        unlock();

        return 0;
}

void
close_file(block_t inode_no)
{
        struct open_file needle = {
                .inode=inode_no,
                .count=0,
        };
        struct open_file **of;

        lock();

        of=tfind(&needle,&tree,cmp);

        assert(of);
        assert((*of)->inode == inode_no);

        (*of)->count--;

        debug("close_file(): inode %d has %d opens\n",inode_no,
              (*of)->count);

        if (!(*of)->count)
        {
                struct open_file *tofree=*of;                

                tdelete(&needle,&tree,cmp);
                free(tofree);
        }

        unlock();
}

bool
is_file_opened(block_t inode_no)
{
        struct open_file needle = {
                .inode=inode_no,
                .count=0,
        };
        void *n;

        lock();
        
        n=tfind(&needle,&tree,cmp);
        
        unlock();

        return n != NULL;
}
