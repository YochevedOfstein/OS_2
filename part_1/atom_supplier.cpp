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
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <server> <port>" << "\n";
        return 1;
    }
    const char* server = argv[1];
    const char* port = argv[2];

    addrinfo hints{}, *res;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if(getaddrinfo(server, port, &hints, &res) != 0) {
        std::cerr << "Error getting address info" << "\n";
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
        std::cerr << "Error connecting to server" << "\n";
        return 1;
    }
    std::cout << "Connected to server" << server << ":" << port << "\n";
    std::cout << "Type ADD CARBON|OXYGEN|HYDROGEN <number>, or QUIT to exit\n";

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
        std::string line2 = readLine(sock);
        std::string line3 = readLine(sock);
        if(!line2.empty()) {
            std::cout << line2 << "\n";
        }
        if(!line3.empty()) {
            std::cout << line3 << "\n";
        }

    }
    close(sock);
    std::cout << "Disconnected from server\n";
    return 0;
    }