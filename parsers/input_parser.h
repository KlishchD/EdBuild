#pragma once

#include "readers/input_reader.h"
#include "project.h"

class compiler_input_parser
{
  using dependencies_list = std::vector<const std::string*>;
public:
  using option_parser = compiler_options(*)(const std::string& option_name);

  void register_option_parser(option_parser parser)
  {
    option_parsers.push_back(parser);
  }

  project_configuration parse(input_reader& reader) const
  {
    estd::log("Project parsing starts.");

    std::vector<dependencies_list> dependencies_lists;
    dependencies_list temporary_dependencies;

    project_configuration project;
    parse_project(reader, project, temporary_dependencies);
    reader.next_subproject();

    // TODO: Can reserve here.
    while (reader.is_subproject_valid())
    {
      subproject_configuration subproject;
      parse_project(reader, subproject, temporary_dependencies);

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

      build.name = *name;
      build.subproject_name = *subproject;

      project.builds.push_back(std::move(build));
    }

    const std::size_t subprojects_count = dependencies_lists.size();

    std::map<std::string, std::size_t> name_mapping;
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

        const std::size_t dependency_index = name_mapping[*dependency_name];
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
    compiler_options type = static_cast<compiler_options>(-1);
    for (const auto& parser : option_parsers)
    {
      compiler_options result = parser(*input.name);
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
  inline void parse_project(input_reader& reader, project_type& project, dependencies_list& dependencies) const
  {
    project.name = *reader.project_name();

    while (reader.has_next_option())
    {
      std::optional<input_reader::option_data> option = reader.next_option();
      if (option.has_value()) project.options.push_back(std::move(parse_option(option.value())));
    }

    while (reader.has_next_define())
    {
      std::optional<input_reader::define_data> define = reader.next_define();
      if (define.has_value()) project.defines.push_back(std::move(parse_define(define.value())));
    }

    if constexpr (std::is_same_v<project_type, subproject_configuration>)
    {
      while (reader.has_next_source())
      {
        const std::string* source = reader.next_source();
        if (!source) continue;

        estd::stack_string_1024 path;
        path.append(g_cli_parameters.get_project_path());
        path.append(source->c_str());

        if (std::filesystem::is_directory(path.c_str()))
        {
          project.includes.push_back(path.c_str());
        }

        append_all_files(path, ".cpp", project.sources);
      }

      while (reader.has_next_include())
      {
        const std::string* include = reader.next_include();
        if (!include) continue;

        estd::stack_string_1024 path;
        path.append(g_cli_parameters.get_project_path());
        path.append(include->c_str());

        project.includes.push_back(path.c_str());
      }

      if (const std::string* precompile_header = reader.precompile_header())
      {
        estd::stack_string_1024 path;
        path.append(g_cli_parameters.get_project_path());
        path.append(precompile_header->c_str());

        const bool path_is_not_present = !std::filesystem::exists(path.c_str());
        if (path_is_not_present) estd::throw_error<std::invalid_argument>("Cound't find a precompile header [{}].", path.c_str());

        const bool is_directory = std::filesystem::is_directory(path.c_str());
        if (is_directory) estd::throw_error<std::invalid_argument>("Expected precompile header [{}] to be a file not a directory.", path.c_str());

        project.precompile_header = path;
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
          artifact.static_library().append(g_cli_parameters.get_project_path());
          artifact.static_library().append(*static_library);
        }
        else if (preproduced_dynamic_library)
        {
          artifact.type = artifact_types::dynamic_library;

          if (import_library)
          {
            artifact.import_library().append(g_cli_parameters.get_project_path());
            artifact.import_library().append(*import_library);
          }

          artifact.dynamic_library().append(g_cli_parameters.get_project_path());
          artifact.dynamic_library().append(*dynamic_library);
        }
        else
        {
          artifact.type = artifact_types::excutable;
          artifact.executable().append(g_cli_parameters.get_project_path());
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
          artifact.resources.append(g_cli_parameters.get_project_path());
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
  inline void append_all_files(const estd::stack_string_1024& path, const char* extension, destination_type& destination) const
  {
    // TODO: This does way to many allocations, need to explore custom wrappers, or accept it as part of file manipulation life.
    const bool path_is_not_present = !std::filesystem::exists(path.c_str());
    if (path_is_not_present) estd::throw_error<std::invalid_argument>("Cound't find a source [{}].", path.c_str());

    const bool is_file = !std::filesystem::is_directory(path.c_str());
    if (is_file) { destination.emplace_back(path.c_str()); return; }

    for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(path.c_str()))
    {
      const std::filesystem::path& path = entry.path();
      if (path.extension().compare(extension)) continue;
      destination.push_back(std::move(path.string()));
    }
  }
protected:
  std::vector<option_parser> option_parsers;
};
