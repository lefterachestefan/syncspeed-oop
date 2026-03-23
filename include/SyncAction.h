#ifndef SYNCACTION_H
#define SYNCACTION_H

#include <filesystem>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

namespace Sync {

struct CreateDir {
	std::filesystem::path relative_path;
};

struct DeleteDir {
	std::filesystem::path relative_path;
};

struct UpdateFile {
	std::filesystem::path relative_path;
	std::string hash;
};

struct DeleteFile {
	std::filesystem::path relative_path;
};

struct ConflictFile {
	std::filesystem::path relative_path;
	std::string remote_hash;
};

using Action = std::variant<CreateDir, DeleteDir, UpdateFile, DeleteFile, ConflictFile>;

}  // namespace Sync

class Directory;

class SyncAction {
	Sync::Action action;

   public:
	explicit SyncAction(Sync::Action action);

	[[nodiscard]] const Sync::Action& get_action() const;

	friend std::ostream& operator<<(std::ostream& os, const SyncAction& action);

	[[nodiscard]] std::string get_type_string() const;
	[[nodiscard]] std::filesystem::path get_path() const;
};

std::vector<SyncAction> compute_diff(const Directory& local, const Directory& remote);

#endif
