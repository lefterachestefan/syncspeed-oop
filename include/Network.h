#ifndef NETWORK_H
#define NETWORK_H

#include <cstdint>
#include <expected>
#include <iostream>
#include <string>

class NetworkConnection {
	int socket_fd;

   public:
	explicit NetworkConnection(int fd);
	~NetworkConnection();

	NetworkConnection(const NetworkConnection&) = delete;
	NetworkConnection& operator=(const NetworkConnection&) = delete;

	NetworkConnection(NetworkConnection&& other) noexcept;
	NetworkConnection& operator=(NetworkConnection&& other) noexcept;

	std::expected<void, std::string> send_exact(const void* data, size_t len) const;
	std::expected<void, std::string> recv_exact(void* data, size_t len) const;

	[[nodiscard]] std::expected<void, std::string> send_string(const std::string& str) const;
	[[nodiscard]] std::expected<std::string, std::string> recv_string() const;

	friend std::ostream& operator<<(std::ostream& os, const NetworkConnection& conn);
};

class NetworkServer {
	int server_fd;

   public:
	NetworkServer();
	~NetworkServer();

	[[nodiscard]] std::expected<void, std::string> bind_and_listen(uint16_t port) const;
	[[nodiscard]] std::expected<NetworkConnection, std::string> accept_connection() const;

	friend std::ostream& operator<<(std::ostream& os, const NetworkServer& server);

	void stop();
};

class NetworkClient {
   public:
	static std::expected<NetworkConnection, std::string> connect_to(const std::string& ip,
																	uint16_t port);
};

#endif
