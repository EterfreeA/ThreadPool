/*
* 文件名称：SpinAdapter.h
* 语言标准：C++20
*
* 创建日期：2023年01月22日
* 更新日期：2023年09月02日
*
* 摘要
* 1.定义自旋适配者抽象类SpinAdaptee，而自旋适配器类SpinAdapter定义于此文件，实现于SpinAdapter.cpp。
* 2.自旋适配者声明事件接口，统称为适配者接口，包括开启、停止、执行。
* 3.无论单线程还是多线程环境，自旋适配器确保适配者接口的执行顺序。
* 4.自旋适配器确保接口的线程安全性，支持复制语义和移动语义，不过只有一个实例拥有析构停止权，即析构自动处理停止事件。
*   复制语义用于共享适配者，而不会共享析构停止权；移动语义用于移交适配者和析构停止权。
*
* 作者：许聪
* 邮箱：solifree@qq.com
*
* 版本：v1.2.0
* 变化
* v1.1.0
* 1.自旋适配器不再引用适配者，但持有指向适配者的共享指针，用以灵活管理适配者生命周期。
* v1.2.0
* 1.自旋适配器不再使用隐式接口，除继承自旋适配者的类之外，无法适配其他仅声明适配者接口的类，不过可以降低编译依存性。
*/

#pragma once

#include <memory>
#include <mutex>

#include "Common.hpp"

ETERFREE_SPACE_BEGIN

class SpinAdaptee;

class SpinAdapter final
{
public:
	struct Structure;

private:
	using Adaptee = std::shared_ptr<SpinAdaptee>;
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

	void stop(bool _master);

public:
	SpinAdapter(const Adaptee& _adaptee);

	SpinAdapter(Adaptee&& _adaptee);

	SpinAdapter(const SpinAdapter& _another) : \
		_master(false), _data(_another.load()) {}

	SpinAdapter(SpinAdapter&& _another) noexcept;

	~SpinAdapter() noexcept;

	SpinAdapter& operator=(const SpinAdapter& _adapter);

	SpinAdapter& operator=(SpinAdapter&& _adapter) noexcept;

	void operator()();

	bool start();

	void stop();
};

class SpinAdaptee : \
	public std::enable_shared_from_this<SpinAdaptee>
{
	friend struct SpinAdapter::Structure;

private:
	virtual void start() = 0;

	virtual void stop() = 0;

	virtual void execute() = 0;

public:
	virtual ~SpinAdaptee() noexcept {}
};

ETERFREE_SPACE_END
