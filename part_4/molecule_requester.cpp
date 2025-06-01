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

enum{
    UDP_BUFFER_SIZE = 1024,
};


int main(int argc, char* argv[]) {
    const char* host = nullptr;
    const char* port = nullptr;
    int opt;

    while ((opt = getopt(argc, argv, "h:p:")) != -1) {
        switch (opt) {
            case 'h':
                host = optarg;
                break;
            case 'p':
                port = optarg;
                break;
            default:
                std::cerr << "Usage: " << argv[0] << " -h <hostname/IP> -p <port>\n";
                return 1;
        }
    }

    if (host == nullptr || port == nullptr) {
        std::cerr << "ERROR: Hostname/IP and port must be specified\n";
        return 1;
    }

    addrinfo UDPhints{}, *resUDP;
    UDPhints.ai_family = AF_UNSPEC;
    UDPhints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(host, port, &UDPhints, &resUDP) != 0) {
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

    std::cout << "Connected to server: " << host << "UDP port: " << port << "\n";
    std::cout << "Type 'DELIVER WATER|CARBON DIOXIDE|ALCOHOL|GLUCOSE <count>', or QUIT to exit.\n";

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

    freeaddrinfo(resUDP);
    close(udpsock);
    std::cout << "Disconnected from server\n";
    return 0;

}