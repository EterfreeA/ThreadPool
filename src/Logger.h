/*
* 文件名称：Logger.h
* 语言标准：C++20
* 
* 创建日期：2023年01月21日
* 
* 摘要
* 1.日志记录器抽象类Logger定义于此文件，其静态函数实现于Logger.cpp。
* 2.提供静态函数输出日志，默认使用标准流std::clog，支持多种描述类型。
* 3.提供日志层级，可选空层级，即无层级标识。
* 
* 作者：许聪
* 邮箱：solifree@qq.com
* 
* 版本：v1.0.0
*/

#pragma once

#include <functional>
#include <memory>
#include <cstdint>
#include <exception>
#include <system_error>
#include <string>

#include "Core.hpp"

ETERFREE_SPACE_BEGIN

class Logger
{
public:
	enum class Level : std::uint8_t
	{
		EMPTY, RUN, DEBUG, WARN, ERROR
	};

	enum class Mode : std::uint8_t
	{
		SINGLE_THREAD, MULTI_THREAD
	};

public:
	using Functor = std::function<std::string&(std::string&)>;

public:
	static std::shared_ptr<Logger> get(Mode _mode) noexcept;

	static void output(Level _level, \
		const std::source_location& _location, \
		const char* _description) noexcept;

	static void output(Level _level, \
		const std::source_location& _location, \
		const std::string& _description) noexcept;

	static void output(Level _level, \
		const std::source_location& _location, \
		const std::exception& _exception) noexcept;

	static void output(Level _level, \
		const std::source_location& _location, \
		const std::error_code& _code) noexcept;

public:
	virtual ~Logger() noexcept {}

	virtual void input(Level _level, \
		const std::source_location& _location, \
		const Functor& _functor) noexcept = 0;

	virtual void execute() noexcept {}
};

ETERFREE_SPACE_END
