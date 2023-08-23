#define THREAD_POOL 1
#define THREAD 2
#define DOUBLE_QUEUE 3

#define TEST THREAD_POOL

#if TEST == THREAD_POOL
#include "ThreadPool/test.cpp"

#elif TEST == THREAD
#include "Thread/test.cpp"

#elif TEST == DOUBLE_QUEUE
#include "DoubleQueue/test.cpp"
#endif
