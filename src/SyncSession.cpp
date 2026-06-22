#include <fmt/core.h>
#include <fmt/ostream.h>

#include <expected>
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

#include "Directory.h"
#include "Logger.h"
#include "SyncAction.h"
#include "SyncException.h"
#include "SyncSession.h"

SyncSession::SyncSession(std::filesystem::path local_path, bool verbose, size_t max_retries)
	: local_sync_folder(std::move(local_path)), verbose(verbose), max_retries(max_retries) {
	if (!std::filesystem::exists(local_sync_folder)) {
		throw FileSystemError("Sync folder does not exist: " + local_sync_folder.string());
	}
}

bool SyncSession::is_verbose() const { return verbose; }

size_t SyncSession::get_max_retries() const { return max_retries; }

std::expected<void, std::string> send_actions(const NetworkConnection& conn,
											  const std::filesystem::path& local_sync_folder,
											  const std::vector<ActionWrapper>& actions) {
	for (const auto& sync_action : actions) {
		if (dynamic_cast<const ConflictFileAction*>(&sync_action.get()) != nullptr) {
			fmt::print("Handling conflict for: {}\n", sync_action.get().get_path().string());
		}
		const auto res = sync_action.execute(conn, local_sync_folder);
		if (!res) {
			return res;
		}
	}
	const auto res = conn.send_string("DONE_ACTIONS");
	if (!res) {
		return res;
	}
	return {};
}

std::expected<void, std::string> serve_requests(const NetworkConnection& conn,
												const std::filesystem::path& local_sync_folder) {
	while (true) {
		const auto msg_res = conn.recv_string();
		if (!msg_res) {
			return std::unexpected<std::string>(msg_res.error());
		}
		const std::string& msg = *msg_res;

		if (msg == "DONE_ACTIONS") {
			break;
		} else if (msg.starts_with("REQUEST ")) {
			const std::string relative_path = msg.substr(8);
			const auto full_path = local_sync_folder / relative_path;

			std::ifstream ifs(full_path, std::ios::binary);
			if (!ifs) {
				return std::unexpected<std::string>("Failed to open file for sending: " +
												   full_path.string());
			}

			std::ostringstream file_oss;
			file_oss << ifs.rdbuf();
			const auto s_res = conn.send_string(file_oss.str());
			if (!s_res) {
				return s_res;
			}
		} else {
			return std::unexpected<std::string>("Unknown command: " + msg);
		}
	}
	return {};
}

std::expected<void, std::string> SyncSession::run_client_side(const NetworkConnection& conn) {
	auto& logger = SyncLogger::instance();
	logger.log("Client sync started for: " + local_sync_folder.string());

	// 1. Send Local Directory to Server
	const auto local_dir_res = Directory::try_create(local_sync_folder);
	if (!local_dir_res) {
		return std::unexpected<std::string>("Failed to read local directory");
	}

	std::ostringstream oss;
	local_dir_res->serialize(oss);
	const auto send_res = conn.send_string(oss.str());
	// Template function log_result<T> — instantiated with T=void
	log_result(send_res, "Send local directory tree", logger);
	if (!send_res) {
		return send_res;
	}

	// 2. Serve files the Server requests based on its diff computes
	const auto serve_res = serve_requests(conn, local_sync_folder);
	if (!serve_res) {
		return serve_res;
	}

	// 3. Receive Remote Directory from Server
	const auto rec_res = conn.recv_string();
	// Template function log_result<T> — instantiated with T=std::string
	log_result(rec_res, "Receive remote directory tree", logger);
	if (!rec_res) {
		return std::unexpected<std::string>("Failed to receive directory tree from server: " +
											rec_res.error());
	}

	std::istringstream iss(*rec_res);
	const auto remote_dir_res = Directory::deserialize(iss, std::filesystem::path{""});
	if (!remote_dir_res) {
		return std::unexpected<std::string>("Failed to deserialize directory");
	}

	// 4. Compute what we need from the Server and request it
	const auto new_local_dir_res = Directory::try_create(local_sync_folder);
	if (!new_local_dir_res) {
		return std::unexpected<std::string>("Failed to read local directory after serving");
	}

	const auto actions = compute_diff(*remote_dir_res, *new_local_dir_res);
	const auto act_res = send_actions(conn, local_sync_folder, actions);
	if (!act_res) {
		return act_res;
	}

	logger.log("Client sync completed successfully");
	return {};
}

std::expected<void, std::string> SyncSession::run_server_side(const NetworkConnection& conn) {
	auto& logger = SyncLogger::instance();
	logger.log("Server sync started for: " + local_sync_folder.string());

	// 1. Receive Remote Directory from Client
	const auto rec_res = conn.recv_string();
	// Template function log_result<T> — instantiated with T=std::string
	log_result(rec_res, "Receive client directory tree", logger);
	if (!rec_res) {
		return std::unexpected<std::string>("Failed to receive directory tree from client: " +
											rec_res.error());
	}

	std::istringstream iss(*rec_res);
	const auto remote_dir_res = Directory::deserialize(iss, std::filesystem::path{""});
	if (!remote_dir_res) {
		return std::unexpected<std::string>("Failed to deserialize directory");
	}

	// 2. Compute what Server needs from Client and request it
	const auto local_dir_res = Directory::try_create(local_sync_folder);
	if (!local_dir_res) {
		return std::unexpected<std::string>("Failed to read local directory");
	}

	const auto actions = compute_diff(*remote_dir_res, *local_dir_res);
	const auto act_res = send_actions(conn, local_sync_folder, actions);
	// Template function log_result<T> — instantiated with T=void
	log_result(act_res, "Execute server-side actions", logger);
	if (!act_res) {
		return act_res;
	}

	// 3. Send Local Directory to Client
	const auto new_local_dir_res = Directory::try_create(local_sync_folder);
	if (!new_local_dir_res) {
		return std::unexpected<std::string>("Failed to read local directory after receiving");
	}

	std::ostringstream oss;
	new_local_dir_res->serialize(oss);
	const auto send_res = conn.send_string(oss.str());
	if (!send_res) {
		return send_res;
	}

	// 4. Serve files the Client requests
	const auto serve_res = serve_requests(conn, local_sync_folder);
	if (!serve_res) {
		return serve_res;
	}

	logger.log("Server sync completed successfully");
	return {};
}

std::ostream& operator<<(std::ostream& os, const SyncSession& session) {
	os << "SyncSession(folder=" << session.local_sync_folder
	   << ", verbose=" << (session.verbose ? "yes" : "no")
	   << ", max_retries=" << session.max_retries << ")";
	return os;
}

// ---- Builder Pattern Implementation ----

SyncSessionBuilder::SyncSessionBuilder(std::filesystem::path folder_path)
	: path(std::move(folder_path)) {}

SyncSessionBuilder& SyncSessionBuilder::set_verbose(bool v) {
	verbose = v;
	return *this;
}

SyncSessionBuilder& SyncSessionBuilder::set_max_retries(size_t retries) {
	max_retries = retries;
	return *this;
}

SyncSession SyncSessionBuilder::build() { return SyncSession(std::move(path), verbose, max_retries); }
