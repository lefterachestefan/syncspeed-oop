#include "Network.h"

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define SHUT_RDWR SD_BOTH
#define close closesocket
using ssize_t = int;

class WinsockInit {
   public:
	WinsockInit() {
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2, 2), &wsaData);
	}
	~WinsockInit() { WSACleanup(); }
};
static WinsockInit global_winsock_init;
#else
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

// --- NetworkConnection ---

NetworkConnection::NetworkConnection(int fd) : socket_fd(fd) {}

NetworkConnection::~NetworkConnection() {
	if (socket_fd >= 0) {
		close(socket_fd);
	}
}

NetworkConnection::NetworkConnection(NetworkConnection&& other) noexcept
	: socket_fd(other.socket_fd) {
	other.socket_fd = -1;
}

NetworkConnection& NetworkConnection::operator=(NetworkConnection&& other) noexcept {
	if (this != &other) {
		if (socket_fd >= 0) {
			close(socket_fd);
		}
		socket_fd = other.socket_fd;
		other.socket_fd = -1;
	}
	return *this;
}

std::expected<void, std::string> NetworkConnection::send_exact(const void* data, size_t len) const {
	const char* ptr = static_cast<const char*>(data);
	size_t bytes_sent = 0;
	while (bytes_sent < len) {
		ssize_t ret = send(socket_fd, ptr + bytes_sent, static_cast<int>(len - bytes_sent), 0);
		if (ret < 0) {
#if defined(_WIN32)
			return std::unexpected<std::string>("send failed: " + std::to_string(WSAGetLastError()));
#else
			return std::unexpected<std::string>("send failed: " + std::string(strerror(errno)));
#endif
		}
		bytes_sent += ret;
	}
	return {};
}

std::expected<void, std::string> NetworkConnection::recv_exact(void* data, size_t len) const {
	char* const ptr = static_cast<char*>(data);
	size_t bytes_recv = 0;
	while (bytes_recv < len) {
		ssize_t ret = recv(socket_fd, ptr + bytes_recv, static_cast<int>(len - bytes_recv), 0);
		if (ret < 0) {
#if defined(_WIN32)
			return std::unexpected<std::string>("recv failed: " + std::to_string(WSAGetLastError()));
#else
			return std::unexpected<std::string>("recv failed: " + std::string(strerror(errno)));
#endif
		}
		if (ret == 0) {
			return std::unexpected<std::string>("Connection closed by peer");
		}
		bytes_recv += ret;
	}
	return {};
}

std::expected<void, std::string> NetworkConnection::send_string(const std::string& str) const {
	const uint64_t len = str.size();
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

	std::string str(static_cast<size_t>(len), '\0');
	auto res2 = recv_exact(str.data(), static_cast<size_t>(len));
	if (!res2) {
		return std::unexpected<std::string>(res2.error());
	}

	return str;
}

std::ostream& operator<<(std::ostream& os, const NetworkConnection& conn) {
	os << "NetworkConnection(fd=" << conn.socket_fd << ")";
	return os;
}

// --- NetworkServer ---

NetworkServer::NetworkServer() : server_fd(-1) {
	server_fd = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
	if (server_fd < 0) {
#if defined(_WIN32)
		throw std::runtime_error("failed to create server socket: " +
								 std::to_string(WSAGetLastError()));
#else
		throw std::runtime_error("failed to create server socket: " + std::string(strerror(errno)));
#endif
	}
#if defined(_WIN32)
	const char opt = 1;
#else
	const int opt = 1;
#endif
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}

NetworkServer::~NetworkServer() {
	if (server_fd >= 0) {
		close(server_fd);
	}
}

std::expected<void, std::string> NetworkServer::bind_and_listen(uint16_t port) const {
	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
#if defined(_WIN32)
		return std::unexpected<std::string>("Bind failed: " + std::to_string(WSAGetLastError()));
#else
		return std::unexpected<std::string>("Bind failed: " + std::string(strerror(errno)));
#endif
	}
	if (listen(server_fd, 5) < 0) {
#if defined(_WIN32)
		return std::unexpected<std::string>("Listen failed: " + std::to_string(WSAGetLastError()));
#else
		return std::unexpected<std::string>("Listen failed: " + std::string(strerror(errno)));
#endif
	}
	return {};
}

std::expected<NetworkConnection, std::string> NetworkServer::accept_connection() const {
	sockaddr_in client_addr{};
#if defined(_WIN32)
	int client_len = sizeof(client_addr);
#else
	socklen_t client_len = sizeof(client_addr);
#endif
	const int client_fd =
		static_cast<int>(accept(server_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len));
	if (client_fd < 0) {
#if defined(_WIN32)
		return std::unexpected<std::string>("Accept failed: " + std::to_string(WSAGetLastError()));
#else
		return std::unexpected<std::string>("Accept failed: " + std::string(strerror(errno)));
#endif
	}
	return NetworkConnection(client_fd);
}

std::ostream& operator<<(std::ostream& os, const NetworkServer& server) {
	os << "NetworkServer(fd=" << server.server_fd << ")";
	return os;
}

void NetworkServer::stop() {
	if (server_fd >= 0) {
		close(server_fd);
		server_fd = -1;
	}
}

// --- NetworkClient ---

std::expected<NetworkConnection, std::string> NetworkClient::connect_to(const std::string& ip,
																		uint16_t port) {
	const int sock = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
	if (sock < 0) {
#if defined(_WIN32)
		return std::unexpected<std::string>("Socket creation failed: " +
											std::to_string(WSAGetLastError()));
#else
		return std::unexpected<std::string>("Socket creation failed: " +
											std::string(strerror(errno)));
#endif
	}

	sockaddr_in serv_addr{};
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(port);

	if (inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr) <= 0) {
		return std::unexpected<std::string>("Invalid address / Address not supported");
	}

	if (connect(sock, reinterpret_cast<struct sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) {
		close(sock);
#if defined(_WIN32)
		return std::unexpected<std::string>("Connection Failed: " +
											std::to_string(WSAGetLastError()));
#else
		return std::unexpected<std::string>("Connection Failed: " + std::string(strerror(errno)));
#endif
	}
	return NetworkConnection(sock);
}
