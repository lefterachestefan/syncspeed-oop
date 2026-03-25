#include <iostream>
#include <map>

#include "Directory.h"
#include "SyncAction.h"

SyncAction::SyncAction(Sync::Action action) : action(std::move(action)) {}

const Sync::Action& SyncAction::get_action() const { return action; }

std::string SyncAction::get_type_string() const {
	return std::visit(
		[](auto&& arg) -> std::string {
			using T = std::decay_t<decltype(arg)>;
			if constexpr (std::is_same_v<T, Sync::CreateDir>) {
				return "CreateDir";
			} else if constexpr (std::is_same_v<T, Sync::DeleteDir>) {
				return "DeleteDir";
			} else if constexpr (std::is_same_v<T, Sync::UpdateFile>) {
				return "UpdateFile";
			} else if constexpr (std::is_same_v<T, Sync::DeleteFile>) {
				return "DeleteFile";
			} else if constexpr (std::is_same_v<T, Sync::ConflictFile>) {
				return "ConflictFile";
			} else {
				return "Unknown";
			}
		},
		action);
}

std::filesystem::path SyncAction::get_path() const {
	return std::visit([](auto&& arg) -> std::filesystem::path { return arg.relative_path; },
					  action);
}

std::ostream& operator<<(std::ostream& os, const SyncAction& sync_action) {
	os << "SyncAction(" << sync_action.get_type_string() << ", path=" << sync_action.get_path()
	   << ")";
	return os;
}

namespace {

// NOLINTNEXTLINE(misc-no-recursion)
void add_all_local(const Directory& local, std::vector<SyncAction>& actions,
				   const std::filesystem::path& current_relative) {
	for (const auto& child : local.get_children()) {
		if (std::holds_alternative<Directory>(child)) {
			const auto& dir = std::get<Directory>(child);
			auto next_rel = current_relative / dir.get_path().filename();
			actions.emplace_back(Sync::CreateDir{next_rel});
			add_all_local(dir, actions, next_rel);
		} else {
			const auto& file = std::get<File>(child);
			auto next_rel = current_relative / file.get_path().filename();
			actions.emplace_back(Sync::UpdateFile{next_rel, file.get_hash()});
		}
	}
}

// NOLINTNEXTLINE(misc-no-recursion)
void compute_diff_impl(const Directory& local, const Directory& remote,
					   std::vector<SyncAction>& actions,
					   const std::filesystem::path& current_relative) {
	std::map<std::string, const Directory*> local_dirs;
	std::map<std::string, const File*> local_files;

	for (const auto& child : local.get_children()) {
		if (std::holds_alternative<Directory>(child)) {
			const auto& dir = std::get<Directory>(child);
			local_dirs[dir.get_path().filename().string()] = &dir;
		} else {
			const auto& file = std::get<File>(child);
			local_files[file.get_path().filename().string()] = &file;
		}
	}

	std::map<std::string, const Directory*> remote_dirs;
	std::map<std::string, const File*> remote_files;

	for (const auto& child : remote.get_children()) {
		if (std::holds_alternative<Directory>(child)) {
			const auto& dir = std::get<Directory>(child);
			remote_dirs[dir.get_path().filename().string()] = &dir;
		} else {
			const auto& file = std::get<File>(child);
			remote_files[file.get_path().filename().string()] = &file;
		}
	}

	// Remove what is strictly remote
	for (const auto& [name, r_file] : remote_files) {
		if (local_files.find(name) == local_files.end()) {
			actions.emplace_back(Sync::DeleteFile{current_relative / name});
		}
	}
	for (const auto& [name, r_dir] : remote_dirs) {
		if (local_dirs.find(name) == local_dirs.end()) {
			actions.emplace_back(Sync::DeleteDir{current_relative / name});
		}
	}

	// Add/Update what is in local
	for (const auto& [name, l_dir] : local_dirs) {
		auto next_relative = current_relative / name;
		auto r_it = remote_dirs.find(name);
		if (r_it == remote_dirs.end()) {
			actions.emplace_back(Sync::CreateDir{next_relative});
			add_all_local(*l_dir, actions, next_relative);
		} else {
			compute_diff_impl(*l_dir, *r_it->second, actions, next_relative);
		}
	}

	for (const auto& [name, l_file] : local_files) {
		auto next_relative = current_relative / name;
		auto r_it = remote_files.find(name);
		if (r_it != remote_files.end() && r_it->second->get_hash() != l_file->get_hash()) {
			// TODO: right now any file diff is treated as conflict, check timestamp later
			actions.emplace_back(Sync::ConflictFile{next_relative, r_it->second->get_hash()});
		} else if (r_it == remote_files.end()) {
			actions.emplace_back(Sync::UpdateFile{next_relative, l_file->get_hash()});
		}
	}
}

}  // namespace

std::vector<SyncAction> compute_diff(const Directory& local, const Directory& remote) {
	std::vector<SyncAction> actions;
	compute_diff_impl(local, remote, actions, std::filesystem::path{""});
	return actions;
}
