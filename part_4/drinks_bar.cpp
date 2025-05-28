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
#include <set>
#include <algorithm>
#include <getopt.h>
#include <sys/time.h>

constexpr size_t READ_BUFFER = 1024;
constexpr unsigned long long MAX_ATOMS = 1000000000000000000ULL;

// Map molecule name to its atom requirements: {C, H, O}
const std::map<std::string, std::array<unsigned long long,3>> molecule_req = {
    {"WATER",          {0, 2, 1}}, // H2O
    {"CARBON DIOXIDE", {1, 0, 2}}, // CO2
    {"GLUCOSE",        {6, 12, 6}}, // C6H12O6
    {"ALCOHOL",        {2, 6, 1}}, // C2H6O
};

int setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return (flags < 0) ? -1 : fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void sendTCPStatus(int fd, unsigned long long carbon, unsigned long long oxygen, unsigned long long hydrogen) {
    std::ostringstream oss;
    oss << "CARBON: " << carbon << "\n"
        << "OXYGEN: " << oxygen << "\n"
        << "HYDROGEN: " << hydrogen << "\n";
    std::string status = oss.str();
    send(fd, status.c_str(), status.size(), 0);
}

void processTCPCommand(int fd, const std::string& line, unsigned long long& carbon, unsigned long long& oxygen, unsigned long long& hydrogen) {
    std::istringstream iss(line);
    std::string cmd, type;
    unsigned long long amount;

    if(!(iss >> cmd >> type >> amount) || cmd != "ADD" || amount > MAX_ATOMS) {
        const char* err = "ERROR: Invalid command\n";
        send(fd, err, strlen(err), 0);
        return;
    }

    unsigned long long *counter = nullptr;
    if (type == "CARBON") {
        counter = &carbon;
    } else if (type == "HYDROGEN") {
        counter = &hydrogen;
    } else if (type == "OXYGEN") {
        counter = &oxygen;
    } else {
        const char* err = "ERROR: Unknown atom type\n";
        send(fd, err, strlen(err), 0);
        return;
    }

    if (*counter + amount > MAX_ATOMS || *counter + amount < *counter) {
        const char* err = "ERROR: Overflow error\n";
        send(fd, err, strlen(err), 0);
        return;
    }

    *counter += amount;
    sendTCPStatus(fd, carbon, oxygen, hydrogen);
}

void processUDPCommand(int sock, const std::string& line, const sockaddr_in& cli_addr, socklen_t cli_len, unsigned long long& carbon, unsigned long long& oxygen, unsigned long long& hydrogen) {
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
        std::ostringstream oss;
        oss << "OK" << "\n" 
        << "CARBON: " << carbon << "\n"
        << "OXYGEN: " << oxygen << "\n"
        << "HYDROGEN: " << hydrogen << "\n" ;
        std::string ok = oss.str();
        sendto(sock, ok.c_str(), ok.size(), 0, (const sockaddr*)&cli_addr, cli_len);
    } else {
        const char* err = "ERROR: insufficient atoms\n";
        sendto(sock, err, strlen(err), 0, (const sockaddr*)&cli_addr, cli_len);
    }
}

int main(int argc, char* argv[]) {

    unsigned long long initcarbon = 0, initoxygen = 0, inithydrogen = 0;
    int timeout_seconds = -1;
    int tcp_port = -1, udp_port = -1;

    static struct option long_options[] = {
        {"oxygen", required_argument, nullptr, 'o'},
        {"hydrogen", required_argument, nullptr, 'h'},
        {"carbon", required_argument, nullptr, 'c'},
        {"timeout", no_argument, nullptr, 't'},
        {"tcp-port", no_argument, nullptr, 'T'},
        {"udp-port", no_argument, nullptr, 'U'},
        {nullptr, 0, nullptr, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "o:h:c:tT:U:", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'o':
                initoxygen = std::stoull(optarg);
                break;
            case 'h':
                inithydrogen = std::stoull(optarg);
                break;
            case 'c':
                initcarbon = std::stoull(optarg);
                break;
            case 't':
                timeout_seconds = 10; // Default timeout
                break;
            case 'T':
                tcp_port = std::stoi(optarg);
                break;
            case 'U':
                udp_port = std::stoi(optarg);
                break;
            default:
                std::cerr << "Usage: " << argv[0] << " [-o amount] [-c amount] [-h amount] [-t seconds] [-T port] [-U port] \n";
                return 1;
        }
    }

    if (tcp_port < 0 || udp_port < 0){
        std::cerr << "ERROR: TCP and UDP ports must be specified\n";
        return 1;
    }

    unsigned long long carbon = initcarbon;
    unsigned long long oxygen = initoxygen;
    unsigned long long hydrogen = inithydrogen;

    std::set<std::string> udp_peers; // Track UDP peers if needed

    // --- TCP socket setup ---
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        std::cerr << "Error creating TCP socket\n";
        return 1;
    }
    int sockopt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(tcp_port);
    if (bind(listener, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Error binding TCP socket\n";
        return 1;
    }
    if (listen(listener, SOMAXCONN) < 0) {
        std::cerr << "Error listening on TCP socket\n";
        return 1;
    }
    setNonBlocking(listener);
    

    // --- UDP socket setup ---
    int udpSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSock < 0) {
        std::cerr << "Error creating UDP socket\n";
        return 1;
    }
    sockaddr_in udp_addr{};
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons(udp_port);
    if(bind(udpSock, (sockaddr*)&udp_addr, sizeof(udp_addr)) < 0) {
        std::cerr << "Error binding UDP socket\n";
        return 1;
    }
    setNonBlocking(udpSock);

    // Shared state
    std::vector<int> clients;
    std::map<int, std::string> recv_buffer;

    fd_set read_fds;
    while (true) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(listener, &read_fds);
        FD_SET(udpSock, &read_fds);

        for(int fd : clients) {
            FD_SET(fd, &read_fds);
        }

        int max_fd = listener;
        max_fd = std::max(max_fd, udpSock);
        max_fd = std::max(max_fd, STDIN_FILENO);
        for(int fd : clients) {
            max_fd = std::max(max_fd, fd);
        }

        struct timeval timeout;
        struct timeval* timeout_ptr = nullptr;

        if(timeout_seconds >= 0) {
            timeout.tv_sec = timeout_seconds;
            timeout.tv_usec = 0;
            timeout_ptr = &timeout;
        } 

        int ready = select(max_fd + 1, &read_fds, nullptr, nullptr, timeout_ptr);
        if (ready < 0) {
            std::cerr << "Error in select\n";
            break;
        }

        if(FD_ISSET(STDIN_FILENO, &read_fds)) {
            std::string input;
            if(std::getline(std::cin, input) ) {
               if(input == "GEN SOFT DRINK"){
                    auto nwater = std::min(hydrogen / 2, oxygen);
                    auto nco2 = std::min(carbon, oxygen / 2);
                    auto nglucose = std::min({carbon / 6, hydrogen / 12, oxygen / 6});
                    auto softdrinksamount = std::min({nwater, nco2, nglucose});
                    std::cout << "SOFT DRINK: " << softdrinksamount << "\n";
               }
               else if(input == "GEN VODKA"){
                    auto nwater = std::min(hydrogen / 6, oxygen);
                    auto nalcohol = std::min({carbon / 2, hydrogen / 6, oxygen});
                    auto nglucose = std::min({carbon / 2, hydrogen / 12, oxygen / 6});
                    auto vodkaamount = std::min({nwater, nalcohol, nglucose});
                    std::cout << "VODKA: " << vodkaamount << "\n";
               }
               else if(input == "GEN CHAMPAGNE"){
                    auto nwater = std::min(hydrogen / 2, oxygen);
                    auto nco2 = std::min(carbon, oxygen / 2);
                    auto nalcohol = std::min({carbon / 2, hydrogen / 6, oxygen});
                    auto champagneamount = std::min({nwater, nco2, nalcohol});
                    std::cout << "CHAMPAGNE: " << champagneamount << "\n";
               }
               else{
                   std::cout << "Unknown command\n";
               }
            }
        }

        // Handle new TCP connections
        if (FD_ISSET(listener, &read_fds)) {
            sockaddr_in cli_addr{};
            socklen_t cli_len = sizeof(cli_addr);
            int client_fd = accept(listener, (sockaddr*)&cli_addr, &cli_len);
            if( client_fd < 0) {
                std::cerr << "Error accepting TCP connection\n";
                continue;
            }
            setNonBlocking(client_fd);
            clients.push_back(client_fd);
            std::cout << "New TCP client connected: " << inet_ntoa(cli_addr.sin_addr) << ":" << ntohs(cli_addr.sin_port) << "\n";
        }
        // Handle UDP requests
        if (FD_ISSET(udpSock, &read_fds)) {
            char buf[READ_BUFFER];
            sockaddr_in cli_addr{};
            socklen_t cli_len = sizeof(cli_addr);
            ssize_t n = recvfrom(udpSock, buf, sizeof(buf), 0, (sockaddr*)&cli_addr, &cli_len);
            if (n < 0) {
                std::cerr << "Error receiving UDP data\n";
                break;
            }
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &cli_addr.sin_addr, ip, sizeof(ip));
            int port = ntohs(cli_addr.sin_port);
            std::string peer_id = std::string(ip) + ":" + std::to_string(port);
            if (udp_peers.insert(peer_id).second) {
                std::cout << "New UDP client connected: " << peer_id << "\n";
            }

            std::string cmd(buf, n);
            if (!cmd.empty() && cmd.back() == '\n')
                cmd.pop_back(); // Remove trailing newline

            processUDPCommand(udpSock, cmd, cli_addr, cli_len, carbon, oxygen, hydrogen);
        }
        
        for(auto it = clients.begin(); it != clients.end();) {
            int client_fd = *it;
            if (FD_ISSET(client_fd, &read_fds)) {
                char buffer[READ_BUFFER];
                ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
                if (bytes_read <= 0) {
                    close(client_fd);
                    it = clients.erase(it);
                    std::cout << "TCP client disconnected\n";
                    continue;
                }
                buffer[bytes_read] = '\0';
                std::string line(buffer);
                if (!line.empty() && line.back() == '\n')
                    line.pop_back();
                processTCPCommand(client_fd, line, carbon, oxygen, hydrogen);
            }
            ++it;
        }
    }

    for(int client_fd : clients) {
        close(client_fd);
    }
    close(listener);
    close(udpSock);

    return 0;
}