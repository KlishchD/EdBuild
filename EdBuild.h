#pragma once

#include <atomic>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <locale>
#include <memory>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <iostream>
#include <cstdint>
#include <format>
#include <thread>
#include <cstdlib>
#include <print>
#include <cstdio>
#include <tuple>
#include <utility>
#include <map>
#include <set>
#include <queue>
#include <stdio.h>
#include <iterator>

#include <Windows.h>
#undef min
#undef max

#include "estd/exceptions.h"
#include "estd/strings.h"
#include "estd/shell.h"
#include "estd/threading.h"
#include "estd/memory.h"
#include "estd/json.h"

struct strings
{
  static constexpr uint32_t max_parameter_name_length = 256;
  static constexpr uint32_t max_parameter_length = 256;
  static constexpr uint32_t max_path_length = 260;
  static constexpr uint32_t default_local_string_legth = 512;

  static constexpr const char* target_parameter_name = "-Target";
  static constexpr const char* platform_parameter_name = "-Platform";
  static constexpr const char* intermediate_parameter_name = "-Intermediate";
  static constexpr const char* builds_parameter_name = "-Builds";
  static constexpr const char* project_parameter_name = "-Project";
  static constexpr const char* thread_parameter_name = "-Threads";
  static constexpr const char* ignore_builder_update_name = "-IgnoreBuilderUpdate";

  static constexpr const char* instructions_path = "instructions.json";

  template <uint32_t size = default_local_string_legth>
  static inline char* string()
  {
    return new char[size] { 0 };
  }

  static inline void concat_inline(char* result, uint32_t result_size, const char* str)
  {
    uint32_t str_len = strlen(str);
    estd::assert_condition(str_len < result_size, "Concatenation result exceeds local string size.");
    memcpy(result, str, str_len);
  }

  template <uint32_t size = default_local_string_legth>
  static inline const char* concat(const char* lhs, const char* rhs)
  {
    const uint32_t lhs_size = strlen(lhs);

    char* result = string<default_local_string_legth>();
    concat_inline(result, default_local_string_legth, lhs);
    concat_inline(result + lhs_size, default_local_string_legth, rhs);
    return result;
  }

  static inline void free(const char* string)
  {
    if (!string)
    {
      estd::throw_error<std::invalid_argument>("Can not deallocate nullptr string.");
    }

    delete[] string;
  }
};

enum class error_types
{
  error,
  warning,
  note,
  unknown
};

enum class execution_policy
{
  stop_on_error,
  stop_on_warning,
  stop_on_note,
  stop_on_any,
  disregard_all
};

const char* get_type_name(error_types type)
{
  switch (type)
  {
  case error_types::note: return "Note";
  case error_types::warning: return "Warning";
  case error_types::error: return "Error";
  case error_types::unknown: return "Unknown";
  default: return "N/A";
  }
}

const char* get_type_color(error_types type)
{
  switch (type)
  {
  case error_types::error: return estd::colors::red();
  case error_types::warning: return estd::colors::yellow();
  case error_types::note: return estd::colors::blue();
  default: return estd::colors::reset();
  }
}

using command_string = estd::stack_string_8192;
using commands_list = std::vector<command_string>;
using command_output = std::string;

class command_output_parser
{
public:
  virtual void parse(const char* line, std::size_t length, const void* cookie) = 0;
  virtual void set_execution_policy(execution_policy policy) = 0;
  virtual bool can_proceed() const = 0;
  virtual ~command_output_parser() = default;
};

using command_output_parser_ptr = std::shared_ptr<command_output_parser>;

struct commands_partition
{
  commands_list commands;
  command_output_parser_ptr parser;

  std::size_t get_commands_count() const
  {
    return commands.size();
  }
};

using commands_partitions = std::vector<commands_partition>;


#include "project.h"

#include "cli.h"

#pragma message("Needs better organization.")
inline command_string get_output_path(const compilable_view& view)
{
  command_string result = g_cli_parameters.get_intermediate_path();
  result.append(view.subproject_name);
  append_filename(view.path, result);

  return result;
}

void append_file_data(const command_string& filepath, std::string& store)
{
  std::ifstream file(filepath.c_str(), std::ios_base::in);

  command_string line;
  while (std::getline(file, line))
  {
    store.append(line);
  }
}

void dump_to_file(const command_string& filepath, const std::string& data)
{
  std::ofstream file(filepath.c_str(), std::ios_base::out);
  file << data;
}

#include "tools_registry.h"
#include "builder.h"

#include "readers/json_reader.h"
#include "parsers/input_parser.h"

inline estd::memory_report& builder_memory_report()
{
  static estd::memory_report instance;
  return instance;
}

void* operator new(size_t size)
{
  if (size == 0)
  {
    estd::throw_error<std::invalid_argument>("Can not allocate 0 bytes of memory.");
  }

  builder_memory_report().allocate(size);

  uint32_t* header = reinterpret_cast<uint32_t*>(malloc(size + sizeof(uint32_t)));
  if (!header)
  {
    estd::throw_error<std::logic_error>("Failed to allocate {} bytes.", size);
  }

  (*header) = size;
  return reinterpret_cast<void*>(header + 1);
}

void operator delete(void* data) noexcept
{
  if (data)
  {
    uint32_t* header = reinterpret_cast<uint32_t*>(data) - 1;

    builder_memory_report().deallocate(*header);
    free(header);
  }
}