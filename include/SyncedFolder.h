#ifndef SYNCEDFOLDER_H
#define SYNCEDFOLDER_H

#include <expected>
#include <filesystem>
#include <iostream>
#include <memory>

#include "Directory.h"

enum class SyncDirectoryError {
	AlreadySynced,
	BadPermissions,
	OutOfSpace,
};

class SyncedFolder {
	std::unique_ptr<Directory> top_folder;
	std::filesystem::path path;

   public:
	explicit SyncedFolder(const std::filesystem::path& path);

	static std::expected<SyncedFolder, SyncDirectoryError> try_create(
		const std::filesystem::path& path);

	std::expected<void, FileError> rescan();
	[[nodiscard]] const std::filesystem::path& get_path() const;
	[[nodiscard]] const Directory* get_top_folder() const;

	SyncedFolder(SyncedFolder&&) noexcept = default;
	SyncedFolder& operator=(SyncedFolder&& other) noexcept {
		top_folder = std::move(other.top_folder);
		path = std::move(other.path);
		return *this;
	}

	SyncedFolder(const SyncedFolder&) = delete;
	SyncedFolder& operator=(const SyncedFolder&) = delete;

	~SyncedFolder();

	friend std::ostream& operator<<(std::ostream& os, const SyncedFolder& folder);
};

#endif
