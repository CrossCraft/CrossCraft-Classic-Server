#pragma once
#include <fstream>
#include <vector>

class OPList
{
public:
  OPList();
  ~OPList();

  void add_OP(std::string name);
  void unOP(std::string name);

  bool is_OPned(std::string name);

private:
  std::vector<std::string> OPs;
  void write_OPs();
};