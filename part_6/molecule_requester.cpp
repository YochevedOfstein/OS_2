#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <fcntl.h>

using namespace std;

void usage(const char* progname) {
    cerr << "Usage: " << progname << " --udp-port <port> --server-ip <ip> --molecule <mol_name>"
         << " OR " << progname << " --datagram-path <path> --molecule <mol_name>" << endl;
    exit(1);
}

int main(int argc, char* argv[]) {
    string molecule_name;
    string server_ip;
    int udp_port = -1;
    string datagram_path;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--udp-port") == 0 && i + 1 < argc)
            udp_port = atoi(argv[++i]);
        else if (strcmp(argv[i], "--server-ip") == 0 && i + 1 < argc)
            server_ip = argv[++i];
        else if (strcmp(argv[i], "--molecule") == 0 && i + 1 < argc)
            molecule_name = argv[++i];
        else if (strcmp(argv[i], "--datagram-path") == 0 && i + 1 < argc)
            datagram_path = argv[++i];
    }

    if (molecule_name.empty() || (udp_port == -1 && datagram_path.empty())) {
        usage(argv[0]);
    }

    char buffer[1024];

    if (udp_port != -1) {
        int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        sockaddr_in serv_addr{};
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(udp_port);
        inet_pton(AF_INET, server_ip.c_str(), &serv_addr.sin_addr);

        sendto(sockfd, molecule_name.c_str(), molecule_name.size(), 0, (sockaddr*)&serv_addr, sizeof(serv_addr));

        socklen_t len = sizeof(serv_addr);
        int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&serv_addr, &len);
        buffer[n] = '\0';

        cout << "Server response: " << buffer << endl;
        close(sockfd);

    } else if (!datagram_path.empty()) {
        int sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
        sockaddr_un client_addr{}, serv_addr{};

        client_addr.sun_family = AF_UNIX;
        string tmp_path = "/tmp/client_" + to_string(getpid());
        strcpy(client_addr.sun_path, tmp_path.c_str());
        unlink(tmp_path.c_str());
        bind(sockfd, (sockaddr*)&client_addr, sizeof(client_addr));

        serv_addr.sun_family = AF_UNIX;
        strcpy(serv_addr.sun_path, datagram_path.c_str());

        sendto(sockfd, molecule_name.c_str(), molecule_name.size(), 0, (sockaddr*)&serv_addr, sizeof(serv_addr));

        socklen_t len = sizeof(serv_addr);
        int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&serv_addr, &len);
        buffer[n] = '\0';

        cout << "Server response: " << buffer << endl;
        close(sockfd);
        unlink(tmp_path.c_str());
    }

    return 0;
}
