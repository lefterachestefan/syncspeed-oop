#include <cassert>
#include <cstdint>
#include <expected>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#include "File.h"
#include "SerializeUtils.h"

namespace {
uint64_t fnv1a_hash(const std::string& s) {
	uint64_t hash = 0xcbf29ce484222325ULL;
	for (unsigned char c : s) {
		hash ^= c;
		hash *= 0x100000001b3ULL;
	}
	return hash;
}
}  // namespace

File::File(std::filesystem::path path) : path(std::move(path)) {}

File& File::operator=(const File& other) {
	if (this != &other) {
		hash = other.hash;
		path = other.path;
	}
	// std::cout << "File copy assignment called for " << path << "\n";
	return *this;
}

std::expected<File, FileError> File::try_create(const std::filesystem::path& path) {
	if (!std::filesystem::exists(path)) {
		return std::unexpected<FileError>(FileError::NotFound);
	}

	if (!std::filesystem::is_regular_file(path)) {
		return std::unexpected<FileError>(FileError::NotRegularFile);
	}

	std::ifstream stream(path, std::ios::binary);
	if (!stream) {
		return std::unexpected<FileError>(FileError::Unknown);
	}

	const std::string contents((std::istreambuf_iterator<char>(stream)),
							   std::istreambuf_iterator<char>());

	const uint64_t hash_value = fnv1a_hash(contents);

	std::ostringstream hex_stream;
	hex_stream << std::hex << std::setfill('0') << std::setw(16) << hash_value;

	auto file = File(path);
	file.hash = hex_stream.str();
	return file;
}

const std::string& File::get_hash() const {
	assert(!hash.empty());
	return hash;
}

const std::filesystem::path& File::get_path() const { return path; }

File File::create_remote(const std::filesystem::path& path, const std::string& hash) {
	File f(path);
	f.hash = hash;
	return f;
}

void File::serialize(std::ostream& os) const {
	SerializeUtils::write_string(os, path.filename().string());
	SerializeUtils::write_string(os, hash);
}

std::expected<File, FileError> File::deserialize(std::istream& is,
												 const std::filesystem::path& base_path) {
	const std::string filename = SerializeUtils::read_string(is);
	const std::string hash_val = SerializeUtils::read_string(is);
	if (!is) {
		return std::unexpected<FileError>(FileError::Unknown);
	}
	return create_remote(base_path / filename, hash_val);
}

std::ostream& operator<<(std::ostream& os, const File& file) {
	os << "File(" << file.path.filename().string() << ", hash=" << file.hash.substr(0, 8) << "...)";
	return os;
}

bool File::exists() const { return std::filesystem::exists(path); }

uintmax_t File::size() const {
	if (exists()) {
		return std::filesystem::file_size(path);
	}
	return 0;
}

std::string File::extension() const { return path.extension().string(); }
