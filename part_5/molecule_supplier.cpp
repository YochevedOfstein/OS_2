#include <iostream>
#include <unistd.h>
#include <getopt.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

std::string stream_path;
int tcp_port = -1;
int udp_port = -1;

int uds_stream_fd = -1;

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " --tcp-port <port> --udp-port <port> [--stream-path <path>] [--datagram-path <path>]\n";
    exit(1);
}

void setup_uds_stream() {
    // Remove the existing socket file if it exists
    unlink(stream_path.c_str());

    // Create a UNIX domain socket of type STREAM
    uds_stream_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (uds_stream_fd < 0) {
        perror("socket (AF_UNIX)");
        exit(1);
    }

    // Set up the socket address
    sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, stream_path.c_str(), sizeof(addr.sun_path) - 1);

    // Bind the socket to the given path
    if (bind(uds_stream_fd, (sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind (AF_UNIX)");
        exit(1);
    }

    // Start listening for incoming connections
    if (listen(uds_stream_fd, 5) == -1) {
        perror("listen (AF_UNIX)");
        exit(1);
    }

    std::cout << "Listening on UDS stream at: " << stream_path << std::endl;
}

int main(int argc, char* argv[]) {
    static struct option long_options[] = {
        {"tcp-port", required_argument, 0, 'T'},
        {"udp-port", required_argument, 0, 'U'},
        {"stream-path", required_argument, 0, 's'},
        {"datagram-path", required_argument, 0, 'd'}, // Not yet implemented
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "T:U:s:d:", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'T':
                tcp_port = std::stoi(optarg);
                break;
            case 'U':
                udp_port = std::stoi(optarg);
                break;
            case 's':
                stream_path = optarg;
                break;
            case 'd':
                std::cerr << "Datagram path received (not implemented yet)\n";
                break;
            default:
                print_usage(argv[0]);
        }
    }

    // Require at least one input method: TCP or stream + UDP
    if ((tcp_port == -1 && stream_path.empty()) || udp_port == -1) {
        std::cerr << "Error: Must provide at least TCP or stream path + UDP port\n";
        print_usage(argv[0]);
    }

    if (!stream_path.empty()) {
        setup_uds_stream();
        // Add uds_stream_fd to select()/poll() here
    }

    // TODO: setup TCP socket (if tcp_port is provided)
    // TODO: setup UDP socket (udp_port)
    // TODO: use select()/poll() to monitor all active file descriptors
    // TODO: handle accept() and read() from uds_stream_fd
    // TODO: process ADD / DELIVER commands

    std::cout << "Server running...\n";
    pause(); // Placeholder to keep the program alive

    return 0;
}
