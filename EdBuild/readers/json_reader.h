#pragma once

#include "input_reader.h"
#include "estd/json.h"

class json_modifier_reader
{
public:
  json_modifier_reader()
    : modifier_source(nullptr), option_index(0), define_index(0)
  { }

  bool read_next_option(modifier_data& output)
  {
    if (!modifier_source->contains("Options")) return false;

    const estd::json& options = (*modifier_source)["Options"];
    if (option_index >= options.size()) return false;

    const estd::json& option_object = options[option_index];
    ++option_index;

    output.name = estd::fetch_c_str(option_object, "Name");
    estd::assert_condition(output.name, "Failed to fetch Name parameter from an option.");

    output.value = estd::fetch_c_str(option_object, "Value");
    estd::assert_condition(output.value, "Failed to fetch Value parameter from an option.");

    output.platforms = estd::fetch_c_str(option_object, "Platforms");
    output.targets = estd::fetch_c_str(option_object, "Targets");

    return true;
  }

  bool read_next_define(modifier_data& output)
  {
    if (!modifier_source->contains("Defines")) return false;

    const estd::json& defines = (*modifier_source)["Defines"];
    if (define_index >= defines.size()) return false;

    const estd::json& define_object = defines[define_index];
    ++define_index;

    output.name = estd::fetch_c_str(define_object, "Name");
    estd::assert_condition(output.name, "Failed to fetch Name parameter from a define.");

    output.value = estd::fetch_c_str(define_object, "Value");
    estd::assert_condition(output.value, "Failed to fetch Value parameter from a define.");

    output.platforms = estd::fetch_c_str(define_object, "Platforms");
    output.targets = estd::fetch_c_str(define_object, "Targets");

    return true;
  }

  void reset(const estd::json& new_source)
  {
    modifier_source = &new_source;
    option_index = 0;
    define_index = 0;
  }
protected:
  const estd::json* modifier_source;
  std::size_t option_index;
  std::size_t define_index;
};

class json_globals_reader : public globals_reader, protected json_modifier_reader
{
protected:
  using modifier_reader = json_modifier_reader;
public:
  using inherited = globals_reader;

  json_globals_reader(const estd::json& source)
  {
    estd::assert_condition(source.contains("Globals"), "Globals list must be provided.");
    modifier_reader::reset(source["Globals"]);
  }

  virtual bool read_next_option(modifier_data& output) override
  {
    return modifier_reader::read_next_option(output);
  }

  virtual bool read_next_define(modifier_data& output) override
  {
    return modifier_reader::read_next_define(output);
  }
};

class json_modifier_list_reader : public modifier_list_reader, protected json_modifier_reader
{
protected:
  using modifier_reader = json_modifier_reader;
public:
  using inherited = modifier_list_reader;

  json_modifier_list_reader(const estd::json& source, bool is_define)
    : modifier_index(-1), is_define(is_define)
  {
    estd::assert_condition(source.contains("Modifiers"), "Modifiers list must be provided.");
    modifiers = &source["Modifiers"];
  }

  virtual const char* read_name() override
  {
    if (!is_modifier_active()) return nullptr;

    const estd::json& modifier = get_active_modifier();
    return estd::fetch_c_str(modifier, "Name");
  }

  virtual std::size_t count() override
  {
    return modifiers ? modifiers->size() : 0;
  }

  virtual bool read_next_option(modifier_data& output) override
  {
    if (!is_modifier_active()) return false;
    if (is_define) return false;
    return modifier_reader::read_next_option(output);
  }

  virtual bool read_next_define(modifier_data& output) override
  {
    if (!is_modifier_active()) return false;
    if (!is_define) return false;
    return modifier_reader::read_next_define(output);
  }

  virtual bool next() override
  {
    ++modifier_index;

    if (is_modifier_active())
    {
      modifier_reader::reset(get_active_modifier());
      return true;
    }

    return false;
  }
protected:
  bool is_modifier_active() const
  {
    return modifier_index < modifiers->size();
  }

  const estd::json& get_active_modifier() const
  {
    return is_modifier_active() ? (*modifiers)[modifier_index] : *modifiers;
  }
protected:
  const estd::json* modifiers;
  std::size_t modifier_index;
  bool is_define;
};

class json_projects_reader : public projects_reader, protected json_modifier_reader
{
protected:
  using modifier_reader = json_modifier_reader;
public:
  using inherited = projects_reader;

  json_projects_reader(const estd::json& source) : project_index(-1)
  {
    estd::assert_condition(source.contains("Projects"), "Projects list must be provided.");
    projects = &source["Projects"];
  }

  virtual const char* read_name() override
  {
    if (!is_project_active()) return nullptr;

    const estd::json& project = get_active_project();
    return estd::fetch_c_str(project, "Name");
  }

  virtual std::size_t count() override
  {
    return projects ? projects->size() : 0;
  }

  virtual bool read_next_option(modifier_data& output) override
  {
    if (!is_project_active()) return false;
    return modifier_reader::read_next_option(output);
  }

  virtual bool read_next_define(modifier_data& output) override
  {
    if (!is_project_active()) return false;
    return modifier_reader::read_next_define(output);
  }

  virtual bool read_next_source(const char*& source_path) override
  {
    if (!is_project_active()) return false;

    const estd::json& project = get_active_project();
    if (!project.contains("Sources")) return false;

    const estd::json& sources = project["Sources"];
    if (source_index >= sources.size()) return false;

    source_path = estd::fetch_c_str(sources, source_index++);

    return true;
  }

  virtual bool read_next_include(const char*& include_path) override
  {
    if (!is_project_active()) return false;

    const estd::json& project = get_active_project();
    if (!project.contains("Includes")) return false;

    const estd::json& includes = project["Includes"];
    if (include_index >= includes.size()) return false;

    include_path = estd::fetch_c_str(includes, include_index++);

    return true;
  }

  virtual const char* read_precompile_header_path() override
  {
    if (!is_project_active()) return nullptr;

    const estd::json& project = get_active_project();
    return estd::fetch_c_str(project, "PrecompileHeader");
  }

  virtual const char* read_artifact_type() override
  {
    if (!is_project_active()) return nullptr;

    const estd::json& project = get_active_project();
    return estd::fetch_c_str(project, "ArtifactType");
  }

  virtual const char* read_resources_path() override
  {
    if (!is_project_active()) return nullptr;

    const estd::json& project = get_active_project();
    return estd::fetch_c_str(project, "Resources");
  }

  virtual const char* read_static_library_path() override
  {
    if (!is_project_active()) return nullptr;

    const estd::json& project = get_active_project();
    if (!project.contains("Artifact")) return nullptr;

    const estd::json& artifact = project["Artifact"];
    return estd::fetch_c_str(artifact, "StaticLibrary");
  }

  virtual const char* read_import_library_path() override
  {
    if (!is_project_active()) return nullptr;

    const estd::json& project = get_active_project();
    if (!project.contains("Artifact")) return nullptr;

    const estd::json& artifact = project["Artifact"];
    return estd::fetch_c_str(artifact, "ImportLibrary");
  }

  virtual const char* read_dynamic_library_path() override
  {
    if (!is_project_active()) return nullptr;

    const estd::json& project = get_active_project();
    if (!project.contains("Artifact")) return nullptr;

    const estd::json& artifact = project["Artifact"];
    return estd::fetch_c_str(artifact, "DynamicLibrary");
  }

  virtual const char* read_executable_path() override
  {
    if (!is_project_active()) return nullptr;

    const estd::json& project = get_active_project();
    if (!project.contains("Artifact")) return nullptr;

    const estd::json& artifact = project["Artifact"];
    return estd::fetch_c_str(artifact, "Executable");
  }

  virtual bool read_next_dependency(const char*& path) override
  {
    if (!is_project_active()) return false;

    const estd::json& project = get_active_project();
    if (!project.contains("Dependencies")) return false;

    const estd::json& dependencies = project["Dependencies"];
    path = estd::fetch_c_str(dependencies, dependency_index++);

    return path;
  }

  virtual bool next() override
  {
    ++project_index;

    source_index = 0;
    include_index = 0;
    dependency_index = 0;

    if (is_project_active())
    {
      modifier_reader::reset(get_active_project());
      return true;
    }

    return false;
  }
protected:
  bool is_project_active() const
  {
    return project_index < projects->size();
  }

  const estd::json& get_active_project() const
  {
    return is_project_active() ? (*projects)[project_index] : *projects;
  }
protected:
  const estd::json* projects;
  std::size_t project_index;
  std::size_t source_index;
  std::size_t include_index;
  std::size_t dependency_index;
};

class json_builds_reader : public builds_reader, protected json_modifier_reader
{
protected:
  using modifier_reader = json_modifier_reader;
public:
  using inherited = builds_reader;

  json_builds_reader(const estd::json& source) : build_index(-1)
  {
    estd::assert_condition(source.contains("Builds"), "Builds list must be provided.");
    builds = &source["Builds"];
  }

  virtual const char* read_name() override
  {
    if (!is_build_active()) return nullptr;

    const estd::json& build = get_active_build();
    return estd::fetch_c_str(build, "Name");
  }

  virtual std::size_t count() override
  {
    return builds ? builds->size() : 0;
  }

  virtual bool read_next_option(modifier_data& output) override
  {
    if (!is_build_active()) return false;
    return modifier_reader::read_next_option(output);
  }

  virtual bool read_next_define(modifier_data& output) override
  {
    if (!is_build_active()) return false;
    return modifier_reader::read_next_define(output);
  }

  virtual const char* read_subprorject() override
  {
    if (!is_build_active()) return nullptr;

    const estd::json& build = get_active_build();
    return estd::fetch_c_str(build, "Project");
  }

  virtual bool next() override
  {
    ++build_index;

    if (is_build_active())
    {
      modifier_reader::reset(get_active_build());
      return true;
    }

    return false;
  }
protected:
  bool is_build_active() const
  {
    return build_index < builds->size();
  }

  const estd::json& get_active_build() const
  {
    return is_build_active() ? (*builds)[build_index] : *builds;
  }
protected:
  const estd::json* builds;
  std::size_t build_index;
};

class json_instructions_reader : public instructions_reader
{
public:
  json_instructions_reader(const estd::json& data)
    : data(data),
    globals(data),
    options(data, false),
    defines(data, true),
    projects(data),
    builds(data)
  { }

  virtual globals_reader* read_globals() override
  {
    return reinterpret_cast<globals_reader*>(&globals);
  }

  virtual modifier_list_reader* read_options_modifiers() override
  {
    return reinterpret_cast<modifier_list_reader*>(&options);
  }

  virtual modifier_list_reader* read_defines_modifiers() override
  {
    return reinterpret_cast<modifier_list_reader*>(&defines);
  }

  virtual projects_reader* read_projects() override
  {
    return reinterpret_cast<projects_reader*>(&projects);
  }

  virtual builds_reader* read_builds() override
  {
    return reinterpret_cast<builds_reader*>(&builds);
  }
protected:
  const estd::json& data;
  json_globals_reader globals;
  json_modifier_list_reader options;
  json_modifier_list_reader defines;
  json_projects_reader projects;
  json_builds_reader builds;
};