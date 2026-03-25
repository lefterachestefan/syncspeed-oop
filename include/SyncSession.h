#ifndef SYNCSESSION_H
#define SYNCSESSION_H

#include <expected>
#include <filesystem>
#include <iostream>
#include <string>

#include "Network.h"

class SyncSession {
	const std::filesystem::path local_sync_folder;

   public:
	explicit SyncSession(std::filesystem::path local_path);

	std::expected<void, std::string> run_server_side(const NetworkConnection& conn);
	std::expected<void, std::string> run_client_side(const NetworkConnection& conn);

	// [[nodiscard]] const std::filesystem::path& get_local_path() const; // Currently unused

	// static std::expected<void, std::string> server_sync(
	// 	NetworkConnection& conn, const std::filesystem::path& local_sync_folder); // Currently
	// unused static std::expected<void, std::string> client_sync( 	NetworkConnection& conn, const
	// std::filesystem::path& local_sync_folder); // Currently unused

	friend std::ostream& operator<<(std::ostream& os, const SyncSession& session);
};

#endif
