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

	std::expected<void, std::string> send_string(const std::string& str) const;
	std::expected<std::string, std::string> recv_string() const;

	// [[nodiscard]] int get_fd() const { return socket_fd; } // Currently unused

	friend std::ostream& operator<<(std::ostream& os, const NetworkConnection& conn);

	// [[nodiscard]] bool is_active() const; // Currently unused
	// void close_connection(); // Currently unused
};

class NetworkServer {
	int server_fd;

   public:
	NetworkServer();
	~NetworkServer();

	std::expected<void, std::string> bind_and_listen(uint16_t port) const;
	std::expected<NetworkConnection, std::string> accept_connection() const;

	friend std::ostream& operator<<(std::ostream& os, const NetworkServer& server);

	// [[nodiscard]] bool is_listening() const; // Currently unused
	void stop();
};

class NetworkClient {
   public:
	static std::expected<NetworkConnection, std::string> connect_to(const std::string& ip,
																	uint16_t port);

	// static bool test_connection(const std::string& ip, uint16_t port); // Currently unused
};

#endif
