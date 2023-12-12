#include "TaskManager.h"
#include "Eterfree/Sequence/Sorter.hpp"

#include <map>
#include <atomic>
#include <mutex>
#include <shared_mutex>

CONCURRENCY_SPACE_BEGIN

struct TaskManager::Structure
{
	struct Record;

	using Atomic = std::atomic<SizeType>;

	using Mapper = std::map<IndexType, PoolType>;
	using Sorter = Sequence::Sorter<IndexType, Record>;

	mutable std::mutex _notifyMutex;
	ThreadNotify _threadNotify;
	TaskNotify _taskNotify;

	Atomic _size;

	mutable std::shared_mutex _poolMutex;
	Mapper _mapper;
	Sorter _sorter;

	static auto get(const Atomic& _atomic) noexcept
	{
		return _atomic.load(std::memory_order::relaxed);
	}

	static void set(Atomic& _atomic, SizeType _size) noexcept
	{
		_atomic.store(_size, std::memory_order::relaxed);
	}

	ThreadNotify get() const
	{
		std::lock_guard lock(_notifyMutex);
		return _threadNotify;
	}

	void set(const ThreadNotify& _notify)
	{
		std::lock_guard lock(_notifyMutex);
		_threadNotify = _notify;
	}

	void configure(const ThreadNotify& _notify);

	bool valid() const;

	bool empty() const noexcept
	{
		return get(_size) <= 0;
	}

	SizeType size() const;

	bool take(TaskType& _task);

	PoolType find(IndexType _index);

	bool insert(const PoolType& _pool);

	void remove(IndexType _index);

	void clear();
};

struct TaskManager::Structure::Record
{
	using TimeType = TaskPool::TimeType;

	IndexType _index;
	TimeType _time;

	explicit operator IndexType() const noexcept
	{
		return _index;
	}

	bool operator<(const Record& _record) const noexcept;
};

bool TaskManager::Structure::Record::operator<(const Record& _record) const noexcept
{
	return this->_time < _record._time \
		or this->_time == _record._time and this->_index < _record._index;
}

void TaskManager::Structure::configure(const ThreadNotify& _notify)
{
	std::unique_lock lock(_poolMutex);
	set(_notify);

	for (auto& [_, pool] : _mapper)
		pool->configure(_taskNotify);

	bool notifiable = not _sorter.empty();
	lock.unlock();

	if (notifiable and _notify) _notify();
}

bool TaskManager::Structure::valid() const
{
	std::shared_lock lock(_poolMutex);
	for (auto& [_, pool] : _mapper)
		if (pool->size() > 0) return true;
	return false;
}

auto TaskManager::Structure::size() const \
->SizeType
{
	SizeType size = 0;

	std::shared_lock lock(_poolMutex);
	for (auto& [_, pool] : _mapper)
		size += pool->size();
	return size;
}

bool TaskManager::Structure::take(TaskType& _task)
{
	std::lock_guard lock(_poolMutex);
	auto record = _sorter.front(true);
	if (not record) return false;

	auto index = record->_index;
	auto iterator = _mapper.find(index);
	if (iterator == _mapper.end()) return false;

	auto& pool = iterator->second;
	if (not pool->take(_task)) return false;

	if (auto time = pool->time())
		_sorter.update({ index, time.value() });
	else
	{
		_sorter.remove(index);
		set(_size, _sorter.size());
	}
	return true;
}

auto TaskManager::Structure::find(IndexType _index) \
-> PoolType
{
	std::shared_lock lock(_poolMutex);
	auto iterator = _mapper.find(_index);
	return iterator != _mapper.end() ? \
		iterator->second : nullptr;
}

bool TaskManager::Structure::insert(const PoolType& _pool)
{
	if (not _pool) return false;
	auto index = _pool->index();

	std::unique_lock lock(_poolMutex);
	if (auto iterator = _mapper.find(index); \
		iterator == _mapper.end())
		_mapper.emplace(index, _pool);
	else
	{
		iterator->second->configure(nullptr);
		iterator->second = _pool;
	}

	_pool->configure(_taskNotify);

	if (auto time = _pool->time())
	{
		_sorter.update({ index, time.value() });
		set(_size, _sorter.size());
	}
	lock.unlock();

	if (not _pool->empty())
		if (auto notify = get()) notify();
	return true;
}

void TaskManager::Structure::remove(IndexType _index)
{
	std::lock_guard lock(_poolMutex);
	if (auto iterator = _mapper.find(_index); \
		iterator != _mapper.end())
	{
		iterator->second->configure(nullptr);
		_mapper.erase(iterator);

		_sorter.remove(_index);
		set(_size, _sorter.size());
	}
}

void TaskManager::Structure::clear()
{
	std::lock_guard poolLock(_poolMutex);
	set(_size, 0);
	_sorter.clear();

	for (auto& [_, pool] : _mapper)
		pool->configure(nullptr);
	_mapper.clear();
}

TaskManager::TaskManager() : \
	_data(std::make_shared<Structure>())
{
	_data->_taskNotify = [_data = std::weak_ptr(_data)](IndexType _index)
	{
		if (auto data = _data.lock())
		{
			bool notifiable = false;

			std::unique_lock lock(data->_poolMutex);
			if (auto iterator = data->_mapper.find(_index); \
				iterator != data->_mapper.end())
				if (auto time = iterator->second->time())
				{
					data->_sorter.update({ _index, time.value() });
					data->set(data->_size, data->_sorter.size());
					notifiable = true;
				}
			lock.unlock();

			if (notifiable)
				if (auto notify = data->get()) notify();
		}
	};
}

void TaskManager::configure(const ThreadNotify& _notify)
{
	if (_data) _data->configure(_notify);
}

bool TaskManager::valid() const
{
	return _data ? \
		_data->valid() : false;
}

bool TaskManager::empty() const noexcept
{
	return _data ? _data->empty() : true;
}

auto TaskManager::size() const -> SizeType
{
	return _data ? _data->size() : 0;
}

bool TaskManager::take(TaskType& _task)
{
	return _data ? \
		_data->take(_task) : false;
}

auto TaskManager::find(IndexType _index) -> PoolType
{
	return _data ? _data->find(_index) : nullptr;
}

bool TaskManager::insert(const PoolType& _pool)
{
	return _data ? _data->insert(_pool) : false;
}

bool TaskManager::remove(IndexType _index)
{
	bool result = false;
	if (_data)
	{
		_data->remove(_index);
		result = true;
	}
	return result;
}

void TaskManager::clear()
{
	if (_data)
		_data->clear();
}

CONCURRENCY_SPACE_END
