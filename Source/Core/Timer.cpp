#include "Timer.h"
#include "Logger.h"
#include "Platform/Core/Common.h"

#include <utility>
#include <exception>

ETERFREE_SPACE_BEGIN

// 获取后续执行时间
auto PeriodicTask::getNextTime(const SystemTime& _timePoint, Duration _duration, \
	const SystemTime::duration& _realTime) noexcept -> SystemTime
{
	using namespace std::chrono;

	auto time = duration_cast<TimeType>(_realTime).count();
	auto remainder = time % _duration;
	time = time - remainder + (remainder != 0 ? _duration : 0);

	auto duration = duration_cast<SystemTime::duration>(TimeType(time));
	return _timePoint + duration;
}

// 获取执行时间
auto PeriodicTask::getTime() noexcept -> SystemTime
{
	auto timePoint = getTimePoint();
	auto duration = getDuration();
	if (duration <= 0) return timePoint;

	return getNextTime(timePoint, duration, \
		getSystemTime() - timePoint);
}

// 等待相对时间
void Timer::waitFor(SteadyTime& _timePoint, \
	Duration& _correction, Duration _duration)
{
	using namespace std::chrono;

	if (_duration <= 0)
	{
		_timePoint = TimedTask::getSteadyTime();
		return;
	}

	auto timePoint = TimedTask::getSteadyTime();
	auto time = duration_cast<TimeType>(timePoint - _timePoint);
	_timePoint = timePoint;

	auto realTime = time.count() % _duration;
	auto difference = _duration - _correction % _duration;
	if (difference <= realTime) difference += _duration;

	auto sleepTime = difference - realTime;
	sleepFor(sleepTime);

	timePoint = TimedTask::getSteadyTime();
	time = duration_cast<TimeType>(timePoint - _timePoint);
	_timePoint = timePoint;

	realTime = time.count();
	_correction = realTime - sleepTime;
}

// 启动定时器
void Timer::start()
{
	std::lock_guard lock(_timeMutex);
	_timePoint = TimedTask::getSteadyTime();
	_correction = 0;
}

// 更新定时任务
void Timer::update()
{
	typename TaskQueue::Vector outVector;
	auto time = TimedTask::getSystemTime();

	std::unique_lock lock(_taskMutex);
	if (not _taskQueue.pop(time, outVector)) return;
	lock.unlock();

	decltype(outVector) inVector;
	inVector.reserve(outVector.size());

	for (auto& task : outVector)
	{
		try
		{
			if (task->valid())
			{
				task->execute();

				if (task->persistent())
					inVector.push_back(std::move(task));
			}
		}
		catch (std::exception& exception)
		{
			Logger::output(Logger::Level::ERROR, \
				std::source_location::current(), \
				exception);
		}
	}

	lock.lock();
	for (auto& task : inVector)
	{
		try
		{
			_taskQueue.push(task->getTime(), \
				std::move(task));
		}
		catch (std::exception& exception)
		{
			Logger::output(Logger::Level::ERROR, \
				std::source_location::current(), \
				exception);
		}
	}
}

// 等待后续轮询
void Timer::wait()
{
	auto duration = getDuration();

	std::lock_guard lock(_timeMutex);
	waitFor(_timePoint, _correction, duration);
}

// 放入定时任务
bool Timer::pushTask(const TaskType& _task)
{
	if (not _task) return false;

	std::lock_guard lock(_taskMutex);
	return _taskQueue.push(_task->getTime(), _task);
}

// 放入定时任务
bool Timer::pushTask(TaskType&& _task)
{
	if (not _task) return false;

	std::lock_guard lock(_taskMutex);
	return _taskQueue.push(_task->getTime(), \
		std::forward<TaskType>(_task));
}

ETERFREE_SPACE_END
