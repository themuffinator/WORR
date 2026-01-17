/*Copyright (c) 2024 The DarkMatter Project
Licensed under the GNU General Public License 2.0.

logger.cpp implementation.*/

#include "logger.hpp"

#include <algorithm>
#include <atomic>
#include <array>
#include <cctype>
#include <cstdlib>
#include <format>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>

namespace worr {
namespace {

std::string g_module_name;
std::atomic<LogLevel> g_log_level = LogLevel::Warn;
std::function<void(std::string_view)> g_print_sink;
std::function<void(std::string_view)> g_error_sink;
std::mutex g_logger_mutex;

struct LoggerState {
	std::string module_name;
	std::function<void(std::string_view)> print_sink;
	std::function<void(std::string_view)> error_sink;
};

/*
=============
EnsureSink

Call the provided sink if it exists.
=============
*/
void EnsureSink(const std::function<void(std::string_view)>& sink, std::string_view message)
{
	if (sink)
		sink(message);
}

/*
=============
SnapshotLoggerState

Retrieve a thread-safe snapshot of logger configuration.
=============
*/
LoggerState SnapshotLoggerState()
{
	std::lock_guard lock(g_logger_mutex);
	return LoggerState{ g_module_name, g_print_sink, g_error_sink };
}

} // namespace

/*
=============
ParseLogLevel

Parse the provided environment value into a LogLevel.
=============
*/
LogLevel ParseLogLevel(std::string_view value)
{
	std::string lowered(value);
	std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	if (lowered == "trace")
		return LogLevel::Trace;
	if (lowered == "debug")
		return LogLevel::Debug;
	if (lowered == "warn" || lowered == "warning")
		return LogLevel::Warn;
	if (lowered == "error")
		return LogLevel::Error;

	return LogLevel::Warn;
}

/*
=============
ReadLogLevelFromEnv

Retrieve the log level from WORR_LOG_LEVEL or return the default.
=============
*/
LogLevel ReadLogLevelFromEnv()
{
	const char* env_value = std::getenv("WORR_LOG_LEVEL");
	if (!env_value)
		return LogLevel::Warn;

	return ParseLogLevel(env_value);
}

/*
=============
LevelWeight

Assign a numeric weight to a log level for comparison.
=============
*/
int LevelWeight(LogLevel level)
{
	switch (level) {
	case LogLevel::Trace:
		return 0;
	case LogLevel::Debug:
		return 1;
	case LogLevel::Info:
		return 2;
	case LogLevel::Warn:
		return 3;
	case LogLevel::Error:
	default:
		return 4;
	}
}

/*
=============
FormatMessage

Build a structured log message for output.
=============
*/
std::string FormatMessage(LogLevel level, std::string_view module_name, std::string_view message)
{
	static constexpr std::array prefixes{ "[TRACE]", "[DEBUG]", "[INFO]", "[WARN]", "[ERROR]" };
	const size_t prefix_index = static_cast<size_t>(LevelWeight(level));
	std::string_view level_label = prefixes[std::min(prefix_index, prefixes.size() - 1)];

	std::string formatted = std::format("[WORR][{}] {} {}", module_name, level_label, message);
	if (!formatted.empty() && formatted.back() != '\n')
		formatted.push_back('\n');

	return formatted;
}

/*
=============
InitLogger

Initialize the logger with module metadata and output sinks.
=============
*/
void InitLogger(std::string_view module_name, std::function<void(std::string_view)> print_sink, std::function<void(std::string_view)> error_sink)
{
	{
		std::lock_guard lock(g_logger_mutex);
		g_module_name = module_name;
		g_print_sink = std::move(print_sink);
		g_error_sink = std::move(error_sink);
	}

	g_log_level.store(ReadLogLevelFromEnv(), std::memory_order_relaxed);
}

/*
=============
SetLogLevel

Override the current logging level programmatically.
=============
*/
void SetLogLevel(LogLevel level)
{
	g_log_level.store(level, std::memory_order_relaxed);
}

/*
=============
GetLogLevel

Fetch the currently active log level.
=============
*/
LogLevel GetLogLevel()
{
	return g_log_level.load(std::memory_order_relaxed);
}

/*
=============
IsLogLevelEnabled

Return whether the provided log level should emit output.
=============
*/
bool IsLogLevelEnabled(LogLevel level)
{
	return LevelWeight(level) >= LevelWeight(g_log_level.load(std::memory_order_relaxed));
}

/*
=============
LoggerPrint

Hook-compatible printer that respects the configured log level.
=============
*/
void LoggerPrint(const char* message)
{
	if (IsLogLevelEnabled(LogLevel::Info))
		Log(LogLevel::Info, message);
}

/*
=============
LoggerError

Hook-compatible error printer that always emits output.
=============
*/
void LoggerError(const char* message)
{
	const LoggerState state = SnapshotLoggerState();
	const std::string formatted = FormatMessage(LogLevel::Error, state.module_name, message);

	if (IsLogLevelEnabled(LogLevel::Error))
		EnsureSink(state.print_sink, formatted);

	EnsureSink(state.error_sink, formatted);
}

/*
=============
Log

Log a pre-formatted message if the level is enabled.
=============
*/
void Log(LogLevel level, std::string_view message)
{
	if (!IsLogLevelEnabled(level))
		return;

	const LoggerState state = SnapshotLoggerState();
	EnsureSink(state.print_sink, FormatMessage(level, state.module_name, message));
}

/*
=============
LogLevelLabel

Provide a short string label for the supplied log level.
=============
*/
const char* LogLevelLabel(LogLevel level)
{
	switch (level) {
	case LogLevel::Trace:
		return "TRACE";
	case LogLevel::Debug:
		return "DEBUG";
	case LogLevel::Info:
		return "INFO";
	case LogLevel::Warn:
		return "WARN";
	case LogLevel::Error:
	default:
		return "ERROR";
	}
}

} // namespace worr
