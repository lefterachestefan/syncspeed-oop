#ifndef DIRECTORY_H
#define DIRECTORY_H

#include <expected>
#include <filesystem>
#include <iostream>
#include <variant>
#include <vector>

#include "File.h"

class Directory {
	std::vector<std::variant<Directory, File>> children;
	std::filesystem::path path;

   public:
	explicit Directory(std::filesystem::path path);

	Directory(std::filesystem::path path, std::vector<std::variant<Directory, File>> children);

	static std::expected<Directory, FileError> try_create(const std::filesystem::path& path);
	static Directory create_remote(const std::filesystem::path& path,
								   std::vector<std::variant<Directory, File>> children);

	[[nodiscard]] const std::filesystem::path& get_path() const;
	[[nodiscard]] const std::vector<std::variant<Directory, File>>& get_children() const;

	void serialize(std::ostream& os) const;
	static std::expected<Directory, FileError> deserialize(std::istream& is,
														   const std::filesystem::path& path);

	friend std::ostream& operator<<(std::ostream& os, const Directory& dir);

	[[nodiscard]] size_t count_files() const;
	[[nodiscard]] size_t count_directories() const;
	[[nodiscard]] bool is_empty() const;
};

#endif
