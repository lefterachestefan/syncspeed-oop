#include <expected>
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

#include "Directory.h"
#include "SyncAction.h"
#include "SyncSession.h"

SyncSession::SyncSession(std::filesystem::path local_path)
	: local_sync_folder(std::move(local_path)) {}

// const std::filesystem::path& SyncSession::get_local_path() const { return local_sync_folder; } //
// Currently unused

std::expected<void, std::string> send_actions(const NetworkConnection& conn,
											  const std::filesystem::path& local_sync_folder,
											  const std::vector<SyncAction>& actions) {
	for (const auto& sync_action : actions) {
		const auto& action = sync_action.get_action();
		if (std::holds_alternative<Sync::DeleteFile>(action)) {
			const auto& act = std::get<Sync::DeleteFile>(action);
			std::filesystem::remove(local_sync_folder / act.relative_path);
		} else if (std::holds_alternative<Sync::DeleteDir>(action)) {
			const auto& act = std::get<Sync::DeleteDir>(action);
			std::filesystem::remove_all(local_sync_folder / act.relative_path);
		} else if (std::holds_alternative<Sync::CreateDir>(action)) {
			const auto& act = std::get<Sync::CreateDir>(action);
			std::filesystem::create_directories(local_sync_folder / act.relative_path);
		} else if (std::holds_alternative<Sync::UpdateFile>(action) ||
				   std::holds_alternative<Sync::ConflictFile>(action)) {
			std::filesystem::path rel_path;
			bool is_conflict = false;

			if (std::holds_alternative<Sync::UpdateFile>(action)) {
				rel_path = std::get<Sync::UpdateFile>(action).relative_path;
			} else {
				rel_path = std::get<Sync::ConflictFile>(action).relative_path;
				is_conflict = true;
			}

			auto req_res = conn.send_string("REQUEST " + rel_path.string());
			if (!req_res) {
				return req_res;
			}

			auto file_content_res = conn.recv_string();
			if (!file_content_res) {
				return std::unexpected<std::string>("Failed to recv file: " +
													file_content_res.error());
			}

			auto target_path = local_sync_folder / rel_path;
			if (is_conflict) {
				std::filesystem::rename(target_path, target_path.string() + ".conflict");
			}

			std::filesystem::create_directories(target_path.parent_path());
			std::ofstream ofs(target_path, std::ios::binary);
			// TODO: check later for all CPU's if this conversion to long is okay
			ofs.write(file_content_res->data(), (long)file_content_res->size());
		}
	}
	auto res = conn.send_string("DONE_ACTIONS");
	if (!res) {
		return res;
	}
	return {};
}

std::expected<void, std::string> serve_requests(NetworkConnection& conn,
												const std::filesystem::path& local_sync_folder) {
	while (true) {
		auto msg_res = conn.recv_string();
		if (!msg_res) {
			return std::unexpected<std::string>(msg_res.error());
		}
		std::string msg = *msg_res;

		if (msg == "DONE_ACTIONS") {
			break;
		} else if (msg.starts_with("REQUEST ")) {
			std::string relative_path = msg.substr(8);
			auto full_path = local_sync_folder / relative_path;

			std::ifstream ifs(full_path, std::ios::binary);
			if (!ifs) {
				return std::unexpected<std::string>("Failed to open file for sending: " +
													full_path.string());
			}

			std::ostringstream file_oss;
			file_oss << ifs.rdbuf();
			auto s_res = conn.send_string(file_oss.str());
			if (!s_res) {
				return s_res;
			}
		} else {
			return std::unexpected<std::string>("Unknown command: " + msg);
		}
	}
	return {};
}

std::expected<void, std::string> SyncSession::run_client_side(NetworkConnection& conn) {
	// 1. Send Local Directory to Server
	auto local_dir_res = Directory::try_create(local_sync_folder);
	if (!local_dir_res) {
		return std::unexpected<std::string>("Failed to read local directory");
	}

	std::ostringstream oss;
	local_dir_res->serialize(oss);
	auto send_res = conn.send_string(oss.str());
	if (!send_res) {
		return send_res;
	}

	// 2. Serve files the Server requests based on its diff computes
	auto serve_res = serve_requests(conn, local_sync_folder);
	if (!serve_res) {
		return serve_res;
	}

	// 3. Receive Remote Directory from Server
	auto rec_res = conn.recv_string();
	if (!rec_res) {
		return std::unexpected<std::string>("Failed to receive directory tree from server: " +
											rec_res.error());
	}

	std::istringstream iss(*rec_res);
	auto remote_dir_res = Directory::deserialize(iss, std::filesystem::path{""});
	if (!remote_dir_res) {
		return std::unexpected<std::string>("Failed to deserialize directory");
	}

	// 4. Compute what we need from the Server and request it
	auto new_local_dir_res = Directory::try_create(local_sync_folder);
	if (!new_local_dir_res) {
		return std::unexpected<std::string>("Failed to read local directory after serving");
	}

	auto actions = compute_diff(*remote_dir_res, *new_local_dir_res);
	auto act_res = send_actions(conn, local_sync_folder, actions);
	if (!act_res) {
		return act_res;
	}

	return {};
}

std::expected<void, std::string> SyncSession::run_server_side(NetworkConnection& conn) {
	// 1. Receive Remote Directory from Client
	auto rec_res = conn.recv_string();
	if (!rec_res) {
		return std::unexpected<std::string>("Failed to receive directory tree from client: " +
											rec_res.error());
	}

	std::istringstream iss(*rec_res);
	auto remote_dir_res = Directory::deserialize(iss, std::filesystem::path{""});
	if (!remote_dir_res) {
		return std::unexpected<std::string>("Failed to deserialize directory");
	}

	// 2. Compute what Server needs from Client and request it
	auto local_dir_res = Directory::try_create(local_sync_folder);
	if (!local_dir_res) {
		return std::unexpected<std::string>("Failed to read local directory");
	}

	auto actions = compute_diff(*remote_dir_res, *local_dir_res);
	auto act_res = send_actions(conn, local_sync_folder, actions);
	if (!act_res) {
		return act_res;
	}

	// 3. Send Local Directory to Client
	auto new_local_dir_res = Directory::try_create(local_sync_folder);
	if (!new_local_dir_res) {
		return std::unexpected<std::string>("Failed to read local directory after receiving");
	}

	std::ostringstream oss;
	new_local_dir_res->serialize(oss);
	auto send_res = conn.send_string(oss.str());
	if (!send_res) {
		return send_res;
	}

	// 4. Serve files the Client requests
	auto serve_res = serve_requests(conn, local_sync_folder);
	if (!serve_res) {
		return serve_res;
	}

	return {};
}

/*
std::expected<void, std::string> SyncSession::server_sync( // Currently unused
	NetworkConnection& conn, const std::filesystem::path& local_sync_folder) {
	SyncSession session(local_sync_folder);
	return session.run_server_side(conn);
}
*/

// Currently unused
/*
std::expected<void, std::string> SyncSession::client_sync(
	NetworkConnection& conn, const std::filesystem::path& local_sync_folder) {
	SyncSession session(local_sync_folder);
	return session.run_client_side(conn);
}
*/

std::ostream& operator<<(std::ostream& os, const SyncSession& session) {
	os << "SyncSession(folder=" << session.local_sync_folder << ")";
	return os;
}
