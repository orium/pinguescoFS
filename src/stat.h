#ifndef STAT_H__
#define STAT_H__

#include <sys/stat.h>
#include <sys/statvfs.h>

extern int pfs_getattr(const char *path, struct stat *stats);
extern int pfs_statfs(const char *, struct statvfs *stats);

#endif
