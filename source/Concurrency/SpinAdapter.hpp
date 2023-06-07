/*
* 文件名称：SpinAdapter.hpp
* 语言标准：C++20
*
* 创建日期：2023年01月22日
* 更新日期：2023年04月06日
*
* 摘要
* 1.定义自旋适配器类模板SpinAdapter与自旋适配者抽象类SpinAdaptee。
* 2.自旋适配者声明事件接口，统称为适配者接口，包括开启、停止、执行。
* 3.自旋适配器使用隐式接口，可以适配任何声明适配者接口的类，无关乎是否继承自旋适配者。
* 4.对于多线程并发，自旋适配器确保适配者接口的执行顺序。
* 5.自旋适配器确保接口的线程安全性，支持复制语义和移动语义，不过只有一个实例拥有析构停止权，即析构自动处理停止事件。
*   复制语义用于共享适配者，而不会共享析构停止权；移动语义用于移交适配者和析构停止权。
*
* 作者：许聪
* 邮箱：solifree@qq.com
*
* 版本：v1.1.0
* 变化
* v1.1.0
* 1.自旋适配器不再引用适配者，但持有指向适配者的共享指针，用以灵活管理适配者生命周期。
*/

#pragma once

#include <utility>
#include <memory>
#include <cstdint>
#include <mutex>

#include "Core/Common.hpp"
#include "Condition.hpp"

ETERFREE_SPACE_BEGIN

class SpinAdaptee;

template <typename _SpinAdaptee = SpinAdaptee>
class SpinAdapter final
{
public:
	struct Structure;

private:
	using SpinAdaptee = std::shared_ptr<_SpinAdaptee>;
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
	SpinAdapter(const SpinAdaptee& _adaptee) : \
		_master(true), \
		_data(std::make_shared<Structure>(_adaptee)) {}

	SpinAdapter(SpinAdaptee&& _adaptee) : \
		_master(true), \
		_data(std::make_shared<Structure>(std::forward<SpinAdaptee>(_adaptee))) {}

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

class SpinAdaptee : \
	public std::enable_shared_from_this<SpinAdaptee>
{
	friend struct SpinAdapter<SpinAdaptee>::Structure;

private:
	virtual void start() = 0;

	virtual void stop() = 0;

	virtual void execute() = 0;

public:
	virtual ~SpinAdaptee() noexcept {}
};

template <typename _SpinAdaptee>
struct SpinAdapter<_SpinAdaptee>::Structure
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

	SpinAdaptee _adaptee;

	Structure(const SpinAdaptee& _adaptee) : \
		_adaptee(_adaptee), _state(State::INITIAL) {}

	Structure(SpinAdaptee&& _adaptee) : \
		_adaptee(std::forward<SpinAdaptee>(_adaptee)), \
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

template <typename _SpinAdaptee>
auto SpinAdapter<_SpinAdaptee>::Structure::setState(State _state)
{
	std::lock_guard lock(_stateMutex);
	auto state = this->_state;
	this->_state = _state;
	return state;
}

template <typename _SpinAdaptee>
bool SpinAdapter<_SpinAdaptee>::Structure::start()
{
	std::lock_guard lock(_threadMutex);
	if (getState() != State::INITIAL) return false;

	if (_adaptee) _adaptee->start();
	setState(State::RUNNABLE);

	_condition.notify_one(Condition::Strategy::RELAXED);
	return true;
}

template <typename _SpinAdaptee>
void SpinAdapter<_SpinAdaptee>::Structure::stop()
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

template <typename _SpinAdaptee>
void SpinAdapter<_SpinAdaptee>::Structure::execute()
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

	while (getState() == State::RUNNING && _adaptee)
		_adaptee->execute();

	_condition.exit();
}

template <typename _SpinAdaptee>
void SpinAdapter<_SpinAdaptee>::move(SpinAdapter& _left, \
	SpinAdapter&& _right) noexcept
{
	_left._master = _right._master;
	_right._master = false;
	_left._data = std::move(_right._data);
}

template <typename _SpinAdaptee>
SpinAdapter<_SpinAdaptee>::SpinAdapter(SpinAdapter&& _another)
{
	std::lock_guard lock(_another._mutex);
	move(*this, std::forward<SpinAdapter>(_another));
}

template <typename _SpinAdaptee>
auto SpinAdapter<_SpinAdaptee>::operator=(const SpinAdapter& _another) \
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

template <typename _SpinAdaptee>
auto SpinAdapter<_SpinAdaptee>::operator=(SpinAdapter&& _another) \
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

template <typename _SpinAdaptee>
bool SpinAdapter<_SpinAdaptee>::start()
{
	auto data = load();
	return data ? data->start() : false;
}

ETERFREE_SPACE_END
