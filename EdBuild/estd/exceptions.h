#pragma once

#include "logging.h"

namespace estd
{
  // Small note from me to me) due to exploration of std::format.
  // std::format expects format_string which has only consteval constructor.
  // Which means function has to receive format_string that will be constructed at compile time
  // rather than here during runtime.
  template <typename error_type, typename... args_types>
  inline void throw_error(const std::format_string<args_types...> format, args_types... args)
  {
    estd::log(format, std::forward<args_types>(args)...);
    throw error_type(std::vformat(format.get(), std::make_format_args(args...)));
  }

  template <typename object_type, typename function_type, typename... args_types>
  inline void try_or_log_on_fail(object_type* object, function_type function, args_types... args)
  {
    try
    {
      (object->*function)(args...);
    }
    catch (const std::exception& exception)
    {
      estd::log(exception.what());
      std::exit(1);
    }
  }

  template <typename... args_types>
  inline void assert_condition(bool condition, const std::format_string<args_types...>& format, args_types... args)
  {
    constexpr bool assertions_enabled = true;
    if constexpr (assertions_enabled)
    {
      if (!condition)
      {
        throw_error<std::logic_error>(format, std::forward<args_types>(args)...);
      }
    }
  }

  inline const char* fetch_error_message_friendly(int32_t error_code)
  {
    constexpr uint32_t message_capacity = 256;
    char* message = new char[message_capacity];
    errno_t error = strerror_s(message, message_capacity, error_code);

    if (error)
    {
      throw_error<std::logic_error>("Failed to fetch error message with code {}.", error);
    }

    return message ? message : "Failed to fetch error message.";
  }

  template <typename... args_types>
  inline void no_default(const std::format_string<args_types...> format, args_types... args)
  {
    throw_error<std::logic_error>(format, std::forward<args_types>(args)...);
  }
}