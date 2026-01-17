#pragma once

#include <format>
#include <functional>
#include <string>
#include <string_view>

namespace worr {
	enum class LogLevel {
	Trace = 0,
	Debug,
	Info,
	Warn,
	Error
	};

	/*
	=============
	ParseLogLevel

	Parse the provided environment value into a LogLevel.
	=============
	*/
	LogLevel ParseLogLevel(std::string_view value);

	/*
	=============
	ReadLogLevelFromEnv

	Retrieve the log level from WORR_LOG_LEVEL or return the default.
	=============
	*/
	LogLevel ReadLogLevelFromEnv();

	/*
	=============
	LevelWeight

	Assign a numeric weight to a log level for comparison.
	=============
	*/
	int LevelWeight(LogLevel level);

	/*
	=============
	FormatMessage

	Build a structured log message for output.
	=============
	*/
	std::string FormatMessage(LogLevel level, std::string_view module_name, std::string_view message);

	/*
	=============
	InitLogger

	Initialize the logger with module metadata and output sinks.
	=============
	*/
	void InitLogger(std::string_view module_name, std::function<void(std::string_view)> print_sink, std::function<void(std::string_view)> error_sink);

	/*
	=============
	SetLogLevel

	Override the current logging level programmatically.
	=============
	*/
	void SetLogLevel(LogLevel level);

	/*
	=============
	GetLogLevel

	Fetch the currently active log level.
	=============
	*/
	LogLevel GetLogLevel();

	/*
	=============
	IsLogLevelEnabled

	Return whether the provided log level should emit output.
	=============
	*/
	bool IsLogLevelEnabled(LogLevel level);

	/*
	=============
	LoggerPrint

	Hook-compatible printer that respects the configured log level.
	=============
	*/
	void LoggerPrint(const char* message);

	/*
	=============
	LoggerError

	Hook-compatible error printer that always emits output.
	=============
	*/
	void LoggerError(const char* message);

	/*
	=============
	Log

	Log a pre-formatted message if the level is enabled.
	=============
	*/
	void Log(LogLevel level, std::string_view message);

	/*
	=============
	Logf

	Format a message and log it if the level is enabled.
	=============
	*/
	template<typename... Args>
	inline void Logf(LogLevel level, std::format_string<Args...> format_str, Args &&... args)
	{
		if (!IsLogLevelEnabled(level))
			return;

		Log(level, std::format(format_str, std::forward<Args>(args)...));
	}

	/*
	=============
	LogLevelLabel

	Provide a short string label for the supplied log level.
	=============
	*/
	const char* LogLevelLabel(LogLevel level);

} // namespace worr
