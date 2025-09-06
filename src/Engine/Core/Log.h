#pragma once

#include <mutex>

enum class LogLevel : uint8_t
{
	FATAL = 0,
	ERROR,
	WARN,
	INFO,
	DEBUG,
	TRACE
};

class Log
{
public:
	static Log& Get()
	{
		static Log instance;
		return instance;
	}

	static void SetMinConsoleLevel(LogLevel minLevel) { Get().m_MinLevelConsole = minLevel; }
	static void SetMinFileLevel(LogLevel minLevel) { Get().m_MinLevelFile = minLevel; }
	static void SetConsoleLogging(bool log) { Get().m_LogConsole = log; }
	static void SetFileLogging(bool log) { Get().m_LogFile = log; }
	static void Write(LogLevel loglevel, const char* fmtStr, ...);

private:
	Log(const char* filePath = "EngineLog.txt");
	~Log();
	void InternalWrite(LogLevel loglevel, const char* fmtStr, ...);

private:
	LogLevel m_MinLevelConsole = LogLevel::INFO;
	LogLevel m_MinLevelFile = LogLevel::INFO;
	bool m_LogConsole = true;
	bool m_LogFile = true;
};
