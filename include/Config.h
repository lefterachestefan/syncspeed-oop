#ifndef CONFIG_H
#define CONFIG_H

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

#include "SyncException.h"

class Config {
	std::string device_name;
	uint16_t default_port;

   public:
	explicit Config(const std::filesystem::path& path) {
		if (!std::filesystem::exists(path)) {
			device_name = "default-device";
			default_port = 8080;
			return;
		}

		std::ifstream ifs(path);
		if (!ifs) {
			throw ConfigError("Failed to open config file: " + path.string());
		}

		try {
			nlohmann::json j;
			ifs >> j;
			device_name = j.value("device_name", "default-device");
			default_port = j.value("default_port", 8080);
		} catch (const nlohmann::json::exception& e) {
			throw ConfigError("JSON Parse Error: " + std::string(e.what()));
		}
	}

	[[nodiscard]] const std::string& get_device_name() const { return device_name; }
	[[nodiscard]] uint16_t get_default_port() const { return default_port; }
};

#endif
