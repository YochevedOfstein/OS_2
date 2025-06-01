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

    int sockfd;
    struct sockaddr_un addr;

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect");
        return 1;
    }

    std::cout << "Connected to server at " << socket_path << ". Type messages (type 'quit' to exit):\n";

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "quit") break;
        send(sockfd, line.c_str(), line.size(), 0);
    }

    close(sockfd);
    return 0;
}
