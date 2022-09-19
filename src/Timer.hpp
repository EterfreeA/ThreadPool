#pragma once

#include <mutex>

#include "Timer.h"
#include "TimeoutQueue.hpp"

ETERFREE_SPACE_BEGIN

template <typename _IndexType>
class Timer
{
public:
	using IndexType = _IndexType;

	using SteadyTime = TimedTask::SteadyTime;
	using Duration = TimedTask::Duration;

	using SystemTime = TimedTask::SystemTime;
	using TaskQueue = TimeoutQueue<IndexType, TimedTask*, SystemTime>;

private:
	SteadyTime _timePoint;
	Duration _correction;

	std::atomic<Duration> _duration;
	std::mutex _mutex;
	TaskQueue _queue;

public:
	Timer() noexcept
		: _timePoint(TimedTask::getSteadyTime()), _correction(0), _duration(0) {}

	// 设置暂停间隔
	auto setDuration(Duration _duration) noexcept
	{
		return this->_duration.exchange(_duration, \
			std::memory_order::relaxed);
	}

	// 获取暂停间隔
	auto getDuration() const noexcept
	{
		return _duration.load(std::memory_order::relaxed);
	}

	// 放入定时任务
	bool pushTask(const IndexType& _index, TimedTask* _task);

	// 取出定时任务
	TimedTask* popTask(const IndexType& _index);

	// 启动定时器
	bool start() noexcept;

	// 停止定时器
	void stop() noexcept {}

	// 轮询定时任务
	void execute();
};

// 放入定时任务
template <typename _IndexType>
bool Timer<_IndexType>::pushTask(const IndexType& _index, TimedTask* _task)
{
	if (_task == nullptr) return false;

	std::lock_guard lock(_mutex);
	return _queue.push(_index, _task, _task->getTime());
}

// 取出定时任务
template <typename _IndexType>
TimedTask* Timer<_IndexType>::popTask(const IndexType& _index)
{
	std::unique_lock lock(_mutex);
	auto result = _queue.pop(_index);
	lock.unlock();

	return result ? result.value() : nullptr;
}

// 启动定时器
template <typename _IndexType>
bool Timer<_IndexType>::start() noexcept
{
	_timePoint = TimedTask::getSteadyTime();
	_correction = 0;
	return true;
}

// 轮询定时任务
template <typename _IndexType>
void Timer<_IndexType>::execute()
{
	typename TaskQueue::VectorType outVector;
	std::unique_lock lock(_mutex);
	bool result = _queue.pop(TimedTask::getSystemTime(), outVector);
	lock.unlock();

	if (result)
	{
		decltype(outVector) inVector;
		inVector.reserve(outVector.size());

		for (auto& [index, task] : outVector)
		{
			task->execute();

			if (task->continuous())
				inVector.emplace_back(index, task);
		}

		if (inVector.size() > 0)
		{
			lock.lock();
			for (auto& [index, task] : inVector)
				_queue.push(index, task, task->getTime());
			lock.unlock();
		}
	}

	auto duration = getDuration();
	if (duration <= 0)
	{
		_timePoint = TimedTask::getSteadyTime();
		return;
	}

	auto timePoint = TimedTask::getSteadyTime();
	auto time = (timePoint - _timePoint).count();
	_timePoint = timePoint;

	auto difference = duration - _correction;
	if (time >= difference)
	{
		_correction = (time - difference) % duration;
		return;
	}

	auto sleepTime = difference - time;
	TimedTask::sleep(sleepTime);

	timePoint = TimedTask::getSteadyTime();
	auto realTime = (timePoint - _timePoint).count();
	_timePoint = timePoint;

	_correction = (realTime - sleepTime) % duration;
}

ETERFREE_SPACE_END
