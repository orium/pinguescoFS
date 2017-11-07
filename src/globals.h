#ifndef GLOBALS_H__
#define GLOBALS_H__

#include <pthread.h>

extern int devfd;
extern pthread_mutex_t mutex;

#if 1
#define lock() pthread_mutex_lock(&mutex)
#define unlock() pthread_mutex_unlock(&mutex)
#else
#include <stdio.h>
#define lock() ({ printf("try lock at %s:%d\n",__FILE__,__LINE__);      \
                  pthread_mutex_lock(&mutex);                           \
                  printf("acquired lock at %s:%d\n",__FILE__,__LINE__); \
                })
#define unlock() ({ pthread_mutex_unlock(&mutex);                       \
                    printf("unlocked at %s:%d\n",__FILE__,__LINE__);    \
                })
#endif

#endif
