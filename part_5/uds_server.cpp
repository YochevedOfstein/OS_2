#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>

#define SOCKET_PATH "/tmp/my_stream_socket"

int main() {
    int server_fd, client_fd;
    struct sockaddr_un addr;
    char buffer[1024];

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    unlink(SOCKET_PATH);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    // bind
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        return 1;
    }

    // listen
    if (listen(server_fd, 5) == -1) {
        perror("listen");
        return 1;
    }

    std::cout << "Waiting for a connection...\n";

    // accept
    client_fd = accept(server_fd, nullptr, nullptr);
    if (client_fd == -1) {
        perror("accept");
        return 1;
    }

    std::cout << "Client connected.\n";

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t n = read(client_fd, buffer, sizeof(buffer) - 1);
        if (n <= 0) break;
        std::cout << "Received: " << buffer;
    }

    close(client_fd);
    close(server_fd);
    unlink(SOCKET_PATH); 
    return 0;
}
