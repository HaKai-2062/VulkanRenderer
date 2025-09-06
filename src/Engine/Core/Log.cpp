#include <fmt/core.h>
#include <fmt/os.h>
#include <fmt/color.h>
#include <string>
#include <stdarg.h>
#include <unordered_map>
#include <fstream>

#include "Core/Log.h"

static std::ofstream g_File;
static std::mutex g_LogMutex;
static std::unordered_map<LogLevel, fmt::color> g_LogColors
{
	{ LogLevel::FATAL, fmt::color::indian_red },
	{ LogLevel::ERROR, fmt::color::red },
	{ LogLevel::WARN, fmt::color::yellow },
	{ LogLevel::INFO, fmt::color::green },
	{ LogLevel::DEBUG, fmt::color::white },
	{ LogLevel::TRACE, fmt::color::white },
};
static std::unordered_map<LogLevel, std::string> g_LogString
{
	{ LogLevel::FATAL, "FATAL"},
	{ LogLevel::ERROR, "ERROR"},
	{ LogLevel::WARN,  "WARN"},
	{ LogLevel::INFO,  "INFO"},
	{ LogLevel::DEBUG, "DEBUG"},
	{ LogLevel::TRACE, "TRACE"},
};

Log::Log(const char* filePath)
{
	g_File.open(filePath, std::ios::out | std::ios::trunc);
	if (!g_File.is_open())
	{
		fmt::print(fmt::fg(g_LogColors[LogLevel::ERROR]), "Failed to open log file\n");
	}
}

Log::~Log()
{
	if (g_File.is_open())
		g_File.close();
}

void Log::Write(LogLevel loglevel, const char* fmtStr, ...)
{
	va_list args;
	va_start(args, fmtStr);
	std::string msg = fmt::vformat(fmtStr, fmt::make_format_args(args));
	Get().InternalWrite(loglevel, msg.c_str());
	va_end(args);
}

void Log::InternalWrite(LogLevel logLevel, const char* fmtStr,  ...)
{
	// If both logs disabled return
	if (!m_LogConsole && !m_LogFile)
		return;
	
	va_list args;
	va_start(args, fmtStr);
	std::string msg = fmt::vformat(fmtStr, fmt::make_format_args(args));
	va_end(args);
	std::lock_guard<std::mutex> lock(g_LogMutex);

	if (m_LogConsole && logLevel <= m_MinLevelConsole)
	{
		fmt::print(fmt::fg(g_LogColors[logLevel]), "{}: {}\n", g_LogString[logLevel], msg);
	}

	if (m_LogFile && logLevel <= m_MinLevelFile && g_File)
	{
		g_File << fmt::format("{}: {}", g_LogString[logLevel], msg) << std::endl;
	}
}