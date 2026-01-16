#pragma once

#include "nlohmann\json.hpp"
#include "strings.h"
#include "files.h"

namespace estd
{
  using json = nlohmann::json;

  template <typename value_type>
  const value_type* fetch_value(const json& input, const char* value_name)
  {
    if (!input.contains(value_name)) return nullptr;
    return input[value_name].get_ptr<const value_type*>();
  }

  template <typename value_type>
  const value_type* fetch_value(const json& input, std::size_t index)
  {
    if (index >= input.size()) return nullptr;
    return input[index].get_ptr<const value_type*>();
  }

  json read_json(const stack_string_512& path);

  void write_json(const stack_string_512& path, const json& object);
}