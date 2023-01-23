/*
* 文件名称：Timer.hpp
* 语言标准：C++20
*
* 创建日期：2023年01月23日
*
* 摘要
* 1.定义定时器类模板Timer，以原子操作与互斥元确保接口的线程安全性。
* 2.定时器类模板继承自旋适配者抽象类SpinAdaptee，由自旋适配器驱动定时器运行。
* 3.引入超时队列类模板TimeoutQueue，支持管理定时任务。
* 4.支持自定义轮询间隔时间，用于计算阻塞时间，确保轮询的及时性和精确度。
*
* 作者：许聪
* 邮箱：solifree@qq.com
*
* 版本：v1.0.0
*/

#pragma once

#include <exception>
#include <mutex>

#include "Timer.h"
#include "SpinAdapter.hpp"
#include "TimeoutQueue.hpp"
#include "Logger.h"

ETERFREE_SPACE_BEGIN

template <typename _IndexType>
class Timer : public SpinAdaptee
{
public:
	using IndexType = _IndexType;

	using SteadyTime = TimedTask::SteadyTime;
	using SystemTime = TimedTask::SystemTime;
	using Duration = TimedTask::Duration;

private:
	using TaskQueue = TimeoutQueue<IndexType, TimedTask*, SystemTime>;

private:
	std::mutex _timeMutex;
	SteadyTime _timePoint;
	Duration _correction;

	std::atomic<Duration> _duration;

	std::mutex _taskMutex;
	TaskQueue _taskQueue;

private:
	// 启动定时器
	virtual void start() override;

	// 停止定时器
	virtual void stop() noexcept override {}

	// 轮询定时任务
	virtual void execute() override
	{
		update();
		wait();
	}

private:
	// 更新定时任务
	void update();

	// 等待后续轮询
	void wait();

public:
	Timer() : \
		_timePoint(TimedTask::getSteadyTime()), \
		_correction(0), _duration(0) {}

	Timer(const Timer&) = delete;

	Timer& operator=(const Timer&) = delete;

	virtual ~Timer() = default;

	// 获取暂停间隔
	auto getDuration() const noexcept
	{
		return _duration.load(std::memory_order::relaxed);
	}

	// 设置暂停间隔
	auto setDuration(Duration _duration) noexcept
	{
		return this->_duration.exchange(_duration, \
			std::memory_order::relaxed);
	}

	// 放入定时任务
	bool pushTask(const IndexType& _index, TimedTask* _task);

	// 取出定时任务
	TimedTask* popTask(const IndexType& _index);
};

// 启动定时器
template <typename _IndexType>
void Timer<_IndexType>::start()
{
	std::lock_guard lock(_timeMutex);
	_timePoint = TimedTask::getSteadyTime();
	_correction = 0;
}

// 更新定时任务
template <typename _IndexType>
void Timer<_IndexType>::update()
{
	typename TaskQueue::VectorType outVector;
	auto time = TimedTask::getSystemTime();

	std::unique_lock lock(_taskMutex);
	if (not _taskQueue.pop(time, outVector)) return;
	lock.unlock();

	decltype(outVector) inVector;
	inVector.reserve(outVector.size());

	for (auto& [index, task] : outVector)
	{
		try
		{
			task->execute();

			if (task->continuous())
				inVector.emplace_back(index, task);
		}
		catch (std::exception& exception)
		{
			Logger::output(Logger::Level::ERROR, \
				std::source_location::current(), \
				exception);
		}
	}

	lock.lock();
	for (auto& [index, task] : inVector)
	{
		try
		{
			_taskQueue.push(index, task, \
				task->getTime());
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
template <typename _IndexType>
void Timer<_IndexType>::wait()
{
	auto duration = getDuration();
	if (duration <= 0)
	{
		std::lock_guard lock(_timeMutex);
		_timePoint = TimedTask::getSteadyTime();
		return;
	}

	std::lock_guard lock(_timeMutex);
	auto timePoint = TimedTask::getSteadyTime();
	auto realTime = (timePoint - _timePoint).count() % duration;
	_timePoint = timePoint;

	auto difference = duration - _correction % duration;
	if (realTime >= difference)
	{
		_correction = (realTime - difference) % duration;
		return;
	}

	auto sleepTime = difference - realTime;
	TimedTask::sleep(sleepTime);

	timePoint = TimedTask::getSteadyTime();
	realTime = (timePoint - _timePoint).count();
	_timePoint = timePoint;

	_correction = (realTime - sleepTime) % duration;
}

// 放入定时任务
template <typename _IndexType>
bool Timer<_IndexType>::pushTask(const IndexType& _index, \
	TimedTask* _task)
{
	if (_task == nullptr) return false;

	std::lock_guard lock(_taskMutex);
	return _taskQueue.push(_index, _task, _task->getTime());
}

// 取出定时任务
template <typename _IndexType>
TimedTask* Timer<_IndexType>::popTask(const IndexType& _index)
{
	std::unique_lock lock(_taskMutex);
	auto result = _taskQueue.pop(_index);
	lock.unlock();

	return result ? result.value() : nullptr;
}

ETERFREE_SPACE_END
