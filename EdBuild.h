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

  static constexpr const char* root_path_parameter_name = "-Root";
  static constexpr const char* target_parameter_name = "-Target";
  static constexpr const char* platform_parameter_name = "-Platform";
  static constexpr const char* intermediate_parameter_name = "-Intermediate";
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

#include "project.h"

#include "cli.h"

#pragma warning "Needs better organization."
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