#include <poll.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <iostream>
#include <stdexcept>
#include <utility>

#include "Watcher.h"

DirectoryWatcher::DirectoryWatcher(std::filesystem::path path)
	: root_path(std::move(path)), running(false) {
	inotify_fd = inotify_init1(IN_NONBLOCK);
	if (inotify_fd < 0) {
		throw std::runtime_error("Failed to initialize inotify");
	}
}

DirectoryWatcher::~DirectoryWatcher() {
	stop();
	if (inotify_fd >= 0) {
		close(inotify_fd);
	}
}

void DirectoryWatcher::add_watches_recursive(const std::filesystem::path& path) {
	int wd = inotify_add_watch(inotify_fd, path.c_str(),
							   IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO);
	if (wd >= 0) {
		wd_to_path[wd] = path;
	}

	if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
		for (const auto& entry : std::filesystem::directory_iterator(path)) {
			if (entry.is_directory()) {
				add_watches_recursive(entry.path());
			}
		}
	}
}

void DirectoryWatcher::watch_loop(std::function<void()> on_change) {
	const size_t buf_size = 4096;
	char buffer[buf_size];

	struct pollfd pfd;
	pfd.fd = inotify_fd;
	pfd.events = POLLIN;

	while (running) {
		int ret = poll(&pfd, 1, 500);  // Wait up to 500ms
		if (ret < 0) {
			break;
		}
		if (ret == 0) {
			continue;  // Timeout
		}

		if (pfd.revents & POLLIN) {
			ssize_t len = read(inotify_fd, buffer, buf_size);
			if (len < 0) {
				continue;
			}

			bool triggers_change = false;
			for (char* ptr = buffer; ptr < buffer + len;) {
				const auto* event = reinterpret_cast<const struct inotify_event*>(ptr);
				ptr += sizeof(struct inotify_event) + event->len;

				if (event->len > 0) {
					std::string name(event->name);
					if (!name.ends_with(".conflict")) {
						triggers_change = true;
					}
				} else {
					triggers_change = true;
				}
			}

			if (triggers_change) {
				on_change();
			}
		}
	}
}

void DirectoryWatcher::start(std::function<void()> on_change) {
	if (running) {
		return;
	}

	wd_to_path.clear();
	add_watches_recursive(root_path);

	running = true;
	watch_thread = std::thread([this, on_change]() { this->watch_loop(on_change); });
}

void DirectoryWatcher::stop() {
	running = false;
	if (watch_thread.joinable()) {
		watch_thread.join();
	}
}

// bool DirectoryWatcher::is_running() const { return running; } // Currently unused

// const std::filesystem::path& DirectoryWatcher::get_watched_path() const { return root_path; } // Currently unused

std::ostream& operator<<(std::ostream& os, const DirectoryWatcher& watcher) {
	os << "DirectoryWatcher(path=" << watcher.root_path
	   << ", running=" << (watcher.running ? "yes" : "no") << ")";
	return os;
}
