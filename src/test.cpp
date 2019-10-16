#include "ThreadPool.h"
//#include <threadpool/threadpool.hpp>

#include <thread>
#include <atomic>
#include <ctime>
//#include <fstream>
#include <iostream>

std::atomic_ulong counter = 0;

void process()
{
	for (int counter = 0; counter < 100; ++counter);
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	++counter;
}

void Eterfree(eterfree::ThreadPool &threadPool)
{
	for (int i = 0; i < 100000; ++i)
		threadPool.pushTask(process);
	std::list<eterfree::ThreadPool::functor> tasks;
	for (int i = 0; i < 100000; ++i)
		tasks.push_back(process);
	threadPool.pushTask(tasks);
}

// void Boost(boost::threadpool::thread_pool<> &threadPool)
// {
// 	for (int i = 0; i < 100000; ++i)
// 	{
// 		threadPool.schedule(process);
// 	}
// 	for (int i = 0; i < 100000; ++i)
// 	{
// 		threadPool.schedule(process);
// 	}
// }

int main()
{
	eterfree::ThreadPool threadPool(100);
	//boost::threadpool::thread_pool<> threadPool(100);

	//std::ofstream ofs("ThreadPool.log", std::ios::app);
	//auto os = std::cout.rdbuf(ofs.rdbuf());
	clock_t begin = clock();

	Eterfree(threadPool);
	//Boost(threadPool);

	std::this_thread::sleep_for(std::chrono::milliseconds(10000));
	std::cout << "任务数量：" << counter << std::endl;
	std::cout << "执行时间：" << (double)(clock() - begin)/CLOCKS_PER_SEC*1000 << std::endl;
	eterfree::ThreadPool(std::move(threadPool));

	//std::cout << std::endl;
	//std::cout.rdbuf(os);
	return 0;
}
