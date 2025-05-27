#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <array>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Buffer size for reading commands
constexpr size_t READ_BUFFER = 1024;
// Maximum atoms (10^18)
constexpr unsigned long long MAX_ATOMS = 1000000000000000000ULL;

// Map molecule name to its atom requirements: {C, H, O}
const std::map<std::string, std::array<unsigned long long,3>> molecule_req = {
    {"WATER",          {0, 2, 1}}, // H2O
    {"CARBON DIOXIDE", {1, 0, 2}}, // CO2
    {"GLUCOSE",        {6,12, 6}}, // C6H12O6
    {"ALCOHOL",        {2, 6, 1}}, // C2H6O
};

int setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return (flags < 0) ? -1 : fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void processTcpCommand(int fd, const std::string& line,
                       unsigned long long& carbon,
                       unsigned long long& oxygen,
                       unsigned long long& hydrogen) {
    // reuse ADD logic from warehouse_atom (not shown)
}

void processUdpCommand(int sock,
                       const std::string& line,
                       const sockaddr_in& cli_addr,
                       socklen_t cli_len,
                       unsigned long long& carbon,
                       unsigned long long& oxygen,
                       unsigned long long& hydrogen) {
    std::istringstream iss(line);
    std::string cmd;
    unsigned long long count;
    if (!(iss >> cmd) || cmd != "DELIVER") {
        const char* err = "ERROR: invalid command\n";
        sendto(sock, err, strlen(err), 0,
               (const sockaddr*)&cli_addr, cli_len);
        return;
    }
    // reconstruct molecule name (rest of tokens except last)
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token)
        tokens.push_back(token);
    if (tokens.size() < 2) {
        const char* err = "ERROR: invalid command\n";
        sendto(sock, err, strlen(err), 0,
               (const sockaddr*)&cli_addr, cli_len);
        return;
    }
    // last token is count
    try {
        count = std::stoull(tokens.back());
    } catch (...) {
        const char* err = "ERROR: invalid number\n";
        sendto(sock, err, strlen(err), 0,
               (const sockaddr*)&cli_addr, cli_len);
        return;
    }
    // join tokens[0..n-2] into molecule name
    std::string mol = tokens[0];
    for (size_t i = 1; i + 1 < tokens.size(); ++i) {
        mol += " ";
        mol += tokens[i];
    }
    auto it = molecule_req.find(mol);
    if (it == molecule_req.end()) {
        const char* err = "ERROR: unknown molecule\n";
        sendto(sock, err, strlen(err), 0,
               (const sockaddr*)&cli_addr, cli_len);
        return;
    }
    auto req = it->second; // {C, H, O}
    unsigned long long needC = req[0] * count;
    unsigned long long needH = req[1] * count;
    unsigned long long needO = req[2] * count;
    if (carbon >= needC && hydrogen >= needH && oxygen >= needO) {
        carbon   -= needC;
        hydrogen -= needH;
        oxygen   -= needO;
        const char* ok = "OK\n";
        sendto(sock, ok, strlen(ok), 0,
               (const sockaddr*)&cli_addr, cli_len);
    } else {
        const char* err = "ERROR: insufficient atoms\n";
        sendto(sock, err, strlen(err), 0,
               (const sockaddr*)&cli_addr, cli_len);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <tcp_port> <udp_port>\n";
        return EXIT_FAILURE;
    }
    int tcp_port = std::stoi(argv[1]);
    int udp_port = std::stoi(argv[2]);

    // 1) Set up TCP listener (as before)
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    // bind, listen, setNonBlocking(listener)...

    // 2) Set up UDP socket
    int udpSock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in udp_addr{};
    udp_addr.sin_family      = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port        = htons(udp_port);
    bind(udpSock, (sockaddr*)&udp_addr, sizeof(udp_addr));
    setNonBlocking(udpSock);

    // Shared state
    std::vector<int> clients;
    std::map<int, std::string> recv_buffer;
    unsigned long long carbon = 0, oxygen = 0, hydrogen = 0;

    fd_set read_fds;
    while (true) {
        FD_ZERO(&read_fds);
        FD_SET(listener, &read_fds);
        FD_SET(udpSock, &read_fds);
        int max_fd = std::max(listener, udpSock);
        for (int fd : clients) {
            FD_SET(fd, &read_fds);
            max_fd = std::max(max_fd, fd);
        }
        select(max_fd + 1, &read_fds, nullptr, nullptr, nullptr);

        // Handle new TCP connections
        if (FD_ISSET(listener, &read_fds)) {
            sockaddr_in cli_addr{};
            socklen_t cli_len = sizeof(cli_addr);
            int client_fd = accept(listener,
                        (sockaddr*)&cli_addr, &cli_len);
            setNonBlocking(client_fd);
            clients.push_back(client_fd);
        }
        // Handle UDP requests
        if (FD_ISSET(udpSock, &read_fds)) {
            char buf[READ_BUFFER];
            sockaddr_in cli_addr{};
            socklen_t cli_len = sizeof(cli_addr);
            ssize_t n = recvfrom(udpSock, buf, sizeof(buf), 0,
                                 (sockaddr*)&cli_addr, &cli_len);
            if (n > 0) {
                std::string line(buf, n);
                if (!line.empty() && line.back() == '\n')
                    line.pop_back();
                processUdpCommand(udpSock, line, cli_addr,
                                  cli_len, carbon, oxygen, hydrogen);
            }
        }
        // Handle existing TCP clients (not shown)
    }

    return EXIT_SUCCESS;
}