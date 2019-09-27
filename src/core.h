#pragma once

#ifndef __cplusplus

#error You must use this file in C++ compiler, and you need to use ".cpp" as the suffix name of file.

#endif

#define ETERFREE_BEGIN namespace eterfree{
#define ETERFREE_END }

// 自动加双引号
#define GET_STR(x) #x

#if _MSC_VER >= 1900
#define u8(str) u8##str
#elif _MSC_VER >= 1800
#define u8(str) QStringLiteral(str)
#else
#define u8(str) str
#endif
