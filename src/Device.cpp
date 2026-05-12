#include "Device.h"

#include <iostream>
#include <utility>

int Device::total_devices = 0;

Device::Device(std::string device_id) : device_id(std::move(device_id)) {
#if defined(_WIN32)
	kind = DeviceKind::Windows;
#elif defined(__APPLE__)
	kind = DeviceKind::MacOS;
#else
	kind = DeviceKind::Linux;
#endif
	total_devices++;
}

Device::~Device() { total_devices--; }

int Device::get_total_devices() { return total_devices; }

std::expected<void, FileError> Device::rescan() {
	for (auto& folder : folders) {
		auto res = folder.rescan();
		if (!res) {
			return std::unexpected<FileError>(res.error());
		}
	}
	return {};
}

Device::SyncException Device::sync_folder(const std::filesystem::path& folder_path) {
	auto res = SyncedFolder::try_create(folder_path);
	if (!res) {
		return std::unexpected<SyncDirectoryError>(SyncDirectoryError::InvalidPath);
	}
	folders.push_back(std::move(*res));
	return {};
}

std::ostream& operator<<(std::ostream& os, const Device& device) {
	os << "Device(id=" << device.device_id << ", kind=";
	switch (device.kind) {
		case DeviceKind::Linux:
			os << "Linux";
			break;
		case DeviceKind::Android:
			os << "Android";
			break;
		case DeviceKind::Windows:
			os << "Windows";
			break;
		case DeviceKind::MacOS:
			os << "MacOS";
			break;
	}
	os << ", folders=[";
	for (size_t i = 0; i < device.folders.size(); ++i) {
		os << device.folders[i];
		if (i < device.folders.size() - 1) {
			os << ", ";
		}
	}
	os << "])";
	return os;
}
