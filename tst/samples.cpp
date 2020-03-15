#define BOOST_TEST_MODULE samples

#include "project_info.hpp"
#include "memmanagment.hpp"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <exception>
#include <future>
#include <string>
#include <vector>
#include <list>
#include <iterator>
#include <utility>
#include <random>

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

BOOST_AUTO_TEST_CASE(thread_test) try {
  constexpr std::size_t input_str_len{static_pool_size / 2};
  constexpr std::size_t threads_cnt{12};

  auto get_rand_string = [] (std::size_t const len) {
    std::string str{};

    str.reserve(len);

    std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<char> dis('a', 'z');
    for(std::size_t i{0}; i < len; ++i) {
      str += dis(gen);
    }
    return str;
  };
  std::string const input_str{get_rand_string(input_str_len)};

  static memmanagment::mem_pool<static_pool_size, static_pool_chunk_size> static_pool{};
  memmanagment::mem_allocator<char, decltype(static_pool)> ch_allocator{static_pool};

  auto cpy_str = [&allocator = ch_allocator](auto &&str) {
    return std::basic_string<char, std::char_traits<char>,
                      std::remove_reference_t<decltype(allocator)>>{std::forward<decltype(str)>(str), allocator};
  };

  using future_type = decltype(std::async(std::launch::async, cpy_str, std::string{}));
  std::vector<future_type> futures;

  std::size_t const chars_per_thread(std::ceil(static_cast<double>(input_str.length()) / threads_cnt));
  for(std::size_t i{0}; i < input_str.length(); i += chars_per_thread) {
    futures.emplace_back(std::async(std::launch::async, cpy_str, input_str.substr(i, chars_per_thread)));
  }

  std::string output_str{};

  std::for_each(std::begin(futures), std::end(futures), [&str = output_str](auto &fut){ str.append(fut.get());});

  if (!std::equal(std::begin(input_str), std::end(input_str), std::begin(output_str), std::end(output_str)))
    BOOST_FAIL("input and output strings should be equal");

} catch(std::exception const &e) {
  BOOST_FAIL(e.what());
} catch(...) {
  BOOST_FAIL("Unknown exception");
}

BOOST_AUTO_TEST_SUITE_END()
