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

constexpr size_t TCP_BUFFER_SIZE = 1024;

std::string readTCPLine(int fd) {
    std::string line;
    char c;
    while(true) {
        ssize_t n = recv(fd, &c, 1, 0);
        if (n <= 0){
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
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <server> <tcp_port> <udp_port>\n";
        return 1;
    }
    const char* server = argv[1];
    const char* tcp_port = argv[2];
    const char* udp_port = argv[3];

    // --- TCP setup for ADD commands --- 
    addrinfo TCPhints{}, *resTCP;
    TCPhints.ai_family = AF_UNSPEC;
    TCPhints.ai_socktype = SOCK_STREAM;

    if(getaddrinfo(server, tcp_port, &TCPhints, &resTCP) != 0) {
        std::cerr << "Error getting address info (TCP)" << "\n";
        return 1;
    }

    int tcpsock = -1;
    for (addrinfo* p = resTCP; p != nullptr; p = p->ai_next) {
        tcpsock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (tcpsock < 0) {
            continue;
        }
        if (connect(tcpsock, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }
        close(tcpsock);
        tcpsock = -1;
    }
    freeaddrinfo(resTCP);
    if (tcpsock < 0){
        std::cerr << "ERROR: Unable to connect TCP socket" << "\n";
        return 1;
    }

    // --- UDP setup for DELIVER commands ---

    addrinfo UDPhints{}, *resUDP;
    UDPhints.ai_family = AF_UNSPEC;
    UDPhints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(server, udp_port, &UDPhints, &resUDP) != 0) {
        std::cerr << "Error getting address info (UDP)\n";
        return 1;
    }

    int udpsock = -1;
    addrinfo* p;
    for (p = resUDP; p != nullptr; p = p->ai_next) {
        udpsock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (udpsock < 0) {
            continue;
        }
        break;
    }

    if(p == nullptr) {
        std::cerr << "Failed creating UDP socket\n";
        freeaddrinfo(resUDP);
        return 1;
    }

    std::cout << "Connected to server: " << server << "(TCP:" << tcp_port << ", UDP: " << udp_port << ")\n";
    std::cout << "Type 'ADD <count>' or 'DELIVER <count>', or QUIT to exit.\n";

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

        bool is_add_command = input.rfind("ADD ", 0) == 0;

        if(is_add_command) {
            if (send(tcpsock, input.c_str(), input.size(), 0) < 0) {
                std::cerr << "Error sending data to server (TCP)" << "\n";
                break;
            }
            for(int i = 0; i < 3; ++i) {
                std::string line = readTCPLine(tcpsock);
                if (line.empty()) {
                    std::cerr << "Server closed TCP connection" << "\n";
                    goto cleanup;
                }
                std::cout << line << "\n";
            }
        }

        else{
            ssize_t sent = sendto(udpsock, input.c_str(), input.size(), 0, resUDP->ai_addr, resUDP->ai_addrlen);
            if (sent < 0) {
                std::cerr << "Error sending data" << "\n";
                break;
            }
            char buffer[UDP_BUFFER_SIZE];
            ssize_t n = recvfrom(udpsock, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&peer, &peer_len);
            if (n < 0) {
                std::cerr << "Error receiving data" << "\n";
                break;
            }
            buffer[n] = '\0'; // Null-terminate the received data
            std::cout << "Received from server: " << buffer << "\n";
        }

    }
    cleanup:
        freeaddrinfo(resUDP);
        close(tcpsock);
        close(udpsock);
        std::cout << "Disconnected from server\n";
        return 0;

}