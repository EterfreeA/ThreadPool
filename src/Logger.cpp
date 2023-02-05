#include "Logger.h"

#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>

ETERFREE_SPACE_BEGIN

class CommonLogger : public Logger
{
public:
	using TimeType = std::chrono::nanoseconds::rep;
	using TimePoint = std::chrono::system_clock::time_point;

public:
	static TimeType getTime(const TimePoint& _timePoint) noexcept;
};

class ThreadID
{
public:
	using IDType = std::thread::id;

private:
	IDType _id;

private:
	friend std::ostream& operator<<(std::ostream& _stream, \
		const ThreadID& _id)
	{
		return _stream << "[thread " << _id._id << ']';
	}

public:
	ThreadID(IDType _id) noexcept : _id(_id) {}
};

static const char* LEVEL[] =
{
	"", "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

std::ostream& operator<<(std::ostream& _stream, Logger::Level _level)
{
	if (_level > Logger::Level::EMPTY)
	{
		auto level = static_cast<std::uint8_t>(_level);
		if (level < size(LEVEL))
			_stream << '[' << LEVEL[level] << ']';
	}
	return _stream;
}

std::ostream& operator<<(std::ostream& _stream, \
	const CommonLogger::TimePoint& _timePoint)
{
	return _stream << '[' \
		<< CommonLogger::getTime(_timePoint) << ']';
}

std::shared_ptr<Logger> Logger::get(Mode _mode) noexcept
{
	switch (_mode)
	{
	case Mode::SINGLE_THREAD:
	case Mode::MULTI_THREAD:
	default:
		try
		{
			std::ostringstream stream;
			stream << "unknown mode " \
				<< static_cast<std::uint16_t>(_mode);
			output(Level::ERROR, \
				std::source_location::current(), \
				stream.str());
		}
		catch (std::exception& exception)
		{
			output(Level::ERROR, \
				std::source_location::current(), \
				exception);
		}
	}
	return nullptr;
}

void Logger::output(Level _level, \
	const std::source_location& _location, \
	const char* _description) noexcept
{
	try
	{
		std::ostringstream stream;
		stream << std::chrono::system_clock::now() \
			<< ThreadID(std::this_thread::get_id()) \
			<< _level << _location << ": " \
			<< _description << std::endl;
		std::clog << stream.str();
	}
	catch (std::exception&) {}
}

void Logger::output(Level _level, \
	const std::source_location& _location, \
	const std::string& _description) noexcept
{
	try
	{
		std::ostringstream stream;
		stream << std::chrono::system_clock::now() \
			<< ThreadID(std::this_thread::get_id()) \
			<< _level << _location << ": " \
			<< _description << std::endl;
		std::clog << stream.str();
	}
	catch (std::exception&) {}
}

void Logger::output(Level _level, \
	const std::source_location& _location, \
	const std::exception& _exception) noexcept
{
	try
	{
		std::ostringstream stream;
		stream << std::chrono::system_clock::now() \
			<< ThreadID(std::this_thread::get_id()) \
			<< _level << _location << ": " \
			<< _exception.what() << std::endl;
		std::clog << stream.str();
	}
	catch (std::exception&) {}
}

void Logger::output(Level _level, \
	const std::source_location& _location, \
	const std::error_code& _code) noexcept
{
	try
	{
		std::ostringstream stream;
		stream << std::chrono::system_clock::now() \
			<< ThreadID(std::this_thread::get_id()) \
			<< _level << _location << ": " \
			<< _code.message() << std::endl;
		std::clog << stream.str();
	}
	catch (std::exception&) {}
}

CommonLogger::TimeType CommonLogger::getTime(const TimePoint& _timePoint) noexcept
{
	try
	{
		using namespace std::chrono;
		auto duration = _timePoint.time_since_epoch();
		return duration_cast<nanoseconds>(duration).count();
	}
	catch (std::exception& exception)
	{
		output(Level::ERROR, std::source_location::current(), exception);
	}
	return 0;
}

ETERFREE_SPACE_END
