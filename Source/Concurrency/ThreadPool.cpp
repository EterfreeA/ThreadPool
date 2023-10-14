#include "ThreadPool.h"
#include "Thread.h"
#include "Core/Condition.hpp"
#include "Core/Logger.h"

#include <utility>
#include <cstdint>
#include <exception>
#include <list>
#include <thread>

ETERFREE_SPACE_BEGIN

// 生成原子的设置函数
#define SET_ATOMIC(SizeType, Arithmetic, functor, field) \
SizeType functor(SizeType _size, Arithmetic _arithmetic) noexcept \
{ \
	constexpr auto MEMORY_ORDER = std::memory_order::relaxed; \
	switch (_arithmetic) \
	{ \
	case Arithmetic::REPLACE: \
		return field.exchange(_size, MEMORY_ORDER); \
	case Arithmetic::INCREASE: \
		return field.fetch_add(_size, MEMORY_ORDER); \
	case Arithmetic::DECREASE: \
		return field.fetch_sub(_size, MEMORY_ORDER); \
	default: \
		return field.load(MEMORY_ORDER); \
	} \
}

// 线程池数据结构体
struct ThreadPool::Structure
{
	// 算术枚举
	enum class Arithmetic : std::uint8_t
	{
		REPLACE,	// 替换
		INCREASE,	// 自增
		DECREASE	// 自减
	};

	using Condition = Condition<>;

	using Notify = TaskManager::ThreadNotify;
	using FetchType = Thread::FetchType;
	using ReplyType = Thread::ReplyType;

	std::atomic_bool _valid;			// 线程有效性
	Condition _condition;				// 强化条件变量
	std::thread _thread;				// 守护线程
	std::list<Thread> _threadTable;		// 线程表

	std::atomic<SizeType> _capacity;	// 线程池容量
	std::atomic<SizeType> _totalSize;	// 总线程数量
	std::atomic<SizeType> _idleSize;	// 闲置线程数量

	TaskManager _taskManager;			// 任务管理器

	Notify _notify;						// 通知函数子
	FetchType _fetch;					// 获取函数子
	ReplyType _reply;					// 回复函数子

	// 守护线程是否有效
	bool isValid() const noexcept
	{
		return _valid.load(std::memory_order::relaxed);
	}

	// 设置有效性
	void setValid(bool _valid) noexcept
	{
		this->_valid.store(_valid, \
			std::memory_order::relaxed);
	}

	// 获取线程池容量
	auto getCapacity() const noexcept
	{
		return _capacity.load(std::memory_order::relaxed);
	}

	// 设置线程池容量
	void setCapacity(SizeType _capacity, bool _notified = false);

	// 获取总线程数量
	auto getTotalSize() const noexcept
	{
		return _totalSize.load(std::memory_order::relaxed);
	}

	// 设置总线程数量
	SET_ATOMIC(SizeType, Arithmetic, setTotalSize, _totalSize);

	// 获取闲置线程数量
	auto getIdleSize() const noexcept
	{
		return _idleSize.load(std::memory_order::relaxed);
	}

	// 设置闲置线程数量
	SET_ATOMIC(SizeType, Arithmetic, setIdleSize, _idleSize);

	// 任务管理器是否有效
	bool isValidManager() const
	{
		return _taskManager.valid();
	}

	// 任务管理器是否为空
	bool isEmptyManager() const noexcept
	{
		return _taskManager.empty();
	}
};

#undef SET_ATOMIC

// 设置线程池容量
void ThreadPool::Structure::setCapacity(SizeType _capacity, \
	bool _notified)
{
	auto capacity = this->_capacity.exchange(_capacity, \
		std::memory_order::relaxed);
	if (_notified and capacity != _capacity)
		_condition.notify_one(Condition::Strategy::RELAXED);
}

// 获取线程池容量
auto ThreadPool::Proxy::getCapacity() const noexcept \
-> SizeType
{
	return _data ? _data->getCapacity() : 0;
}

// 设置线程池容量
bool ThreadPool::Proxy::setCapacity(SizeType _capacity)
{
	if (_capacity <= 0 or not _data) return false;

	_data->setCapacity(_capacity, true);
	return true;
}

// 获取总线程数量
auto ThreadPool::Proxy::getTotalSize() const noexcept \
-> SizeType
{
	return _data ? _data->getTotalSize() : 0;
}

// 获取闲置线程数量
auto ThreadPool::Proxy::getIdleSize() const noexcept \
-> SizeType
{
	return _data ? _data->getIdleSize() : 0;
}

// 获取任务管理器
auto ThreadPool::Proxy::getTaskManager() const \
-> TaskManager*
{
	return _data ? &_data->_taskManager : nullptr;
}

// 创建线程池
void ThreadPool::create(DataType&& _data, SizeType _capacity)
{
	using Arithmetic = Structure::Arithmetic;

	// 保证线程池容量合法
	_capacity = _capacity > 0 ? _capacity : 1;

	// 移除线程表中多余线程
	auto size = _data->_threadTable.size();
	for (auto iterator = _data->_threadTable.cbegin(); \
		iterator != _data->_threadTable.cend() \
		and size > _capacity; \
		iterator = _data->_threadTable.erase(iterator), --size);

	// 初始化已有线程
	for (auto& thread : _data->_threadTable)
	{
		thread.create();
		thread.configure(_data->_fetch, _data->_reply);
	}

	// 新增线程并放入线程表
	for (; size < _capacity; ++size)
	{
		Thread thread;
		thread.configure(_data->_fetch, _data->_reply);
		_data->_threadTable.push_back(std::move(thread));
	}

	_data->setCapacity(_capacity); // 设置线程池容量
	_data->setTotalSize(_capacity, Arithmetic::REPLACE); // 设置总线程数量
	_data->setIdleSize(_capacity, Arithmetic::REPLACE); // 设置闲置线程数量

	// 守护线程设为有效
	_data->setValid(true);

	// 创建std::thread对象，即守护线程，以_data为参数，执行函数execute
	_data->_thread = std::thread(execute, _data);
}

// 销毁线程池
void ThreadPool::destroy(DataType&& _data)
{
	using Arithmetic = Structure::Arithmetic;
	using Strategy = Structure::Condition::Strategy;

	// 避免重复销毁
	if (not _data->isValid()) return;

	// 守护线程设为无效
	_data->setValid(false);

	// 通知守护线程退出
	_data->_condition.notify_all(Strategy::RELAXED);

	// 分离守护线程
	//_data->_thread.detach();

	// 挂起直到守护线程退出
	if (_data->_thread.joinable())
		_data->_thread.join();

	_data->setCapacity(0); // 设置线程池容量
	_data->setTotalSize(0, Arithmetic::REPLACE); // 设置总线程数量
	_data->setIdleSize(0, Arithmetic::REPLACE); // 设置闲置线程数量
}

// 调整线程数量
auto ThreadPool::adjust(DataType& _data) -> SizeType
{
	using Arithmetic = Structure::Arithmetic;

	auto size = _data->getTotalSize();
	auto capacity = _data->getCapacity();

	// 1.删减线程
	if (size >= capacity) return size - capacity;

	// 2.增加线程
	size = capacity - size;

	// 添加线程至线程表
	for (decltype(size) index = 0; index < size; ++index)
	{
		Thread thread;
		thread.configure(_data->_fetch, _data->_reply);
		_data->_threadTable.push_back(std::move(thread));
	}

	// 增加总线程数量
	_data->setTotalSize(size, Arithmetic::INCREASE);

	// 增加闲置线程数量
	_data->setIdleSize(size, Arithmetic::INCREASE);
	return 0;
}

// 守护线程主函数
void ThreadPool::execute(DataType _data)
{
	using Arithmetic = Structure::Arithmetic;

	/*
	 * 条件变量的谓词，不必等待通知的条件
	 * 1.在守护线程有效的情况下：
	 *     a.任务管理器非空并且存在闲置线程。
	 *     b.任务管理器非空并且需要增加线程。
	 *     c.存在闲置线程并且需要删减线程。
	 * 2.在守护线程无效的情况下：
	 *     a.任务管理器非空并且存在闲置线程
	 *     b.所有线程闲置
	 */
	auto predicate = [&_data]
	{
		bool empty = _data->isEmptyManager();

		if (_data->isValid())
		{
			bool idle = _data->getIdleSize() > 0;
			auto size = _data->getTotalSize();
			auto capacity = _data->getCapacity();
			return not empty \
				and (idle or size < capacity) \
				or idle and size > capacity;
		}
		else
		{
			auto size = _data->getIdleSize();
			auto capacity = _data->getTotalSize();
			bool idle = size > 0;
			return not empty and idle \
				or size >= capacity;
		}
	};

	// 若谓词非真，自动解锁互斥元，阻塞守护线程，直至通知激活，再次锁定互斥元
	_data->_condition.wait(predicate);

	/*
	 * 守护线程退出条件
	 * 1.守护线程无效
	 * 2.任务管理器无效
	 * 3.所有线程闲置
	 */
	while (_data->isValid() or _data->isValidManager() \
		or _data->getIdleSize() < _data->getTotalSize())
	{
		// 调整线程数量
		auto size = adjust(_data);

		// 遍历线程表，访问闲置线程
		for (auto iterator = _data->_threadTable.begin(); \
			iterator != _data->_threadTable.end() \
			and _data->getIdleSize() > 0;)
		{
			// 若线程处于闲置状态
			if (auto& thread = *iterator; thread.idle())
			{
				// 若通知线程执行任务成功，则减少闲置线程数量
				if (thread.notify())
					_data->setIdleSize(1, Arithmetic::DECREASE);
				// 删减线程
				else if (size > 0)
				{
					iterator = _data->_threadTable.erase(iterator);
					_data->setIdleSize(1, Arithmetic::DECREASE);
					_data->setTotalSize(1, Arithmetic::DECREASE);
					--size;
					continue;
				}
			}
			++iterator;
		}

		// 根据谓词真假，决定是否阻塞守护线程
		_data->_condition.wait(predicate);
	}

	// 销毁线程
	for (auto& thread : _data->_threadTable)
		thread.destroy();
}

// 获取支持的并发线程数量
auto ThreadPool::getConcurrency() noexcept -> SizeType
{
	auto concurrency = std::thread::hardware_concurrency();
	return concurrency > 0 ? concurrency : 1;
}

/*
 * 默认构造函数
 *
 * 若先以运算符new创建实例，再交由共享指针std::shared_ptr托管，
 * 则至少二次分配内存，先为实例分配内存，再为共享指针的控制块分配内存。
 * 而std::make_shared典型地仅分配一次内存，实例内存和控制块内存连续。
 */
ThreadPool::ThreadPool(SizeType _capacity) : \
	_atomic(std::make_shared<Structure>())
{
	using Strategy = Structure::Condition::Strategy;
	using TaskType = TaskManager::TaskType;

	// 加载非原子数据
	auto data = load();

	// 定义通知函数子
	data->_notify = [_data = std::weak_ptr(data)]
	{
		if (auto data = _data.lock())
			data->_condition.notify_one(Strategy::RELAXED);
	};

	// 定义获取函数子
	data->_fetch = [_data = std::weak_ptr(data)](TaskType& _task)
	{
		auto data = _data.lock();
		return data ? data->_taskManager.take(_task) : false;
	};

	// 定义回复函数子
	data->_reply = [_data = std::weak_ptr(data)](Thread::ThreadID _id, bool _idle)
	{
		// 线程并非闲置状态
		if (not _idle) return;

		// 若在增加之前，无闲置线程，或者在增加之后，所有线程闲置，则通知守护线程
		if (auto data = _data.lock(); data \
			and (data->setIdleSize(1, Structure::Arithmetic::INCREASE) == 0 \
			or data->getIdleSize() >= data->getTotalSize()))
			data->_condition.notify_one(Strategy::RELAXED);
	};

	// 配置通知函数子
	data->_taskManager.configure(data->_notify);

	// 创建线程池
	create(std::move(data), _capacity);
}

// 默认析构函数
ThreadPool::~ThreadPool() noexcept
{
	// 数据非空才进行销毁，以支持移动语义
	if (auto data = exchange(_atomic, nullptr))
	{
		try
		{
			destroy(std::move(data));
		}
		catch (std::exception& exception)
		{
			Logger::output(Logger::Level::ERROR, \
				std::source_location::current(), \
				exception);
		}
	}
}

// 默认移动赋值运算符函数
auto ThreadPool::operator=(ThreadPool&& _another) noexcept \
-> ThreadPool&
{
	if (&_another != this)
	{
		auto data = exchange(_another._atomic, nullptr);
		if (data = exchange(this->_atomic, data))
		{
			try
			{
				destroy(std::move(data));
			}
			catch (std::exception& exception)
			{
				Logger::output(Logger::Level::ERROR, \
					std::source_location::current(), \
					exception);
			}
		}
	}
	return *this;
}

// 获取线程池容量
auto ThreadPool::getCapacity() const noexcept \
-> SizeType
{
	auto data = load();
	return data ? data->getCapacity() : 0;
}

// 设置线程池容量
bool ThreadPool::setCapacity(SizeType _capacity)
{
	if (_capacity > 0)
		if (auto data = load())
		{
			data->setCapacity(_capacity, true);
			return true;
		}
	return false;
}

// 获取总线程数量
auto ThreadPool::getTotalSize() const noexcept \
-> SizeType
{
	auto data = load();
	return data ? data->getTotalSize() : 0;
}

// 获取闲置线程数量
auto ThreadPool::getIdleSize() const noexcept \
-> SizeType
{
	auto data = load();
	return data ? data->getIdleSize() : 0;
}

// 获取任务管理器
auto ThreadPool::getTaskManager() const -> TaskManager*
{
	auto data = load();
	return data ? &data->_taskManager : nullptr;
}

// 获取代理
auto ThreadPool::getProxy() const noexcept \
-> Proxy
{
	return load();
}

ETERFREE_SPACE_END
