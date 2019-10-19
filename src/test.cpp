#include "ThreadPool.h"
//#include <threadpool/threadpool.hpp>

#include <thread>
#include <atomic>
#include <chrono>
//#include <filesystem>
//#include <fstream>
#include <iostream>

std::atomic_ulong counter = 0;

void process()
{
	for (volatile int counter = 0; counter < 10000; ++counter);
	std::this_thread::sleep_for(std::chrono::milliseconds(3));
	++counter;
}

void Eterfree(eterfree::ThreadPool &threadPool)
{
	for (int i = 0; i < 100000; ++i)
		threadPool.pushTask(process);
	std::list<eterfree::ThreadPool::functor> tasks;
	for (int i = 0; i < 200000; ++i)
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
	eterfree::ThreadPool threadPool(100, 100);
	//boost::threadpool::thread_pool<> threadPool(100);

	//constexpr auto file = "ThreadPool.log";
	//std::filesystem::remove(file);
	//std::ofstream ofs(file, std::ios::app);
	using std::cout;
	//auto os = cout.rdbuf(ofs.rdbuf());

	using namespace std::chrono;
	auto begin = system_clock::now();
	Eterfree(threadPool);
	//Boost(threadPool);

	std::this_thread::sleep_for(milliseconds(10000));
	using std::endl;
	cout << "任务数量：" << counter << endl;
	auto end = system_clock::now();
	auto duration = duration_cast<milliseconds>(end - begin);
	cout << "执行时间：" << duration.count() << endl;
	eterfree::ThreadPool(std::move(threadPool));

	//cout << endl;
	//std::cout.rdbuf(os);
	return 0;
}
