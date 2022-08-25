#include "OPList.hpp"
#include <iostream>
#include <string>

OPList::OPList() {
    std::ifstream file("./OPs.txt");

    std::string line;
    while (std::getline(file, line)) {
        OPs.push_back(line.substr(0, line.find_first_of('\n')));
    }

    file.close();
}
OPList::~OPList() { write_OPs(); }

void OPList::add_OP(std::string name) {
    if (!is_OPned(name)) {
        OPs.push_back(name);
        write_OPs();
    }
}
void OPList::unOP(std::string name) {
    int id = 0;
    for (int i = 0; i < OPs.size(); i++) {
        if (OPs[i] == name) {
            id = i;
            break;
        }
    }

    OPs.erase(OPs.begin() + id);
    write_OPs();
}

bool OPList::is_OPned(std::string name) {
    for (auto n : OPs) {
        if (n == name)
            return true;
    }

    return false;
}

void OPList::write_OPs() {
    std::ofstream file("OPs.txt");

    for (auto n : OPs) {
        file << n << std::endl;
    }

    file.close();
}
