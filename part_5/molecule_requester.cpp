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

enum {
    UDP_BUFFER_SIZE = 1024
};

int main(int argc, char* argv[]) {
    const char* host = nullptr;
    const char* port = nullptr;
    const char* uds_path = nullptr;
    int opt;

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
    sockaddr_storage server_addr{}; 
    socklen_t server_len = 0;

    if (uds_path) {
        // --- UDS-datagram client ---
        sock = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (sock < 0) {
            std::cerr << "Error creating UDS-datagram socket\n";
            return 1;
        }

        // We must bind to a local UDS path so we can receive replies
        std::string client_path = "/tmp/udp_client_" + std::to_string(getpid());
        unlink(client_path.c_str()); // remove old file if exists

        sockaddr_un cli_addr_un{};
        cli_addr_un.sun_family = AF_UNIX;
        strncpy(cli_addr_un.sun_path, client_path.c_str(), sizeof(cli_addr_un.sun_path) - 1);
        if (bind(sock, (sockaddr*)&cli_addr_un, sizeof(cli_addr_un)) < 0) {
            std::cerr << "Error binding UDS client socket to " << client_path << "\n";
            close(sock);
            return 1;
        }

        // Set up server address struct
        sockaddr_un* srv_un = reinterpret_cast<sockaddr_un*>(&server_addr);
        srv_un->sun_family = AF_UNIX;
        strncpy(srv_un->sun_path, uds_path, sizeof(srv_un->sun_path) - 1);
        server_len = sizeof(sockaddr_un);

        std::cout << "Connected (UDS) to server path: " << uds_path << "\n";
    }
    else {
        // --- UDP-IP client ---
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) {
            std::cerr << "Error creating UDP socket\n";
            return 1;
        }

        addrinfo hints{}, *res;
        hints.ai_family   = AF_UNSPEC;
        hints.ai_socktype = SOCK_DGRAM;
        if (getaddrinfo(host, port, &hints, &res) != 0) {
            std::cerr << "Error getting address info (UDP)\n";
            return 1;
        }
        // We do not need to bind, the OS will choose an ephemeral port for us
        memcpy(&server_addr, res->ai_addr, res->ai_addrlen);
        server_len = res->ai_addrlen;
        freeaddrinfo(res);

        std::cout << "Connected (UDP-IP) to " << host << " port " << port << "\n";
    }

    std::cout << "Type 'DELIVER WATER|CARBON DIOXIDE|ALCOHOL|GLUCOSE <count>', or QUIT to exit.\n";
    std::string input;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, input) || input == "QUIT") {
            break;
        }
        if (input.empty()) {
            continue;
        }
        if (input.back() != '\n') {
            input.push_back('\n');
        }

        // Send to server (AF_INET or AF_UNIX-dgram)
        ssize_t sent = sendto(sock, input.c_str(), input.size(), 0,
                              (sockaddr*)&server_addr, server_len);
        if (sent < 0) {
            std::cerr << "Error sending data\n";
            break;
        }

        // Receive a reply
        char buffer[UDP_BUFFER_SIZE];
        sockaddr_storage recv_addr{};
        socklen_t recv_len = sizeof(recv_addr);
        ssize_t n = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                             (sockaddr*)&recv_addr, &recv_len);
        if (n < 0) {
            std::cerr << "Error receiving data\n";
            break;
        }
        buffer[n] = '\0';
        std::cout << "Received from server:\n" << buffer << "\n";
    }

    close(sock);
    if (uds_path) {
        // Remove client UDS socket file
        std::string client_path = "/tmp/udp_client_" + std::to_string(getpid());
        unlink(client_path.c_str());
    }
    std::cout << "Disconnected from server\n";
    return 0;
}