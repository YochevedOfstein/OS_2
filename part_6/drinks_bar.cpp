#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <cstdlib>
#include <sys/wait.h>
#include <string>

using namespace std;

vector<string> split_line(const string& line) {
    vector<string> tokens;
    stringstream ss(line);
    string token;
    while (getline(ss, token, ',')) {
        tokens.push_back(token);
    }
    return tokens;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        cerr << "Usage: " << argv[0] << " <path_to_molecule_list.txt> <mode: udp|uds> <target>" << endl;
        return 1;
    }

    string list_file = argv[1];
    string mode = argv[2];
    string target = argv[3];

    ifstream infile(list_file);
    if (!infile) {
        cerr << "Failed to open molecule list file." << endl;
        return 1;
    }

    string line;
    vector<pid_t> children;

    while (getline(infile, line)) {
        vector<string> mols = split_line(line);
        stringstream molecule_combined;
        for (size_t i = 0; i < mols.size(); ++i) {
            molecule_combined << mols[i];
            if (i < mols.size() - 1) molecule_combined << ",";
        }

        pid_t pid = fork();
        if (pid == 0) {
            if (mode == "udp") {
                execl("../part_6/molecule_requester", "molecule_requester",
                      "--udp-port", target.c_str(),
                      "--server-ip", "127.0.0.1",
                      "--molecule", molecule_combined.str().c_str(),
                      (char*)nullptr);
            } else if (mode == "uds") {
                execl("../part_6/molecule_requester", "molecule_requester",
                      "--datagram-path", target.c_str(),
                      "--molecule", molecule_combined.str().c_str(),
                      (char*)nullptr);
            } else {
                cerr << "Unknown mode: " << mode << endl;
                exit(1);
            }
        } else if (pid > 0) {
            children.push_back(pid);
        } else {
            cerr << "Fork failed" << endl;
        }
    }

    for (pid_t child : children) {
        waitpid(child, nullptr, 0);
    }

    return 0;
}
