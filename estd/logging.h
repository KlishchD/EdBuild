#pragma once

// TODO: Try to remove copy cost when passing std::string as argument.
namespace estd
{
  template <std::size_t capacity>
  struct stack_string;

  template <typename... args_types>
  inline void assert_condition(bool condition, const std::format_string<args_types...>& format, args_types... args);

  template <std::size_t capacity = 8192>
  [[nodiscard]] stack_string<capacity> vformat_stack(const std::string_view _Fmt, const std::format_args _Args) {
    assert_condition(capacity >= _Fmt.size() + _Args._Estimate_required_capacity(), "Exceeded capacity of stack format [{}>{}].", _Fmt.size() + _Args._Estimate_required_capacity(), capacity);
    stack_string<capacity> _Str;
    std::vformat_to(std::back_insert_iterator{ _Str }, _Fmt, _Args);
    return _Str;
  }

  template <typename... args_types>
  inline void log(const std::format_string<args_types...>& format, args_types... args)
  {
    constexpr bool logging_enabled = true;
    if constexpr (logging_enabled)
    {
      std::cout << estd::vformat_stack(format.get(), std::make_format_args(args...)) << "\n";
    }
  }

  inline void log(const char* string)
  {
    log("{}", string);
  }
}