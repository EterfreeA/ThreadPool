/*
* 文件名称：Timer.h
* 语言标准：C++20
*
* 创建日期：2023年01月21日
*
* 摘要
* 1.定时任务抽象类TimedTask和周期任务抽象类Periodic定义于此文件，实现于Timer.cpp。
* 2.定时任务抽象类定义类族接口，提供获取单调时间、获取系统时间、暂停指定时间等通用方法。
* 3.周期任务抽象类继承定时任务抽象类，提供计算周期时间的默认实现，支持自定义起始时间和间隔时间，并以原子操作确保其线程安全性。
*
* 作者：许聪
* 邮箱：solifree@qq.com
*
* 版本：v1.0.0
*/

#pragma once

#include <chrono>
#include <atomic>

#include "Core.hpp"

ETERFREE_SPACE_BEGIN

class TimedTask
{
public:
	using SteadyTime = std::chrono::steady_clock::time_point;
	using SystemTime = std::chrono::system_clock::time_point;
	using Duration = std::chrono::steady_clock::rep;

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

	// 暂停指定时间
	static void sleep(Duration _duration);

public:
	virtual ~TimedTask() noexcept {}

	// 获取执行时间
	virtual SystemTime getTime() = 0;

	// 是否继续执行
	virtual bool continuous() const = 0;

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
		Duration _target, const SystemTime::duration& _reality) noexcept;

public:
	// 获取执行时间
	virtual SystemTime getTime() noexcept override;

	// 是否继续执行
	virtual bool continuous() const noexcept override
	{
		return getDuration() > 0;
	}

private:
	// 获取时间点
	auto getTimePoint() const noexcept
	{
		return _timePoint.load(std::memory_order::relaxed);
	}

	// 获取间隔时间
	Duration getDuration() const noexcept
	{
		return _duration.load(std::memory_order::relaxed);
	}

public:
	PeriodicTask() noexcept : \
		_timePoint(getSystemTime()), _duration(0) {}

	virtual ~PeriodicTask() noexcept = default;

	// 设置时间点
	void setTimePoint(const SystemTime& _timePoint) noexcept
	{
		this->_timePoint.store(_timePoint, \
			std::memory_order::relaxed);
	}

	// 设置间隔时间
	void setDuration(Duration _duration) noexcept
	{
		this->_duration.store(_duration, \
			std::memory_order::relaxed);
	}
};

ETERFREE_SPACE_END
