#include "Timer.h"
#include "Logger.h"

#include <iostream>
#include <thread>

#ifdef _WIN32
#include <string>
#include <sstream>

#include <Windows.h>
#pragma comment(lib, "winmm.lib")

#undef ERROR

// 获取错误描述
static std::string& getErrorInfo(std::string& _info, DWORD _error)
{
	auto size = _info.size();
	_info.resize(size + FORMAT_MESSAGE_ALLOCATE_BUFFER);
	if (::FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, \
		_error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), \
		_info.data() + size, \
		FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL) == 0)
	{
		std::ostringstream stream;
		stream << "FormatMessage error: " << ::GetLastError();

		USING_ETERFREE_SPACE;
		Logger::output(Logger::Level::ERROR, \
			std::source_location::current(), stream.str());
	}
	return _info;
}
#endif

ETERFREE_SPACE_BEGIN

// 暂停指定时间
void TimedTask::sleep(Duration _duration)
{
	if (_duration <= 0) return;

#ifdef _WIN32
	constexpr UINT PERIOD = 1;
	auto result = ::timeBeginPeriod(PERIOD);
	if (result != TIMERR_NOERROR)
	{
		std::string info = "timeBeginPeriod error: ";
		info += std::to_string(result) += ' ';
		Logger::output(Logger::Level::ERROR, \
			std::source_location::current(), \
			getErrorInfo(info, result));
	}
#endif

	auto duration = SteadyTime::duration(_duration);
	std::this_thread::sleep_for(duration);

#ifdef _WIN32
	result = ::timeEndPeriod(PERIOD);
	if (result != TIMERR_NOERROR)
	{
		std::string info = "timeEndPeriod error: ";
		info += std::to_string(result) += ' ';
		Logger::output(Logger::Level::ERROR, \
			std::source_location::current(), \
			getErrorInfo(info, result));
	}
#endif
}

// 获取后续执行时间
auto PeriodicTask::getNextTime(const SystemTime& _timePoint, Duration _target, \
	const SystemTime::duration& _reality) noexcept -> SystemTime
{
	using namespace std::chrono;
	auto time = duration_cast<SteadyTime::duration>(_reality).count();
	auto remainder = time % _target;
	time = time - remainder + (remainder > 0 ? _target : 0);
	std::cout << time << std::endl; // 测试代码

	auto duration = duration_cast<SystemTime::duration>(SteadyTime::duration(time));
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

ETERFREE_SPACE_END
