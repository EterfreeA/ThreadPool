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
	virtual SystemTime getTime() = 0;

	virtual bool continuous() = 0;

	virtual void execute() = 0;
};

class PeriodicTask : public TimedTask
{
private:
	std::atomic<SystemTime> _timePoint;
	std::atomic<Duration> _duration;

private:
	static SystemTime getNextTime(SystemTime _timePoint, \
		Duration _target, const SystemTime::duration& _reality) noexcept;

public:
	virtual SystemTime getTime() override;

	virtual bool continuous() override
	{
		return getDuration() > 0;
	}

private:
	auto getTimePoint() const noexcept
	{
		return _timePoint.load(std::memory_order::relaxed);
	}

	Duration getDuration() const noexcept
	{
		return _duration.load(std::memory_order::relaxed);
	}

public:
	PeriodicTask() noexcept
		: _timePoint(getSystemTime()), _duration(0) {}

	void setTimePoint(const SystemTime& _timePoint) noexcept
	{
		this->_timePoint.store(_timePoint, \
			std::memory_order::relaxed);
	}

	void setDuration(Duration _duration) noexcept
	{
		this->_duration.store(_duration, \
			std::memory_order::relaxed);
	}
};

ETERFREE_SPACE_END
