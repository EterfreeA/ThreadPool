#include "TaskQueue.h"

ETERFREE_SPACE_BEGIN

// 过滤无效任务
//template <typename _QueueType>
//static auto filter(_QueueType& _queue)
//{
//	decltype(_queue.size()) size = 0;
//	std::erase_if(_queue, \
//		[&size](const _QueueType::value_type& _task) noexcept
//		{
//			if (not _task) return true;
//
//			++size;
//			return false;
//		});
//	return size;
//}

// 执行通知函数子
void TaskQueue::notify() const
{
	std::unique_lock lock(_mutex);
	auto notify = _notify;
	lock.unlock();

	if (notify) notify();
}

// 放入单任务
bool TaskQueue::push(const TaskType& _task)
{
	if (not _task) return false;

	auto result = _queue.push(_task);
	if (result and result.value() == 0)
		notify();
	return result.has_value();
}

// 放入单任务
bool TaskQueue::push(TaskType&& _task)
{
	if (not _task) return false;

	auto result = _queue.push(std::forward<TaskType>(_task));
	if (result and result.value() == 0)
		notify();
	return result.has_value();
}

// 批量放入任务
bool TaskQueue::put(QueueType& _queue)
{
	auto result = this->_queue.push(_queue);
	if (result and result.value() == 0)
		notify();
	return result.has_value();
}

// 批量放入任务
bool TaskQueue::put(QueueType&& _queue)
{
	auto result = this->_queue.push(std::forward<QueueType>(_queue));
	if (result and result.value() == 0)
		notify();
	return result.has_value();
}

ETERFREE_SPACE_END
