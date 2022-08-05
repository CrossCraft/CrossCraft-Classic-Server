#include "Banlist.hpp"
#include <iostream>

BanList::BanList() {
    std::ifstream file("bans.txt");

    std::string line;
    while (std::getline(file, line)) {
        bans.push_back(line.substr(0, line.find_first_of('\n')));
    }

    file.close();
}
BanList::~BanList() { write_bans(); }

void BanList::add_ban(std::string name) {
    bans.push_back(name);
    write_bans();
}
void BanList::unban(std::string name) {
    int id = 0;
    for (int i = 0; i < bans.size(); i++) {
        if (bans[i] == name) {
            id = i;
            break;
        }
    }

    bans.erase(bans.begin() + id);
}

bool BanList::is_banned(std::string name) {
    for (auto n : bans) {
        if (n == name)
            return true;
    }

    return false;
}

void BanList::write_bans() {
    std::ofstream file("bans.txt");

    for (auto n : bans) {
        file << n << std::endl;
    }

    file.close();
}
