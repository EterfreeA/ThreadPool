#include "Common.h"

#include <iostream>
#include <thread>

#ifdef _WIN32
#include <Windows.h>
#endif // _WIN32

void sleep_for(const std::chrono::milliseconds& time)
{
#ifdef _WIN32
	constexpr UINT PERIOD = 1;
	auto result = ::timeBeginPeriod(PERIOD);
	if (result != TIMERR_NOERROR)
		std::cerr << "timeBeginPeriod error " << result;
#endif

	std::this_thread::sleep_for(time);

#ifdef _WIN32
	result = ::timeEndPeriod(PERIOD);
	if (result != TIMERR_NOERROR)
		std::cerr << "timeEndPeriod error " << result;
#endif
}
