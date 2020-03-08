#include "lib/memmanagment.hpp"

#include <iostream>
#include <vector>
#include <string>

signed main()
{
  std::vector<std::string, memmanagment::mem_allocator<std::string>> vec{"a", "b", "c",};
  vec.emplace_back("d");
  vec.emplace_back("e");

  for( auto &val: vec)
    std::cout << val << std::endl;

  return 0;
}
