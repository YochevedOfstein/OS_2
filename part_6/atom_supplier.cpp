#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <vector>
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>

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
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <atom_type> <input_file>" << endl;
        return 1;
    }

    string atom_type = argv[1];
    string input_file = argv[2];

    ifstream infile(input_file);
    if (!infile) {
        cerr << "Failed to open input file." << endl;
        return 1;
    }

    string output_dir = "atom_types";
    mkdir(output_dir.c_str(), 0777);

    string output_path = output_dir + "/" + atom_type + ".txt";
    ofstream outfile(output_path, ios::app);

    string line;
    while (getline(infile, line)) {
        vector<string> atoms = split_line(line);
        for (const string& atom : atoms) {
            outfile << atom << endl;
        }
    }

    outfile.close();
    infile.close();

    cout << "Wrote atoms to " << output_path << endl;

    return 0;
}
