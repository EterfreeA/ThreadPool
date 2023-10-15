#include "TaskQueue.h"

ETERFREE_SPACE_BEGIN

// 过滤无效任务
//template <typename _ListType>
//static auto filter(_ListType& _list)
//{
//	decltype(_list.size()) size = 0;
//	std::erase_if(_list, \
//		[&size](const _ListType::value_type& _task) noexcept
//		{
//			if (not _task) return true;
//
//			++size;
//			return false;
//		});
//	return size;
//}

// 最小时间
auto TaskQueue::time() const \
-> std::optional<TimeType>
{
	std::lock_guard exitLock(_exitMutex);
	if (empty())
		return std::nullopt;

	if (not _exitTime.empty())
		return _exitTime.front();

	std::lock_guard entryLock(_entryMutex);
	return _entryTime.front();
}

// 取出单任务
bool TaskQueue::take(TaskType& _task)
{
	std::lock_guard lock(_exitMutex);
	if (empty()) return false;

	if (_exitTask.empty())
	{
		std::lock_guard lock(_entryMutex);
		_exitTask.swap(_entryTask);
		_exitTime.swap(_entryTime);
	}

	subtract(1);
	_task = std::move(_exitTask.front());
	_exitTask.pop_front();
	_exitTime.pop_front();
	return true;
}

// 执行通知函数子
void TaskQueue::notify() const
{
	std::unique_lock lock(_notifyMutex);
	auto notify = _notify;
	lock.unlock();

	if (notify) notify(index());
}

// 未达到队列容量限制
bool TaskQueue::valid(TaskList& _taskList) const noexcept
{
	auto capacity = this->capacity();
	if (capacity <= 0) return true;

	auto size = this->size();
	return size < capacity \
		and _taskList.size() <= capacity - size;
}

// 放入单任务
bool TaskQueue::push(const TaskType& _task)
{
	if (not _task) return false;

	std::unique_lock lock(_entryMutex);
	if (auto capacity = this->capacity(); \
		capacity > 0 and size() >= capacity)
		return false;

	_entryTask.push_back(_task);
	auto time = TimedTask::getSteadyTime();
	_entryTime.push_back(time);

	bool notifiable = add(1) == 0;
	lock.unlock();

	if (notifiable) notify();
	return true;
}

// 放入单任务
bool TaskQueue::push(TaskType&& _task)
{
	if (not _task) return false;

	std::unique_lock lock(_entryMutex);
	if (auto capacity = this->capacity(); \
		capacity > 0 and size() >= capacity)
		return false;

	_entryTask.push_back(std::forward<TaskType>(_task));
	_entryTime.push_back(TimedTask::getSteadyTime());

	bool notifiable = add(1) == 0;
	lock.unlock();

	if (notifiable) notify();
	return true;
}

// 批量放入任务
bool TaskQueue::put(TaskList& _taskList)
{
	auto size = _taskList.size();
	auto time = TimedTask::getSteadyTime();
	TimeList timeList(size, time);

	std::unique_lock lock(_entryMutex);
	if (not valid(_taskList))
		return false;

	_entryTask.splice(_entryTask.cend(), \
		_taskList);
	_entryTime.splice(_entryTime.cend(), \
		std::move(timeList));

	bool notifiable = add(size) == 0;
	lock.unlock();

	if (notifiable) notify();
	return true;
}

// 批量放入任务
bool TaskQueue::put(TaskList&& _taskList)
{
	auto size = _taskList.size();
	auto time = TimedTask::getSteadyTime();
	TimeList timeList(size, time);

	std::unique_lock lock(_entryMutex);
	if (not valid(_taskList))
		return false;

	_entryTask.splice(_entryTask.cend(), \
		std::forward<TaskList>(_taskList));
	_entryTime.splice(_entryTime.cend(), \
		std::move(timeList));

	bool notifiable = add(size) == 0;
	lock.unlock();

	if (notifiable) notify();
	return true;
}

// 批量取出任务
bool TaskQueue::take(TaskList& _taskList)
{
	std::lock_guard exitLock(_exitMutex);
	if (empty()) return false;

	_taskList.splice(_taskList.cend(), \
		_exitTask);
	_exitTime.clear();

	std::lock_guard entryLock(_entryMutex);
	_taskList.splice(_taskList.cend(), \
		_entryTask);
	_entryTime.clear();

	set(_size, 0);
	return true;
}

// 清空所有任务
auto TaskQueue::clear() -> SizeType
{
	std::scoped_lock lock(_exitMutex, \
		_entryMutex);

	_exitTask.clear();
	_exitTime.clear();

	_entryTask.clear();
	_entryTime.clear();

	return exchange(_size, 0);
}

ETERFREE_SPACE_END
