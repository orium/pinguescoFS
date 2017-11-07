#ifndef DEBUG__H_
#define DEBUG__H_

#if DEBUG
#  include <stdio.h>
#  define debug(...) printf(__VA_ARGS__)
#  define debug_red(...) ({                                             \
                        printf("\e[00;31m");                            \
                        debug(__VA_ARGS__);                             \
                        printf("\e[0m");                                \
                })
#else
#  define debug(...)
#  define debug_red(...)
#endif

#endif
