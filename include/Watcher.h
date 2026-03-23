#ifndef WATCHER_H
#define WATCHER_H

#include <atomic>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <thread>

class DirectoryWatcher {
	int inotify_fd;
	std::filesystem::path root_path;
	std::atomic<bool> running;
	std::thread watch_thread;

	// Map watch descriptor to path to reconstruct full paths
	std::map<int, std::filesystem::path> wd_to_path;

	void add_watches_recursive(const std::filesystem::path& path);
	void watch_loop(std::function<void()> on_change);

   public:
	explicit DirectoryWatcher(std::filesystem::path path);
	~DirectoryWatcher();

	DirectoryWatcher(const DirectoryWatcher&) = delete;
	DirectoryWatcher& operator=(const DirectoryWatcher&) = delete;

	void start(std::function<void()> on_change);
	void stop();

	// [[nodiscard]] bool is_running() const; // Currently unused
	// [[nodiscard]] const std::filesystem::path& get_watched_path() const; // Currently unused

	friend std::ostream& operator<<(std::ostream& os, const DirectoryWatcher& watcher);
};

#endif
