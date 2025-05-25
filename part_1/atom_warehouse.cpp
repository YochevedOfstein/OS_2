#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
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

int setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void sendStatus(int client_fd, unsigned long long carbon, unsigned long long hydrogen, unsigned long long oxygen) {
    std::ostringstream oss;
    oss << "CARBON: " << carbon << "\n" <<"OXYGEN: " << oxygen << "\n" << "HYDROGEN: " << hydrogen << "\n";
    std::string response = oss.str();
    send(client_fd, response.c_str(), response.size(), 0);
}

void processCommand(int client_fd, const std::string& command, unsigned long long& carbon, unsigned long long& hydrogen, unsigned long long& oxygen) {
    std::istringstream iss(command);
    std::string cmd, type;
    unsigned long long amount;

    if(!(iss >> cmd >> type >> amount) || cmd != "ADD" || amount > MAX_ATOMS) {
        std::string err = "Invalid command format\n";
        send(client_fd, err.c_str(), err.size(), 0);
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
        std::string err = "Unknown atom type\n";
        send(client_fd, err.c_str(), err.size(), 0);
        return;
    }

    if (*counter + amount > MAX_ATOMS || *counter + amount < *counter) {
        // Check for overflow

        std::string err = "Overflow error\n";
        send(client_fd, err.c_str(), err.size(), 0);
        return;
    }

    *counter += amount;
    sendStatus(client_fd, carbon, hydrogen, oxygen);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return 1;
    }
    
    int port = std::stoi(argv[1]);
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        std::cerr << "Error creating socket" << std::endl;
        return 1;
    }

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if (bind(listener, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Error binding socket" << std::endl;
        close(listener);
        return 1;
    }
    if (listen(listener, SOMAXCONN) < 0) {
        std::cerr << "Error listening on socket" << std::endl;
        close(listener);
        return 1;
    }

    setNonBlocking(listener);
    std::vector<int> clients;
    std::map<int, std::string> recv_buffer;

    unsigned long long carbon = 0, hydrogen = 0, oxygen = 0;

    std::cout << "warehouse_atom started on port " << port << "\n";

    while(true) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(listener, &read_fds);
        int max_fd = listener;

        for (int client_fd : clients) {
            FD_SET(client_fd, &read_fds);
            if (client_fd > max_fd) {
                max_fd = client_fd;
            }
        }

        if (select(max_fd + 1, &read_fds, nullptr, nullptr, nullptr) < 0) {
            std::cerr << "Error in select" << std::endl;
            break;
        }

        if (FD_ISSET(listener, &read_fds)) {
            sockaddr_in cli_addr{};
            socklen_t cli_len = sizeof(cli_addr);
            int client_fd = accept(listener, (sockaddr*)&cli_addr, &cli_len);
            if (client_fd < 0) {
                std::cerr << "Error accepting connection" << std::endl;
                continue;
            }
            setNonBlocking(client_fd);
            clients.push_back(client_fd);
            std::cout << "New connection from " << inet_ntoa(cli_addr.sin_addr) << ":" << ntohs(cli_addr.sin_port) << "\n";
        }

        for (auto it = clients.begin(); it != clients.end();) {
            int client_fd = *it;
            if (FD_ISSET(client_fd, &read_fds)) {
                char buffer[READ_BUFFER];
                ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
                if (bytes_read <= 0) {
                    close(client_fd);
                    it = clients.erase(it);
                    std::cout << "Client disconnected\n";
                } else {
                    recv_buffer[client_fd].append(buffer, bytes_read);
                    size_t pos;
                    while ((pos = recv_buffer[client_fd].find('\n')) != std::string::npos) {
                        std::string command = recv_buffer[client_fd].substr(0, pos);
                        recv_buffer[client_fd].erase(0, pos + 1);
                        processCommand(client_fd, command, carbon, hydrogen, oxygen);
                    }
                    ++it;
                }
            } else {
                ++it;
            }
        }
    }

    for (int client_fd : clients) {
        close(client_fd);
    }
    close(listener);
    return 0;
}