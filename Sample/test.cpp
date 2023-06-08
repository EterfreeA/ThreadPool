#define THREAD_POOL 1
#define THREAD 2
#define TASK_QUEUE 3
#define TASK_POOL 4
#define ACTOR 5
#define TIMER 6

#define TEST THREAD_POOL

#if TEST == THREAD_POOL
#include "ThreadPool/test.cpp"

#elif TEST == THREAD
#include "Thread/test.cpp"

#elif TEST == TASK_QUEUE
#include "TaskQueue/test.cpp"

#elif TEST == TASK_POOL
#include "TaskPool/test.cpp"

#elif TEST == ACTOR
#include "Actor/test.cpp"

#elif TEST == TIMER
#include "Timer/test.cpp"
#endif
