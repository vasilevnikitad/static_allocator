#include "project_info.hpp"
#include "lib/memmanagment.hpp"

#include <iostream>
#include <vector>
#include <string>

static memmanagment::mem_pool<project_info::managment::memory::get_static_pool_size(),
                              project_info::managment::memory::get_static_pool_chunk_size()> static_pool{};

signed main()
{
  memmanagment::mem_allocator<std::string, decltype(static_pool)> allocator{static_pool};

  std::vector<std::string, decltype(allocator)> vec{allocator};
  vec.emplace_back("1");
  vec.emplace_back("2");
  vec.emplace_back("3");
  vec.emplace_back("4");
  vec.emplace_back("5");
  vec.emplace_back("6");

  for( auto &val: vec)
    std::cout << val << std::endl;

  return 0;
}
