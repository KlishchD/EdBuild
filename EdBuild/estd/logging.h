#pragma once

#include <format>
#include <mutex>
#include <iostream>
#include <array>

// TODO: Try to remove copy cost when passing std::string as argument.
namespace estd
{
  template <typename buffer_type>
  class buffer_insert_iterator {
  public:
    using iterator_category = std::output_iterator_tag;
    using value_type = buffer_type;
    using pointer = void;
    using reference = void;

    using difference_type = ptrdiff_t;

    explicit buffer_insert_iterator(value_type* store, std::size_t size) : store(store), size(size)
    { }

    buffer_insert_iterator& operator=(const value_type& value)
    {
      (*store) = value;
      ++store;
      return *this;
    }

    buffer_insert_iterator& operator=(value_type&& value)
    {
      (*store) = std::move<value_type>(value);
      return *this;
    }

    buffer_insert_iterator& operator*()
    {
      return *this;
    }

    buffer_insert_iterator& operator++()
    {
      return *this;
    }

    buffer_insert_iterator operator++(int)
    {
      return *this;
    }

  protected:
    value_type* store;
    std::size_t size;
  };


  template <std::size_t capacity>
  void vformat_stack(const std::string_view _Fmt, const std::format_args _Args, char (&buffer)[capacity]) {
    if (_Fmt.size() + _Args._Estimate_required_capacity() >= capacity)
    {
      throw std::logic_error(std::format("Exceeded capacity of stack format [{}>{}].", _Fmt.size() + _Args._Estimate_required_capacity(), capacity));
    }

    std::vformat_to(buffer_insert_iterator<char> { buffer, capacity }, _Fmt, _Args);
  }

  template <typename... args_types>
  inline void log(const std::format_string<args_types...>& format, args_types... args)
  {
    static std::mutex lock;
    std::lock_guard _(lock);

    constexpr bool logging_enabled = true;
    if constexpr (logging_enabled)
    {
      char buffer[8192] = { 0 };
      estd::vformat_stack(format.get(), std::make_format_args(args...), buffer);
      std::cout << buffer << "\n";
    }
  }

  inline void log(const char* string)
  {
    log("{}", string);
  }
}