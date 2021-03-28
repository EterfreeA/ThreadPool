#pragma once

#ifndef __cplusplus
#error The file requires a C++ compiler.
#endif

// 字符串化
#define STRING(content) #content

// 拼接
#define SPLICE(front, back) front##back

// 自定义名称空间
#define ETERFREE_BEGIN namespace eterfree {
#define ETERFREE_END }

#define DEPRECATED \
[[deprecated("The name for this item is deprecated.")]]

#define REPLACEMENT(description) \
[[deprecated("The name for this item is deprecated. Instead, use the name: " STRING(reason) ".")]]
