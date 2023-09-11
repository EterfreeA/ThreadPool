#include "SpinAdapter.h"
#include "Condition.hpp"
#include "Logger.h"

#include <utility>
#include <cstdint>
#include <exception>

ETERFREE_SPACE_BEGIN

struct SpinAdapter::Structure
{
	enum class State : std::uint8_t
	{
		INITIAL,	// 初始态
		RUNNABLE,	// 就绪态
		RUNNING,	// 运行态
		FINAL,		// 最终态
	};

	using Condition = Condition<>;

	std::mutex _threadMutex;
	Condition _condition;

	mutable std::mutex _stateMutex;
	State _state;

	Adaptee _adaptee;

	Structure(const Adaptee& _adaptee) : \
		_adaptee(_adaptee), _state(State::INITIAL) {}

	Structure(Adaptee&& _adaptee) : \
		_adaptee(std::forward<Adaptee>(_adaptee)), \
		_state(State::INITIAL) {}

	auto getState() const
	{
		std::lock_guard lock(_stateMutex);
		return _state;
	}

	auto setState(State _state);

	bool start();

	void stop();

	void execute();
};

auto SpinAdapter::Structure::setState(State _state)
{
	std::lock_guard lock(_stateMutex);
	auto state = this->_state;
	this->_state = _state;
	return state;
}

bool SpinAdapter::Structure::start()
{
	std::lock_guard lock(_threadMutex);
	if (getState() != State::INITIAL)
		return false;

	if (_adaptee) _adaptee->start();
	setState(State::RUNNABLE);

	_condition.notify_one(Condition::Strategy::RELAXED);
	return true;
}

void SpinAdapter::Structure::stop()
{
	std::lock_guard lock(_threadMutex);
	auto state = setState(State::FINAL);
	if (state == State::FINAL) return;

	_condition.notify_one(Condition::Strategy::RELAXED);

	if (state == State::RUNNING)
		_condition.wait([this]
			{ return not _condition; });

	if (_adaptee) _adaptee->stop();
}

void SpinAdapter::Structure::execute()
{
	_condition.wait([this]
		{
			std::lock_guard lock(_stateMutex);
			if (_state != State::RUNNABLE \
				and _state != State::FINAL)
				return false;

			if (_state == State::RUNNABLE)
				_state = State::RUNNING;
			return true;
		});

	while (getState() == State::RUNNING \
		and _adaptee) _adaptee->execute();

	_condition.exit();
}

void SpinAdapter::move(SpinAdapter& _left, \
	SpinAdapter&& _right) noexcept
{
	_left._master = _right._master;
	_right._master = false;
	_left._data = std::move(_right._data);
}

void SpinAdapter::stop(bool _master)
{
	if (_master and _data)
		_data->stop();
}

SpinAdapter::SpinAdapter(const Adaptee& _adaptee) : \
	_master(true), \
	_data(std::make_shared<Structure>(_adaptee))
{

}

SpinAdapter::SpinAdapter(Adaptee&& _adaptee) : \
	_master(true), \
	_data(std::make_shared<Structure>(std::forward<Adaptee>(_adaptee)))
{

}

SpinAdapter::SpinAdapter(SpinAdapter&& _another) noexcept
{
	try
	{
		std::lock_guard lock(_another._mutex);
		move(*this, std::forward<SpinAdapter>(_another));
	}
	catch (std::exception& exception)
	{
		Logger::output(Logger::Level::ERROR, \
			std::source_location::current(), exception);
	}
}

SpinAdapter::~SpinAdapter() noexcept
{
	try
	{
		std::lock_guard lock(_mutex);
		stop(_master);
	}
	catch (std::exception& exception)
	{
		Logger::output(Logger::Level::ERROR, \
			std::source_location::current(), \
			exception);
	}
}

auto SpinAdapter::operator=(const SpinAdapter& _another) \
-> SpinAdapter&
{
	if (&_another != this)
	{
		std::lock_guard lock(this->_mutex);
		stop(this->_master);

		this->_master = false;
		this->_data = _another.load();
	}
	return *this;
}

auto SpinAdapter::operator=(SpinAdapter&& _another) noexcept \
-> SpinAdapter&
{
	if (&_another != this)
	{
		try
		{
			std::lock_guard thisLock(this->_mutex);
			stop(this->_master);

			std::lock_guard anotherLock(_another._mutex);
			move(*this, std::forward<SpinAdapter>(_another));
		}
		catch (std::exception& exception)
		{
			Logger::output(Logger::Level::ERROR, \
				std::source_location::current(), exception);
		}
	}
	return *this;
}

void SpinAdapter::operator()()
{
	if (auto data = load())
		data->execute();
}

bool SpinAdapter::start()
{
	auto data = load();
	return data ? \
		data->start() : false;
}

void SpinAdapter::stop()
{
	if (auto data = load())
		data->stop();
}

ETERFREE_SPACE_END
