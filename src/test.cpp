#include "ThreadPool.h"
//#include <threadpool/threadpool.hpp>

#include <ctime>
#include <iostream>
#include <thread>
#include <mutex>

int taskCounter = 0;
int callbackCounter = 0;
std::mutex taskMutex;
std::mutex callbackMutex;

void task()
{
	for (int counter = 0; counter < 100; ++counter);
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	taskMutex.lock();
	++taskCounter;
	taskMutex.unlock();
}

void callback()
{
	for (int counter = 0; counter < 100; ++counter);
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	callbackMutex.lock();
	++callbackCounter;
	callbackMutex.unlock();
}

void free(ThreadPool &threadPool)
{
	for (int i = 0; i < 100000; ++i)
		//threadPool.pushTask(task, nullptr);
		threadPool.pushTask(TaskPair(task, nullptr));
	std::list<TaskPair> tasks;
	for (int i = 0; i < 100000; ++i)
		tasks.push_back(TaskPair(task, nullptr));
	threadPool.pushTask(tasks);
}

// void Boost(boost::threadpool::thread_pool<> &threadPool)
// {
// 	for (int i = 0; i < 100000; ++i)
// 	{
// 		threadPool.schedule(task);
// 		//threadPool.schedule(callback);
// 	}
// 	for (int i = 0; i < 100000; ++i)
// 	{
// 		threadPool.schedule(task);
// 		//threadPool.schedule(callback);
// 	}
// }

int main()
{
	ThreadPool threadPool(100);
	//boost::threadpool::thread_pool<> threadPool(4);
	clock_t begin = clock();

	free(threadPool);
	//Boost(threadPool);

	std::this_thread::sleep_for(std::chrono::milliseconds(10000));
	std::cout << taskCounter << ' ' << callbackCounter << std::endl;
	std::cout << (double)(clock() - begin)/CLOCKS_PER_SEC*1000 << std::endl;
	//std::cout << threadPool.size();

	return 0;
}
