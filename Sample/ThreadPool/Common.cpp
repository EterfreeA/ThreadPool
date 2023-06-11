#include "Common.h"

#include <thread>

#ifdef _WIN32
#include <iostream>

#include <Windows.h>
#pragma comment(lib, "Winmm.lib")
#endif // _WIN32

void sleep_for(const std::chrono::milliseconds& time)
{
#ifdef _WIN32
	using std::cerr, std::endl;

	constexpr UINT PERIOD = 1;
	auto result = ::timeBeginPeriod(PERIOD);
	if (result != TIMERR_NOERROR)
		cerr << "timeBeginPeriod error " << result << endl;
#endif // _WIN32

	std::this_thread::sleep_for(time);

#ifdef _WIN32
	result = ::timeEndPeriod(PERIOD);
	if (result != TIMERR_NOERROR)
		cerr << "timeEndPeriod error " << result << endl;
#endif // _WIN32
}
