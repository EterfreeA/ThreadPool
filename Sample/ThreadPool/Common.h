#pragma once

//#define FILE_STREAM
//#define FILE_SYSTEM

#include <chrono>

#ifdef FILE_STREAM
#include <fstream>

#ifdef FILE_SYSTEM
#include <filesystem>
#endif // FILE_SYSTEM
#endif // FILE_STREAM

void sleepFor(std::chrono::nanoseconds::rep _duration);
