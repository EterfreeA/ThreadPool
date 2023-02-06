#include "Timer.h"
#include "TaskQueue.h"
#include "ThreadPool.hpp"

#include <cstdlib>
#include <chrono>
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
	ThreadPool threadPool(1);
	auto taskQueue = std::make_shared<TaskQueue>();
	threadPool.setTaskManager(taskQueue);

	using namespace std::this_thread;
	using namespace std::chrono;
	sleep_for(seconds(1));

	Timer timer;
	timer.setDuration(2000000);

	SpinAdapter<SpinAdaptee> adapter(timer);
	taskQueue->put(adapter);
	adapter.start();

	auto task = std::make_shared<Task>();
	task->setDuration(200000000);
	timer.pushTask(task);

	sleep_for(seconds(2));
	task->cancel();

	sleep_for(seconds(1));
	return EXIT_SUCCESS;
}
