#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <iterator>
#include <mutex>
#include <type_traits>
#include <iostream>
#include <vector>

template<std::size_t POOL_SIZE = 100>
class mem_pool {
  private:
    using pool_type = std::aligned_storage_t<sizeof(std::uint8_t), alignof(std::uint8_t)>;
    static constexpr std::size_t max_align = alignof(std::max_align_t);

    pool_type   pool[POOL_SIZE]{};
    bool is_reserved[POOL_SIZE]{};
    std::mutex pool_mutex;

  public:
    mem_pool()
    {
    }

    [[nodiscard]]
    void *allocate(std::size_t const bytes_cnt, std::size_t alignment = max_align) noexcept
    {
      //FIXME: memory alignof
      //
      std::lock_guard<std::mutex> guard{pool_mutex};

      auto const it = std::search_n(std::begin(is_reserved),
                                    std::end(is_reserved), bytes_cnt, false);
      if (it == std::end(is_reserved))
        return nullptr;

      std::fill_n(it, bytes_cnt, true);

      std::size_t const pool_index = std::distance(std::begin(is_reserved), it);
      return reinterpret_cast<void *>(&pool[pool_index]);
    }

    void deallocate(void *const ptr,
                    std::size_t const bytes_cnt,
                    std::size_t const alignment = max_align) noexcept
    {
      if (ptr == nullptr || bytes_cnt == 0)
        return;

      std::size_t const index = reinterpret_cast<decltype(&pool[0])>(ptr) - pool;

      std::lock_guard<std::mutex> guard{pool_mutex};
      std::fill_n(std::next(std::begin(is_reserved), index), bytes_cnt, false);
    }
};

constexpr std::size_t STATIC_POOL_SIZE{100};

static mem_pool<STATIC_POOL_SIZE> static_pool{};

template<class T, typename MEM_POOL = decltype(static_pool)>
class mem_allocator{
  private:
    MEM_POOL &pool;

  public:
    using value_type = T;

    explicit mem_allocator(MEM_POOL &mem_pool = static_pool) noexcept : pool{mem_pool}
    {}

    template<class U>
    mem_allocator(mem_allocator<U> const &allctr) noexcept : pool{allctr.pool}
    {}

    T* allocate(std::size_t n) noexcept
    {
      void *ptr = pool.allocate(n * sizeof(T));

      if (ptr == nullptr) {
        std::cout << "ASSERT" << std::endl;
      }
      return reinterpret_cast<T*>(ptr);
    }

    void deallocate(T* const ptr, std::size_t const sz) noexcept
    {
      pool.deallocate(ptr, sz);
    }

};

template<class T>
bool operator ==(mem_allocator<T> const &a, mem_allocator<T> const &b)
{
  return std::addressof(a.pool) == std::addressof(b.pool);
}

template<class T>
bool operator !=(mem_allocator<T> const &a, mem_allocator<T> const &b)
{
  return !(a == b);
}

signed main()
{
  std::vector<char, mem_allocator<char>> vec{1, 2, 3,};
  vec.emplace_back(4);
  vec.emplace_back(5);

  for( auto &val: vec)
    std::cout << +val << std::endl;

  return 0;
}
