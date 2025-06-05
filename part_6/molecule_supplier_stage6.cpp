#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <set>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>

using namespace std;

unordered_map<string, multiset<string>> atom_inventory;

void load_atoms(const string& folder) {
    vector<string> types = {"hydrogen", "oxygen", "carbon"};
    for (const string& type : types) {
        ifstream infile(folder + "/" + type + ".txt");
        string atom;
        while (getline(infile, atom)) {
            atom_inventory[type].insert(atom);
        }
    }
}

bool can_build_molecule(const vector<string>& atoms) {
    unordered_map<string, int> required;
    for (const string& atom : atoms) {
        required[atom]++;
    }

    for (const auto& [type, count] : required) {
        if (atom_inventory[type].size() < count) return false;
    }

    for (const auto& [type, count] : required) {
        for (int i = 0; i < count; ++i) atom_inventory[type].erase(atom_inventory[type].begin());
    }
    return true;
}

vector<string> parse_atoms(const string& input) {
    vector<string> atoms;
    stringstream ss(input);
    string atom;
    while (getline(ss, atom, ',')) atoms.push_back(atom);
    return atoms;
}

void serve_udp(int udp_port) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in serv_addr{}, cli_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(udp_port);

    bind(sockfd, (sockaddr*)&serv_addr, sizeof(serv_addr));
    char buffer[1024];

    while (true) {
        socklen_t len = sizeof(cli_addr);
        int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&cli_addr, &len);
        buffer[n] = '\0';
        vector<string> atoms = parse_atoms(buffer);
        string response = can_build_molecule(atoms) ? "SUCCESS" : "FAILURE";
        sendto(sockfd, response.c_str(), response.size(), 0, (sockaddr*)&cli_addr, len);
    }
}

void serve_uds(const string& path) {
    int sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    sockaddr_un serv_addr{}, cli_addr{};

    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, path.c_str());
    unlink(path.c_str());
    bind(sockfd, (sockaddr*)&serv_addr, sizeof(serv_addr));
    char buffer[1024];

    while (true) {
        socklen_t len = sizeof(cli_addr);
        int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&cli_addr, &len);
        buffer[n] = '\0';
        vector<string> atoms = parse_atoms(buffer);
        string response = can_build_molecule(atoms) ? "SUCCESS" : "FAILURE";
        sendto(sockfd, response.c_str(), response.size(), 0, (sockaddr*)&cli_addr, len);
    }
}

int main(int argc, char* argv[]) {
    int udp_port = -1;
    string datagram_path;
    string atom_dir = "atom_types";

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--udp-port") == 0 && i + 1 < argc) udp_port = atoi(argv[++i]);
        else if (strcmp(argv[i], "--datagram-path") == 0 && i + 1 < argc) datagram_path = argv[++i];
    }

    if (udp_port == -1 && datagram_path.empty()) {
        cerr << "Usage: " << argv[0] << " --udp-port <port> | --datagram-path <path>" << endl;
        return 1;
    }

    load_atoms(atom_dir);

    if (udp_port != -1) {
        serve_udp(udp_port);
    } else {
        serve_uds(datagram_path);
    }

    return 0;
}
