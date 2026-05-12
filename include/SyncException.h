#ifndef SYNC_EXCEPTION_H
#define SYNC_EXCEPTION_H

#include <exception>
#include <string>
#include <utility>

class SyncError : public std::exception {
	std::string message;

   public:
	explicit SyncError(std::string msg) : message(std::move(msg)) {}
	[[nodiscard]] const char* what() const noexcept override { return message.c_str(); }
};

class NetworkError : public SyncError {
   public:
	explicit NetworkError(const std::string& msg) : SyncError("Network Error: " + msg) {}
};

class FileSystemError : public SyncError {
   public:
	explicit FileSystemError(const std::string& msg) : SyncError("FileSystem Error: " + msg) {}
};

class ConfigError : public SyncError {
   public:
	explicit ConfigError(const std::string& msg) : SyncError("Config Error: " + msg) {}
};

#endif
