#ifndef FILE_H
#define FILE_H

#include <expected>
#include <filesystem>
#include <iostream>
#include <string>

enum class FileError {
	NotFound,
	NotRegularFile,
	Unknown,
};

class File {
	std::string hash;
	std::filesystem::path path;

   public:
	explicit File(const std::filesystem::path& path);

	File(const File& other);
	File& operator=(const File& other);
	~File();

	static std::expected<File, FileError> try_create(const std::filesystem::path& path);

	[[nodiscard]] const std::string& get_hash() const;
	[[nodiscard]] const std::filesystem::path& get_path() const;

	void serialize(std::ostream& os) const;
	static std::expected<File, FileError> deserialize(std::istream& is,
													  const std::filesystem::path& path);

	static File create_remote(const std::filesystem::path& path, const std::string& hash);

	friend std::ostream& operator<<(std::ostream& os, const File& file);

	[[nodiscard]] bool exists() const;
	[[nodiscard]] uintmax_t size() const;
	[[nodiscard]] std::string extension() const;
};

#endif
