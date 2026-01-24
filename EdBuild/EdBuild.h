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
#include "estd/hasing.h"
#include "estd/platform.h"
#include "estd/console/console.h"

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

inline const char* get_type_name(error_types type)
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

inline const char* get_type_color(error_types type)
{
  switch (type)
  {
  case error_types::error: return estd::colors::red();
  case error_types::warning: return estd::colors::yellow();
  case error_types::note: return estd::colors::blue();
  default: return estd::colors::reset();
  }
}

using file_clock = std::chrono::file_clock;
using file_time = std::filesystem::file_time_type;

using command_string = estd::stack_string_8192;
using command_strings = std::vector<command_string>;

using command_alias = estd::stack_string_512;

struct command_description
{
   command_string value;
   command_alias alias;
};

using commands_list = std::vector<command_description>;
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

#pragma message("Needs better organization.")
inline void append_file_data(const command_string& filepath, std::string& store)
{
  std::ifstream file(filepath.c_str(), std::ios_base::in);

  command_string line;
  while (std::getline(file, line))
  {
    store.append(line);
  }
}

inline void dump_to_file(const command_string& filepath, const std::string& data)
{
  std::ofstream file(filepath.c_str(), std::ios_base::out);
  file << data;
}

inline estd::memory_report& builder_memory_report()
{
  static estd::memory_report instance;
  return instance;
}

using name_string = estd::stack_string_128;
using path_string = estd::stack_string_512;