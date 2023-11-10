#ifndef __MY_THREADS_H__
#define __MY_THREADS_H__

#include <stdint.h>

extern int threads_init(void);
extern rt_mutex_t dynamic_mutex;

#endif