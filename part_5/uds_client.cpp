#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>

#define SOCKET_PATH "/tmp/my_stream_socket"

int main() {
    int sockfd;
    struct sockaddr_un addr;

    // Create a UNIX domain socket of type STREAM
    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    // Configure the socket address
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    // Connect to the server
    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect");
        return 1;
    }

    std::cout << "Connected to server. Type messages (type 'quit' to exit):\n";

    // Loop for sending user input to the server
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit") break;
        send(sockfd, line.c_str(), line.size(), 0);
    }

    close(sockfd);
    return 0;
}
