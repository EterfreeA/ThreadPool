#include "Timer.hpp"
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
	std::cout << duration_cast<SteadyTime::duration>(duration) << std::endl;
}

int main()
{
	auto taskQueue = std::make_shared<TaskQueue>();
	ThreadPool<TaskManager> threadPool(1);
	threadPool.setTaskManager(taskQueue);

	using namespace std::this_thread;
	using namespace std::chrono;
	sleep_for(seconds(1));

	Timer<void*> timer;
	timer.setDuration(2000000);

	SpinAdapter<SpinAdaptee> adapter(timer);
	taskQueue->put(adapter);
	adapter.start();

	Task task;
	task.setDuration(200000000);
	timer.pushTask(&task, &task);

	sleep_for(seconds(2));

	timer.popTask(&task);
	sleep_for(seconds(1));
	return EXIT_SUCCESS;
}
