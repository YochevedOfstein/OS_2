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


constexpr size_t TCP_BUFFER_SIZE = 1024;


std::string readLine(int fd) {
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

    addrinfo hints{}, *res;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if(getaddrinfo(host, port, &hints, &res) != 0) {
        std::cerr << "Error getting address info (TCP)" << "\n";
        return 1;
    }

    int sock = -1;
    for (addrinfo* p = res; p != nullptr; p = p->ai_next) {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock < 0) {
            continue;
        }
        if (connect(sock, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }
        close(sock);
        sock = -1;
    }
    freeaddrinfo(res);
    if (sock < 0){
        std::cerr << "ERROR: Unable to connect" << "\n";
        return 1;
    }
    std::cout << "Connected to server" << host << "TCP port:" << port << "\n";
    std::cout << "Type ADD CARBON|OXYGEN|HYDROGEN <number> to request atoms, or QUIT to exit\n";

    std::string input;
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);
        if (input == "QUIT") {
            break;
        }
        if(input.back() != '\n') {
            input.push_back('\n');
        }
        if(send(sock, input.c_str(), input.size(), 0) < 0) {
            std::cerr << "Error sending data to server" << "\n";
            break;
        }

        std::string line1 = readLine(sock);
        if (line1.empty()) {
            std::cerr << "Server disconnected" << "\n";
            break;
        }
        std::cout << line1 << "\n";

        if(line1.rfind("CARBON", 0) == 0){
            std::string line2 = readLine(sock);
            std::string line3 = readLine(sock);
            if(!line2.empty()) {
                std::cout << line2 << "\n";
            }
            if(!line3.empty()) {
                std::cout << line3 << "\n";
            }
        }

    }
    close(sock);
    std::cout << "Disconnected from server\n";
    return 0;
    }