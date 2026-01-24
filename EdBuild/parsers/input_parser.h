#pragma once

#include "readers/input_reader.h"
#include "project.h"

class builder_input_parser
{
  using dependencies_list = std::vector<const std::string*>;
public:
  using option_parser = builder_options(*)(const std::string& option_name);

  builder_input_parser(const platform& active_platform, const target& active_target)
    : active_platform(active_platform), active_target(active_target)
  { }

  void register_option_parser(option_parser parser)
  {
    option_parsers.push_back(parser);
  }

  project_configuration parse(const estd::path& project_path, input_reader& reader) const
  {
    estd::log("Project parsing starts.");

#pragma message("Need to validate basic data for the compiles like version etc.")

    if (!reader.is_project_name_present()) estd::throw_error<std::logic_error>("'Name' string must be present in the instructions list.");
    if (!reader.is_globals_list_present()) estd::throw_error<std::logic_error>("'Globals' list must be present in the instructions list.");
    if (!reader.is_projects_list_present()) estd::throw_error<std::logic_error>("'Projects' list must be present in the instructions list.");
    if (!reader.is_builds_list_present()) estd::throw_error<std::logic_error>("'Builds' list must be resent in the instructions list");

    std::vector<dependencies_list> dependencies_lists;
    dependencies_list temporary_dependencies;

    project_configuration project;
    parse_project(project_path, reader, project, temporary_dependencies);
    reader.next_subproject();

    estd::log("Globals parsed.");

    // TODO: Can reserve here.
    while (reader.is_subproject_valid())
    {
      subproject_configuration subproject;
      parse_project(project_path, reader, subproject, temporary_dependencies);

      project.subprojects.push_back(std::move(subproject));
      dependencies_lists.push_back(std::move(temporary_dependencies));

      reader.next_subproject();
    }

    while (reader.next_build())
    {
      build_configuration build;

      const std::string* name = reader.build_name();
      const std::string* subproject = reader.build_subproject_name();

      estd::assert_condition(name, "Build configuration must include name.");
      estd::assert_condition(subproject, "Build [{}] configuration must include subproject name.", name->c_str());

      build.name = name->c_str();
      build.subproject_name = subproject->c_str();

      project.builds.push_back(std::move(build));
    }

    const std::size_t subprojects_count = dependencies_lists.size();

    std::map<name_string, std::size_t> name_mapping;
    for (std::size_t subproject_index = 0; subproject_index < subprojects_count; ++subproject_index)
    {
      const auto& subproject = project.subprojects[subproject_index];
      name_mapping[subproject.name] = subproject_index;
    }

    for (std::size_t subproject_index = 0; subproject_index < subprojects_count; ++subproject_index)
    {
      auto& subproject = project.subprojects[subproject_index];
      subproject.dependencies_count = dependencies_lists[subproject_index].size();

      for (const std::string* dependency_name : dependencies_lists[subproject_index])
      {
        estd::log("PP: {}, {}.", project.subprojects[subproject_index].name, *dependency_name);

        const std::size_t dependency_index = name_mapping[dependency_name->c_str()];
        auto& dependency_subrproject = project.subprojects[dependency_index];
        dependency_subrproject.dependants.push_back(subproject_index);
      }
    }

    estd::log("Project parsing finished.");

    return project;
  }
protected:
  inline option_description parse_option(const input_reader::option_data& input) const
  {
    builder_options type = static_cast<builder_options>(-1);
    for (const auto& parser : option_parsers)
    {
      builder_options result = parser(*input.name);
      if (static_cast<int32_t>(result) >= 0)
      {
        type = result;
        break;
      }
    }

    estd::assert_condition(static_cast<int32_t>(type) >= 0, "Failed to parse option [{}].", input.name->c_str());
    return option_description{ type, *input.value };
  }

  inline define_description parse_define(const input_reader::define_data& input) const
  {
    return define_description{ *input.name, *input.value };
  }

  template <typename project_type>
  inline void parse_project(const estd::path& project_path, input_reader& reader, project_type& project, dependencies_list& dependencies) const
  {
    estd::log("Started parsing project.");
    project.name = reader.project_name()->c_str();

    estd::log("Parsing project options.");
    while (reader.has_next_option())
    {
      input_reader::option_data data = reader.next_option();
      if (!data.name) continue;

      const bool platform_filtered = data.platforms && !active_platform.match(data.platforms->c_str());
      if (platform_filtered) continue;

      const bool target_filtered = data.targets && !active_target.match(data.targets->c_str());
      if (target_filtered) continue;

      auto& destination = project.options;
      destination.push_back(parse_option(data));
    }

    estd::log("Parsing project defines.");
    while (reader.has_next_define())
    {
      input_reader::define_data data = reader.next_define();
      if (!data.name) continue;

      const bool platform_filtered = data.platforms && !active_platform.match(data.platforms->c_str());
      if (platform_filtered) continue;

      const bool target_filtered = data.targets && !active_target.match(data.targets->c_str());
      if (target_filtered) continue;

      auto& destination = project.defines;
      destination.push_back(parse_define(data));
    }

    if constexpr (std::is_same_v<project_type, subproject_configuration>)
    {
      while (reader.has_next_source())
      {
        const std::string* source = reader.next_source();
        if (!source) continue;

        estd::path path;
        path
          .append(project_path)
          .append(source->c_str());

        append_all_files(path, ".cpp", project.sources);

        if (path.is_directory())
        {
          project.includes.push_back(std::move(path));
        }
      }

      while (reader.has_next_include())
      {
        const std::string* include = reader.next_include();
        if (!include) continue;

        project.includes.emplace_back();
        project.includes.back()
          .append(project_path)
          .append(include->c_str());
      }

      if (const std::string* precompile_header = reader.precompile_header())
      {
        estd::path& path = project.precompile_header.path;
        path.append(project_path).append(precompile_header->c_str());

        if (!path.exists())
        {
          estd::throw_error<std::invalid_argument>("Cound't find a precompile header [{}].", path);
        }

        if (path.is_directory())
        {
          estd::throw_error<std::invalid_argument>("Expected precompile header [{}] to be a file not a directory.", path);
        }
      }

      const std::string* artifact_type = reader.artifact_type();
      const std::string* static_library = reader.static_library();
      const std::string* import_library = reader.import_library();
      const std::string* dynamic_library = reader.dynamic_library();
      const std::string* executable = reader.executable();

      const bool requests_artifact_creation = artifact_type;
      const bool provides_preproduced_artifact = static_library || import_library || dynamic_library || executable;

      const uint32_t artifact_requests = requests_artifact_creation + provides_preproduced_artifact;
      estd::assert_condition(artifact_requests != 2, "Project must either provide preproduced artifact or request type to generate and not both.");

      estd::log("Artifact status: [{}], {}, {}.", project.name, requests_artifact_creation, provides_preproduced_artifact);

      if (provides_preproduced_artifact)
      {
        artifact_description& artifact = project.artifact;
        artifact.preproduced = true;

        const bool preproduced_static_library = static_library;
        const bool preproduced_dynamic_library = dynamic_library;
        const bool preproduced_executable = executable;

        const uint32_t preproduced_artifacts = preproduced_static_library + preproduced_dynamic_library + preproduced_executable;
        estd::assert_condition(preproduced_artifacts == 1, "Project can not provide multiple preproduced artifact types.");

        if (preproduced_static_library)
        {
          artifact.type = artifact_types::static_library;
          artifact.static_library().append(project_path);
          artifact.static_library().append(*static_library);
        }
        else if (preproduced_dynamic_library)
        {
          artifact.type = artifact_types::dynamic_library;

          if (import_library)
          {
            artifact.import_library().append(project_path);
            artifact.import_library().append(*import_library);
          }

          artifact.dynamic_library().append(project_path);
          artifact.dynamic_library().append(*dynamic_library);
        }
        else
        {
          artifact.type = artifact_types::excutable;
          artifact.executable().append(project_path);
          artifact.executable().append(*executable);
        }
      }
      else
      {
        artifact_description& artifact = project.artifact;
        artifact.preproduced = false;

        if (artifact_type)
        {
          const std::string type_name = *artifact_type;

          if (type_name == "Executable") artifact.type = artifact_types::excutable;
          else if (type_name == "DynamicLibrary") artifact.type = artifact_types::dynamic_library;
          else if (type_name == "StaticLibary") artifact.type = artifact_types::static_library;
          else artifact.type = artifact_types::static_library;
        }
        else
        {
          artifact.type = artifact_types::static_library;
        }

        if (const std::string* resources = reader.resources())
        {
          artifact.resources.append(project_path);
          artifact.resources.append(*resources);
        }
      }

      while (reader.has_next_dependency())
      {
        const std::string* dependency = reader.next_dependency();
        dependencies.push_back(dependency);
      }
    }
  }

  template <typename destination_type>
  inline void append_all_files(const estd::path& path, const char* extension, destination_type& destination) const
  {
    // TODO: This does way to many allocations, need to explore custom wrappers, or accept it as part of file manipulation life.
    if (!path.exists()) estd::throw_error<std::invalid_argument>("Cound't find a source [{}].", path);

    if (path.is_directory())
    {
      for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(path.c_str()))
      {
        const std::filesystem::path& path = entry.path();
        if (path.extension().compare(extension)) continue;
        destination.emplace_back(path.string().c_str());
      }
    }
    else
    {
      destination.push_back(path);
    }
  }
protected:
  const platform& active_platform;
  const target& active_target;

  std::vector<option_parser> option_parsers;
};
