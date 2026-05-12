#include <fstream>
#include <iostream>
#include <map>
#include <utility>

#include "Directory.h"
#include "SyncAction.h"

// --- CreateDirAction ---
CreateDirAction::CreateDirAction(std::filesystem::path path) : relative_path(std::move(path)) {}
std::unique_ptr<SyncAction> CreateDirAction::clone() const {
	return std::make_unique<CreateDirAction>(*this);
}
std::string CreateDirAction::get_type_string() const { return "CreateDir"; }
std::filesystem::path CreateDirAction::get_path() const { return relative_path; }
std::expected<void, std::string> CreateDirAction::execute(const NetworkConnection&,
														  const std::filesystem::path& base_path) const {
	std::filesystem::create_directories(base_path / relative_path);
	return {};
}
void CreateDirAction::print(std::ostream& os) const {
	os << "CreateDir(" << relative_path << ")";
}

// --- DeleteDirAction ---
DeleteDirAction::DeleteDirAction(std::filesystem::path path) : relative_path(std::move(path)) {}
std::unique_ptr<SyncAction> DeleteDirAction::clone() const {
	return std::make_unique<DeleteDirAction>(*this);
}
std::string DeleteDirAction::get_type_string() const { return "DeleteDir"; }
std::filesystem::path DeleteDirAction::get_path() const { return relative_path; }
std::expected<void, std::string> DeleteDirAction::execute(const NetworkConnection&,
														  const std::filesystem::path& base_path) const {
	std::filesystem::remove_all(base_path / relative_path);
	return {};
}
void DeleteDirAction::print(std::ostream& os) const {
	os << "DeleteDir(" << relative_path << ")";
}

// --- FileAction ---
FileAction::FileAction(std::filesystem::path path) : relative_path(std::move(path)) {}
std::filesystem::path FileAction::get_path() const { return relative_path; }

// --- UpdateFileAction ---
UpdateFileAction::UpdateFileAction(std::filesystem::path path, std::string hash)
	: FileAction(std::move(path)), hash(std::move(hash)) {}
std::unique_ptr<SyncAction> UpdateFileAction::clone() const {
	return std::make_unique<UpdateFileAction>(*this);
}
std::string UpdateFileAction::get_type_string() const { return "UpdateFile"; }
std::expected<void, std::string> UpdateFileAction::execute(const NetworkConnection& conn,
														   const std::filesystem::path& base_path) const {
	const auto req_res = conn.send_string("REQUEST " + relative_path.string());
	if (!req_res) {
		return req_res;
	}

	const auto file_content_res = conn.recv_string();
	if (!file_content_res) {
		return std::unexpected<std::string>("Failed to recv file: " + file_content_res.error());
	}

	const auto target_path = base_path / relative_path;
	std::filesystem::create_directories(target_path.parent_path());
	std::ofstream ofs(target_path, std::ios::binary);
	ofs.write(file_content_res->data(), static_cast<std::streamsize>(file_content_res->size()));
	return {};
}
void UpdateFileAction::print(std::ostream& os) const {
	os << "UpdateFile(" << relative_path << ", hash=" << hash << ")";
}

// --- DeleteFileAction ---
DeleteFileAction::DeleteFileAction(std::filesystem::path path) : FileAction(std::move(path)) {}
std::unique_ptr<SyncAction> DeleteFileAction::clone() const {
	return std::make_unique<DeleteFileAction>(*this);
}
std::string DeleteFileAction::get_type_string() const { return "DeleteFile"; }
std::expected<void, std::string> DeleteFileAction::execute(const NetworkConnection&,
														   const std::filesystem::path& base_path) const {
	std::filesystem::remove(base_path / relative_path);
	return {};
}
void DeleteFileAction::print(std::ostream& os) const {
	os << "DeleteFile(" << relative_path << ")";
}

// --- ConflictFileAction ---
ConflictFileAction::ConflictFileAction(std::filesystem::path path, std::string remote_hash)
	: FileAction(std::move(path)), remote_hash(std::move(remote_hash)) {}
std::unique_ptr<SyncAction> ConflictFileAction::clone() const {
	return std::make_unique<ConflictFileAction>(*this);
}
std::string ConflictFileAction::get_type_string() const { return "ConflictFile"; }
std::expected<void, std::string> ConflictFileAction::execute(
	const NetworkConnection& conn, const std::filesystem::path& base_path) const {
	const auto req_res = conn.send_string("REQUEST " + relative_path.string());
	if (!req_res) {
		return req_res;
	}

	const auto file_content_res = conn.recv_string();
	if (!file_content_res) {
		return std::unexpected<std::string>("Failed to recv file: " + file_content_res.error());
	}

	const auto target_path = base_path / relative_path;
	if (std::filesystem::exists(target_path)) {
		std::filesystem::rename(target_path, target_path.string() + ".conflict");
	}

	std::filesystem::create_directories(target_path.parent_path());
	std::ofstream ofs(target_path, std::ios::binary);
	ofs.write(file_content_res->data(), static_cast<std::streamsize>(file_content_res->size()));
	return {};
}
void ConflictFileAction::print(std::ostream& os) const {
	os << "ConflictFile(" << relative_path << ", remote_hash=" << remote_hash << ")";
}

// --- RenameAction ---
RenameAction::RenameAction(std::filesystem::path old_p, std::filesystem::path new_p)
	: old_path(std::move(old_p)), new_path(std::move(new_p)) {}
std::unique_ptr<SyncAction> RenameAction::clone() const {
	return std::make_unique<RenameAction>(*this);
}
std::string RenameAction::get_type_string() const { return "Rename"; }
std::filesystem::path RenameAction::get_path() const { return new_path; }
std::expected<void, std::string> RenameAction::execute(const NetworkConnection&,
													   const std::filesystem::path& base_path) const {
	std::filesystem::rename(base_path / old_path, base_path / new_path);
	return {};
}
void RenameAction::print(std::ostream& os) const {
	os << "Rename(" << old_path << " -> " << new_path << ")";
}

// --- ActionWrapper ---
ActionWrapper::ActionWrapper(std::unique_ptr<SyncAction> act) : action(std::move(act)) {}
ActionWrapper::ActionWrapper(const ActionWrapper& other)
	: action(other.action ? other.action->clone() : nullptr) {}
ActionWrapper& ActionWrapper::operator=(ActionWrapper other) {
	swap(*this, other);
	return *this;
}
void swap(ActionWrapper& first, ActionWrapper& second) noexcept {
	using std::swap;
	swap(first.action, second.action);
}
const SyncAction& ActionWrapper::get() const { return *action; }
std::ostream& operator<<(std::ostream& os, const ActionWrapper& wrapper) {
	if (wrapper.action) {
		wrapper.action->display(os);
	} else {
		os << "Empty ActionWrapper";
	}
	return os;
}

namespace {

// NOLINTNEXTLINE(misc-no-recursion)
void add_all_local(const Directory& local, std::vector<std::unique_ptr<SyncAction>>& actions,
				   const std::filesystem::path& current_relative) {
	for (const auto& child : local.get_children()) {
		if (std::holds_alternative<Directory>(child)) {
			const auto& dir = std::get<Directory>(child);
			const auto next_rel = current_relative / dir.get_path().filename();
			actions.emplace_back(std::make_unique<CreateDirAction>(next_rel));
			add_all_local(dir, actions, next_rel);
		} else {
			const auto& file = std::get<File>(child);
			const auto next_rel = current_relative / file.get_path().filename();
			actions.emplace_back(std::make_unique<UpdateFileAction>(next_rel, file.get_hash()));
		}
	}
}

// NOLINTNEXTLINE(misc-no-recursion)
void compute_diff_impl(const Directory& local, const Directory& remote,
					   std::vector<std::unique_ptr<SyncAction>>& actions,
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
		if (!local_files.contains(name)) {
			actions.emplace_back(std::make_unique<DeleteFileAction>(current_relative / name));
		}
	}
	for (const auto& [name, r_dir] : remote_dirs) {
		if (!local_dirs.contains(name)) {
			actions.emplace_back(std::make_unique<DeleteDirAction>(current_relative / name));
		}
	}

	// Add/Update what is in local
	for (const auto& [name, l_dir] : local_dirs) {
		const auto next_relative = current_relative / name;
		const auto r_it = remote_dirs.find(name);
		if (r_it == remote_dirs.end()) {
			actions.emplace_back(std::make_unique<CreateDirAction>(next_relative));
			add_all_local(*l_dir, actions, next_relative);
		} else {
			compute_diff_impl(*l_dir, *r_it->second, actions, next_relative);
		}
	}

	for (const auto& [name, l_file] : local_files) {
		const auto next_relative = current_relative / name;
		const auto r_it = remote_files.find(name);
		if (r_it != remote_files.end() && r_it->second->get_hash() != l_file->get_hash()) {
			actions.emplace_back(
				std::make_unique<ConflictFileAction>(next_relative, r_it->second->get_hash()));
		} else if (r_it == remote_files.end()) {
			actions.emplace_back(std::make_unique<UpdateFileAction>(next_relative, l_file->get_hash()));
		}
	}
}

}  // namespace

std::vector<std::unique_ptr<SyncAction>> compute_diff(const Directory& local,
													  const Directory& remote) {
	std::vector<std::unique_ptr<SyncAction>> actions;
	compute_diff_impl(local, remote, actions, std::filesystem::path{""});
	return actions;
}
