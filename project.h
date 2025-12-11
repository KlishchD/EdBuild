#pragma once

enum class compiler_options : int8_t
{
  language_standard = 0,
  waringings_level,
  disable_warnings
};

// TODO: Consider pointers or moving.
struct option_description
{
  compiler_options type;
  std::string value;
};

struct define_description
{
  std::string key;
  std::string value;
};

using option_descriptions = std::vector<option_description>;
using define_descriptions = std::vector<define_description>;
using dependecy_libraries = std::vector<std::string>;
using source_files = std::vector<std::string>;
using include_files = std::vector<std::string>;

enum class artifact_types : uint8_t
{
  excutable = 0,
  static_library,
  dynamic_library
};

struct subproject_configuration
{
  std::string name;

  option_descriptions options;
  define_descriptions defines;
  dependecy_libraries dependencies;

  source_files sources;
  include_files includes;
  std::string precompile_header;

  artifact_types artifact_type;
  std::string artifact_name;

  bool has_precompile_header() const
  {
    return precompile_header.size();
  }

  bool is_precompiled() const
  {
    return artifact_name.size();
  }
};

using subproject_configurations = std::vector<subproject_configuration>;

struct compilable_view
{
  const char* path = nullptr;
  const char* extension = nullptr;

  const char* subproject_name = nullptr;
  std::size_t subproject_index = -1;

  bool is_source = false;
  bool has_precompile_header = false;

  operator bool() const
  {
    return !!path;
  }
};

struct project_configuration
{
  subproject_configurations subprojects;
  option_descriptions options;
  define_descriptions defines;
  std::string name;

  compilable_view get_source_view(std::size_t subproject_index, std::size_t source_index) const
  {
    compilable_view result{ };

    if (subproject_index < subprojects.size())
    {
      const auto& subproject = subprojects[subproject_index];
      if (source_index < subproject.sources.size())
      {
        result.path = subproject.sources[source_index].c_str();
        result.extension = ".obj";

        result.subproject_name = subproject.name.c_str();
        result.subproject_index = subproject_index;

        result.is_source = true;
        result.has_precompile_header = !subproject.precompile_header.empty();
      }
    }

    return result;
  }

  compilable_view get_precompile_header_view(std::size_t subproject_index) const
  {
    compilable_view result{ };

    if (subproject_index < subprojects.size())
    {
      const auto& subproject = subprojects[subproject_index];
      if (subproject.precompile_header.size())
      {
        result.path = subproject.precompile_header.c_str();
        result.extension = ".pch";

        result.subproject_name = subproject.name.c_str();
        result.subproject_index = subproject_index;

        result.is_source = false;
        result.has_precompile_header = false;
      }
    }

    return result;
  }
};

using dependencies_list = std::vector<const std::string*>;