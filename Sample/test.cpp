#define THREAD_POOL 1
#define THREAD 2
#define TASK_MANAGER 3
#define TASK_QUEUE 4
#define TASK_MAPPER 5
#define ACTOR 6
#define TIMER 7

#define TEST THREAD_POOL

#if TEST == THREAD_POOL
#include "ThreadPool/test.cpp"

#elif TEST == THREAD
#include "Thread/test.cpp"

#elif TEST == TASK_MANAGER
#include "TaskManager/test.cpp"

#elif TEST == TASK_QUEUE
#include "TaskQueue/test.cpp"

#elif TEST == TASK_MAPPER
#include "TaskMapper/test.cpp"

#elif TEST == ACTOR
#include "Actor/test.cpp"

#elif TEST == TIMER
#include "Timer/test.cpp"
#endif
