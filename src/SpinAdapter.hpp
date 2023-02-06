/*
* 文件名称：SpinAdapter.hpp
* 语言标准：C++20
*
* 创建日期：2023年01月22日
*
* 摘要
* 1.定义自旋适配器类模板SpinAdapter与自旋适配者抽象类SpinAdaptee。
* 2.自旋适配者声明事件接口，统称为适配者接口，包括开启、停止、执行。
* 3.自旋适配器使用隐式接口，可以适配任何声明适配者接口的类，无关乎是否继承自旋适配者。
* 4.对于多线程并发，自旋适配器确保适配者接口的执行顺序，异步处理开启事件，而同步处理停止事件。
* 5.自旋适配器确保接口的线程安全性，支持复制语义和移动语义，不过只有一个实例拥有析构停止权，即析构自动处理停止事件。
*   复制语义用于共享适配者，而不会共享析构停止权；移动语义用于移交适配者和析构停止权。
*
* 作者：许聪
* 邮箱：solifree@qq.com
*
* 版本：v1.0.0
*/

#pragma once

#include <utility>
#include <memory>
#include <cstdint>
#include <mutex>

#include "Core.hpp"
#include "Condition.hpp"

ETERFREE_SPACE_BEGIN

class SpinAdaptee;

template <typename _Adaptee = SpinAdaptee>
class SpinAdapter
{
public:
	struct Structure;

private:
	using Adaptee = _Adaptee;
	using DataType = std::shared_ptr<Structure>;

private:
	mutable std::mutex _mutex;
	bool _master;
	DataType _data;

private:
	static void move(SpinAdapter& _left, \
		SpinAdapter&& _right) noexcept;

private:
	auto load() const
	{
		std::lock_guard lock(_mutex);
		return _data;
	}

	void stop(bool _master)
	{
		if (_master and _data)
			_data->stop();
	}

public:
	SpinAdapter(Adaptee& _adaptee) : _master(true), \
		_data(std::make_shared<Structure>(_adaptee)) {}

	SpinAdapter(const SpinAdapter& _another) : \
		_master(false), _data(_another.load()) {}

	SpinAdapter(SpinAdapter&& _another);

	~SpinAdapter()
	{
		std::lock_guard lock(_mutex);
		stop(_master);
	}

	SpinAdapter& operator=(const SpinAdapter& _another);

	SpinAdapter& operator=(SpinAdapter&& _another);

	void operator()()
	{
		if (auto data = load())
			data->execute();
	}

	bool start();

	void stop()
	{
		if (auto data = load())
			data->stop();
	}
};

class SpinAdaptee
{
	friend struct SpinAdapter<SpinAdaptee>::Structure;

private:
	virtual void start() = 0;

	virtual void stop() = 0;

	virtual void execute() = 0;

public:
	virtual ~SpinAdaptee() noexcept {}
};

template <typename _Adaptee>
struct SpinAdapter<_Adaptee>::Structure
{
	enum class State : std::uint8_t
	{
		INITIAL,	// 初始态
		RUNNABLE,	// 就绪态
		RUNNING,	// 运行态
		FINAL,		// 最终态
	};

	using Condition = Condition<>;

	Adaptee& _adaptee;
	std::mutex _threadMutex;
	Condition _condition;

	mutable std::mutex _stateMutex;
	State _state;

	Structure(Adaptee& _adaptee) : \
		_adaptee(_adaptee), _state(State::INITIAL) {}

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

template <typename _Adaptee>
auto SpinAdapter<_Adaptee>::Structure::setState(State _state)
{
	std::lock_guard lock(_stateMutex);
	auto state = this->_state;
	this->_state = _state;
	return state;
}

template <typename _Adaptee>
bool SpinAdapter<_Adaptee>::Structure::start()
{
	std::lock_guard lock(_threadMutex);
	if (getState() != State::INITIAL)
		return false;

	_adaptee.start();
	setState(State::RUNNABLE);
	_condition.notify_one(Condition::Strategy::RELAXED);
	return true;
}

template <typename _Adaptee>
void SpinAdapter<_Adaptee>::Structure::stop()
{
	std::lock_guard lock(_threadMutex);
	auto state = setState(State::FINAL);
	if (state == State::FINAL) return;

	_condition.notify_one(Condition::Strategy::RELAXED);

	if (state == State::RUNNING)
		_condition.wait([this]
			{ return not _condition; });

	_adaptee.stop();
}

template <typename _Adaptee>
void SpinAdapter<_Adaptee>::Structure::execute()
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

	while (getState() == State::RUNNING)
		_adaptee.execute();

	_condition.exit();
}

template <typename _Adaptee>
void SpinAdapter<_Adaptee>::move(SpinAdapter& _left, \
	SpinAdapter&& _right) noexcept
{
	_left._master = _right._master;
	_right._master = false;
	_left._data = std::move(_right._data);
}

template <typename _Adaptee>
SpinAdapter<_Adaptee>::SpinAdapter(SpinAdapter&& _another)
{
	std::lock_guard lock(_another._mutex);
	move(*this, std::forward<SpinAdapter>(_another));
}

template <typename _Adaptee>
auto SpinAdapter<_Adaptee>::operator=(const SpinAdapter& _another) \
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

template <typename _Adaptee>
auto SpinAdapter<_Adaptee>::operator=(SpinAdapter&& _another) \
-> SpinAdapter&
{
	if (&_another != this)
	{
		std::lock_guard thisLock(this->_mutex);
		stop(this->_master);

		std::lock_guard anotherLock(_another._mutex);
		move(*this, std::forward<SpinAdapter>(_another));
	}
	return *this;
}

template <typename _Adaptee>
bool SpinAdapter<_Adaptee>::start()
{
	auto data = load();
	return data ? \
		data->start() : false;
}

ETERFREE_SPACE_END
