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
};

enum class UnsyncDirectoryError {
	NotSynced,
};

class Device {
	std::string device_id;
	DeviceKind kind;
	std::vector<SyncedFolder> folders;

   public:
	explicit Device(std::string device_id);

	std::expected<void, FileError> rescan();

	using SyncException = std::expected<void, SyncDirectoryError>;
	SyncException sync_folder(const std::filesystem::path& folder_path);

	using UnsyncException = std::expected<void, UnsyncDirectoryError>;
	UnsyncException unsync_folder(const std::filesystem::path& folder_path);

	[[nodiscard]] const std::string& get_id() const;
	[[nodiscard]] DeviceKind get_kind() const;

	friend std::ostream& operator<<(std::ostream& os, const Device& device);
};

#endif
