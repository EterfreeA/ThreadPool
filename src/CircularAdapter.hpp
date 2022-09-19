#pragma once

#include "Core.hpp"
#include "Condition.hpp"

#include <memory>
#include <cstdint>
#include <atomic>

ETERFREE_SPACE_BEGIN

template <typename _Adaptee>
class CircularAdapter
{
	using Adaptee = _Adaptee;
	using Condition = Condition<>;

private:
	enum class State : uint8_t
	{
		INITIALIZED,
		EXECUTING,
		TERMINATED,
	};

	struct Structure;

	using DataType = std::shared_ptr<Structure>;
	using AtomicType = std::atomic<DataType>;

private:
	bool master;
	AtomicType _atomic;

private:
	// 加载非原子数据
	auto load() const noexcept
	{
		return _atomic.load(std::memory_order::relaxed);
	}

public:
	CircularAdapter(Adaptee& _adaptee)
		: master(true), _atomic(std::make_shared<Structure>(_adaptee)) {}

	CircularAdapter(const CircularAdapter& _another) noexcept
		: master(false), _atomic(_another.load()) {}

	~CircularAdapter()
	{
		if (master)
			if (auto data = load())
				data->stop();
	}

	void operator()()
	{
		if (auto data = load())
			data->execute();
	}

	bool start()
	{
		auto data = load();
		return data ? data->start() : false;
	}

	void stop()
	{
		if (auto data = load())
			data->stop();
	}
};

template <typename _Adaptee>
struct CircularAdapter<_Adaptee>::Structure
{
	Adaptee& _adaptee;
	Condition _condition;
	std::atomic_bool _flag;
	std::atomic<State> _state;

	Structure(Adaptee& _adaptee)
		: _adaptee(_adaptee), _flag(false), _state(State::INITIALIZED) {}

	void setFlag(bool _flag) noexcept
	{
		this->_flag.store(_flag, \
			std::memory_order::relaxed);
	}

	auto getFlag() const noexcept
	{
		return _flag.load(std::memory_order::relaxed);
	}

	auto setState(State _state) noexcept
	{
		return this->_state.exchange(_state, \
			std::memory_order::relaxed);
	}

	auto getState() const noexcept
	{
		return _state.load(std::memory_order::relaxed);
	}

	bool start();

	void stop();

	void execute();
};

template <typename _Adaptee>
bool CircularAdapter<_Adaptee>::Structure::start()
{
	if (getState() != State::INITIALIZED)
		return false;

	auto result = _adaptee.start();
	if (result)
	{
		setState(State::EXECUTING);
		_condition.notify_one(Condition::Strategy::RELAXED);
	}
	return result;
}

template <typename _Adaptee>
void CircularAdapter<_Adaptee>::Structure::stop()
{
	if (setState(State::TERMINATED) == State::TERMINATED)
		return;

	_condition.notify_one(Condition::Strategy::RELAXED);
	_condition.wait([this]() { return !getFlag(); });
	_adaptee.stop();
}

template <typename _Adaptee>
void CircularAdapter<_Adaptee>::Structure::execute()
{
	_condition.wait([this]()
		{
			if (getState() == State::INITIALIZED)
				return false;

			setFlag(true);
			return true;
		});

	while (getState() == State::EXECUTING)
		_adaptee.execute();

	setFlag(false);
	_condition.exit();
}

ETERFREE_SPACE_END
