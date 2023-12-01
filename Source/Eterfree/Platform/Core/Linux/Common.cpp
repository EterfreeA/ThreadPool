#include "Eterfree/Platform/Core/Common.h"

#include <thread>

PLATFORM_SPACE_BEGIN

// 暂停指定时间
void sleepFor(std::chrono::nanoseconds::rep _duration)
{
	using namespace std::chrono;

	if (_duration > 0)
	{
		auto duration = nanoseconds(_duration);
		std::this_thread::sleep_for(duration);
	}
}

PLATFORM_SPACE_END
