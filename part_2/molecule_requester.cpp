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

enum{
    UDP_BUFFER_SIZE = 1024,
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <server> <udp_port>\n";
        return 1;
    }
    const char* server = argv[1];
    const char* port = argv[2];

    addrinfo hints{}, *res;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(server, port, &hints, &res) != 0) {
        std::cerr << "Error getting address info\n";
        return 1;
    }

    int sock = -1;
    addrinfo* p;
    for (p = res; p != nullptr; p = p->ai_next) {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock < 0) {
            continue;
        }
        break;
    }

    if(p == nullptr) {
        std::cerr << "Failed creating UDP socket\n";
        freeaddrinfo(res);
        return 1;
    }

    std::cout << "Connected to server: " << server << ":" << port << "using UDP\n";
    std::cout << "Type 'DELIVER WATER|GLUCOSE|CARBON DIOXIDE|ALCHOHOL <count>' to request molecules, or QUIT to exit.\n";

    std::string input;
    sockaddr_storage peer;
    socklen_t peer_len = sizeof(peer);

    while (true) {
        std::cout << "> ";
        if(!std::getline(std::cin, input) || input == "QUIT") {
            break;
        }
        if (input.empty()) {
            continue;
        }
        if (input.back() != '\n') {
            input.push_back('\n');
        }

        ssize_t sent = sendto(sock, input.c_str(), input.size(), 0, res->ai_addr, res->ai_addrlen);
        if (sent < 0) {
            std::cerr << "Error sending data" << "\n";
            break;
        }
        char buffer[UDP_BUFFER_SIZE];
        ssize_t n = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&peer, &peer_len);
        if (n < 0) {
            std::cerr << "Error receiving data" << "\n";
            break;
        }
        buffer[n] = '\0'; // Null-terminate the received data
        std::string resp(buffer);
        if(!resp.empty() && resp.back() != '\n') {
            resp.push_back('\n');
        }
        std::cout << "Response from server: " << resp;
    }
    freeaddrinfo(res);
    close(sock);
    std::cout << "Disconnected from server\n";
    return 0;

}