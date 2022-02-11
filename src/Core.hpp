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

#define DEPRECATED \
[[deprecated("The name for this item is deprecated.")]]

#define REPLACEMENT(signature) \
[[deprecated("The name for this item is deprecated. Instead, use the name: " STRING(signature) ".")]]

// 自定义名称空间
#define ETERFREE_SPACE_BEGIN namespace eterfree {
#define ETERFREE_SPACE_END }
#define ETERFREE_SPACE using namespace eterfree;

inline std::ostream& operator<<(std::ostream& _stream, const std::source_location& location)
{
	return _stream << '[' << location.file_name() << ':' << location.function_name() << ':' << location.line() << ']';
}

ETERFREE_SPACE_BEGIN

//template <typename _Type, const decltype(sizeof(0)) _SIZE>
//constexpr auto size(_Type(&_array)[_SIZE])
//{
//	return sizeof _array / sizeof _array[0];
//}

template <typename _Type, const decltype(sizeof(0)) _SIZE>
constexpr auto size(_Type(&_array)[_SIZE])
{
	return _SIZE;
}

ETERFREE_SPACE_END
