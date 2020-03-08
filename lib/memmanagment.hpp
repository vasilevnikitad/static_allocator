#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <iterator>
#include <memory>
#include <mutex>
#include <type_traits>
#include <iostream>

#define DEBUG_PRINT(value) std::cout << __PRETTY_FUNCTION__ << " "#value ": " << (value) << std::endl

template<std::size_t POOL_SIZE = 1000,
         std::size_t CHUNK_SIZE = 10>
class mem_pool {

  static_assert(CHUNK_SIZE < POOL_SIZE,
      "chunk size should be less or equal to POOL_SIZE ");

  private:

    template<typename T, typename U>
    static constexpr auto div_with_round(T const &a, U const &b)
    {
      return a / b + !!(a % b);
    }

    template <std::size_t SIZE = 10>
    struct chunk {
      std::uint8_t bytes[SIZE];
    };
    struct chunk_info_t {
      bool is_reserved;

      constexpr chunk_info_t(bool const reserved = false) : is_reserved{reserved} {};
    };

    static constexpr std::size_t max_align = alignof(std::max_align_t);
    static constexpr std::size_t chunks_cnt = div_with_round(POOL_SIZE, CHUNK_SIZE);

    using chunk_type = std::aligned_storage_t<sizeof(chunk<CHUNK_SIZE>),
                                              alignof(chunk<CHUNK_SIZE>)>;
    struct {
      chunk_type chunk[chunks_cnt]{};
      chunk_info_t chunk_info[chunks_cnt]{};
    } pool{};

    std::mutex pool_mutex;

    static constexpr std::size_t bytes_cnt2chunks_cnt(std::size_t const bytes_cnt) noexcept
    {
      return div_with_round(bytes_cnt, sizeof(pool.chunk[0]));
    }

    inline std::size_t get_chunk_index_by_ptr(void const *ptr) noexcept
    {
      std::uintptr_t const a(reinterpret_cast<std::uintptr_t>(ptr));
      std::uintptr_t const b(reinterpret_cast<std::uintptr_t>(&pool.chunk[0]));

      return  (a - b) / sizeof(pool.chunk[0]);
    }

  public:
    inline mem_pool()
    {
      std::cout << __PRETTY_FUNCTION__ <<": chunks count = " << chunks_cnt << std::endl;
      std::cout << __PRETTY_FUNCTION__ <<": pool address = " << &pool.chunk[0] << std::endl;
    }

    [[nodiscard]]
    inline void *allocate(std::size_t const bytes_cnt, std::size_t const alignment = max_align) noexcept
    {
      std::size_t const min_chunks_count{div_with_round(bytes_cnt, CHUNK_SIZE)};

      auto const search_free_chunks = [count = min_chunks_count](auto const it_begin, auto const it_last) {
        return std::search_n(it_begin, it_last, count, chunk_info_t{false}, [](auto const &a, auto const &b){
            return a.is_reserved == b.is_reserved;
        });
      };
      auto const it_chunk_info_begin{std::begin(pool.chunk_info)};
      auto const it_chunk_info_end{std::end(pool.chunk_info)};

      std::lock_guard<std::mutex> guard{pool_mutex};

      for(auto it{search_free_chunks(it_chunk_info_begin, it_chunk_info_end)};
          it != it_chunk_info_end;
          it = search_free_chunks(std::next(it), it_chunk_info_end)) {

        std::size_t const first_chunk_index(std::distance(it_chunk_info_begin, it));
        void *align_ptr{&pool.chunk[first_chunk_index]};
        std::size_t space(reinterpret_cast<std::uint8_t const*>(&pool.chunk + 1) - reinterpret_cast<std::uint8_t const*>(align_ptr));

        if (!std::align(alignment, bytes_cnt, align_ptr, space))
          return nullptr;


        DEBUG_PRINT(first_chunk_index);
        DEBUG_PRINT(align_ptr);
        auto const align_first_index{get_chunk_index_by_ptr(align_ptr)};
        DEBUG_PRINT(align_first_index);

        void* some_ptr{reinterpret_cast<std::uint8_t *>(align_ptr) + bytes_cnt};
        DEBUG_PRINT(some_ptr);
        auto const align_last_index{get_chunk_index_by_ptr(reinterpret_cast<std::uint8_t *>(align_ptr) + bytes_cnt)};
        DEBUG_PRINT(align_last_index);

        // if (align_first_index < first_chunk_index + min_chunks_count) &&
        //     (align_last_index < first_chunk_index + min_chunks_count)
        // then it's already checked that they are not reserved. But i don't want make logic too hard to read,
        // so let it be just stupid.

        if (auto align_it_begin{std::next(it_chunk_info_begin, align_first_index)},
                 align_it_end{std::next(it_chunk_info_begin, align_last_index + 1)};
            std::none_of(align_it_begin, align_it_end, [](auto info){ return info.is_reserved; })) {

          std::fill(align_it_begin, align_it_end, chunk_info_t{true});
          return align_ptr;
        }
      }

      return nullptr;
    }

    inline void deallocate(void *const ptr,
                    std::size_t const bytes_cnt,
                    [[maybe_unused]]std::size_t const alignment = max_align) noexcept
    {
      if (ptr == nullptr || bytes_cnt == 0)
        return;

      std::size_t const first_chunk_index{get_chunk_index_by_ptr(ptr)};
      std::size_t const last_chunk_index{get_chunk_index_by_ptr(reinterpret_cast<std::uint8_t *>(ptr) + bytes_cnt)};
      std::size_t const chunks2deallocate{last_chunk_index - first_chunk_index};

      std::lock_guard<std::mutex> guard{pool_mutex};

      std::fill_n(std::next(std::begin(pool.chunk_info), first_chunk_index), chunks2deallocate, false);
    }
};

constexpr std::size_t STATIC_POOL_SIZE{1000};
constexpr std::size_t STATIC_POOL_CHUNK_SIZE{100};

static inline mem_pool<STATIC_POOL_SIZE, STATIC_POOL_CHUNK_SIZE> static_pool{};

template<class T, typename MEM_POOL = decltype(static_pool)>
class mem_allocator{
  private:
    MEM_POOL &pool;

  public:
    using value_type = T;

    inline explicit mem_allocator(MEM_POOL &mem_pool = static_pool) noexcept : pool{mem_pool}
    {}

    template<class U>
    inline mem_allocator(mem_allocator<U> const &allctr) noexcept : pool{allctr.pool}
    {}

    inline T* allocate(std::size_t n) noexcept
    {
      std::cout << __PRETTY_FUNCTION__ <<": allocating " << n << " elementes of size " << sizeof(T) << std::endl;
      void *ptr = pool.allocate(n * sizeof(T), alignof(T));


      if (ptr == nullptr) {
        std::cout << "ASSERT" << std::endl;
      }
      return reinterpret_cast<T*>(ptr);
    }

    inline void deallocate(T* const ptr, std::size_t const sz) noexcept
    {
      pool.deallocate(ptr, sz);
    }

};

template<class T>
inline bool operator ==(mem_allocator<T> const &a, mem_allocator<T> const &b)
{
  return std::addressof(a.pool) == std::addressof(b.pool);
}

template<class T>
inline bool operator !=(mem_allocator<T> const &a, mem_allocator<T> const &b)
{
  return !(a == b);
}

