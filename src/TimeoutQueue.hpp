/*
* 文件名称：TimeoutQueue.hpp
* 语言标准：C++20
* 
* 创建日期：2022年01月28日
* 更新日期：2023年01月13日
* 
* 摘要
* 1.定义超时队列类模板TimeoutQueue，按照时间因子对元素排序，支持自定义时间因子。
* 2.可选指定容量和动态调整容量，若容量为零则无数量限制。
* 3.提供查找、放入、取出、清空等方法。
* 4.支持根据索引放入与取出单元素。
* 5.支持指定时间因子批量取出超时元素，支持取出所有元素。
* 
* 作者：许聪
* 邮箱：solifree@qq.com
* 
* 版本：v1.0.2
* 变化
* v1.0.2
* 1.新增查找方法。
*/

#pragma once

#include <utility>
#include <optional>
#include <vector>
#include <map>

#include "Core.hpp"

ETERFREE_SPACE_BEGIN

template <typename _IndexType, typename _ElementType, typename _TimeType>
class TimeoutQueue
{
public:
	using IndexType = _IndexType;
	using ElementType = _ElementType;
	using TimeType = _TimeType;

	using PairType = std::pair<ElementType, TimeType>;
	using VectorType = std::vector<std::pair<IndexType, ElementType>>;
	using SizeType = VectorType::size_type;

private:
	using QueueType = std::multimap<TimeType, IndexType>;
	using PoolType = std::map<IndexType, PairType>;

private:
	SizeType _capacity;
	QueueType _queue;
	PoolType _pool;

private:
	void erase(TimeType _time, const IndexType& _index);

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

	bool exist(const IndexType& _index) const
	{
		return _pool.contains(_index);
	}

	const PairType* find(const IndexType& _index) const;

	bool push(const IndexType& _index, \
		const ElementType& _element, TimeType _time);
	bool push(const IndexType& _index, \
		ElementType&& _element, TimeType _time);

	bool pop(const IndexType& _index, ElementType& _element);
	std::optional<ElementType> pop(const IndexType& _index);

	bool pop(TimeType _time, VectorType& _vector);
	bool pop(VectorType& _vector);

	void clear() noexcept
	{
		_queue.clear();
		_pool.clear();
	}
};

template <typename _IndexType, typename _ElementType, typename _TimeType>
void TimeoutQueue<_IndexType, _ElementType, _TimeType>::erase(TimeType _time, \
	const IndexType& _index)
{
	auto iterator = _queue.lower_bound(_time);
	if (iterator == _queue.end()) return;

	for (auto end = _queue.upper_bound(_time); iterator != end; ++iterator)
		if (iterator->second == _index)
		{
			_queue.erase(iterator);
			break;
		}
}

template <typename _IndexType, typename _ElementType, typename _TimeType>
auto TimeoutQueue<_IndexType, _ElementType, _TimeType>::find(const IndexType& _index) const \
-> const PairType*
{
	auto iterator = _pool.find(_index);
	return iterator != _pool.end() ? &iterator->second : nullptr;
}

template <typename _IndexType, typename _ElementType, typename _TimeType>
bool TimeoutQueue<_IndexType, _ElementType, _TimeType>::push(const IndexType& _index, \
	const ElementType& _element, TimeType _time)
{
	if (_capacity > 0 and size() >= _capacity) return false;
	if (exist(_index)) return false;

	_queue.emplace(_time, _index);
	_pool.emplace(_index, std::make_pair(_element, _time));
	return true;
}

template <typename _IndexType, typename _ElementType, typename _TimeType>
bool TimeoutQueue<_IndexType, _ElementType, _TimeType>::push(const IndexType& _index, \
	ElementType&& _element, TimeType _time)
{
	if (_capacity > 0 and size() >= _capacity) return false;
	if (exist(_index)) return false;

	_queue.emplace(_time, _index);
	_pool.emplace(_index, std::make_pair(std::forward<ElementType>(_element), _time));
	return true;
}

template <typename _IndexType, typename _ElementType, typename _TimeType>
bool TimeoutQueue<_IndexType, _ElementType, _TimeType>::pop(const IndexType& _index, \
	ElementType& _element)
{
	auto iterator = _pool.find(_index);
	if (iterator == _pool.end()) return false;

	auto& [element, time] = iterator->second;
	_element = std::move(element);

	erase(time, _index);
	_pool.erase(iterator);
	return true;
}

template <typename _IndexType, typename _ElementType, typename _TimeType>
auto TimeoutQueue<_IndexType, _ElementType, _TimeType>::pop(const IndexType& _index) \
-> std::optional<ElementType>
{
	auto iterator = _pool.find(_index);
	if (iterator == _pool.end()) return std::nullopt;

	auto& [element, time] = iterator->second;
	std::optional result = std::move(element);

	erase(time, _index);
	_pool.erase(iterator);
	return result;
}

template <typename _IndexType, typename _ElementType, typename _TimeType>
bool TimeoutQueue<_IndexType, _ElementType, _TimeType>::pop(TimeType _time, VectorType& _vector)
{
	bool result = false;
	for (auto iterator = _queue.begin(), end = _queue.upper_bound(_time); \
		iterator != end; iterator = _queue.erase(iterator))
	{
		const auto& index = iterator->second;
		if (auto iterator = _pool.find(index); iterator != _pool.end())
		{
			_vector.emplace_back(iterator->first, std::move(iterator->second.first));
			_pool.erase(iterator);
			result = true;
		}
	}
	return result;
}

template <typename _IndexType, typename _ElementType, typename _TimeType>
bool TimeoutQueue<_IndexType, _ElementType, _TimeType>::pop(VectorType& _vector)
{
	if (empty()) return false;

	_vector.reserve(_vector.size() + size());
	for (const auto& [_, index] : _queue)
		if (auto iterator = _pool.find(index); iterator != _pool.end())
			_vector.emplace_back(iterator->first, std::move(iterator->second.first));

	clear();
	return true;
}

ETERFREE_SPACE_END
