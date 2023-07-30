/*
* 文件名称：Sorter.hpp
* 语言标准：C++20
* 
* 创建日期：2020年11月10日
* 更新日期：2023年05月25日
* 
* 摘要
* 1.结合无序映射与有序集合，封装通常版排序者类模板Sorter，以及共享指针版排序者类模板SharedSorter。
* 2.以无序映射关联ID和记录，以有序集合对记录排序，一次排序反复更新。
* 3.排序者类模板支持复制语义和移动语义，提供查询、更新、移除、清空、排名、序列化等方法。
* 4.通常版排序者的无序映射与有序结合会保存相同记录，即同一记录存在两个实例，因此通常版排序者适用于数据量小的记录。
* 5.共享指针版排序者的无序映射与有序结合共享相同记录，因此共享指针版排序者适用于数据量大的记录。
* 
* 作者：许聪
* 邮箱：solifree@qq.com
* 
* 版本：v1.0.4
* 变化
* v1.0.3
* 1.定义排序记录抽象类，排序记录可选继承抽象类，或者单独实现其接口亦可。
* v1.0.4
* 1.更新方法减少有序集合节点的销毁再创建操作。
*/

#pragma once

#include <utility>
#include <memory>
#include <vector>
#include <set>
#include <unordered_map>
#include <iterator>
#include <algorithm>

#include "Core/Common.hpp"

ETERFREE_SPACE_BEGIN

template <typename _IDType>
class SortedRecord
{
public:
	using IDType = _IDType;

public:
	virtual ~SortedRecord() noexcept {}

	virtual explicit operator IDType() const = 0;

	bool operator<(const SortedRecord& _another) const noexcept
	{
		return false;
	}
};

template <typename _IDType, typename _RecordType>
class Sorter final
{
public:
	using IDType = _IDType;
	using RecordType = _RecordType;

	using RecordList = std::vector<RecordType>;
	using SizeType = RecordList::size_type;

private:
	using IDMapper = std::unordered_map<IDType, RecordType>;
	using RecordSet = std::set<RecordType>;

private:
	IDMapper _idMapper;
	RecordSet _recordSet;

private:
	/*
	 * 根据指定方向遍历，获取指定ID的排名
	 * 若无指定ID，则返回0。
	 */
	template <typename _Iterator>
	auto rank(IDType _id, _Iterator _iterator) const;

	/*
	 * 从指定位置起，向指定方向遍历，获取指定数量的记录
	 * 倘若指定数量为零，默认获取所有记录，否则获取指定数量的记录。
	 */
	template <typename _Iterator>
	void get(RecordList& _recordList, SizeType _index, \
		SizeType _size, _Iterator _iterator) const;

public:
	// 是否为空
	bool empty() const noexcept
	{
		return _recordSet.empty();
	}

	// 获取记录数量
	auto size() const noexcept
	{
		return _recordSet.size();
	}

	// 是否存在指定记录
	bool exist(IDType _id) const
	{
		return _idMapper.contains(_id);
	}

	// 查找指定ID的原始记录
	const RecordType* find(IDType _id) const;

	/*
	 * 新增或者更新记录
	 * 倘若无指定记录，直接插入作为新记录，否则先删除旧记录，再插入新记录。
	 */
	void update(const RecordType& _record);

	// 移除单条记录，返回移除结果
	bool remove(IDType _id);

	// 清空数据
	void clear() noexcept
	{
		_idMapper.clear();
		_recordSet.clear();
	}

	// 获取首记录
	const RecordType* front(bool _forward = false) const noexcept;

	// 获取尾记录
	auto back(bool _forward = false) const noexcept
	{
		return front(not _forward);
	}

	/*
	 * 根据指定方向遍历，获取指定ID的排名
	 * 若无指定ID，则返回0。
	 */
	auto rank(IDType _id, bool _forward = false) const
	{
		return _forward ? \
			rank(_id, _recordSet.cbegin()) : rank(_id, _recordSet.crbegin());
	}

	/*
	 * 从指定位置起，向指定方向遍历，获取指定数量的记录
	 * 倘若指定数量为零，默认获取所有记录，否则获取指定数量的记录。
	 */
	bool get(RecordList& _recordList, SizeType _index = 0, \
		SizeType _size = 0, bool _forward = false) const;
};

template <typename _IDType, typename _RecordType>
class SharedSorter final
{
	struct Node;

public:
	using IDType = _IDType;
	using RecordType = _RecordType;

	using RecordList = std::vector<RecordType>;
	using SizeType = RecordList::size_type;

private:
	using NodeType = Node;
	using IDMapper = std::unordered_map<IDType, NodeType>;
	using PairType = IDMapper::value_type;
	using NodeSet = std::set<NodeType>;

private:
	IDMapper _idMapper;
	NodeSet _nodeSet;

private:
	/*
	 * 复制数据
	 * 复制所有记录，拷贝内存数据。
	 */
	void copy(const SharedSorter& _another);

	/*
	 * 根据指定方向遍历，获取指定ID的排名
	 * 若无指定ID，则返回0。
	 */
	template <typename _Iterator>
	auto rank(IDType _id, _Iterator _iterator) const;

	/*
	 * 从指定位置起，向指定方向遍历，获取指定数量的记录
	 * 倘若指定数量为零，默认获取所有记录，否则获取指定数量的记录。
	 */
	template <typename _Iterator>
	void get(RecordList& _recordList, SizeType _index, \
		SizeType _size, _Iterator _iterator) const;

public:
	SharedSorter() = default;

	SharedSorter(const SharedSorter& _another) : \
		_idMapper(_another._idMapper.bucket_count())
	{
		copy(_another);
	}

	SharedSorter(SharedSorter&&) = default;

	SharedSorter& operator=(const SharedSorter& _another);

	SharedSorter& operator=(SharedSorter&&) = default;

	// 是否为空
	bool empty() const noexcept
	{
		return _nodeSet.empty();
	}

	// 获取记录数量
	auto size() const noexcept
	{
		return _nodeSet.size();
	}

	// 是否存在指定记录
	bool exist(IDType _id) const
	{
		return _idMapper.contains(_id);
	}

	// 查找指定ID的原始记录
	std::shared_ptr<const RecordType> find(IDType _id) const;

	/*
	 * 新增或者更新记录
	 * 倘若无指定记录，直接插入作为新记录，否则先删除旧记录，再插入新记录。
	 */
	void update(const RecordType& _record);

	// 移除单条记录，返回移除结果
	bool remove(IDType _id);

	// 清空数据
	void clear() noexcept
	{
		_idMapper.clear();
		_nodeSet.clear();
	}

	// 获取首记录
	std::shared_ptr<const RecordType> front(bool _forward = false) const noexcept;

	// 获取尾记录
	auto back(bool _forward = false) const noexcept
	{
		return front(not _forward);
	}

	/*
	 * 根据指定方向遍历，获取指定ID的排名
	 * 若无指定ID，则返回0。
	 */
	auto rank(IDType _id, bool _forward = false) const
	{
		return _forward ? \
			rank(_id, _nodeSet.cbegin()) : rank(_id, _nodeSet.crbegin());
	}

	/*
	 * 从指定位置起，向指定方向遍历，获取指定数量的记录
	 * 倘若指定数量为零，默认获取所有记录，否则获取指定数量的记录。
	 */
	bool get(RecordList& _recordList, SizeType _index = 0, \
		SizeType _size = 0, bool _forward = false) const;

	/*
	 * 从指定位置起，向指定方向遍历，获取指定数量的记录
	 * 返回记录列表的共享指针，避免复制对象和拷贝内存，并且易于共享访问向量。
	 */
	std::shared_ptr<RecordList> get(SizeType _index = 0, \
		SizeType _size = 0, bool _forward = false) const;
};

template <typename _IDType, typename _RecordType>
struct SharedSorter<_IDType, _RecordType>::Node
{
	std::shared_ptr<RecordType> _record;

	Node() noexcept = default;
	Node(const RecordType& _record) : \
		_record(std::make_shared<RecordType>(_record)) {}

	bool operator<(const Node& _another) const noexcept
	{
		return *this->_record < *_another._record;
	}
};

// 获取指定ID的排名
template <typename _IDType, typename _RecordType>
template <typename _Iterator>
auto Sorter<_IDType, _RecordType>::rank(IDType _id, \
	_Iterator _iterator) const
{
	decltype(size()) ranking = 0;
	for (decltype(ranking) index = 0; \
		index < size(); ++index, ++_iterator)
		if (static_cast<IDType>(*_iterator) == _id)
		{
			ranking = index + 1;
			break;
		}
	return ranking;
}

// 获取指定数量的记录
template <typename _IDType, typename _RecordType>
template <typename _Iterator>
void Sorter<_IDType, _RecordType>::get(RecordList& _recordList, \
	SizeType _index, SizeType _size, _Iterator _iterator) const
{
	for (; _index > 0; --_index, ++_iterator);

	for (; _size > 0; --_size)
		_recordList.push_back(*_iterator++);
}

// 查找指定ID的原始记录
template <typename _IDType, typename _RecordType>
auto Sorter<_IDType, _RecordType>::find(IDType _id) const \
-> const RecordType*
{
	auto iterator = _idMapper.find(_id);
	return iterator != _idMapper.end() ? \
		&iterator->second : nullptr;
}

// 新增或者更新记录
template <typename _IDType, typename _RecordType>
void Sorter<_IDType, _RecordType>::update(const RecordType& _record)
{
	auto id = static_cast<IDType>(_record);
	if (auto iterator = _idMapper.find(id); \
		iterator == _idMapper.end())
	{
		_idMapper.emplace(id, _record);
		_recordSet.insert(_record);
	}
	else
	{
		auto node = _recordSet.extract(iterator->second);
		node.value() = iterator->second = _record;
		_recordSet.insert(std::move(node));
	}
}

// 移除单条记录
template <typename _IDType, typename _RecordType>
bool Sorter<_IDType, _RecordType>::remove(IDType _id)
{
	auto iterator = _idMapper.find(_id);
	if (iterator == _idMapper.end()) return false;

	_recordSet.erase(iterator->second);
	_idMapper.erase(iterator);
	return true;
}

// 获取首记录
template <typename _IDType, typename _RecordType>
auto Sorter<_IDType, _RecordType>::front(bool _forward) const noexcept \
-> const RecordType*
{
	if (empty()) return nullptr;
	return _forward ? &*_recordSet.cbegin() : &*_recordSet.crbegin();
}

// 获取指定数量的记录
template <typename _IDType, typename _RecordType>
bool Sorter<_IDType, _RecordType>::get(RecordList& _recordList, \
	SizeType _index, SizeType _size, bool _forward) const
{
	_recordList.clear();
	if (_index >= size()) return false;

	if (_size <= 0) _size = size() - _index;
	else _size = std::min(size() - _index, _size);
	_recordList.reserve(_size);

	if (_forward)
		get(_recordList, _index, _size, _recordSet.cbegin());
	else
		get(_recordList, _index, _size, _recordSet.crbegin());
	return true;
}

// 复制数据
template <typename _IDType, typename _RecordType>
void SharedSorter<_IDType, _RecordType>::copy(const SharedSorter& _another)
{
	std::transform(_another._idMapper.cbegin(), _another._idMapper.cend(), \
		std::inserter(this->_idMapper, this->_idMapper.begin()), \
		[this](const PairType& _pair)
		{
			NodeType node(*_pair.second._record);
			this->_nodeSet.insert(node);
			return std::make_pair(_pair.first, std::move(node));
		});
}

// 获取指定ID的排名
template <typename _IDType, typename _RecordType>
template <typename _Iterator>
auto SharedSorter<_IDType, _RecordType>::rank(IDType _id, \
	_Iterator _iterator) const
{
	decltype(size()) ranking = 0;
	for (decltype(ranking) index = 0; \
		index < size(); ++index, ++_iterator)
		if (static_cast<IDType>(*_iterator->_record) == _id)
		{
			ranking = index + 1;
			break;
		}
	return ranking;
}

// 获取指定数量的记录
template <typename _IDType, typename _RecordType>
template <typename _Iterator>
void SharedSorter<_IDType, _RecordType>::get(RecordList& _recordList, \
	SizeType _index, SizeType _size, _Iterator _iterator) const
{
	for (; _index > 0; --_index, ++_iterator);

	for (; _size > 0; --_size)
		_recordList.push_back(*(*_iterator++)._record);
}

template <typename _IDType, typename _RecordType>
auto SharedSorter<_IDType, _RecordType>::operator=(const SharedSorter& _another) \
-> SharedSorter&
{
	if (&_another != this)
	{
		clear();
		this->_idMapper.rehash(_another._idMapper.bucket_count());
		copy(_another);
	}
	return *this;
}

// 查找指定ID的原始记录
template <typename _IDType, typename _RecordType>
auto SharedSorter<_IDType, _RecordType>::find(IDType _id) const \
-> std::shared_ptr<const RecordType>
{
	auto iterator = _idMapper.find(_id);
	return iterator != _idMapper.end() ? \
		iterator->second._record : nullptr;
}

// 新增或者更新记录
template <typename _IDType, typename _RecordType>
void SharedSorter<_IDType, _RecordType>::update(const RecordType& _record)
{
	auto id = static_cast<IDType>(_record);
	if (auto iterator = _idMapper.find(id); iterator == _idMapper.end())
	{
		NodeType node(_record);
		_idMapper.emplace(id, node);
		_nodeSet.insert(std::move(node));
	}
	else
	{
		auto node = _nodeSet.extract(iterator->second);
		*node.value()._record = *iterator->second._record = _record;
		_nodeSet.insert(std::move(node));
	}
}

// 移除单条记录
template <typename _IDType, typename _RecordType>
bool SharedSorter<_IDType, _RecordType>::remove(IDType _id)
{
	auto iterator = _idMapper.find(_id);
	if (iterator == _idMapper.end()) return false;

	_nodeSet.erase(iterator->second);
	_idMapper.erase(iterator);
	return true;
}

// 获取首记录
template <typename _IDType, typename _RecordType>
auto SharedSorter<_IDType, _RecordType>::front(bool _forward) const noexcept \
-> std::shared_ptr<const RecordType>
{
	if (empty()) return nullptr;
	return _forward ? _nodeSet.cbegin()->_record : _nodeSet.crbegin()->_record;
}

// 获取指定数量的记录
template <typename _IDType, typename _RecordType>
bool SharedSorter<_IDType, _RecordType>::get(RecordList& _recordList, \
	SizeType _index, SizeType _size, bool _forward) const
{
	_recordList.clear();
	if (_index >= size()) return false;

	if (_size <= 0) _size = size() - _index;
	else _size = std::min(size() - _index, _size);
	_recordList.reserve(_size);

	if (_forward) get(_recordList, _index, _size, _nodeSet.cbegin());
	else get(_recordList, _index, _size, _nodeSet.crbegin());
	return true;
}

// 获取指定数量的记录
template <typename _IDType, typename _RecordType>
auto SharedSorter<_IDType, _RecordType>::get(SizeType _index, \
	SizeType _size, bool _forward) const -> std::shared_ptr<RecordList>
{
	if (_index >= size()) return nullptr;

	if (_size <= 0) _size = size() - _index;
	else _size = std::min(size() - _index, _size);

	auto recordList = std::make_shared<RecordList>(0);
	recordList->reserve(_size);

	if (_forward) get(*recordList, _index, _size, _nodeSet.cbegin());
	else get(*recordList, _index, _size, _nodeSet.crbegin());
	return recordList;
}

ETERFREE_SPACE_END
