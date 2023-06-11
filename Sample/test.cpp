#define THREAD_POOL 1
#define THREAD 2
#define DOUBLE_QUEUE 3

#define TEST THREAD_POOL

#if TEST == THREAD_POOL
#include "test1.cpp"

#elif TEST == THREAD
#include "test2.cpp"

#elif TEST == DOUBLE_QUEUE
#include "test3.cpp"
#endif
