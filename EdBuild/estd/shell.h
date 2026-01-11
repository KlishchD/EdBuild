#pragma once

namespace estd
{
  template <typename tested_type>
  concept shell_output_parser = requires(tested_type object, const char* line, std::size_t length, const void* cookie) {
    { object.parse(line, length, cookie) } -> std::same_as<void>;
  };

  struct default_shell_parser
  {
    void parse(const char* line, std::size_t length, const void* cookie)
    { /* Intentionally left empty. */ }
  };

  struct colors
  {
    static inline const char* red() { return "\x1b[31m"; }
    static inline const char* green() { return "\x1b[32m"; }
    static inline const char* yellow() { return "\x1b[33m"; }
    static inline const char* blue() { return "\x1b[34m"; }
    static inline const char* reset() { return "\x1b[0m"; }
  };

#pragma message("Platform specific code.")
  template <std::size_t line_capacity = 2048>
  class shell final
  {
  public:
    template <typename command_string_type, shell_output_parser parser_type>
    void run(command_string_type command, parser_type& parser, const void* cookie)
    {
      FILE* pipe = _popen(command.c_str(), "rt");
      if (!pipe) throw_error<std::runtime_error>("Failed to open shell pipe.");

      estd::stack_string<line_capacity> line;

      constexpr std::size_t buffer_capacity = 512;
      char buffer[buffer_capacity];

      while (fgets(buffer, buffer_capacity, pipe))
      {
        for (std::size_t index{ 0 }; index < buffer_capacity; ++index)
        {
          const char symbol = buffer[index];

          const bool new_line = symbol == '\n';
          const bool end_of_file = feof(pipe) && symbol == '\0';
          if (new_line || end_of_file)
          {
            parser.parse(line.c_str(), line.size(), cookie);
            line.clear();
            break;
          }

          line.push_back(symbol);
        }
      }

      if (ferror(pipe))
      {
        throw_error<std::runtime_error>("Failed to read from shell pipe.");
      }

      _pclose(pipe);
    }

    template <typename command_string_type, shell_output_parser parser_type>
    void run(command_string_type command, parser_type parser)
    {
      run(command, parser, nullptr);
    }

    template <typename command_string_type>
    void run(command_string_type command)
    {
      default_shell_parser parser;
      run(command, parser, nullptr);
    }
  };
}