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

struct project_configuration
{
  struct resource_view
  {
    const project_configuration& project;
    std::size_t subproject_index;

    const std::string& get_subproject_name() const
    {
      return project.subprojects[subproject_index].name;
    }
  };

  struct source_view : public resource_view
  {
    std::size_t source_index;

    const std::string& get_path() const
    {
      return project.subprojects[subproject_index].sources[source_index];
    }

    bool has_precompiler_header() const
    {
      return project.subprojects[subproject_index].has_precompile_header();
    }
  };

  struct precompile_header_view : public resource_view
  {
    const std::string& get_path() const
    {
      return project.subprojects[subproject_index].precompile_header;
    }

    operator bool() const
    {
      return project.subprojects[subproject_index].has_precompile_header();
    }
  };

  subproject_configurations subprojects;
  option_descriptions options;
  define_descriptions defines;
  std::string name;

  source_view get_source_view(std::size_t subproject, std::size_t source) const
  {
    // TODO: Add assert.
    return { *this, subproject, source };
  }

  precompile_header_view get_precompile_header_view(std::size_t subproject) const
  {
    // TODO: Add assert.
    return { *this, subproject };
  }
};

using source_view = project_configuration::source_view;
using precompile_header_view = project_configuration::precompile_header_view;
using dependencies_list = std::vector<const std::string*>;