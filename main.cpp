#include <atomic>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include <fmt/core.h>
#include <fmt/ostream.h>

#include "include/Config.h"
#include "include/Device.h"
#include "include/Directory.h"
#include "include/Network.h"
#include "include/SyncAction.h"
#include "include/SyncException.h"
#include "include/SyncSession.h"
#include "include/Watcher.h"

template <> struct fmt::formatter<Directory> : ostream_formatter {};
template <> struct fmt::formatter<Device> : ostream_formatter {};
template <> struct fmt::formatter<NetworkServer> : ostream_formatter {};
template <> struct fmt::formatter<NetworkConnection> : ostream_formatter {};
template <> struct fmt::formatter<DirectoryWatcher> : ostream_formatter {};

int main(const int argc, const char** argv) {
	try {
		Config config("config.json");

		if (argc < 2) {
			fmt::print("Usage:\n");
			fmt::print("  {} server <port> <folder>\n", argv[0]);
			fmt::print("  {} client <ip> <port> <folder>\n", argv[0]);
			fmt::print("  {} info <folder> (displays OOP info about folder)\n", argv[0]);
			return 0;
		}

		const std::string mode = argv[1];

		if (mode == "info") {
			if (argc < 3) {
				fmt::print(stderr, "Usage: {} info <folder>\n", argv[0]);
				return 1;
			}
			const std::filesystem::path folder = argv[2];
			const auto dir_res = Directory::try_create(folder);
			if (!dir_res) {
				throw FileSystemError("Error reading directory: " + folder.string());
			}
			fmt::print("Directory Info:\n{}\n", *dir_res);
			fmt::print("Files: {}\n", dir_res->count_files());
			fmt::print("Subdirs: {}\n", dir_res->count_directories());

			Device dev(config.get_device_name());
			const auto sync_res = dev.sync_folder(folder);
			if (sync_res) {
				fmt::print("Device status: {}\n", dev);
			}
			fmt::print("Total devices active: {}\n", Device::get_total_devices());
			return 0;
		}

		if (mode == "server") {
			if (argc < 4) {
				return 1;
			}
			const uint16_t port = static_cast<uint16_t>(std::stoi(argv[2]));
			const std::filesystem::path folder = argv[3];
			std::filesystem::create_directories(folder);

			const NetworkServer server;
			const auto res = server.bind_and_listen(port);
			if (!res) {
				throw NetworkError("Server bind/listen failed: " + res.error());
			}
			fmt::print("{} listening on port {}...\n", server, port);

			SyncSession session(folder);
			while (true) {
				const auto client_res = server.accept_connection();
				if (!client_res) {
					fmt::print(stderr, "Accept error: {}\n", client_res.error());
					continue;
				}
				fmt::print("Client connected: {}\n", *client_res);
				const auto sync_res = session.run_server_side(*client_res);
				if (!sync_res) {
					fmt::print(stderr, "Sync session failed: {}\n", sync_res.error());
				} else {
					fmt::print("Sync session completed successfully.\n");
				}
			}
		} else if (mode == "client") {
			if (argc < 5) {
				return 1;
			}
			const std::string ip = argv[2];
			const uint16_t port = static_cast<uint16_t>(std::stoi(argv[3]));
			const std::filesystem::path folder = argv[4];
			std::filesystem::create_directories(folder);

			DirectoryWatcher watcher(folder);
			fmt::print("Initiated {}\n", watcher);

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
					fmt::print("Changes detected. Connecting to {}:{}...\n", ip, port);
					const auto conn_res = NetworkClient::connect_to(ip, port);
					if (!conn_res) {
						fmt::print(stderr, "Client connection error: {}\n", conn_res.error());
					} else {
						const auto sync_res = session.run_client_side(*conn_res);
						if (!sync_res) {
							fmt::print(stderr, "Sync failed: {}\n", sync_res.error());
						} else {
							fmt::print("Sync successful.\n");
						}
					}
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
			}
		}
	} catch (const SyncError& e) {
		fmt::print(stderr, "Sync Error: {}\n", e.what());
		return 1;
	} catch (const std::exception& e) {
		fmt::print(stderr, "General Error: {}\n", e.what());
		return 1;
	}

	return 0;
}
