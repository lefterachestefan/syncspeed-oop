#include <algorithm>
#include <expected>
#include <iostream>

#include "Device.h"

Device::Device(std::string device_id) : device_id(std::move(device_id)) {
	// TODO: multi platform support
	kind = DeviceKind::Linux;
}

std::expected<void, FileError> Device::rescan() {
	for (auto& folder : folders) {
		const auto result = folder.rescan();
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
		return std::unexpected<SyncDirectoryError>(SyncDirectoryError::AlreadySynced);
	}

	return SyncedFolder::try_create(folder_path)
		.and_then([this](auto folder) -> SyncException {
			folders.push_back(std::move(folder));
			return {};
		})
		.or_else(
			[](auto err) -> SyncException { return std::unexpected<SyncDirectoryError>(err); });
}

/*
Device::UnsyncException Device::unsync_folder(const std::filesystem::path& folder_path) { //
Currently unused auto it = std::find_if(folders.begin(), folders.end(),
						   [&folder_path](const auto& f) { return f.get_path() == folder_path; });

	if (it == folders.end()) {
		return std::unexpected<UnsyncDirectoryError>(UnsyncDirectoryError::NotSynced);
	}

	folders.erase(it);
	return {};
}

const std::string& Device::get_id() const { return device_id; } // Currently unused

DeviceKind Device::get_kind() const { return kind; } // Currently unused
*/

std::ostream& operator<<(std::ostream& os, const Device& device) {
	os << "Device(id=" << device.device_id
	   << ", kind=" << (device.kind == DeviceKind::Linux ? "Linux" : "Android")
	   << ", folders=" << device.folders.size() << ")";
	return os;
}
