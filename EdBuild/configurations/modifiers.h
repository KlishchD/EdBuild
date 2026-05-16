#pragma once

#include "EdBuild.h"

enum class builder_options : int8_t
{
  language_standard = 0,
  waringings_level,
  disable_warnings,
  generate_debug_information,
  generate_symbols_database,
  symbols_database_source
};

struct option_description
{
  std::string value;
  builder_options type;

  uint32_t get_hash() const;
};

struct define_description
{
  std::string name;
  std::string value;

  uint32_t get_hash() const;
};

using options_list = std::vector<option_description>;
using defines_list = std::vector<define_description>;

template <typename modified_type_list>
struct modifier_description
{
  name_string name;
  modified_type_list options;

  uint32_t get_hash() const
  {
    uint32_t hash = 0;

    for (const auto& option : options)
    {
      uint32_t option_hash = option.get_hash();
      hash = estd::crc32_append(hash, &option_hash, sizeof(option_hash));
    }

    return hash;
  }
};

using options_modifier = modifier_description<options_list>;
using defines_modifier = modifier_description<defines_list>;

using options_modifier_list = std::vector<options_modifier>;
using defines_modifier_list = std::vector<defines_modifier>;

struct modifiers_configuration
{
  options_modifier_list options;
  defines_modifier_list defines;
};