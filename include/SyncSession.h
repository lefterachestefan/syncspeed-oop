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
	explicit SyncSession(const std::filesystem::path& local_path);

	std::expected<void, std::string> run_server_side(NetworkConnection& conn);
	std::expected<void, std::string> run_client_side(NetworkConnection& conn);

	[[nodiscard]] const std::filesystem::path& get_local_path() const;

	static std::expected<void, std::string> server_sync(
		NetworkConnection& conn, const std::filesystem::path& local_sync_folder);
	static std::expected<void, std::string> client_sync(
		NetworkConnection& conn, const std::filesystem::path& local_sync_folder);

	friend std::ostream& operator<<(std::ostream& os, const SyncSession& session);
};

#endif
