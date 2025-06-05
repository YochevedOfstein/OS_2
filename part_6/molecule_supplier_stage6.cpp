#include <iostream>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include <csignal>
#include <fstream>

#define BUFFER_SIZE 1024

using namespace std;

vector<string> parse_request(const string& request) {
    vector<string> tokens;
    size_t start = 0, end;
    while ((end = request.find(',', start)) != string::npos) {
        tokens.push_back(request.substr(start, end - start));
        start = end + 1;
    }
    tokens.push_back(request.substr(start));
    return tokens;
}

bool check_atom_availability(const string& molecule_name) {
    string atom_path = "../part_6/atom_types/" + molecule_name + ".txt";
    ifstream infile(atom_path);
    return infile.good();
}

bool process_request(const string& request) {
    vector<string> tokens = parse_request(request);
    for (const string& mol : tokens) {
        if (!check_atom_availability(mol)) {
            return false;
        }
    }
    return true;
}

int setup_udp_socket(int port) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    bind(sockfd, (sockaddr*)&addr, sizeof(addr));
    return sockfd;
}

int setup_uds_socket(const string& path) {
    int sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path.c_str());
    unlink(path.c_str());
    bind(sockfd, (sockaddr*)&addr, sizeof(addr));
    return sockfd;
}

int main(int argc, char* argv[]) {
    int udp_port = -1;
    string uds_path;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--udp-port") == 0 && i + 1 < argc)
            udp_port = atoi(argv[++i]);
        else if (strcmp(argv[i], "--datagram-path") == 0 && i + 1 < argc)
            uds_path = argv[++i];
    }

    if (udp_port == -1 && uds_path.empty()) {
        cerr << "Usage: " << argv[0] << " --udp-port <port> --datagram-path <path>\n";
        return 1;
    }

    int udp_sock = -1, uds_sock = -1;
    if (udp_port != -1)
        udp_sock = setup_udp_socket(udp_port);
    if (!uds_path.empty())
        uds_sock = setup_uds_socket(uds_path);

    fd_set readfds;
    int maxfd = max(udp_sock, uds_sock) + 1;

    char buffer[BUFFER_SIZE];

    while (true) {
        FD_ZERO(&readfds);
        if (udp_sock != -1) FD_SET(udp_sock, &readfds);
        if (uds_sock != -1) FD_SET(uds_sock, &readfds);

        int activity = select(maxfd, &readfds, nullptr, nullptr, nullptr);
        if (activity < 0) continue;

        if (udp_sock != -1 && FD_ISSET(udp_sock, &readfds)) {
            sockaddr_in client_addr;
            socklen_t len = sizeof(client_addr);
            int n = recvfrom(udp_sock, buffer, BUFFER_SIZE, 0, (sockaddr*)&client_addr, &len);
            buffer[n] = '\0';
            string response = process_request(buffer) ? "SUCCESS" : "FAILURE";
            sendto(udp_sock, response.c_str(), response.length(), 0, (sockaddr*)&client_addr, len);
        }

        if (uds_sock != -1 && FD_ISSET(uds_sock, &readfds)) {
            sockaddr_un client_addr;
            socklen_t len = sizeof(client_addr);
            int n = recvfrom(uds_sock, buffer, BUFFER_SIZE, 0, (sockaddr*)&client_addr, &len);
            buffer[n] = '\0';
            string response = process_request(buffer) ? "SUCCESS" : "FAILURE";
            sendto(uds_sock, response.c_str(), response.length(), 0, (sockaddr*)&client_addr, len);
        }
    }

    if (udp_sock != -1) close(udp_sock);
    if (uds_sock != -1) close(uds_sock);

    return 0;
}
