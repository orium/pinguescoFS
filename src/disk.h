#ifndef DISK_H__
#define DISK_H__

#include <unistd.h>

extern ssize_t cread(int fd, void *buf, size_t size);
extern ssize_t cwrite(int fd, const void const *buf, size_t size);
extern off_t seek(int fd, off_t pos);

#endif
