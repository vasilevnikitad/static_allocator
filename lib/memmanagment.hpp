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
#include <vector>

#ifdef __FUNCSIG__
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

namespace memmanagment {

  template<std::size_t POOL_SIZE = 1000,
           std::size_t CHUNK_SIZE = 10>
  class mem_pool {

    static_assert(POOL_SIZE % CHUNK_SIZE == 0,
        "Pool size must be a multiple of chunk size");

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


      static constexpr std::size_t max_align = alignof(std::max_align_t);
      static constexpr std::size_t chunks_cnt = div_with_round(POOL_SIZE, sizeof(chunk<CHUNK_SIZE>));

      using chunk_type = std::aligned_storage_t<sizeof(chunk<CHUNK_SIZE>),
                                                alignof(chunk<CHUNK_SIZE>)>;
      struct {

        template <typename T, std::size_t N>
        struct vec_all {
          bool is_free{true};

          using value_type = T;

          template <typename U>
            struct rebind {
              using other = vec_all<U, N>;
            };

          constexpr vec_all() noexcept = default;

          constexpr vec_all(vec_all<bool, N> const &) noexcept {
          }

          [[nodiscard]]
          T *allocate(std::size_t const n) {
            alignas(T) static std::uint8_t storage[div_with_round(N, std::numeric_limits<std::uint8_t>::digits)];

            if (is_free && n > std::size(storage)) {
              std::cerr << __PRETTY_FUNCTION__ << ": Cannot allocate enough memmory" << std::endl;
              exit(EXIT_FAILURE);
            }

            is_free = false;
            return reinterpret_cast<T*>(storage);
          }

          constexpr
            void deallocate(T *const, std::size_t const) noexcept {
              is_free = true;
            }
        };

        chunk_type chunk[chunks_cnt]{};
        std::vector<bool, vec_all<bool, chunks_cnt>> chunk_reserved;
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

      inline mem_pool()
      {
          pool.chunk_reserved.resize(chunks_cnt);
      }

      [[nodiscard]]
      inline void *allocate(std::size_t const bytes_cnt, std::size_t const alignment = max_align) noexcept
      {
        if (bytes_cnt == 0)
          return &pool.chunk[0];

        auto const &&is_free = [](auto const &a){return !a;};
        auto const &&get_chunk = [this](auto const info_it) {
          std::size_t const index(std::distance(std::begin(pool.chunk_reserved), info_it));
          return &pool.chunk[index];
        };


        for( auto it = std::begin(pool.chunk_reserved), end{std::end(pool.chunk_reserved)}; it != end;
            it = std::find_if(std::next(it), end, is_free)) {
          void *ptr{get_chunk(it)};
          std::size_t space{sizeof pool.chunk[0]};

          if (std::align(alignment, 0, ptr, space)) {

            if (bytes_cnt <= space) {
              *it = false;
              return ptr;
            }

            decltype(std::distance(it, end)) const
                additional_chunks_needed(div_with_round(bytes_cnt - space, sizeof pool.chunk[0]));

            if (std::distance(it, end) >= additional_chunks_needed) {
              auto range_end = std::next(it, additional_chunks_needed + 1);

              if (std::all_of(std::next(it), range_end, is_free)) {
                std::fill(it, range_end, true);
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

        std::fill_n(std::next(std::begin(pool.chunk_reserved), first_chunk_index), chunks2deallocate, false);
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

      template<class U>
      inline mem_allocator(mem_allocator<U, MEM_POOL> const &allctr) noexcept : pool{allctr.pool}
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
        pool.deallocate(ptr, n * sizeof(T), alignof(T));
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
