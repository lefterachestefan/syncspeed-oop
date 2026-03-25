#include <expected>
#include <iostream>
#include <utility>

#include "SyncedFolder.h"

SyncedFolder::SyncedFolder(std::filesystem::path path) : path(std::move(path)) {}

std::expected<SyncedFolder, SyncDirectoryError> SyncedFolder::try_create(
	const std::filesystem::path& path) {
	auto incomplete = SyncedFolder(path);
	const auto res = incomplete.rescan();
	if (!res) {
		return std::unexpected<SyncDirectoryError>(SyncDirectoryError::BadPermissions);
	}
	return incomplete;
}

std::expected<void, FileError> SyncedFolder::rescan() {
	return Directory::try_create(path).and_then([this](auto dir) -> std::expected<void, FileError> {
		top_folder = std::make_unique<Directory>(std::move(dir));
		return {};
	});
}

const std::filesystem::path& SyncedFolder::get_path() const { return path; }

// const Directory* SyncedFolder::get_top_folder() const { return top_folder.get(); } // Currently
// unused

std::ostream& operator<<(std::ostream& os, const SyncedFolder& folder) {
	os << "SyncedFolder(path=" << folder.path << ")";
	if (folder.top_folder) {
		os << " [Scanning active]";
	}
	return os;
}
