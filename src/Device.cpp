#include <algorithm>
#include <iostream>

#include "Device.h"

Device::Device(const std::string& device_id) : device_id(device_id) {
	// Default to Linux as in original code
	kind = DeviceKind::Linux;
}

std::expected<void, FileError> Device::rescan() {
	for (auto& folder : folders) {
		auto result = folder.rescan();
		if (!result) {
			return result;
		}
	}
	return {};
}

Device::SyncException Device::sync_folder(const std::filesystem::path& folder_path) {
	const bool has_duplicate =
		std::any_of(folders.begin(), folders.end(),
					[&folder_path](const SyncedFolder& f) { return f.get_path() == folder_path; });

	if (has_duplicate) {
		return std::unexpected(SyncDirectoryError::AlreadySynced);
	}

	return SyncedFolder::try_create(folder_path)
		.and_then([this](auto folder) -> SyncException {
			folders.push_back(std::move(folder));
			return {};
		})
		.or_else([](auto err) -> SyncException { return std::unexpected(err); });
}

Device::UnsyncException Device::unsync_folder(const std::filesystem::path& folder_path) {
	auto it = std::find_if(folders.begin(), folders.end(),
						   [&folder_path](const auto& f) { return f.get_path() == folder_path; });

	if (it == folders.end()) {
		return std::unexpected(UnsyncDirectoryError::NotSynced);
	}

	folders.erase(it);
	return {};
}

const std::string& Device::get_id() const { return device_id; }

DeviceKind Device::get_kind() const { return kind; }

std::ostream& operator<<(std::ostream& os, const Device& device) {
	os << "Device(id=" << device.device_id
	   << ", kind=" << (device.kind == DeviceKind::Linux ? "Linux" : "Android")
	   << ", folders=" << device.folders.size() << ")";
	return os;
}
