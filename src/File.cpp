#include <cassert>
#include <fstream>
#include <functional>
#include <iomanip>
#include <sstream>
#include <iostream>

#include "File.h"
#include "SerializeUtils.h"

File::File(const std::filesystem::path &path) : path(path) {}

// Rule of three
File::File(const File& other) : hash(other.hash), path(other.path) {
    // std::cout << "File copy constructor called for " << path << "\n";
}

File& File::operator=(const File& other) {
    if (this != &other) {
        hash = other.hash;
        path = other.path;
    }
    // std::cout << "File copy assignment called for " << path << "\n";
    return *this;
}

File::~File() {
    // std::cout << "File destructor called for " << path << "\n";
}

std::expected<File, FileError>
File::try_create(const std::filesystem::path &path) {
  if (!std::filesystem::exists(path)) {
    return std::unexpected(FileError::NotFound);
  }

  if (!std::filesystem::is_regular_file(path)) {
    return std::unexpected(FileError::NotRegularFile);
  }

  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return std::unexpected(FileError::Unknown);
  }

  std::string contents((std::istreambuf_iterator<char>(stream)),
                       std::istreambuf_iterator<char>());

  auto hash_value = std::hash<std::string>{}(contents);

  std::ostringstream hex_stream;
  hex_stream << std::hex << std::setfill('0')
             << std::setw(sizeof(hash_value) * 2) << hash_value;

  auto file = File(path);
  file.hash = hex_stream.str();
  return file;
}

const std::string &File::get_hash() const {
  assert(!hash.empty());
  return hash;
}

const std::filesystem::path &File::get_path() const { return path; }

File File::create_remote(const std::filesystem::path &path,
                         const std::string &hash) {
  File f(path);
  f.hash = hash;
  return f;
}

void File::serialize(std::ostream &os) const {
  SerializeUtils::write_string(os, path.filename().string());
  SerializeUtils::write_string(os, hash);
}

std::expected<File, FileError>
File::deserialize(std::istream &is, const std::filesystem::path &base_path) {
  std::string filename = SerializeUtils::read_string(is);
  std::string hash_val = SerializeUtils::read_string(is);
  if (!is) {
    return std::unexpected(FileError::Unknown);
  }
  return create_remote(base_path / filename, hash_val);
}

std::ostream& operator<<(std::ostream& os, const File& file) {
    os << "File(" << file.path.filename().string() << ", hash=" << file.hash.substr(0, 8) << "...)";
    return os;
}

bool File::exists() const {
    return std::filesystem::exists(path);
}

uintmax_t File::size() const {
    if (exists()) {
        return std::filesystem::file_size(path);
    }
    return 0;
}

std::string File::extension() const {
    return path.extension().string();
}
