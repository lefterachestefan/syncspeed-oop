#ifndef WATCHER_H
#define WATCHER_H

#include <atomic>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <thread>

class DirectoryWatcher {
#ifdef __linux__
	int inotify_fd;
#endif
	std::filesystem::path root_path;
	std::atomic<bool> running;
	std::thread watch_thread;

#ifdef __linux__
	// Map watch descriptor to path to reconstruct full paths (Linux)
	// I also don't know if my program is bad or whatever but the watch for change is really bad on
	// any platform
	std::map<int, std::filesystem::path> wd_to_path;
#else
	// For polling-based watcher (Windows/macOS)
	std::map<std::filesystem::path, std::filesystem::file_time_type> last_write_times;
#endif

#ifdef __linux__
	void add_watches_recursive(const std::filesystem::path& path);
#else
	bool poll_changes();
#endif
	void watch_loop(const std::function<void()>& on_change);

   public:
	explicit DirectoryWatcher(std::filesystem::path path);
	~DirectoryWatcher();

	DirectoryWatcher(const DirectoryWatcher&) = delete;
	DirectoryWatcher& operator=(const DirectoryWatcher&) = delete;

	void start(const std::function<void()>& on_change);
	void stop();

	friend std::ostream& operator<<(std::ostream& os, const DirectoryWatcher& watcher);
};

#endif
