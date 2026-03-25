#include <atomic>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include "include/Device.h"
#include "include/Directory.h"
#include "include/Network.h"
#include "include/SyncAction.h"
#include "include/SyncSession.h"
#include "include/Watcher.h"

int main([[maybe_unused]] int argc, [[maybe_unused]] char** argv) {
#if defined(_WIN32) || defined(_WIN64)
	std::cout << "Windows is not yet implemented\n";
	return 0;
#elif defined(__APPLE__) || defined(__MACH__)
	std::cout << "macOS is not yet implemented\n";
	return 0;
#else  // Linux (hopefully)

	if (argc < 2) {
		std::cout << "Usage:\n";
		std::cout << "  " << argv[0] << " server <port> <folder>\n";
		std::cout << "  " << argv[0] << " client <ip> <port> <folder>\n";
		std::cout << "  " << argv[0] << " info <folder> (displays OOP info about folder)\n";
		return 0;
	}

	std::string mode = argv[1];

	if (mode == "info") {
		if (argc < 3) {
			std::cerr << "Usage: " << argv[0] << " info <folder>\n";
			return 1;
		}
		std::filesystem::path folder = argv[2];
		auto dir_res = Directory::try_create(folder);
		if (!dir_res) {
			std::cerr << "Error reading directory\n";
			return 1;
		}
		std::cout << "Directory Info:\n" << *dir_res << "\n";
		std::cout << "Files: " << dir_res->count_files() << "\n";
		std::cout << "Subdirs: " << dir_res->count_directories() << "\n";

		Device dev("local-device");
		auto sync_res = dev.sync_folder(folder);
		if (sync_res) {
			std::cout << "Device status: " << dev << "\n";
		}
		return 0;
	}

	if (mode == "server") {
		if (argc < 4) {
			return 1;
		}
		uint16_t port = std::stoi(argv[2]);
		std::filesystem::path folder = argv[3];
		std::filesystem::create_directories(folder);

		NetworkServer server;
		auto res = server.bind_and_listen(port);
		if (!res) {
			std::cerr << "Server error: " << res.error() << "\n";
			return 1;
		}
		std::cout << server << " listening on port " << port << "...\n";

		SyncSession session(folder);
		while (true) {
			auto client_res = server.accept_connection();
			if (!client_res) {
				std::cerr << "Accept error: " << client_res.error() << "\n";
				continue;
			}
			std::cout << "Client connected: " << *client_res << "\n";
			auto sync_res = session.run_server_side(*client_res);
			if (!sync_res) {
				std::cerr << "Sync session failed: " << sync_res.error() << "\n";
			} else {
				std::cout << "Sync session completed successfully.\n";
			}
		}
	} else if (mode == "client") {
		if (argc < 5) {
			return 1;
		}
		std::string ip = argv[2];
		uint16_t port = std::stoi(argv[3]);
		std::filesystem::path folder = argv[4];
		std::filesystem::create_directories(folder);

		DirectoryWatcher watcher(folder);
		std::cout << "Initiated " << watcher << "\n";

		std::atomic<bool> trigger_sync{true};
		std::mutex mtx;

		watcher.start([&trigger_sync, &mtx]() {
			std::lock_guard<std::mutex> lock(mtx);
			trigger_sync = true;
		});

		SyncSession session(folder);
		while (true) {
			bool should_sync = false;
			{
				std::lock_guard<std::mutex> lock(mtx);
				should_sync = trigger_sync;
				trigger_sync = false;
			}

			if (should_sync) {
				std::cout << "Changes detected. Connecting to " << ip << ":" << port << "...\n";
				auto conn_res = NetworkClient::connect_to(ip, port);
				if (!conn_res) {
					std::cerr << "Client connection error: " << conn_res.error() << "\n";
				} else {
					auto sync_res = session.run_client_side(*conn_res);
					if (!sync_res) {
						std::cerr << "Sync failed: " << sync_res.error() << "\n";
					} else {
						std::cout << "Sync successful.\n";
					}
				}
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
	}

	return 0;
#endif
}
