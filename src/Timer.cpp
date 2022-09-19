#include "Timer.h"

#ifdef _WIN32
#include <Windows.h>
#pragma comment(lib, "winmm.lib")
#endif

#include <iostream>
#include <thread>

ETERFREE_SPACE_BEGIN

// 暂停指定时间
void TimedTask::sleep(Duration _duration)
{
	if (_duration <= 0) return;

#ifdef _WIN32
	UINT period = 1;
	auto result = timeBeginPeriod(period);
	if (result != TIMERR_NOERROR)
		std::cerr << result << std::endl;
#endif

	auto duration = SteadyTime::duration(_duration);
	std::this_thread::sleep_for(duration);

#ifdef _WIN32
	result = timeEndPeriod(period);
	if (result != TIMERR_NOERROR)
		std::cerr << result << std::endl;
#endif
}

PeriodicTask::SystemTime PeriodicTask::getNextTime(SystemTime _timePoint, \
	Duration _target, const SystemTime::duration& _reality) noexcept
{
	using namespace std::chrono;
	auto time = duration_cast<SteadyTime::duration>(_reality).count();
	time = (time / _target + (time % _target != 0 ? 1 : 0)) * _target;
	std::cout << time << std::endl; // 测试代码

	auto duration = duration_cast<SystemTime::duration>(SteadyTime::duration(time));
	return _timePoint + duration;
}

auto PeriodicTask::getTime() -> SystemTime
{
	auto timePoint = getTimePoint();
	auto duration = getDuration();
	if (duration <= 0) return timePoint;

	return getNextTime(timePoint, \
		duration, getSystemTime() - timePoint);
}

ETERFREE_SPACE_END
