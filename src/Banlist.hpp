#pragma once
#include <fstream>
#include <vector>

class BanList {
  public:
    BanList();
    ~BanList();

    void add_ban(std::string name);
    void unban(std::string name);

    bool is_banned(std::string name);

  private:
    std::vector<std::string> bans;
    void write_bans();
};