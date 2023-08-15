#include "Concurrency/TaskQueue.h"
#include "Concurrency/ThreadPool.hpp"
#include "Concurrency/Timer.h"

#include <chrono>
#include <cstdlib>
#include <memory>
#include <iostream>
#include <thread>

USING_ETERFREE_SPACE

class Task : public PeriodicTask
{
public:
	virtual void execute() override;
};

void Task::execute()
{
	using namespace std::chrono;

	auto duration = getSystemTime().time_since_epoch();
	auto time = duration_cast<SteadyTime::duration>(duration);
	std::cout << time << std::endl;
}

int main()
{
	using namespace std::chrono;
	using namespace std::this_thread;

	ThreadPool threadPool(1);
	auto taskQueue = std::make_shared<TaskQueue>();
	threadPool.setTaskManager(taskQueue);

	auto timer = std::make_shared<Timer>();
	timer->setDuration(2000000);

	SpinAdapter<SpinAdaptee> adapter(timer);
	taskQueue->put(adapter);
	adapter.start();

	auto task = std::make_shared<Task>();
	task->setDuration(200000000);
	timer->pushTask(task);

	sleep_for(seconds(2));
	task->cancel();

	sleep_for(seconds(1));
	return EXIT_SUCCESS;
}
