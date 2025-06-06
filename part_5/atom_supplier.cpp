#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>
#include <sys/un.h>

constexpr size_t TCP_BUFFER_SIZE = 1024;

std::string readLine(int fd) {
    std::string line;
    char c;
    while (true) {
        ssize_t n = recv(fd, &c, 1, 0);
        if (n <= 0) {
            return std::string();
        }
        if (c == '\n') {
            break;
        }
        line.push_back(c);
    }
    return line;
}

int main(int argc, char* argv[]) {
    const char* host = nullptr;
    const char* port = nullptr;
    const char* uds_path = nullptr;
    int opt;

    // We accept either (-h host -p port) OR (-f uds_path), but not both.
    while ((opt = getopt(argc, argv, "h:p:f:")) != -1) {
        switch (opt) {
            case 'h':
                host = optarg;
                break;
            case 'p':
                port = optarg;
                break;
            case 'f':
                uds_path = optarg;
                break;
            default:
                std::cerr << "Usage: " << argv[0]
                          << " (-h <hostname/IP> -p <port>) | (-f <uds_socket_path>)\n";
                return 1;
        }
    }

    // Conflict check: if uds_path is set, host/port must be null; if host/port set, uds_path must be null
    if (uds_path) {
        if (host || port) {
            std::cerr << "ERROR: Cannot mix -f <uds_path> with -h/-p\n";
            return 1;
        }
    } else {
        if (!(host && port)) {
            std::cerr << "ERROR: Must specify either -h <hostname> -p <port> OR -f <uds_path>\n";
            return 1;
        }
    }

    int sock = -1;

    if (uds_path) {
        // --- connect over UDS-stream ---
        sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock < 0) {
            std::cerr << "Error creating UDS socket\n";
            return 1;
        }
        sockaddr_un addr_un{};
        addr_un.sun_family = AF_UNIX;
        strncpy(addr_un.sun_path, uds_path, sizeof(addr_un.sun_path) - 1);

        if (connect(sock, (sockaddr*)&addr_un, sizeof(addr_un)) < 0) {
            std::cerr << "ERROR: Unable to connect to UDS path: " << uds_path << "\n";
            return 1;
        }
        std::cout << "Connected (UDS) to: " << uds_path << "\n";
    } else {
        // --- connect over TCP ---
        addrinfo hints{}, *res;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(host, port, &hints, &res) != 0) {
            std::cerr << "Error getting address info (TCP)\n";
            return 1;
        }
        for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
            sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (sock < 0) continue;
            if (connect(sock, p->ai_addr, p->ai_addrlen) == 0) {
                break;
            }
            close(sock);
            sock = -1;
        }
        freeaddrinfo(res);
        if (sock < 0) {
            std::cerr << "ERROR: Unable to connect (TCP)\n";
            return 1;
        }
        std::cout << "Connected (TCP) to " << host << " port " << port << "\n";
    }

    std::cout << "Type ADD CARBON|OXYGEN|HYDROGEN <number> to request atoms, or QUIT to exit\n";
    std::string input;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, input)) {
            break;
        }
        if (input == "QUIT") {
            break;
        }
        if (input.back() != '\n') {
            input.push_back('\n');
        }
        if (send(sock, input.c_str(), input.size(), 0) < 0) {
            std::cerr << "Error sending data to server\n";
            break;
        }

        // First response line
        std::string line1 = readLine(sock);
        if (line1.empty()) {
            std::cerr << "Server disconnected\n";
            break;
        }
        std::cout << line1 << "\n";

        // If it starts with "CARBON", read two more lines
        if (line1.rfind("CARBON", 0) == 0) {
            std::string line2 = readLine(sock);
            std::string line3 = readLine(sock);
            if (!line2.empty()) std::cout << line2 << "\n";
            if (!line3.empty()) std::cout << line3 << "\n";
        }
    }

    close(sock);
    std::cout << "Disconnected from server\n";
    return 0;
}