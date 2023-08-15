/*
* 文件名称：Timer.h
* 语言标准：C++20
*
* 创建日期：2023年02月04日
*
* 摘要
* 1.定时任务抽象类TimedTask、周期任务抽象类Periodic和定时器类Timer定义于此文件，实现于Timer.cpp。
* 2.定时任务抽象类定义类族接口，提供获取单调时间、获取系统时间、暂停指定时间等通用方法。
* 3.周期任务抽象类继承定时任务抽象类，提供计算周期时间的默认实现，支持自定义起始时间和间隔时间，并以原子操作确保其线程安全性。
* 4.定时器类继承自旋适配者抽象类SpinAdaptee，由自旋适配器驱动定时器运行，以原子操作与互斥元确保接口的线程安全性。
* 5.定时器类支持自定义轮询间隔时间，用于计算阻塞时间，确保轮询的及时性和精确度。
* 6.引入超时队列类模板TimeoutQueue，以支持管理定时任务。
*
* 作者：许聪
* 邮箱：solifree@qq.com
*
* 版本：v1.0.0
*/

#pragma once

#include <chrono>
#include <memory>
#include <atomic>
#include <mutex>

#include "Sequence/TimeoutQueue.hpp"
#include "SpinAdapter.hpp"

ETERFREE_SPACE_BEGIN

class TimedTask : \
	public std::enable_shared_from_this<TimedTask>
{
public:
	using SteadyTime = std::chrono::steady_clock::time_point;
	using SystemTime = std::chrono::system_clock::time_point;

	using TimeType = std::chrono::nanoseconds;
	using Duration = TimeType::rep;

public:
	// 获取单调时间
	static SteadyTime getSteadyTime() noexcept
	{
		return std::chrono::steady_clock::now();
	}

	// 获取系统时间
	static SystemTime getSystemTime() noexcept
	{
		return std::chrono::system_clock::now();
	}

public:
	virtual ~TimedTask() noexcept {}

	// 获取执行时间
	virtual SystemTime getTime() = 0;

	// 是否有效
	virtual bool valid() const = 0;

	// 是否持续
	virtual bool persistent() const = 0;

	// 取消任务
	virtual bool cancel() = 0;

	// 执行任务
	virtual void execute() = 0;
};

class PeriodicTask : public TimedTask
{
	std::atomic<SystemTime> _timePoint;
	std::atomic<Duration> _duration;

private:
	// 获取后续执行时间
	static SystemTime getNextTime(const SystemTime& _timePoint, \
		Duration _duration, const SystemTime::duration& _realTime) noexcept;

public:
	// 获取执行时间
	virtual SystemTime getTime() noexcept override;

	// 是否有效
	virtual bool valid() const noexcept override
	{
		return getDuration() >= 0;
	}

	// 是否持续
	virtual bool persistent() const noexcept override
	{
		return getDuration() > 0;
	}

	// 取消任务
	virtual bool cancel() noexcept override
	{
		setDuration(-1);
		return true;
	}

public:
	PeriodicTask() noexcept : \
		_timePoint(getSystemTime()), _duration(0) {}

	virtual ~PeriodicTask() noexcept = default;

	// 获取时间点
	auto getTimePoint() const noexcept
	{
		return _timePoint.load(std::memory_order::relaxed);
	}

	// 设置时间点
	void setTimePoint(const SystemTime& _timePoint) noexcept
	{
		this->_timePoint.store(_timePoint, \
			std::memory_order::relaxed);
	}

	// 获取间隔时间
	Duration getDuration() const noexcept
	{
		return _duration.load(std::memory_order::relaxed);
	}

	// 设置间隔时间
	void setDuration(Duration _duration) noexcept
	{
		this->_duration.store(_duration, \
			std::memory_order::relaxed);
	}
};

class Timer : public SpinAdaptee
{
public:
	using SteadyTime = TimedTask::SteadyTime;
	using SystemTime = TimedTask::SystemTime;

	using TimeType = TimedTask::TimeType;
	using Duration = TimedTask::Duration;

	using TaskType = std::shared_ptr<TimedTask>;

private:
	using TaskQueue = TimeoutQueue<SystemTime, TaskType>;

private:
	std::mutex _timeMutex;
	SteadyTime _timePoint;
	Duration _correction;

	std::atomic<Duration> _duration;

	std::mutex _taskMutex;
	TaskQueue _taskQueue;

public:
	// 等待相对时间
	static void waitFor(SteadyTime& _timePoint, \
		Duration& _correction, Duration _duration);

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

	virtual ~Timer() = default;

	Timer& operator=(const Timer&) = delete;

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
	bool pushTask(const TaskType& _task);
	bool pushTask(TaskType&& _task);
};

ETERFREE_SPACE_END
