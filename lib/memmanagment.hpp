#ifndef MEMMANAGMENT_HPP__
#define MEMMANAGMENT_HPP__

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstddef>
#include <iterator>
#include <memory>
#include <mutex>
#include <type_traits>
#include <iostream>

#ifdef __FUNCSIG__
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

namespace memmanagment {

  template<std::size_t POOL_SIZE = 1000,
           std::size_t CHUNK_SIZE = 10>
  class mem_pool {

    static_assert(CHUNK_SIZE <= POOL_SIZE,
        "chunk size should be less or equal to pool size ");

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
        //FIXME: Actually, we could use only 1 bit as a flag per chunk
        // so it would take less additional memory.
        bool is_reserved;

        constexpr chunk_info_t(bool const reserved = false) : is_reserved{reserved} {};
      };

      static constexpr std::size_t max_align = alignof(std::max_align_t);
      static constexpr std::size_t chunks_cnt = div_with_round(POOL_SIZE, sizeof(chunk<CHUNK_SIZE>));

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
        auto const a{reinterpret_cast<std::uintptr_t>(ptr)};
        auto const b{reinterpret_cast<std::uintptr_t>(&pool.chunk[0])};

        return  (a - b) / sizeof(pool.chunk[0]);
      }

    public:

      inline mem_pool() { }

      [[nodiscard]]
      inline void *allocate(std::size_t const bytes_cnt, std::size_t const alignment = max_align) noexcept
      {
        if (bytes_cnt == 0)
          return &pool.chunk[0];

        auto const &&is_free = [](auto const &a){return !a.is_reserved;};
        auto const &&get_chunk = [this](auto const info_it) {
          std::size_t const index(std::distance(std::begin(pool.chunk_info), info_it));
          return &pool.chunk[index];
        };


        for( auto it = std::begin(pool.chunk_info), end{std::end(pool.chunk_info)}; it != end;
            it = std::find_if(std::next(it), end, is_free)) {
          void *ptr{get_chunk(it)};
          std::size_t space{sizeof pool.chunk[0]};

          if (std::align(alignment, 0, ptr, space)) {

            if (bytes_cnt <= space) {
              (*it).is_reserved = false;
              return ptr;
            }

            decltype(std::distance(it, end)) const
                additional_chunks_needed(div_with_round(bytes_cnt - space, sizeof pool.chunk[0]));

            if (std::distance(it, end) >= additional_chunks_needed) {
              auto range_end = std::next(it, additional_chunks_needed + 1);

              if (std::all_of(std::next(it), range_end, is_free)) {
                std::fill(it, range_end, chunk_info_t{true});
                return ptr;
              }
            }
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
        std::size_t const chunks2deallocate{last_chunk_index - first_chunk_index + 1};

        std::lock_guard<std::mutex> guard{pool_mutex};

        std::fill_n(std::next(std::begin(pool.chunk_info), first_chunk_index), chunks2deallocate, false);
      }
  };


  template<class T, typename MEM_POOL>
  class mem_allocator{
    private:
      MEM_POOL &pool;

    public:
      using value_type = T;
      using mem_pool_type = MEM_POOL;

      template<typename U, typename U_MEM_POOL = MEM_POOL>
      struct rebind { using other = mem_allocator<U, U_MEM_POOL>;};

      inline explicit mem_allocator(MEM_POOL &mem_pool) noexcept : pool{mem_pool}
      {}

      template<class U, typename U_MEM_POOL>
      inline mem_allocator(mem_allocator<U, U_MEM_POOL> const &allctr) noexcept : pool{allctr.pool}
      {}

      [[nodiscard]]
      inline T* allocate(std::size_t n) noexcept
      {
        void *ptr = pool.allocate(n * sizeof(T), alignof(T));

        if (!ptr) {
          std::cerr << __PRETTY_FUNCTION__ << ": Cannot allocate enough memmory" << std::endl;
          exit(EXIT_FAILURE);
        }
        return reinterpret_cast<T*>(ptr);
      }

      inline void deallocate(T* const ptr, std::size_t const n) noexcept
      {
        pool.deallocate(ptr, n * sizeof(T));
      }

      template<class U, typename U_MEM_POOL>
      inline bool operator ==(mem_allocator<U, U_MEM_POOL> const &a)
      {
        return std::addressof(pool) == std::addressof(a.pool);
      }

      template<class U, typename U_MEM_POOL>
      inline bool operator !=(mem_allocator<U, U_MEM_POOL> const &a)
      {
        return !(*this == a);
      }

  };

}
#endif
