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
#include <sys/un.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <netdb.h>
#include <signal.h>

constexpr size_t READ_BUFFER = 1024;
constexpr unsigned long long MAX_ATOMS = 1000000000000000000ULL;

static volatile sig_atomic_t keep_running = 1;

struct Inventory {
    unsigned long long carbon;
    unsigned long long hydrogen;
    unsigned long long oxygen;
};

// Map molecule name → {C, H, O} requirements
const std::map<std::string, std::array<unsigned long long,3>> molecule_req = {
    {"WATER",          {0, 2, 1}},    // H₂O
    {"CARBON DIOXIDE", {1, 0, 2}},    // CO₂
    {"GLUCOSE",        {6, 12, 6}},   // C₆H₁₂O₆
    {"ALCOHOL",        {2, 6, 1}},    // C₂H₆O
};

static Inventory* invPtr = nullptr;
static int save_fd = -1;
static bool use_file = false;
static std::string save_path;

// Lock + msync + unlock around every update
void sendTCPStatus(int fd, unsigned long long carbon, unsigned long long oxygen, unsigned long long hydrogen) {
    std::ostringstream oss;
    oss << "CARBON: "  << carbon  << "\n"
        << "OXYGEN: "  << oxygen  << "\n"
        << "HYDROGEN: "<< hydrogen<< "\n";
    std::string status = oss.str();
    send(fd, status.c_str(), status.size(), 0);
}

void processTCPCommand(int fd, const std::string& line)
{
    std::istringstream iss(line);
    std::string cmd, type;
    unsigned long long amount;

    if (!(iss >> cmd >> type >> amount) || cmd != "ADD" || amount > MAX_ATOMS) {
        const char* err = "ERROR: Invalid command\n";
        send(fd, err, strlen(err), 0);
        return;
    }

    unsigned long long* counter = nullptr;
    if (type == "CARBON") {
        counter = &invPtr->carbon;
    } else if (type == "HYDROGEN") {
        counter = &invPtr->hydrogen;
    } else if (type == "OXYGEN") {
        counter = &invPtr->oxygen;
    } else {
        const char* err = "ERROR: Unknown atom type\n";
        send(fd, err, strlen(err), 0);
        return;
    }

    if (*counter + amount < *counter || *counter + amount > MAX_ATOMS) {
        const char* err = "ERROR: Overflow error\n";
        send(fd, err, strlen(err), 0);
        return;
    }

    if (use_file) {
        if (flock(save_fd, LOCK_EX) != 0) {
            std::perror("flock LOCK_EX in processTCPCommand");
            const char* err = "ERROR: Cannot acquire lock\n";
            send(fd, err, strlen(err), 0);
            return;
        }
        *counter += amount;
        if (msync(invPtr, sizeof(Inventory), MS_SYNC) != 0) {
            std::perror("msync in processTCPCommand");
        }
        if (flock(save_fd, LOCK_UN) != 0) {
            std::perror("flock LOCK_UN in processTCPCommand");
        }
    } else {
        *counter += amount;
    }

    sendTCPStatus(fd,
                  invPtr->carbon,
                  invPtr->oxygen,
                  invPtr->hydrogen);
}

void processDatagramCommand(int sock,
                            const std::string& line,
                            const sockaddr* cli_addr,
                            socklen_t cli_len)
{
    std::istringstream iss(line);
    std::string cmd;
    if (!(iss >> cmd) || cmd != "DELIVER") {
        const char* err = "ERROR: invalid command";
        sendto(sock, err, strlen(err), 0, cli_addr, cli_len);
        return;
    }

    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    if (tokens.size() < 2) {
        const char* err = "ERROR: invalid command";
        sendto(sock, err, strlen(err), 0, cli_addr, cli_len);
        return;
    }

    unsigned long long count = 0;
    try {
        count = std::stoull(tokens.back());
    } catch (...) {
        const char* err = "ERROR: invalid number";
        sendto(sock, err, strlen(err), 0, cli_addr, cli_len);
        return;
    }

    std::string mol = tokens[0];
    for (size_t i = 1; i + 1 < tokens.size(); ++i) {
        mol += " ";
        mol += tokens[i];
    }

    auto it = molecule_req.find(mol);
    if (it == molecule_req.end()) {
        const char* err = "ERROR: unknown molecule";
        sendto(sock, err, strlen(err), 0, cli_addr, cli_len);
        return;
    }

    auto req = it->second; // {C, H, O}
    unsigned long long needC = req[0] * count;
    unsigned long long needH = req[1] * count;
    unsigned long long needO = req[2] * count;

    bool canDeliver = false;
    if (use_file) {
        if (flock(save_fd, LOCK_EX) != 0) {
            std::perror("flock LOCK_EX in processDatagramCommand");
            const char* err = "ERROR: Cannot acquire lock";
            sendto(sock, err, strlen(err), 0, cli_addr, cli_len);
            return;
        }
        if (invPtr->carbon >= needC &&
            invPtr->hydrogen >= needH &&
            invPtr->oxygen >= needO)
        {
            invPtr->carbon   -= needC;
            invPtr->hydrogen -= needH;
            invPtr->oxygen   -= needO;
            canDeliver = true;
            if (msync(invPtr, sizeof(Inventory), MS_SYNC) != 0) {
                std::perror("msync in processDatagramCommand");
            }
        }
        if (flock(save_fd, LOCK_UN) != 0) {
            std::perror("flock LOCK_UN in processDatagramCommand");
        }
    } else {
        if (invPtr->carbon >= needC &&
            invPtr->hydrogen >= needH &&
            invPtr->oxygen >= needO)
        {
            invPtr->carbon   -= needC;
            invPtr->hydrogen -= needH;
            invPtr->oxygen   -= needO;
            canDeliver = true;
        }
    }

    if (canDeliver) {
        std::ostringstream oss;
        oss << "OK\n"
            << "CARBON: "   << invPtr->carbon   << "\n"
            << "OXYGEN: "   << invPtr->oxygen   << "\n"
            << "HYDROGEN: " << invPtr->hydrogen;
        std::string ok = oss.str();
        sendto(sock, ok.c_str(), ok.size(), 0, cli_addr, cli_len);
    } else {
        const char* err = "ERROR: insufficient atoms";
        sendto(sock, err, strlen(err), 0, cli_addr, cli_len);
    }
}

void signalHandler(int) {
    keep_running = 0;
}

int main(int argc, char* argv[]) {
    unsigned long long initcarbon = 0, initoxygen = 0, inithydrogen = 0;
    int timeout_seconds = -1;
    int tcp_port = -1, udp_port = -1;
    std::string uds_stream_path, uds_datagram_path;

    signal(SIGINT, signalHandler);

    static struct option long_options[] = {
        {"timeout",       required_argument, nullptr, 't'},
        {"tcp-port",      required_argument, nullptr, 'T'},
        {"udp-port",      required_argument, nullptr, 'U'},
        {"stream-path",   required_argument, nullptr, 's'},
        {"datagram-path", required_argument, nullptr, 'd'},
        {"save-file",     required_argument, nullptr, 'f'},
        {nullptr, 0, nullptr, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "t:T:U:s:d:f:", long_options, nullptr)) != -1) {
        switch (opt) {
            case 't':
                timeout_seconds = std::stoi(optarg);
                break;
            case 'T':
                tcp_port = std::stoi(optarg);
                break;
            case 'U':
                udp_port = std::stoi(optarg);
                break;
            case 's':
                uds_stream_path = optarg;
                break;
            case 'd':
                uds_datagram_path = optarg;
                break;
            case 'f':
                use_file = true;
                save_path = optarg;
                break;
            default:
                std::cerr << "Usage: " << argv[0]
                          << " [-t seconds] [-T tcp_port] [-U udp_port] "
                             "[-s uds_stream_path] [-d uds_datagram_path] [-f save_file_path]\n";
                return 1;
        }
    }

    if (tcp_port < 0 && uds_stream_path.empty()) {
        std::cerr << "ERROR: You must specify either -T <tcp-port> or -s <uds-stream-path>\n";
        return 1;
    }
    if (udp_port < 0 && uds_datagram_path.empty()) {
        std::cerr << "ERROR: You must specify either -U <udp-port> or -d <uds-datagram-path>\n";
        return 1;
    }

    // ---------- PART 6: OPEN OR CREATE THE SAVE‐FILE ----------
    if (use_file) {
        save_fd = open(save_path.c_str(), O_RDWR | O_CREAT, 0666);
        if (save_fd < 0) {
            std::perror("open(save_file)");
            return 1;
        }
        if (flock(save_fd, LOCK_EX) != 0) {
            std::perror("flock LOCK_EX on startup");
            close(save_fd);
            return 1;
        }

        struct stat st;
        if (fstat(save_fd, &st) < 0) {
            std::perror("fstat");
            flock(save_fd, LOCK_UN);
            close(save_fd);
            return 1;
        }

        bool newly_created = false;
        if ((size_t)st.st_size < sizeof(Inventory)) {
            if (ftruncate(save_fd, sizeof(Inventory)) != 0) {
                std::perror("ftruncate");
                flock(save_fd, LOCK_UN);
                close(save_fd);
                return 1;
            }
            newly_created = true;
        }

        void* map = mmap(nullptr, sizeof(Inventory),
                         PROT_READ | PROT_WRITE, MAP_SHARED,
                         save_fd, 0);
        if (map == MAP_FAILED) {
            std::perror("mmap");
            flock(save_fd, LOCK_UN);
            close(save_fd);
            return 1;
        }
        invPtr = reinterpret_cast<Inventory*>(map);

        if (newly_created) {
            invPtr->carbon   = initcarbon;
            invPtr->hydrogen = inithydrogen;
            invPtr->oxygen   = initoxygen;
            if (msync(invPtr, sizeof(Inventory), MS_SYNC) != 0) {
                std::perror("msync(initial)");
            }
        }
        // else: file existed, so invPtr already has the old values

        if (flock(save_fd, LOCK_UN) != 0) {
            std::perror("flock LOCK_UN on startup");
        }
        std::cout << "[server] Loaded inventory from “"
                  << save_path << "” → "
                  << "C=" << invPtr->carbon << ", "
                  << "H=" << invPtr->hydrogen << ", "
                  << "O=" << invPtr->oxygen << "\n";
    }
    else {
        invPtr = new Inventory{ initcarbon, inithydrogen, initoxygen };
        std::cout << "[server] Running in MEMORY mode → "
                  << "C=" << invPtr->carbon << ", "
                  << "H=" << invPtr->hydrogen << ", "
                  << "O=" << invPtr->oxygen << "\n";
    }

    // ---------- SET UP TCP / UDP / UDS sockets (your existing code for Part 1–5) ----------
    int listener = -1;
    if (tcp_port >= 0) {
        listener = socket(AF_INET, SOCK_STREAM, 0);
        int sockopt = 1;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(sockopt));
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(tcp_port);
        bind(listener, (sockaddr*)&addr, sizeof(addr));
        listen(listener, SOMAXCONN);
        // … setNonBlocking(listener) …
        std::cout << "Listening (TCP) on port " << tcp_port << "\n";
    }

    int udpSock = -1;
    if (udp_port >= 0) {
        udpSock = socket(AF_INET, SOCK_DGRAM, 0);
        sockaddr_in udp_addr{};
        udp_addr.sin_family = AF_INET;
        udp_addr.sin_addr.s_addr = INADDR_ANY;
        udp_addr.sin_port = htons(udp_port);
        bind(udpSock, (sockaddr*)&udp_addr, sizeof(udp_addr));
        // … setNonBlocking(udpSock) …
        std::cout << "Listening (UDP) on port " << udp_port << "\n";
    }

    int uds_stream_sock = -1;
    if (!uds_stream_path.empty()) {
        unlink(uds_stream_path.c_str());
        uds_stream_sock = socket(AF_UNIX, SOCK_STREAM, 0);
        sockaddr_un uds_addr{};
        uds_addr.sun_family = AF_UNIX;
        strncpy(uds_addr.sun_path, uds_stream_path.c_str(), sizeof(uds_addr.sun_path)-1);
        bind(uds_stream_sock, (sockaddr*)&uds_addr, sizeof(uds_addr));
        listen(uds_stream_sock, SOMAXCONN);
        // … setNonBlocking(uds_stream_sock) …
        std::cout << "Listening (UDS-stream) on " << uds_stream_path << "\n";
    }

    int uds_dgram_sock = -1;
    if (!uds_datagram_path.empty()) {
        unlink(uds_datagram_path.c_str());
        uds_dgram_sock = socket(AF_UNIX, SOCK_DGRAM, 0);
        sockaddr_un uds_dg_addr{};
        uds_dg_addr.sun_family = AF_UNIX;
        strncpy(uds_dg_addr.sun_path, uds_datagram_path.c_str(), sizeof(uds_dg_addr.sun_path)-1);
        bind(uds_dgram_sock, (sockaddr*)&uds_dg_addr, sizeof(uds_dg_addr));
        // … setNonBlocking(uds_dgram_sock) …
        std::cout << "Listening (UDS-datagram) on " << uds_datagram_path << "\n";
    }

    std::vector<int> clients;
    std::map<int, std::string> recv_buffer;
    std::set<std::string> udp_peers;

    fd_set read_fds;
    while (keep_running) {
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        int max_fd = STDIN_FILENO;
        if (listener >= 0) {
            FD_SET(listener, &read_fds);
            max_fd = std::max(max_fd, listener);
        }
        if (uds_stream_sock >= 0) {
            FD_SET(uds_stream_sock, &read_fds);
            max_fd = std::max(max_fd, uds_stream_sock);
        }
        if (udpSock >= 0) {
            FD_SET(udpSock, &read_fds);
            max_fd = std::max(max_fd, udpSock);
        }
        if (uds_dgram_sock >= 0) {
            FD_SET(uds_dgram_sock, &read_fds);
            max_fd = std::max(max_fd, uds_dgram_sock);
        }
        for (int fd : clients) {
            FD_SET(fd, &read_fds);
            max_fd = std::max(max_fd, fd);
        }

        struct timeval timeout;
        struct timeval* timeout_ptr = nullptr;
        if (timeout_seconds >= 0) {
            timeout.tv_sec = timeout_seconds;
            timeout.tv_usec = 0;
            timeout_ptr = &timeout;
        }

        int ready = select(max_fd + 1, &read_fds, nullptr, nullptr, timeout_ptr);
        if (ready < 0) {
            std::cerr << "Error in select\n";
            break;
        }
        if (ready == 0) {
            std::cout << "Timeout: no activity for " << timeout_seconds << " seconds.\n";
            break;
        }

        // — stdin commands (SOFT DRINK / VODKA / CHAMPAGNE) —
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            std::string input;
            if (std::getline(std::cin, input)) {
                if (input == "GEN SOFT DRINK") {
                    auto nwater   = std::min(invPtr->hydrogen/2,       invPtr->oxygen);
                    auto nco2     = std::min(invPtr->carbon,           invPtr->oxygen/2);
                    auto nglucose = std::min({invPtr->carbon/6,
                                              invPtr->hydrogen/12,
                                              invPtr->oxygen/6});
                    auto softdrinksamount = std::min({nwater, nco2, nglucose});
                    std::cout << "SOFT DRINK: " << softdrinksamount << "\n";
                } else if (input == "GEN VODKA") {
                    auto nwater   = std::min(invPtr->hydrogen/6, invPtr->oxygen);
                    auto nalcohol = std::min({invPtr->carbon/2,
                                              invPtr->hydrogen/6,
                                              invPtr->oxygen});
                    auto nglucose = std::min({invPtr->carbon/2,
                                              invPtr->hydrogen/12,
                                              invPtr->oxygen/6});
                    auto vodkaamount = std::min({nwater, nalcohol, nglucose});
                    std::cout << "VODKA: " << vodkaamount << "\n";
                } else if (input == "GEN CHAMPAGNE") {
                    auto nwater   = std::min(invPtr->hydrogen/2, invPtr->oxygen);
                    auto nco2     = std::min(invPtr->carbon,     invPtr->oxygen/2);
                    auto nalcohol = std::min({invPtr->carbon/2,
                                              invPtr->hydrogen/6,
                                              invPtr->oxygen});
                    auto champagneamount = std::min({nwater, nco2, nalcohol});
                    std::cout << "CHAMPAGNE: " << champagneamount << "\n";
                } else {
                    std::cout << "Unknown command\n";
                }
            }
        }

        // — new TCP connection? —
        if (listener >= 0 && FD_ISSET(listener, &read_fds)) {
            sockaddr_in cli_addr{};
            socklen_t cli_len = sizeof(cli_addr);
            int client_fd = accept(listener, (sockaddr*)&cli_addr, &cli_len);
            if (client_fd < 0) {
                std::cerr << "Error accepting TCP connection\n";
            } else {
                // setNonBlocking(client_fd);
                clients.push_back(client_fd);
                std::cout << "New TCP client: "
                          << inet_ntoa(cli_addr.sin_addr) << ":"
                          << ntohs(cli_addr.sin_port) << "\n";
            }
        }

        // — new UDS-stream connection? —
        if (uds_stream_sock >= 0 && FD_ISSET(uds_stream_sock, &read_fds)) {
            sockaddr_un cli_addr_un{};
            socklen_t cli_len_un = sizeof(cli_addr_un);
            int client_fd = accept(uds_stream_sock,
                                   (sockaddr*)&cli_addr_un,
                                   &cli_len_un);
            if (client_fd < 0) {
                std::cerr << "Error accepting UDS-stream connection\n";
            } else {
                // setNonBlocking(client_fd);
                clients.push_back(client_fd);
                std::cout << "New UDS-stream client connected\n";
            }
        }

        // — UDP datagram request? —
        if (udpSock >= 0 && FD_ISSET(udpSock, &read_fds)) {
            char buf[READ_BUFFER];
            sockaddr_in cli_addr{};
            socklen_t cli_len = sizeof(cli_addr);
            ssize_t n = recvfrom(udpSock, buf, sizeof(buf), 0,
                                 (sockaddr*)&cli_addr, &cli_len);
            if (n < 0) {
                std::cerr << "Error receiving UDP data\n";
            } else {
                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &cli_addr.sin_addr, ip, sizeof(ip));
                int port = ntohs(cli_addr.sin_port);
                std::string peer_id = std::string(ip) + ":" + std::to_string(port);
                if (udp_peers.insert(peer_id).second) {
                    std::cout << "New UDP client: " << peer_id << "\n";
                }
                std::string cmd(buf, n);
                if (!cmd.empty() && cmd.back() == '\n') {
                    cmd.pop_back();
                }
                processDatagramCommand(udpSock, cmd,
                                       (sockaddr*)&cli_addr, cli_len);
            }
        }

        // — UDS-datagram request? —
        if (uds_dgram_sock >= 0 && FD_ISSET(uds_dgram_sock, &read_fds)) {
            char buf[READ_BUFFER];
            sockaddr_un cli_addr_un{};
            socklen_t cli_len_un = sizeof(cli_addr_un);
            ssize_t n = recvfrom(uds_dgram_sock, buf, sizeof(buf), 0,
                                 (sockaddr*)&cli_addr_un, &cli_len_un);
            if (n < 0) {
                std::cerr << "Error receiving UDS-datagram data\n";
            } else {
                std::string peer_path(cli_addr_un.sun_path);
                if (udp_peers.insert(peer_path).second) {
                    std::cout << "New UDS-datagram peer: " << peer_path << "\n";
                }
                std::string cmd(buf, n);
                if (!cmd.empty() && cmd.back() == '\n') {
                    cmd.pop_back();
                }
                processDatagramCommand(uds_dgram_sock, cmd,
                                       (sockaddr*)&cli_addr_un, cli_len_un);
            }
        }

        // — handle data from any “stream” client (TCP or UDS-stream) —
        for (auto it = clients.begin(); it != clients.end();) {
            int client_fd = *it;
            if (FD_ISSET(client_fd, &read_fds)) {
                char buffer[READ_BUFFER];
                ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
                if (bytes_read <= 0) {
                    close(client_fd);
                    it = clients.erase(it);
                    std::cout << "Stream client disconnected\n";
                    continue;
                }
                buffer[bytes_read] = '\0';
                std::string line(buffer);
                if (!line.empty() && line.back() == '\n') {
                    line.pop_back();
                }
                processTCPCommand(client_fd, line);
            }
            ++it;
        }
    }

    // — clean up —
    for (int client_fd : clients) {
        close(client_fd);
    }
    if (listener >= 0)      close(listener);
    if (udpSock >= 0)       close(udpSock);
    if (uds_stream_sock >= 0) {
        close(uds_stream_sock);
        unlink(uds_stream_path.c_str());
    }
    if (uds_dgram_sock >= 0) {
        close(uds_dgram_sock);
        unlink(uds_datagram_path.c_str());
    }

    if (use_file) {
        munmap(invPtr, sizeof(Inventory));
        close(save_fd);
    } else {
        delete invPtr;
    }
    return 0;
}