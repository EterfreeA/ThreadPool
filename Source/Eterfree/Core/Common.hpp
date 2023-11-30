#pragma once

#ifndef __cplusplus
#error The file requires a C++ compiler.
#endif

#include <source_location>
#include <ostream>

// 字符串化
#define STRING(content) #content

// 拼接
#define SPLICE(front, back) front##back

// 弃用
#define DEPRECATED \
[[deprecated("The name for this item is deprecated.")]]

// 替换
#define REPLACEMENT(signature) \
[[deprecated("The name for this item is deprecated. " \
"Instead, use the name: " STRING(signature) ".")]]

// 自定义名称空间
#define ETERFREE_SPACE Eterfree
#define ETERFREE_SPACE_BEGIN namespace ETERFREE_SPACE {
#define ETERFREE_SPACE_END }
#define USING_ETERFREE_SPACE using namespace ETERFREE_SPACE;

ETERFREE_SPACE_BEGIN

//template <typename _Type, const decltype(sizeof 0) _SIZE>
//constexpr auto size(_Type(&_array)[_SIZE]) noexcept
//{
//	return sizeof _array / sizeof _array[0];
//}

template <typename _Type, const decltype(sizeof 0) _SIZE>
constexpr auto size(_Type(&_array)[_SIZE]) noexcept
{
	return _SIZE;
}

inline std::ostream& operator<<(std::ostream& _stream, \
	const std::source_location& _location)
{
	return _stream << "in " << _location.function_name() \
		<< " at " << _location.file_name() << '(' \
		<< _location.line() << ',' \
		<< _location.column() << "): ";
}

ETERFREE_SPACE_END
