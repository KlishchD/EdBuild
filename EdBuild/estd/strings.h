#pragma once

#include <string>
#include "exceptions.h"

namespace estd
{
  template <typename type, std::size_t capacity>
  struct stack_allocator
  {
    using value_type = type;

    template <class _Other>
    struct rebind {
      using other = stack_allocator<_Other, capacity>;
    };

    stack_allocator() = default;
    stack_allocator(const stack_allocator& other) {}

    value_type* allocate(std::size_t size)
    {
      assert_condition(capacity == size, "Expected size to be equal to capacity for static string allocation [{}!={}].", capacity, size);
      return data;
    }

    void deallocate(value_type* ptr, std::size_t size)
    {
      assert_condition(capacity == size, "Expected size to be equal to capacity for static string allocation. [{}!={}]", capacity, size);
    }
  protected:
    value_type data[capacity];
  };

#pragma message("Need to improve (make work) capacity/size limit reached messages.")
  template<std::size_t capacity>
  class stack_string : public std::basic_string<char, std::char_traits<char>, estd::stack_allocator<char, capacity>>
  {
    using super = std::basic_string<char, std::char_traits<char>, estd::stack_allocator<char, capacity>>;
  public:
    static inline constexpr std::size_t get_static_capacity()
    {
      return capacity;
    }

    stack_string() : super()
    {
      constexpr std::size_t small_string_capacity = 16;
      super::reserve(capacity - small_string_capacity);
    }

    stack_string(const char* begin, const char* end) : stack_string()
    {
      super::append(begin, end);
    }

    stack_string(const char* intial_value) : stack_string()
    {
      super::append(intial_value);
    }

    stack_string(const stack_string& other) : stack_string()
    {
      super::append(other.c_str());
    }

    stack_string(stack_string&& other) : stack_string()
    {
      super::append(other.c_str());
    }

    stack_string& operator=(const stack_string& other)
    {
      super::clear();
      super::append(other.c_str());
      return *this;
    }

    stack_string& operator=(stack_string&& other)
    {
      super::clear();
      super::append(other.c_str());
      return *this;
    }
  };

  using stack_string_512 = stack_string<512>;
  using stack_string_1024 = stack_string<1024>;
  using stack_string_2048 = stack_string<2048>;
  using stack_string_4096 = stack_string<4096>;
  using stack_string_8192 = stack_string<8192>;
  using stack_string_16384 = stack_string<16384>;
  using stack_string_32768 = stack_string<32768>;

  template<typename string_type>
  void remove_file_extension(string_type& path)
  {
    while (path.size() && path.back() != '.') path.pop_back();
    if (path.size()) path.pop_back();
  }

  template<typename input_string_type, typename output_string_type>
  void remove_file_extension(const input_string_type& input, const output_string_type& output)
  {
    output = input;
    remove_file_extension(output);
  }

  template <typename string_type>
  void replace_extension(string_type& path, const char* new_extension)
  {
    remove_file_extension(path);
    path.append(new_extension);
  }

  template<typename path_string_type>
  void append_filename(const char* filename, path_string_type& path)
  {
#pragma message("Platform dependent code.")

    if (!filename) return;

    std::size_t size = strnlen(filename, 512);

    std::size_t filename_begin_index = size - 1;
    while (filename_begin_index > 0 && filename[filename_begin_index] != '\\')
    {
      --filename_begin_index;
    }

    for (std::size_t index = filename_begin_index; index < size && filename[index] != '.'; ++index)
    {
      path.push_back(filename[index]);
    }
  }

  template<typename filename_string_type, typename path_string_type>
  void append_filename(const filename_string_type& filename, path_string_type& path)
  {
    append_filename(filename.c_str(), path);
  }

  inline char capitalize(char c)
  {
    return (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;
  }

  inline void capitalize_inline(char& c)
  {
    c = capitalize(c);
  }

  int32_t stoi(const char* start, const char* end);
}