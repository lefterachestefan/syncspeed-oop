#ifndef LOGGER_H
#define LOGGER_H

#include <fmt/core.h>

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

// Template class: Logger<Category>
// A typed logging facility parameterized by a tag type so that different
// subsystems get independent log histories and can be queried separately.
// Instantiated at least twice (with NetworkTag and SyncTag).

struct NetworkTag {};
struct SyncTag {};

template <typename Tag>
class Logger {
	struct Entry {
		std::string message;
		std::chrono::steady_clock::time_point timestamp;
	};

	std::vector<Entry> entries;

	Logger() = default;

   public:
	Logger(const Logger&) = delete;
	Logger& operator=(const Logger&) = delete;

	static Logger& instance() {
		static Logger inst;
		return inst;
	}

	void log(const std::string& msg) {
		entries.push_back({msg, std::chrono::steady_clock::now()});
		fmt::print("[{}] {}\n", tag_name(), msg);
	}

	[[nodiscard]] const std::vector<Entry>& get_entries() const { return entries; }

	[[nodiscard]] size_t count() const { return entries.size(); }

	void clear() { entries.clear(); }

   private:
	[[nodiscard]] static const char* tag_name() {
		if constexpr (std::is_same_v<Tag, NetworkTag>) {
			return "NET";
		} else if constexpr (std::is_same_v<Tag, SyncTag>) {
			return "SYNC";
		} else {
			return "LOG";
		}
	}
};

// Convenience aliases — these are the two required instantiations
using NetworkLogger = Logger<NetworkTag>;
using SyncLogger = Logger<SyncTag>;

// Template function: log_result<T>
// Inspects a std::expected<T, std::string> and logs success/failure.
// Used at least twice (in SyncSession for both client and server sides).
template <typename T>
bool log_result(const std::expected<T, std::string>& result, const std::string& context,
				Logger<SyncTag>& logger) {
	if (result) {
		logger.log(context + ": OK");
		return true;
	}
	logger.log(context + ": FAILED — " + result.error());
	return false;
}

// Overload for NetworkTag logger
template <typename T>
bool log_result(const std::expected<T, std::string>& result, const std::string& context,
				Logger<NetworkTag>& logger) {
	if (result) {
		logger.log(context + ": OK");
		return true;
	}
	logger.log(context + ": FAILED — " + result.error());
	return false;
}

#endif
