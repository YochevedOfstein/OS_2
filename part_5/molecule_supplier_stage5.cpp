#include <iostream>
#include <unistd.h>
#include <getopt.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>

#define BUFFER_SIZE 1024

std::string stream_path;
std::string datagram_path;
int tcp_port = -1;
int udp_port = -1;

int uds_stream_fd = -1;
int uds_dgram_fd = -1;

void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " --tcp-port <port> --udp-port <port> [--stream-path <path>] [--datagram-path <path>]" << std::endl;
    exit(1);
}

void setup_uds_stream() {
    unlink(stream_path.c_str());

    uds_stream_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (uds_stream_fd < 0) {
        perror("socket (AF_UNIX STREAM)");
        exit(1);
    }

    sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, stream_path.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(uds_stream_fd, (sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind (AF_UNIX STREAM)");
        exit(1);
    }

    if (listen(uds_stream_fd, 5) == -1) {
        perror("listen (AF_UNIX STREAM)");
        exit(1);
    }

    std::cout << "Listening on UDS stream at: " << stream_path << std::endl;
}

void setup_uds_datagram() {
    unlink(datagram_path.c_str());

    uds_dgram_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (uds_dgram_fd < 0) {
        perror("socket (AF_UNIX DGRAM)");
        exit(1);
    }

    sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, datagram_path.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(uds_dgram_fd, (sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("bind (AF_UNIX DGRAM)");
        exit(1);
    }

    std::cout << "Listening on UDS datagram at: " << datagram_path << std::endl;
}

int main(int argc, char* argv[]) {
    static struct option long_options[] = {
        {"tcp-port", required_argument, 0, 'T'},
        {"udp-port", required_argument, 0, 'U'},
        {"stream-path", required_argument, 0, 's'},
        {"datagram-path", required_argument, 0, 'd'},
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
                datagram_path = optarg;
                break;
            default:
                print_usage(argv[0]);
        }
    }

    if ((!stream_path.empty() && tcp_port != -1) || (!datagram_path.empty() && udp_port != -1)) {
        std::cerr << "Error: Cannot mix UDS and TCP/UDP modes simultaneously" << std::endl;
        exit(1);
    }

    if (stream_path.empty() && tcp_port == -1) {
        std::cerr << "Error: Must provide either TCP port or stream path" << std::endl;
        print_usage(argv[0]);
    }

    if (datagram_path.empty() && udp_port == -1) {
        std::cerr << "Error: Must provide either UDP port or datagram path" << std::endl;
        print_usage(argv[0]);
    }

    if (!stream_path.empty()) {
        setup_uds_stream();
    }

    if (!datagram_path.empty()) {
        setup_uds_datagram();
    }

    std::cout << "Server is ready." << std::endl;
    pause();

    return 0;
}
