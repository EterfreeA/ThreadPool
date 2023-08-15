/*
* 文件名称：TimeoutQueue.hpp
* 语言标准：C++20
* 
* 创建日期：2022年01月28日
* 更新日期：2023年02月04日
* 
* 摘要
* 1.定义超时队列类模板TimeoutQueue，按照时间因子对元素排序，支持自定义时间因子。
* 2.可选指定容量和动态调整容量，若容量为零则无数量限制。
* 3.提供放入、取出、清空等方法。
* 4.支持根据时间因子批量取出超时元素，以及取出所有元素。
* 
* 作者：许聪
* 邮箱：solifree@qq.com
* 
* 版本：v1.0.2
* 变化
* v1.0.2
* 1.删除索引，取消映射功能。
*/

#pragma once

#include <optional>
#include <utility>
#include <map>
#include <vector>

#include "Core/Common.hpp"

ETERFREE_SPACE_BEGIN

template <typename _TimeType, typename _Element>
class TimeoutQueue final
{
public:
	using TimeType = _TimeType;
	using Element = _Element;

	using Vector = std::vector<Element>;
	using SizeType = Vector::size_type;

private:
	using QueueType = std::multimap<TimeType, Element>;

private:
	SizeType _capacity;
	QueueType _queue;

public:
	TimeoutQueue(decltype(_capacity) _capacity = 0) : \
		_capacity(_capacity) {}

	auto capacity() const noexcept { return _capacity; }
	void reserve(decltype(_capacity) _capacity) noexcept
	{
		this->_capacity = _capacity;
	}

	bool empty() const noexcept { return _queue.empty(); }
	auto size() const noexcept { return _queue.size(); }

	bool push(TimeType _time, const Element& _element);
	bool push(TimeType _time, Element&& _element);

	bool pop(TimeType _time, Vector& _vector);
	bool pop(Vector& _vector);

	void clear() noexcept
	{
		_queue.clear();
	}
};

template <typename _TimeType, typename _Element>
bool TimeoutQueue<_TimeType, _Element>::push(TimeType _time, \
	const Element& _element)
{
	if (_capacity > 0 and size() >= _capacity) return false;

	_queue.emplace(_time, _element);
	return true;
}

template <typename _TimeType, typename _Element>
bool TimeoutQueue<_TimeType, _Element>::push(TimeType _time, \
	Element&& _element)
{
	if (_capacity > 0 and size() >= _capacity) return false;

	_queue.emplace(_time, std::forward<Element>(_element));
	return true;
}

template <typename _TimeType, typename _Element>
bool TimeoutQueue<_TimeType, _Element>::pop(TimeType _time, Vector& _vector)
{
	auto size = _vector.size();
	for (auto iterator = _queue.begin(), end = _queue.upper_bound(_time); \
		iterator != end; iterator = _queue.erase(iterator))
		_vector.push_back(std::move(iterator->second));
	return _vector.size() > size;
}

template <typename _TimeType, typename _Element>
bool TimeoutQueue<_TimeType, _Element>::pop(Vector& _vector)
{
	if (empty()) return false;

	_vector.reserve(_vector.size() + size());
	for (auto& [_, element] : _queue)
		_vector.push_back(std::move(element));

	clear();
	return true;
}

ETERFREE_SPACE_END
