#include <expected>
#include <filesystem>
#include <iostream>

#include "Directory.h"
#include "SerializeUtils.h"

Directory::Directory(std::filesystem::path path) : path(std::move(path)) {}

Directory::Directory(std::filesystem::path path,
					 std::vector<std::variant<Directory, File>> children)
	: children(std::move(children)), path(std::move(path)) {}

const std::filesystem::path& Directory::get_path() const { return path; }

const std::vector<std::variant<Directory, File>>& Directory::get_children() const {
	return children;
}

// NOLINTNEXTLINE(misc-no-recursion)
std::expected<Directory, FileError> Directory::try_create(const std::filesystem::path& path) {
	auto dir = Directory(path);
	auto& children = dir.children;
	if (!std::filesystem::exists(path)) {
		return std::unexpected<FileError>(FileError::NotFound);
	}
	for (const auto& x : std::filesystem::directory_iterator(path)) {
		if (x.is_regular_file()) {
			auto file = File::try_create(x.path());
			if (!file) {
				return std::unexpected<FileError>(file.error());
			}
			children.emplace_back(std::move(*file));
		} else if (x.is_directory()) {
			auto subdir = Directory::try_create(x.path());
			if (!subdir) {
				return std::unexpected<FileError>(subdir.error());
			}
			children.emplace_back(std::move(*subdir));
		}
	}
	return dir;
}

Directory Directory::create_remote(const std::filesystem::path& path,
								   std::vector<std::variant<Directory, File>> children) {
	return {path, std::move(children)};
}

// NOLINTNEXTLINE(misc-no-recursion)
void Directory::serialize(std::ostream& os) const {
	SerializeUtils::write_string(os, path.filename().string());
	size_t num_children = children.size();
	os.write(reinterpret_cast<const char*>(&num_children), sizeof(num_children));
	for (const auto& child : children) {
		bool is_dir = std::holds_alternative<Directory>(child);
		os.write(reinterpret_cast<const char*>(&is_dir), sizeof(is_dir));
		if (is_dir) {
			std::get<Directory>(child).serialize(os);
		} else {
			std::get<File>(child).serialize(os);
		}
	}
}
// NOLINTNEXTLINE(misc-no-recursion)
std::expected<Directory, FileError> Directory::deserialize(std::istream& is,
														   const std::filesystem::path& base_path) {
	const std::string filename = SerializeUtils::read_string(is);
	size_t num_children = 0;
	is.read(reinterpret_cast<char*>(&num_children), sizeof(num_children));
	if (!is) {
		return std::unexpected<FileError>(FileError::Unknown);
	}

	std::vector<std::variant<Directory, File>> children;
	const auto new_path = base_path / filename;
	for (size_t i = 0; i < num_children; ++i) {
		bool is_dir = false;
		is.read(reinterpret_cast<char*>(&is_dir), sizeof(is_dir));
		if (!is) {
			return std::unexpected<FileError>(FileError::Unknown);
		}

		if (is_dir) {
			const auto dir = Directory::deserialize(is, new_path);
			if (!dir) {
				return std::unexpected<FileError>(dir.error());
			}
			children.emplace_back(*dir);
		} else {
			const auto file = File::deserialize(is, new_path);
			if (!file) {
				return std::unexpected<FileError>(file.error());
			}
			children.emplace_back(*file);
		}
	}
	return create_remote(new_path, std::move(children));
}

std::ostream& operator<<(std::ostream& os, const Directory& dir) {
	os << "Directory(" << dir.path.filename().string() << ", children=" << dir.children.size()
	   << ")";
	return os;
}

size_t Directory::count_files() const {
	size_t count = 0;
	for (const auto& child : children) {
		if (std::holds_alternative<File>(child)) {
			count++;
		}
	}
	return count;
}

size_t Directory::count_directories() const {
	size_t count = 0;
	for (const auto& child : children) {
		if (std::holds_alternative<Directory>(child)) {
			count++;
		}
	}
	return count;
}

// currently unused
// bool Directory::is_empty() const { return children.empty(); }
