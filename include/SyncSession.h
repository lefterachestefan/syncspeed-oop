#ifndef SYNCSESSION_H
#define SYNCSESSION_H

#include <expected>
#include <filesystem>
#include <iostream>
#include <string>

#include "Network.h"

class SyncSession {
	const std::filesystem::path local_sync_folder;
	bool verbose;
	size_t max_retries;

	friend class SyncSessionBuilder;

   public:
	SyncSession(std::filesystem::path local_path, bool verbose, size_t max_retries);

	std::expected<void, std::string> run_server_side(const NetworkConnection& conn);
	std::expected<void, std::string> run_client_side(const NetworkConnection& conn);

	[[nodiscard]] bool is_verbose() const;
	[[nodiscard]] size_t get_max_retries() const;

	// [[nodiscard]] const std::filesystem::path& get_local_path() const; // Currently unused

	// static std::expected<void, std::string> server_sync(
	// 	NetworkConnection& conn, const std::filesystem::path& local_sync_folder); // Currently
	// unused static std::expected<void, std::string> client_sync( 	NetworkConnection& conn, const
	// std::filesystem::path& local_sync_folder); // Currently unused

	friend std::ostream& operator<<(std::ostream& os, const SyncSession& session);
};

// Builder Pattern: constructs SyncSession with optional configuration.
class SyncSessionBuilder {
	std::filesystem::path path;
	bool verbose = false;
	size_t max_retries = 3;

   public:
	explicit SyncSessionBuilder(std::filesystem::path folder_path);

	SyncSessionBuilder& set_verbose(bool v);
	SyncSessionBuilder& set_max_retries(size_t retries);

	SyncSession build();
};

#endif
