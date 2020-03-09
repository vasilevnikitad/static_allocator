#define BOOST_TEST_MODULE samples

#include "project_info.hpp"
#include "memmanagment.hpp"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <exception>
#include <string>
#include <vector>
#include <list>
#include <iterator>

BOOST_AUTO_TEST_SUITE(sample)

constexpr auto static_pool_size{project_info::managment::memory::get_static_pool_size()};
constexpr auto static_pool_chunk_size{project_info::managment::memory::get_static_pool_chunk_size()};


template<typename C, typename = std::enable_if_t<std::is_integral_v<typename C::value_type>>>
void container_trivial_operations(C &c) {
  for(auto const &val : {1, 2, 3, 4, 5, 6, 7, 8, 9, 0})
    *std::back_inserter(c) = val;

  C tmp = c;
  std::remove_if(std::begin(c), std::end(c), [](auto const &val){return !!(val % 2);});
  c = tmp;
}

BOOST_AUTO_TEST_CASE(trivial_ops) try {
  static memmanagment::mem_pool<static_pool_size, static_pool_chunk_size> static_pool{};

  using value_type = std::uint64_t;
  memmanagment::mem_allocator<value_type, decltype(static_pool)> allocator{static_pool};

  std::vector<value_type, decltype(allocator)> vec1{allocator};
  std::vector<value_type> vec2;

  container_trivial_operations(vec1);
  container_trivial_operations(vec2);

  if (!std::equal(std::begin(vec1), std::end(vec1), std::begin(vec2), std::end(vec2)))
    BOOST_FAIL("Containers should be equal");

} catch(std::exception const &e) {
  BOOST_FAIL(e.what());
} catch(...) {
  BOOST_FAIL("Unknown exception");
}

BOOST_AUTO_TEST_CASE(mem_reservation) try {
  using value_type = std::uint64_t;

  static memmanagment::mem_pool<static_pool_size, static_pool_chunk_size> static_pool{};

  memmanagment::mem_allocator<value_type, decltype(static_pool)> allocator{static_pool};

  std::vector<value_type, decltype(allocator)> vec{allocator};

  for(std::size_t i{0}; i < 100; ++i ) {
    constexpr auto reserve_cnt{static_pool_size / sizeof(value_type)};
    vec.reserve(reserve_cnt);
    vec.resize(reserve_cnt);
    vec.clear();
    vec.shrink_to_fit();
  }

} catch(std::exception const &e) {
  BOOST_FAIL(e.what());
} catch(...) {
  BOOST_FAIL("Unknown exception");
}

BOOST_AUTO_TEST_CASE(mem_chunks) try {
  constexpr std::size_t iterations_count{10};
  static memmanagment::mem_pool<static_pool_size, static_pool_chunk_size> static_pool{};

  memmanagment::mem_allocator<char,          decltype(static_pool)> allctr1{static_pool};
  memmanagment::mem_allocator<std::uint64_t, decltype(static_pool)> allctr2{static_pool};

  std::vector<decltype(allctr1.allocate(1))> vec1;
  std::vector<decltype(allctr2.allocate(1))> vec2;

  for(std::size_t i{0}; i < iterations_count; ++i) {
    vec1.emplace_back(allctr1.allocate(i));
    vec2.emplace_back(allctr2.allocate(i));
  }

  for(std::size_t i{0}; i < iterations_count; ++i) {
    allctr2.deallocate(vec2[i], i);
    allctr1.deallocate(vec1[i], i);
  }

} catch(std::exception const &e) {
  BOOST_FAIL(e.what());
} catch(...) {
  BOOST_FAIL("Unknown exception");
}

BOOST_AUTO_TEST_SUITE_END()
