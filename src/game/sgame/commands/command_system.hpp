// Copyright (c) DarkMatter Projects 2023-2025
// Licensed under the GNU General Public License 2.0.
//
// command_system.hpp - Public interface for the modernized command system.

#pragma once

#include "../g_local.hpp"
#include <string_view>
#include <optional>
#include <functional>
#include <charconv>
#include <string>
#include <vector>
#include <initializer_list>

// C++20 Heterogeneous Lookup Support for string_view
// Allows searching maps with string_view without creating a std::string.
struct StringViewHash {
	using is_transparent = void;

	/*
	=============
	StringViewHash::operator()

	Hashes a C-style string for heterogeneous lookup.
	=============
	*/
	[[nodiscard]] size_t operator()(const char* txt) const { return std::hash<std::string_view>{}(txt); }

	/*
	=============
	StringViewHash::operator()

	Hashes a std::string_view for heterogeneous lookup.
	=============
	*/
	[[nodiscard]] size_t operator()(std::string_view txt) const { return std::hash<std::string_view>{}(txt); }

	/*
	=============
	StringViewHash::operator()

	Hashes a std::string for heterogeneous lookup.
	=============
	*/
	[[nodiscard]] size_t operator()(const std::string& txt) const { return std::hash<std::string>{}(txt); }
};

enum class CommandFlag : uint32_t {
	None = 0,
	AllowDead = 1 << 0,
	AllowIntermission = 1 << 1,
	AllowSpectator = 1 << 2,
	MatchOnly = 1 << 3,
	AdminOnly = 1 << 4,
	CheatProtect = 1 << 5,
};
template<> struct is_bitmask_enum<CommandFlag> : std::true_type {};

class CommandArgs {
private:
	int _argc;
	std::vector<std::string> _manualArgs;
	std::vector<std::string_view> _args;

	/*
	=============
	CommandArgs::setManualArgs

	Stores manual arguments and refreshes the cached string views.
	=============
	*/
	template<typename It>
	void setManualArgs(It begin, It end) {
		_manualArgs.clear();
		for (auto it = begin; it != end; ++it) {
			_manualArgs.emplace_back(*it);
		}
		_args.clear();
		_args.reserve(_manualArgs.size());
		for (const auto& arg : _manualArgs) {
			_args.emplace_back(arg);
		}
		_argc = static_cast<int>(_args.size());
	}

	/*
	=============
	CommandArgs::setEngineArgs

	Materializes arguments from the game interface for stable access.
	=============
	*/
	void setEngineArgs() {
		_args.clear();
		const int engineArgc = gi.argc();
		_args.reserve(static_cast<size_t>(engineArgc));
		for (int i = 0; i < engineArgc; ++i) {
			_args.emplace_back(gi.argv(i));
		}
		_argc = static_cast<int>(_args.size());
	}

public:
	/*
	=============
	CommandArgs::CommandArgs

	Constructs command arguments from the current engine-provided values.
	=============
	*/
	CommandArgs() {
		setEngineArgs();
	}

	/*
	=============
	CommandArgs::CommandArgs

	Constructs command arguments from a static initializer list.
	=============
	*/
	CommandArgs(std::initializer_list<std::string_view> args) {
		setManualArgs(args.begin(), args.end());
	}

	/*
	=============
	CommandArgs::CommandArgs

	Constructs command arguments from a provided vector of strings.
	=============
	*/
	explicit CommandArgs(std::vector<std::string> args)
			: _argc(0),
			_manualArgs(std::move(args)),
			_args(_manualArgs.begin(), _manualArgs.end()) {
		_argc = static_cast<int>(_args.size());
	}

	/*
	=============
	CommandArgs::count

	Returns the cached argument count.
	=============
	*/
	int count() const { return _argc; }

	/*
	=============
	CommandArgs::getString

	Retrieves the argument at the specified index as a string view.
	=============
	*/
	std::string_view getString(int index) const {
		if (index < 0 || index >= _argc) return "";
		return _args[static_cast<size_t>(index)];
	}

	/*
	=============
	CommandArgs::joinFrom

	Concatenates arguments starting at the specified index.
	=============
	*/
	std::string joinFrom(int index) const {
		if (index < 0 || index >= _argc) {
			return {};
		}

		std::string result;
		auto append = [&result](std::string_view part) {
			if (part.empty()) {
				return;
			}
			if (!result.empty()) {
				result.push_back(' ');
			}
			result.append(part.data(), part.size());
		};

		for (int i = index; i < _argc; ++i) {
			append(_args[static_cast<size_t>(i)]);
		}

		return result;
	}

	/*
	=============
	CommandArgs::getInt

	Retrieves an integer value from the specified argument.
	=============
	*/
	std::optional<int> getInt(int index) const {
		return ParseInt(getString(index));
	}

	/*
	=============
	CommandArgs::ParseInt

	Attempts to parse a string view into an integer.
	=============
	*/
	static std::optional<int> ParseInt(std::string_view str) {
		int value;
		auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
		if (ec == std::errc() && ptr == str.data() + str.size()) {
			return value;
		}
		return std::nullopt;
	}

	/*
	=============
	CommandArgs::getFloat

	Retrieves a floating-point value from the specified argument.
	=============
	*/
	std::optional<float> getFloat(int index) const {
		return ParseFloat(getString(index));
	}

	/*
	=============
	CommandArgs::ParseFloat

	Attempts to parse a string view into a float.
	=============
	*/
	static std::optional<float> ParseFloat(std::string_view str) {
		float value;
		auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
		if (ec == std::errc() && ptr == str.data() + str.size()) {
			return value;
		}
		return std::nullopt;
	}
};

struct Command {
	std::function<void(gentity_t*, const CommandArgs&)> function;
	BitFlags<CommandFlag> flags;
	bool floodExempt = true;
};

struct VoteCommand {
	std::string_view name;
	std::function<bool(gentity_t*, const CommandArgs&)> validate; // Corrected signature
	std::function<void()> execute;
	int32_t flag = 0;
	int8_t minArgs = 1;
	std::string_view argsUsage;
	std::string_view helpText;

	/*
	=============
	VoteCommand::VoteCommand

	Initializes a vote command definition.
	=============
	*/
	VoteCommand(std::string_view name,
		std::function<bool(gentity_t*, const CommandArgs&)> validate,
		std::function<void()> execute,
		int32_t flag,
		int8_t minArgs,
		std::string_view argsUsage,
		std::string_view helpText)
		: name(name), validate(std::move(validate)), execute(std::move(execute)),
		flag(flag), minArgs(minArgs), argsUsage(argsUsage), helpText(helpText) {
	}

	/*
	=============
	VoteCommand::VoteCommand
	=============
	*/
	VoteCommand() = default;
};

// Main dispatcher function called by the engine.
void ClientCommand(gentity_t* ent);

bool CheckFlood(gentity_t* ent);

// Main registration function to be called once at game startup.
void RegisterAllCommands();

bool CheatsOk(gentity_t* ent);
