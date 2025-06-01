#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <getopt.h>
#include <string>

#define BUFFER_SIZE 1024

std::string socket_path;

void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " --stream-path <path>" << std::endl;
}

int main(int argc, char* argv[]) {
    // Parse command-line arguments
    static struct option long_options[] = {
        {"stream-path", required_argument, 0, 's'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "s:", long_options, nullptr)) != -1) {
        switch (opt) {
            case 's':
                socket_path = optarg;
                break;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    if (socket_path.empty()) {
        std::cerr << "Error: --stream-path is required." << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    int server_fd, client_fd;
    struct sockaddr_un addr;
    char buffer[BUFFER_SIZE];

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    unlink(socket_path.c_str());

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind");
        return 1;
    }

    if (listen(server_fd, 5) == -1) {
        perror("listen");
        return 1;
    }

    std::cout << "Waiting for a connection on " << socket_path << "...\n";

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
    unlink(socket_path.c_str());

    return 0;
}
