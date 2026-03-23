#ifndef NETWORK_H
#define NETWORK_H

#include <cstdint>
#include <expected>
#include <iostream>
#include <string>
#include <vector>

class NetworkConnection {
	int socket_fd;

   public:
	explicit NetworkConnection(int fd);
	~NetworkConnection();

	NetworkConnection(const NetworkConnection&) = delete;
	NetworkConnection& operator=(const NetworkConnection&) = delete;

	NetworkConnection(NetworkConnection&& other) noexcept;
	NetworkConnection& operator=(NetworkConnection&& other) noexcept;

	std::expected<void, std::string> send_exact(const void* data, size_t len);
	std::expected<void, std::string> recv_exact(void* data, size_t len);

	std::expected<void, std::string> send_string(const std::string& str);
	std::expected<std::string, std::string> recv_string();

	[[nodiscard]] int get_fd() const { return socket_fd; }

	friend std::ostream& operator<<(std::ostream& os, const NetworkConnection& conn);

	[[nodiscard]] bool is_active() const;
	void close_connection();
};

class NetworkServer {
	int server_fd;

   public:
	NetworkServer();
	~NetworkServer();

	std::expected<void, std::string> bind_and_listen(uint16_t port);
	std::expected<NetworkConnection, std::string> accept_connection();

	friend std::ostream& operator<<(std::ostream& os, const NetworkServer& server);

	[[nodiscard]] bool is_listening() const;
	void stop();
};

class NetworkClient {
   public:
	static std::expected<NetworkConnection, std::string> connect_to(const std::string& ip,
																	uint16_t port);

	static bool test_connection(const std::string& ip, uint16_t port);
};

#endif
