#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>

using namespace std;

unordered_map<string, int> atom_inventory;
string save_file_path = "";

void load_inventory_from_file(const string& path) {
    ifstream infile(path);
    if (!infile.is_open()) return;
    string line;
    while (getline(infile, line)) {
        istringstream iss(line);
        string atom;
        int count;
        if (iss >> atom >> count) {
            atom_inventory[atom] = count;
        }
    }
    infile.close();
}

void save_inventory_to_file(const string& path) {
    ofstream outfile(path);
    for (const auto& [atom, count] : atom_inventory) {
        outfile << atom << " " << count << "\n";
    }
    outfile.close();
}

bool handle_request(const string& request, string& response) {
    istringstream ss(request);
    string atom;
    vector<string> atoms;
    while (getline(ss, atom, ',')) {
        atoms.push_back(atom);
    }
    for (const auto& a : atoms) {
        if (atom_inventory[a] <= 0) {
            response = "FAILURE";
            return false;
        }
    }
    for (const auto& a : atoms) {
        atom_inventory[a]--;
    }
    if (!save_file_path.empty()) {
        save_inventory_to_file(save_file_path);
    }
    response = "SUCCESS";
    return true;
}

void start_udp_server(int port) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in server{}, client{};
    socklen_t len = sizeof(client);

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);

    bind(sock, (sockaddr*)&server, sizeof(server));

    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&client, &len);
        string request(buffer);
        if (request == "quit") break;
        string response;
        handle_request(request, response);
        sendto(sock, response.c_str(), response.size(), 0, (sockaddr*)&client, len);
    }

    close(sock);
}

void start_uds_server(const string& path) {
    int sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    sockaddr_un server{}, client{};
    socklen_t len = sizeof(client);

    server.sun_family = AF_UNIX;
    strncpy(server.sun_path, path.c_str(), sizeof(server.sun_path)-1);
    unlink(path.c_str());

    bind(sock, (sockaddr*)&server, sizeof(server));

    char buffer[1024];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&client, &len);
        string request(buffer);
        if (request == "quit") break;
        string response;
        handle_request(request, response);
        sendto(sock, response.c_str(), response.size(), 0, (sockaddr*)&client, len);
    }

    close(sock);
}

int main(int argc, char* argv[]) {
    int udp_port = -1;
    string uds_path;

    for (int i = 1; i < argc; ++i) {
        if (string(argv[i]) == "--udp-port" && i+1 < argc) {
            udp_port = atoi(argv[++i]);
        } else if (string(argv[i]) == "--datagram-path" && i+1 < argc) {
            uds_path = argv[++i];
        } else if (string(argv[i]) == "--save-file" && i+1 < argc) {
            save_file_path = argv[++i];
        }
    }

    if (!save_file_path.empty()) {
        load_inventory_from_file(save_file_path);
    }

    if (udp_port != -1 && !uds_path.empty()) {
        cerr << "Error: Cannot mix UDS and UDP modes simultaneously" << endl;
        return 1;
    }

    if (udp_port == -1 && uds_path.empty()) {
        cerr << "Error: Must provide either UDP port or UDS path" << endl;
        return 1;
    }

    if (udp_port != -1) {
        start_udp_server(udp_port);
    } else {
        start_uds_server(uds_path);
    }

    return 0;
}
