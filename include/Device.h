#ifndef DEVICE_H
#define DEVICE_H

#include <expected>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "SyncedFolder.h"

enum class DeviceKind {
	Linux,
	Android,
	Windows,
	MacOS,
};

enum class UnsyncDirectoryError {
	NotSynced,
};

class Device {
	std::string device_id;
	DeviceKind kind;
	std::vector<SyncedFolder> folders;

	static int total_devices;

   public:
	explicit Device(std::string device_id);
	~Device();

	static int get_total_devices();

	std::expected<void, FileError> rescan();

	using SyncException = std::expected<void, SyncDirectoryError>;
	SyncException sync_folder(const std::filesystem::path& folder_path);

	friend std::ostream& operator<<(std::ostream& os, const Device& device);
};

#endif
