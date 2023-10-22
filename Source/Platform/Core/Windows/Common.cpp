#include "Platform/Core/Common.h"
#include "Core/Logger.h"

#include <cstdio>
#include <sstream>
#include <thread>

#include <Windows.h>
#pragma comment(lib, "WinMM.Lib")

#undef ERROR

PLATFORM_SPACE_BEGIN

// 获取错误描述
bool formatError(std::string& _buffer, std::uint64_t _error)
{
	auto size = _buffer.size();
	auto error = static_cast<DWORD>(_error);

	DWORD result = 0, length = 0, code = NO_ERROR;
	do
	{
		length += BUFSIZ;
		_buffer.resize(size + length);

		result = ::FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, \
			NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), \
			_buffer.data() + size, length, NULL);
		code = ::GetLastError();
	} while (result == 0 and code == ERROR_INSUFFICIENT_BUFFER);

	if (result == 0)
	{
		std::ostringstream stream;
		stream << "FormatMessageA error " << code;

		Logger::output(Logger::Level::ERROR, \
			std::source_location::current(), stream.str());
	}

	if (result < 2)
		_buffer.resize(size + result);
	else
		_buffer.resize(size + result - 2);
	return result != 0;
}

// 获取错误描述
bool formatError(std::wstring& _buffer, std::uint64_t _error)
{
	auto size = _buffer.size();
	auto error = static_cast<DWORD>(_error);

	DWORD result = 0, length = 0, code = NO_ERROR;
	do
	{
		length += BUFSIZ;
		_buffer.resize(size + length);

		result = ::FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, \
			NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), \
			_buffer.data() + size, length, NULL);
		code = ::GetLastError();
	} while (result == 0 and code == ERROR_INSUFFICIENT_BUFFER);

	if (result == 0)
	{
		std::ostringstream stream;
		stream << "FormatMessageW error " << code;

		Logger::output(Logger::Level::ERROR, \
			std::source_location::current(), stream.str());
	}

	if (result < 2)
		_buffer.resize(size + result);
	else
		_buffer.resize(size + result - 2);
	return result != 0;
}

// 获取模块路径
bool getImagePath(std::string& _path)
{
	DWORD result = 0, length = 0;
	do
	{
		length += MAX_PATH;
		_path.resize(length);

		result = ::GetModuleFileNameA(NULL, \
			_path.data(), length);
	} while (result >= length);

	if (result == 0)
	{
		auto error = ::GetLastError();
		std::string buffer;
		formatError(buffer, error);

		std::ostringstream stream;
		stream << "GetModuleFileNameA error " \
			<< error << ": " << buffer;

		Logger::output(Logger::Level::ERROR, \
			std::source_location::current(), \
			stream.str());

		_path.clear();
		return false;
	}

	_path.resize(result);
	return true;
}

// 获取模块路径
bool getImagePath(std::wstring& _path)
{
	DWORD result = 0, length = 0;
	do
	{
		length += MAX_PATH;
		_path.resize(length);

		result = ::GetModuleFileNameW(NULL, \
			_path.data(), length);
	} while (result >= length);

	if (result == 0)
	{
		auto error = ::GetLastError();
		std::string buffer;
		formatError(buffer, error);

		std::ostringstream stream;
		stream << "GetModuleFileNameW error " \
			<< error << ": " << buffer;

		Logger::output(Logger::Level::ERROR, \
			std::source_location::current(), \
			stream.str());

		_path.clear();
		return false;
	}

	_path.resize(result);
	return true;
}

// 暂停指定时间
void sleepFor(std::chrono::nanoseconds::rep _duration)
{
	if (_duration <= 0) return;

	constexpr UINT PERIOD = 1;

	auto result = ::timeBeginPeriod(PERIOD);
	if (result != TIMERR_NOERROR)
	{
		std::string buffer = "timeBeginPeriod error ";
		buffer += std::to_string(result) += ": ";
		formatError(buffer, result);

		Logger::output(Logger::Level::ERROR, \
			std::source_location::current(), buffer);
	}

	auto duration = std::chrono::nanoseconds(_duration);
	std::this_thread::sleep_for(duration);

	result = ::timeEndPeriod(PERIOD);
	if (result != TIMERR_NOERROR)
	{
		std::string buffer = "timeEndPeriod error ";
		buffer += std::to_string(result) += ": ";
		formatError(buffer, result);

		Logger::output(Logger::Level::ERROR, \
			std::source_location::current(), buffer);
	}
}

PLATFORM_SPACE_END
