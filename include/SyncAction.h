#ifndef SYNCACTION_H
#define SYNCACTION_H

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <expected>

#include "Network.h"

class SyncAction {
   public:
	virtual ~SyncAction() = default;
	[[nodiscard]] virtual std::unique_ptr<SyncAction> clone() const = 0;

	// NVI
	void display(std::ostream& os) const { print(os); }

	[[nodiscard]] virtual std::string get_type_string() const = 0;
	[[nodiscard]] virtual std::filesystem::path get_path() const = 0;

	// Theme specific virtual function
	[[nodiscard]] virtual std::expected<void, std::string> execute(
		const NetworkConnection& conn, const std::filesystem::path& base_path) const = 0;

   protected:
	virtual void print(std::ostream& os) const = 0;
};

class CreateDirAction : public SyncAction {
	std::filesystem::path relative_path;

   public:
	explicit CreateDirAction(std::filesystem::path path);
	[[nodiscard]] std::unique_ptr<SyncAction> clone() const override;
	[[nodiscard]] std::string get_type_string() const override;
	[[nodiscard]] std::filesystem::path get_path() const override;
	[[nodiscard]] std::expected<void, std::string> execute(
		const NetworkConnection& conn, const std::filesystem::path& base_path) const override;

   protected:
	void print(std::ostream& os) const override;
};

class DeleteDirAction : public SyncAction {
	std::filesystem::path relative_path;

   public:
	explicit DeleteDirAction(std::filesystem::path path);
	[[nodiscard]] std::unique_ptr<SyncAction> clone() const override;
	[[nodiscard]] std::string get_type_string() const override;
	[[nodiscard]] std::filesystem::path get_path() const override;
	[[nodiscard]] std::expected<void, std::string> execute(
		const NetworkConnection& conn, const std::filesystem::path& base_path) const override;

   protected:
	void print(std::ostream& os) const override;
};

class FileAction : public SyncAction {
   protected:
	std::filesystem::path relative_path;

   public:
	explicit FileAction(std::filesystem::path path);
	[[nodiscard]] std::filesystem::path get_path() const override;
};

class UpdateFileAction : public FileAction {
	std::string hash;

   public:
	UpdateFileAction(std::filesystem::path path, std::string hash);
	[[nodiscard]] std::unique_ptr<SyncAction> clone() const override;
	[[nodiscard]] std::string get_type_string() const override;
	[[nodiscard]] std::expected<void, std::string> execute(
		const NetworkConnection& conn, const std::filesystem::path& base_path) const override;

   protected:
	void print(std::ostream& os) const override;
};

class DeleteFileAction : public FileAction {
   public:
	explicit DeleteFileAction(std::filesystem::path path);
	[[nodiscard]] std::unique_ptr<SyncAction> clone() const override;
	[[nodiscard]] std::string get_type_string() const override;
	[[nodiscard]] std::expected<void, std::string> execute(
		const NetworkConnection& conn, const std::filesystem::path& base_path) const override;

   protected:
	void print(std::ostream& os) const override;
};

class ConflictFileAction : public FileAction {
	std::string remote_hash;

   public:
	ConflictFileAction(std::filesystem::path path, std::string remote_hash);
	[[nodiscard]] std::unique_ptr<SyncAction> clone() const override;
	[[nodiscard]] std::string get_type_string() const override;
	[[nodiscard]] std::expected<void, std::string> execute(
		const NetworkConnection& conn, const std::filesystem::path& base_path) const override;

   protected:
	void print(std::ostream& os) const override;
};

class RenameAction : public SyncAction {
	std::filesystem::path old_path;
	std::filesystem::path new_path;

   public:
	RenameAction(std::filesystem::path old_p, std::filesystem::path new_p);
	[[nodiscard]] std::unique_ptr<SyncAction> clone() const override;
	[[nodiscard]] std::string get_type_string() const override;
	[[nodiscard]] std::filesystem::path get_path() const override;
	[[nodiscard]] std::expected<void, std::string> execute(
		const NetworkConnection& conn, const std::filesystem::path& base_path) const override;

   protected:
	void print(std::ostream& os) const override;
};

class LogAction : public SyncAction {
	std::string message;

   public:
	explicit LogAction(std::string msg);
	[[nodiscard]] std::unique_ptr<SyncAction> clone() const override;
	[[nodiscard]] std::string get_type_string() const override;
	[[nodiscard]] std::filesystem::path get_path() const override;
	[[nodiscard]] std::expected<void, std::string> execute(
		const NetworkConnection& conn, const std::filesystem::path& base_path) const override;

   protected:
	void print(std::ostream& os) const override;
};

// Class with pointer to base for T2 requirement
class ActionWrapper {
	std::unique_ptr<SyncAction> action;

   public:
	explicit ActionWrapper(std::unique_ptr<SyncAction> act);
	ActionWrapper(const ActionWrapper& other);
	ActionWrapper& operator=(ActionWrapper other);
	~ActionWrapper() = default;

	friend void swap(ActionWrapper& first, ActionWrapper& second) noexcept;

	[[nodiscard]] const SyncAction& get() const;

	[[nodiscard]] std::expected<void, std::string> execute(
		const NetworkConnection& conn, const std::filesystem::path& base_path) const;

	friend std::ostream& operator<<(std::ostream& os, const ActionWrapper& wrapper);
};

class Directory;
std::vector<ActionWrapper> compute_diff(const Directory& local, const Directory& remote);

#endif
