#include "Timer.hpp"
#include "ThreadPool.hpp"
#include "CircularAdapter.hpp"

#include <cstdlib>
#include <chrono>
#include <thread>

USING_ETERFREE_SPACE

class Task : public PeriodicTask
{
public:
	virtual void execute() override
	{
		using namespace std::chrono;

		auto duration = getSystemTime().time_since_epoch();
		std::cout << duration_cast<SteadyTime::duration>(duration) << std::endl;
	}
};

int main()
{
	ThreadPool threadPool(1);

	Timer<void*> timer;
	timer.setDuration(2000000);

	CircularAdapter<decltype(timer)> adapter(timer);
	threadPool.pushTask(adapter);
	adapter.start();

	Task task;
	task.setDuration(200000000);
	timer.pushTask(&task, &task);

	using namespace std::this_thread;
	using namespace std::chrono;
	sleep_for(seconds(2));

	timer.popTask(&task);
	sleep_for(seconds(1));
	return EXIT_SUCCESS;
}
