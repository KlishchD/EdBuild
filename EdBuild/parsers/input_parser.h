#pragma once

#include "readers/input_reader.h"
#include "configurations/instructions.h"

class builder_input_parser
{
  using dependencies_list = std::vector<const char*>;
  using dependencies_lists = std::vector<dependencies_list>;
public:
  using option_parser = builder_options(*)(const std::string& option_name);

  builder_input_parser(const platform& active_platform, const target& active_target)
    : active_platform(active_platform), active_target(active_target)
  { }

  void register_option_parser(option_parser parser)
  {
    option_parsers.push_back(parser);
  }

#pragma message("Need to validate basic data for the compiles like version etc.")
  instructions_description parse(const estd::path& path, instructions_reader* reader)
  {
    estd::log("Project parsing starts.");

    project_path = path;

    instructions_description result;
    project_configuration& project = result.project;
    builds_configurations& builds = result.builds;
    modifiers_configuration& modifiers = result.modifiers;

    std::map<name_string, std::size_t> name_mapping;
    dependencies_lists subprojects_dependencies;

    if (modifier_list_reader* local_reader = reader->read_options_modifiers())
    {
      modifiers.options.resize(local_reader->count());

      std::size_t modifier_index = 0;
      while (local_reader->next())
      {
        options_modifier& modifier = modifiers.options[modifier_index];

        const char* name = local_reader->read_name();
        if (name)
        {
          modifier.name = name;
        }
        else
        {
          estd::throw_error<std::invalid_argument>("Options modifier [{}] must have a name.", modifiers.options.size() - 1);
        }

        parse_options(local_reader, modifier.options, nullptr);

        ++modifier_index;
      }
    }

    if (modifier_list_reader* local_reader = reader->read_defines_modifiers())
    {
      modifiers.defines.resize(local_reader->count());

      std::size_t modifier_index = 0;
      while (local_reader->next())
      {
        defines_modifier& modifier = modifiers.defines[modifier_index];

        const char* name = local_reader->read_name();
        if (name)
        {
          modifier.name = name;
        }
        else
        {
          estd::throw_error<std::invalid_argument>("Defines modifier [{}] must have a name.", modifiers.defines.size() - 1);
        }

        parse_defines(local_reader, modifier.options, nullptr);

        ++modifier_index;
      }
    }

    if (globals_reader* local_reader = reader->read_globals())
    {
      parse_options(local_reader, project.options, nullptr);
      parse_defines(local_reader, project.defines, nullptr);
    }

    if (projects_reader* local_reader = reader->read_projects())
    {
      project.subprojects.resize(local_reader->count());

      std::size_t subproject_index = 0;
      while (local_reader->next())
      {
        subproject_configuration& subproject = project.subprojects[subproject_index];

        if (const char* name = local_reader->read_name())
        {
          subproject.name = name;
          name_mapping[name] = subproject_index;
        }
        else
        {
          estd::throw_error<std::logic_error>("No name was provided for project.");
        }

        parse_options(local_reader, subproject.options, nullptr);
        parse_defines(local_reader, subproject.defines, nullptr);

        parse_sources(local_reader, subproject.sources, subproject.includes);
        parse_includes(local_reader, subproject.includes);
        parse_precompile_header(local_reader, subproject.precompile_header);

        parse_artifacts(local_reader, subproject.artifact);

        subprojects_dependencies.emplace_back();
        dependencies_list& dependencies = subprojects_dependencies.back();

        parse_dependencies(local_reader, dependencies);

        ++subproject_index;
      }
    }

    const std::size_t subprojects_count = subprojects_dependencies.size();
    for (std::size_t subproject_index = 0; subproject_index < subprojects_count; ++subproject_index)
    {
      auto& subproject = project.subprojects[subproject_index];
      subproject.dependencies_count = subprojects_dependencies[subproject_index].size();

      for (const char* dependency_name : subprojects_dependencies[subproject_index])
      {
        estd::log("PP: {}, {}.", project.subprojects[subproject_index].name, dependency_name);

        const std::size_t dependency_index = name_mapping[dependency_name];
        auto& dependency_subrproject = project.subprojects[dependency_index];
        dependency_subrproject.dependants.push_back(subproject_index);
      }
    }

    if (builds_reader* local_reader = reader->read_builds())
    {
      builds.resize(local_reader->count());

      std::size_t build_index = 0;
      while (local_reader->next())
      {
        build_configuration& build = builds[build_index];

        const char* name = local_reader->read_name();
        if (name)
        {
          build.name = name;
        }
        else
        {
          estd::throw_error<std::invalid_argument>("Build [{}] must include name.", builds.size() - 1);
        }

        parse_defines(local_reader, build.defines, nullptr);
        parse_options(local_reader, build.options, nullptr);

        const char* subproject_name = local_reader->read_subprorject();
        if (subproject_name)
        {
          estd::assert_condition(name_mapping.count(subproject_name),
            "Failed to find subproject [{}] for build [{}] creation.",
            subproject_name, name);

          const std::size_t subproject_index = name_mapping[subproject_name];

          build.subproject = &project.subprojects[subproject_index];
        }
        else
        {
          estd::throw_error<std::invalid_argument>("Build configuration [{}] must contain subproject name from which build is created.", name);
        }

        ++build_index;
      }
    }

    estd::log("Instructions parsing finished.");

    return result;
  }
protected:
  template <typename reader_type>
  void parse_options(reader_type* reader, options_list& destination, options_modifier_list* modifiers) const
  {
    modifier_data option;
    while (reader->read_next_option(option))
    {
      const bool platform_filtered = option.platforms && !active_platform.match(option.platforms);
      if (platform_filtered) continue;

      const bool target_filtered = option.targets && !active_target.match(option.targets);
      if (target_filtered) continue;

      destination.push_back(parse_option(option));

      // TODO: Add modifiers handling.
    }
  }

  template <typename reader_type>
  void parse_defines(reader_type* reader, defines_list& destination, defines_modifier_list* modifiers) const
  {
    modifier_data define;
    while (reader->read_next_define(define))
    {
      const bool platform_filtered = define.platforms && !active_platform.match(define.platforms);
      if (platform_filtered) continue;

      const bool target_filtered = define.targets && !active_target.match(define.targets);
      if (target_filtered) continue;

      destination.push_back(parse_define(define));

      // TODO: Add modifiers handling.
    }
  }

  template <typename reader_type>
  void parse_sources(reader_type* reader, sources_list& sources_destination, includes_list& includes_destination) const
  {
    const char* source_path = nullptr;
    while (reader->read_next_source(source_path))
    {
      estd::path path;
      path
        .append(project_path)
        .append(source_path);

      if (!path.exists())
      {
        estd::throw_error<std::invalid_argument>("Cound't find a source [{}].", path);
      }

#pragma message("C++ compiler hardcode.")
      const char* source_extension = ".cpp";

      if (path.is_directory())
      {
        includes_destination.push_back(path);

        for (const std::filesystem::directory_entry& entry : path.iterate_recursively())
        {
          const std::filesystem::path& path = entry.path();

          const bool extension_mismatch = path.extension().compare(source_extension);
          if (extension_mismatch) continue;

          sources_destination.emplace_back(path.string().c_str());
        }
      }
      else
      {
        sources_destination.push_back(path);
      }
    }
  }

  template <typename reader_type>
  void parse_includes(reader_type* reader, includes_list& destination) const
  {
    const char* include_path = nullptr;
    while (reader->read_next_include(include_path))
    {
      estd::path path;
      path
        .append(project_path)
        .append(include_path);

      if (!path.exists())
      {
        estd::throw_error<std::invalid_argument>("Couldn't find include [{}].", path);
      }

      destination.push_back(std::move(path));
    }
  }

  template <typename reader_type>
  void parse_precompile_header(reader_type* reader, compilable_description& destination) const
  {
    const char* precompile_header_path = reader->read_precompile_header_path();
    if (!precompile_header_path) return;

    estd::path& path = destination.path;
    path
      .append(project_path)
      .append(precompile_header_path);

    if (!path.exists())
    {
      estd::throw_error<std::invalid_argument>("Cound't find a precompile header [{}].", path);
    }

    if (!path.is_file())
    {
      estd::throw_error<std::invalid_argument>("Expected precompile header [{}] to be a file.", path);
    }
  }

  template <typename reader_type>
  void parse_artifacts(reader_type* reader, artifact_description& destination) const
  {
    const char* artifact_type = reader->read_artifact_type();
    const char* static_library = reader->read_static_library_path();
    const char* import_library = reader->read_import_library_path();
    const char* dynamic_library = reader->read_dynamic_library_path();
    const char* executable = reader->read_executable_path();

    const bool requests_artifact_creation = artifact_type;
    const bool provides_preproduced_artifact = static_library || import_library || dynamic_library || executable;
    const uint32_t selected_single_path = requests_artifact_creation + provides_preproduced_artifact;
    estd::assert_condition(selected_single_path != 2, "Project must either provide preproduced artifact or request type to generate and not both.");

    estd::log("Artifact status: {}, {}.", requests_artifact_creation, provides_preproduced_artifact);

    if (provides_preproduced_artifact)
    {
      handle_preproduced_artifact(static_library, import_library, dynamic_library, executable, destination);
    }
    else
    {
      handle_produced_artifact(artifact_type, destination);
    }

    const char* resources_path = reader->read_resources_path();
    if (resources_path)
    {
      destination.resources.append(project_path);
      destination.resources.append(resources_path);
    }
  }

  void handle_preproduced_artifact(
    const char* static_library,
    const char* import_library,
    const char* dynamic_library,
    const char* executable,
    artifact_description& destination) const
  {
    destination.preproduced = true;

    const bool preproduced_static_library = static_library;
    const bool preproduced_dynamic_library = dynamic_library;
    const bool preproduced_executable = executable;

    const uint32_t preproduced_artifacts = preproduced_static_library + preproduced_dynamic_library + preproduced_executable;
    estd::assert_condition(preproduced_artifacts == 1, "Project can not provide multiple preproduced artifact types.");

    if (preproduced_static_library)
    {
      destination.type = artifact_types::static_library;
      destination.static_library().append(project_path);
      destination.static_library().append(static_library);
      return;
    }

    if (preproduced_dynamic_library)
    {
      destination.type = artifact_types::dynamic_library;

      if (import_library)
      {
        destination.import_library().append(project_path);
        destination.import_library().append(import_library);
      }

      destination.dynamic_library().append(project_path);
      destination.dynamic_library().append(dynamic_library);

      return;
    }

    destination.type = artifact_types::excutable;
    destination.executable().append(project_path);
    destination.executable().append(executable);
  }

  void handle_produced_artifact(const char* artifact_type, artifact_description& destination) const
  {
    destination.preproduced = false;

    destination.type = artifact_types::static_library;

    if (artifact_type)
    {
      std::string type_name = artifact_type;
      const bool is_executable = std::strcmp(artifact_type, "Executable") == 0;
      const bool is_dynamic_library = std::strcmp(artifact_type, "DynamicLibrary") == 0;
      const bool is_static_library = std::strcmp(artifact_type, "StaticLibary") == 0;

      if (is_executable) destination.type = artifact_types::excutable;
      if (is_dynamic_library) destination.type = artifact_types::dynamic_library;
      if (is_static_library) destination.type = artifact_types::static_library;
    }
  }

  template <typename reader_type>
  void parse_dependencies(reader_type* reader, dependencies_list& destination) const
  {
    const char* dependency_path = nullptr;
    while (reader->read_next_dependency(dependency_path))
    {
      destination.emplace_back(dependency_path);
    }
  }

  option_description parse_option(const modifier_data& input) const
  {
    builder_options type = static_cast<builder_options>(-1);
    for (const auto& parser : option_parsers)
    {
      builder_options result = parser(input.name);
      if (static_cast<int32_t>(result) >= 0)
      {
        type = result;
        break;
      }
    }

    estd::assert_condition(static_cast<int32_t>(type) >= 0, "Failed to parse option [{}].", input.name);
    return option_description{ input.value, type };
  }

  define_description parse_define(const modifier_data& input) const
  {
    return define_description{ input.name, input.value };
  }
protected:
  const platform& active_platform;
  const target& active_target;

  estd::path project_path;

  std::vector<option_parser> option_parsers;
};
