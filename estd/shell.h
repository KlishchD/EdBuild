#pragma once

namespace estd
{
  struct colors
  {
    static inline const char* red() { return "\x1b[31m"; }
    static inline const char* green() { return "\x1b[32m"; }
    static inline const char* yellow() { return "\x1b[33m"; }
    static inline const char* blue() { return "\x1b[34m"; }
    static inline const char* reset() { return "\x1b[0m"; }
  };

#pragma message("Platform specific code.")
  template <typename result_string_type, bool enable_debug_logging = true>
  class shell final
  {
  public:
    template <typename input_string_type>
    result_string_type run(input_string_type command)
    {
      if constexpr (enable_debug_logging)
      {
        estd::log("{}Executing command{}: {:.128}.", colors::green(), colors::reset(), command.c_str());
      }

      FILE* pipe = _popen(command.c_str(), "r");
      if (!pipe) throw_error<std::runtime_error>("Failed to open shell pipe.");

      char buffer[result_string_type::get_static_capacity()] = { 0 };

      std::size_t total_length = 0;
      char* start = buffer;

      while (true)
      {
        char* result = fgets(start, result_string_type::get_static_capacity() - total_length - 1, pipe);
        if (!result) break;

        std::size_t length = strnlen(start, result_string_type::get_static_capacity() - total_length - 1);
        start += length;
        total_length += length;

        if (length == 0) break;
      }

      if (ferror(pipe))
      {
        throw_error<std::runtime_error>("Failed to read from shell pipe.");
      }

      _pclose(pipe);

      return buffer;
    }
  };
}