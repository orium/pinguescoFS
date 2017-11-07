#include <unistd.h>
#include "disk.h"

ssize_t
cread(int fd, void *buf, size_t size)
{
        ssize_t left=size;
        void *cbuf=buf;

        while (left)
        {
                ssize_t z;

                z=read(fd,cbuf,left);

                if (z < 0)
                        return z;
                else if (!z)
                        break;

                left-=z;
                cbuf+=z;
        }

        return size-left;
}

ssize_t
cwrite(int fd, const void const *buf, size_t size)
{
        ssize_t left=size;
        void const *cbuf=buf;

        while (left)
        {
                ssize_t z;

                z=write(fd,cbuf,left);

                if (z < 0)
                        return z;
                else if (!z)
                        break;

                left-=z;
                cbuf+=z;
        }

        return size-left;
}

off_t
seek(int fd, off_t pos)
{
        return lseek(fd,pos,SEEK_SET);
}
