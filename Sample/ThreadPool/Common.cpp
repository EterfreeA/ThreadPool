#include "Common.h"

#include <string>
#include <iostream>
#include <thread>

#ifdef _WIN32
#include <Windows.h>
#pragma comment(lib, "WinMM.Lib")
#endif // _WIN32

void sleepFor(std::chrono::nanoseconds::rep _duration)
{
	if (_duration <= 0) return;

#ifdef _WIN32
	constexpr UINT PERIOD = 1;

	auto result = ::timeBeginPeriod(PERIOD);
	if (result != TIMERR_NOERROR)
	{
		std::string buffer = "timeBeginPeriod error ";
		buffer += std::to_string(result);
		std::cerr << buffer << std::endl;
	}
#endif // _WIN32

	auto duration = std::chrono::nanoseconds(_duration);
	std::this_thread::sleep_for(duration);

#ifdef _WIN32
	result = ::timeEndPeriod(PERIOD);
	if (result != TIMERR_NOERROR)
	{
		std::string buffer = "timeEndPeriod error ";
		buffer += std::to_string(result);
		std::cerr << buffer << std::endl;
	}
#endif // _WIN32
}
