#ifdef __linux__
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <cerrno>
#include <cstring>
#include <expected>
#include <iostream>
#include <stdexcept>

#include "Network.h"

// --- NetworkConnection ---

NetworkConnection::NetworkConnection(int fd) : socket_fd(fd) {}

NetworkConnection::~NetworkConnection() {
#ifdef __linux__
	if (socket_fd >= 0) {
		close(socket_fd);
	}
#endif
}

NetworkConnection::NetworkConnection(NetworkConnection&& other) noexcept
	: socket_fd(other.socket_fd) {
	other.socket_fd = -1;
}

NetworkConnection& NetworkConnection::operator=(NetworkConnection&& other) noexcept {
	if (this != &other) {
#ifdef __linux__
		if (socket_fd >= 0) {
			close(socket_fd);
		}
#endif
		socket_fd = other.socket_fd;
		other.socket_fd = -1;
	}
	return *this;
}

std::expected<void, std::string> NetworkConnection::send_exact([[maybe_unused]] const void* data,
															   [[maybe_unused]] size_t len) const {
#ifdef __linux__
	const char* ptr = static_cast<const char*>(data);
	size_t bytes_sent = 0;
	while (bytes_sent < len) {
		ssize_t ret = send(socket_fd, ptr + bytes_sent, len - bytes_sent, 0);
		if (ret < 0) {
			return std::unexpected<std::string>("send failed: " + std::string(strerror(errno)));
		}
		bytes_sent += ret;
	}
	return {};
#else
	return std::unexpected<std::string>("Not implemented on this platform");
#endif
}

std::expected<void, std::string> NetworkConnection::recv_exact([[maybe_unused]] void* data,
															   [[maybe_unused]] size_t len) const {
#ifdef __linux__
	char* ptr = static_cast<char*>(data);
	size_t bytes_recv = 0;
	while (bytes_recv < len) {
		ssize_t ret = recv(socket_fd, ptr + bytes_recv, len - bytes_recv, 0);
		if (ret < 0) {
			return std::unexpected<std::string>("recv failed: " + std::string(strerror(errno)));
		}
		if (ret == 0) {
			return std::unexpected<std::string>("Connection closed by peer");
		}
		bytes_recv += ret;
	}
	return {};
#else
	return std::unexpected<std::string>("Not implemented on this platform");
#endif
}

std::expected<void, std::string> NetworkConnection::send_string(const std::string& str) const {
	uint64_t len = str.size();
	auto res = send_exact(&len, sizeof(len));
	if (!res) {
		return res;
	}
	return send_exact(str.data(), len);
}

std::expected<std::string, std::string> NetworkConnection::recv_string() const {
	uint64_t len = 0;
	auto res = recv_exact(&len, sizeof(len));
	if (!res) {
		return std::unexpected<std::string>(res.error());
	}

	std::string str(len, '\0');
	auto res2 = recv_exact(str.data(), len);
	if (!res2) {
		return std::unexpected<std::string>(res2.error());
	}

	return str;
}

std::ostream& operator<<(std::ostream& os, const NetworkConnection& conn) {
	os << "NetworkConnection(fd=" << conn.socket_fd << ")";
	return os;
}

// bool NetworkConnection::is_active() const { return socket_fd >= 0; } // Currently unused

// Currently unused
/*
void NetworkConnection::close_connection() {
	if (socket_fd >= 0) {
		close(socket_fd);
		socket_fd = -1;
	}
}
*/

// --- NetworkServer ---

NetworkServer::NetworkServer() : server_fd(-1) {
#ifdef __linux__
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) {
		throw std::runtime_error("failed to create server socket: " + std::string(strerror(errno)));
	}
	int opt = 1;
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
}

NetworkServer::~NetworkServer() {
#ifdef __linux__
	if (server_fd >= 0) {
		close(server_fd);
	}
#endif
}

std::expected<void, std::string> NetworkServer::bind_and_listen(
	[[maybe_unused]] uint16_t port) const {
#ifdef __linux__
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
		return std::unexpected<std::string>("Bind failed: " + std::string(strerror(errno)));
	}
	if (listen(server_fd, 5) < 0) {
		return std::unexpected<std::string>("Listen failed: " + std::string(strerror(errno)));
	}
	return {};
#else
	return std::unexpected<std::string>("Not implemented on this platform");
#endif
}

std::expected<NetworkConnection, std::string> NetworkServer::accept_connection() const {
#ifdef __linux__
	sockaddr_in client_addr{};
	socklen_t client_len = sizeof(client_addr);
	int client_fd =
		accept(server_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
	if (client_fd < 0) {
		return std::unexpected<std::string>("Accept failed: " + std::string(strerror(errno)));
	}
	return NetworkConnection(client_fd);
#else
	return std::unexpected<std::string>("Not implemented on this platform");
#endif
}

std::ostream& operator<<(std::ostream& os, const NetworkServer& server) {
	os << "NetworkServer(fd=" << server.server_fd << ")";
	return os;
}

// bool NetworkServer::is_listening() const { return server_fd >= 0; } // Currently unused

void NetworkServer::stop() {
#ifdef __linux__
	if (server_fd >= 0) {
		close(server_fd);
		server_fd = -1;
	}
#endif
}

// --- NetworkClient ---

std::expected<NetworkConnection, std::string> NetworkClient::connect_to(
	[[maybe_unused]] const std::string& ip, [[maybe_unused]] uint16_t port) {
#ifdef __linux__
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		return std::unexpected<std::string>("Socket creation failed: " +
											std::string(strerror(errno)));
	}

	sockaddr_in serv_addr{};
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);

	if (inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr) <= 0) {
		return std::unexpected<std::string>("Invalid address / Address not supported");
	}

	if (connect(sock, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) {
		close(sock);
		return std::unexpected<std::string>("Connection Failed: " + std::string(strerror(errno)));
	}
	return NetworkConnection(sock);
#else
	return std::unexpected<std::string>("Not implemented on this platform");
#endif
}

/*
bool NetworkClient::test_connection(const std::string& ip, uint16_t port) { // Currently unused
	auto res = connect_to(ip, port);
	return res.has_value();
}
*/
