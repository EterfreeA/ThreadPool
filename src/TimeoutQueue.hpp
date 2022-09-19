#pragma once

#include <utility>
#include <optional>
#include <vector>
#include <map>

template <typename _IndexType, typename _ElementType, typename _TimeType>
class TimeoutQueue
{
public:
	using IndexType = _IndexType;
	using ElementType = _ElementType;
	using TimeType = _TimeType;

	using PairType = std::pair<IndexType, ElementType>;
	using VectorType = std::vector<PairType>;
	using SizeType = VectorType::size_type;

private:
	using QueueType = std::multimap<TimeType, IndexType>;
	using PoolType = std::map<IndexType, std::pair<ElementType, TimeType>>;

private:
	SizeType _capacity;
	QueueType _queue;
	PoolType _pool;

private:
	void erase(TimeType _time, const IndexType& _index);

public:
	TimeoutQueue(decltype(_capacity) _capacity = 0)
		: _capacity(_capacity) {}

	auto capacity() const noexcept { return _capacity; }
	void reserve(decltype(_capacity) _capacity) noexcept
	{
		this->_capacity = _capacity;
	}

	auto size() const noexcept { return _queue.size(); }
	bool empty() const noexcept { return _queue.empty(); }

	bool push(const IndexType& _index, \
		const ElementType& _element, TimeType _time);
	bool push(const IndexType& _index, \
		ElementType&& _element, TimeType _time);

	bool pop(const IndexType& _index, ElementType& _element);
	std::optional<ElementType> pop(const IndexType& _index);

	bool pop(TimeType _time, VectorType& _vector);
	bool pop(VectorType& _vector);

	void clear() noexcept;
};

template <typename _IndexType, typename _ElementType, typename _TimeType>
void TimeoutQueue<_IndexType, _ElementType, _TimeType>::erase(TimeType _time, const IndexType& _index)
{
	auto iterator = _queue.lower_bound(_time);
	if (iterator == _queue.end()) return;

	auto end = _queue.upper_bound(_time);
	for (; iterator != end; ++iterator)
		if (iterator->second == _index)
		{
			_queue.erase(iterator);
			break;
		}
}

template <typename _IndexType, typename _ElementType, typename _TimeType>
bool TimeoutQueue<_IndexType, _ElementType, _TimeType>::push(const IndexType& _index, \
	const ElementType& _element, TimeType _time)
{
	if (_capacity > 0 && _queue.size() >= _capacity) return false;

	_queue.emplace(_time, _index);
	_pool.emplace(_index, std::make_pair(_element, _time));
	return true;
}

template <typename _IndexType, typename _ElementType, typename _TimeType>
bool TimeoutQueue<_IndexType, _ElementType, _TimeType>::push(const IndexType& _index, \
	ElementType&& _element, TimeType _time)
{
	if (_capacity > 0 && _queue.size() >= _capacity) return false;

	_queue.emplace(_time, _index);
	_pool.emplace(_index, std::make_pair(std::forward<ElementType>(_element), _time));
	return true;
}

template <typename _IndexType, typename _ElementType, typename _TimeType>
bool TimeoutQueue<_IndexType, _ElementType, _TimeType>::pop(const IndexType& _index, ElementType& _element)
{
	auto iterator = _pool.find(_index);
	if (iterator == _pool.end()) return false;

	const auto& [element, time] = iterator->second;
	_element = element;

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

	const auto& [element, time] = iterator->second;
	std::optional result = element;

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
			_vector.emplace_back(iterator->first, iterator->second.first);
			_pool.erase(iterator);
			result = true;
		}
	}
	return result;
}

template <typename _IndexType, typename _ElementType, typename _TimeType>
bool TimeoutQueue<_IndexType, _ElementType, _TimeType>::pop(VectorType& _vector)
{
	if (_queue.empty()) return false;

	_vector.reserve(_vector.size() + _queue.size());
	for (const auto& [_, index] : _queue)
		if (auto iterator = _pool.find(index); iterator != _pool.end())
			_vector.emplace_back(iterator->first, iterator->second.first);

	clear();
	return true;
}

template <typename _IndexType, typename _ElementType, typename _TimeType>
void TimeoutQueue<_IndexType, _ElementType, _TimeType>::clear() noexcept
{
	_queue.clear();
	_pool.clear();
}
