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

	friend std::ostream& operator<<(std::ostream& os, const SyncSession& session);
};

// builder pattern
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
