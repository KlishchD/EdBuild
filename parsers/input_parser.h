#pragma once

#include "readers/input_reader.h"
#include "project.h"

class compiler_input_parser
{
public:
  using option_parser = compiler_options(*)(const std::string& option_name);

  void register_option_parser(option_parser parser)
  {
    option_parsers.push_back(parser);
  }

  project_configuration parse(input_reader& reader) const
  {
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

    const std::size_t subprojects_count = dependencies_lists.size();

    std::map<std::string, std::size_t> name_mapping;
    for (std::size_t subproject_index = 0; subproject_index < subprojects_count; ++subproject_index)
    {
      const auto& subproject = project.subprojects[subproject_index];
      name_mapping[subproject.name] = subproject_index;
    }

    std::vector<std::vector<std::size_t>> inverse_dependencies_lists;
    inverse_dependencies_lists.resize(subprojects_count);

    for (std::size_t subproject_index = 0; subproject_index < subprojects_count; ++subproject_index)
    {
      for (const std::string* dependency_name : dependencies_lists[subproject_index])
      {
        estd::log("PP: {}, {}.", project.subprojects[subproject_index].name, *dependency_name);

        const std::size_t dependency_index = name_mapping[*dependency_name];
        inverse_dependencies_lists[dependency_index].push_back(subproject_index);
      }
    }

    struct node_state
    {
      std::size_t rank = 0;
      std::size_t supplied = 0;
    };

    std::vector<node_state> states(subprojects_count);
    std::queue<std::size_t> processing_queue;

    for (std::size_t subproject_index = 0; subproject_index < subprojects_count; ++subproject_index)
    {
      const bool is_leaf_project = dependencies_lists[subproject_index].empty();
      if (is_leaf_project)
      {
        processing_queue.push(subproject_index);
      }
    }

    while (processing_queue.size())
    {
      const std::size_t subproject_index = processing_queue.front();
      processing_queue.pop();

      const auto& subproject = project.subprojects[subproject_index];
      const auto& subproject_includes = subproject.includes;
      const auto& subproject_dependencies = subproject.dependencies;
      //estd::log("Processing: {}, {}.", subproject.name, inverse_dependencies_lists[subproject_index].size());

#pragma warning "Stuff that parser should not know about!"
#pragma warning "Redundant copy."
      estd::stack_string_512 dependency;
      if (subproject.artifact_name.size())
      {
        dependency = subproject.artifact_name.c_str();
      }
      else
      {
        dependency.append(g_cli_parameters.get_intermediate_path());
        dependency.append(subproject.name);
        dependency.push_back('\\');
        dependency.append(subproject.name);
        dependency.append(".lib");
      }

      for (const std::size_t dependant_index : inverse_dependencies_lists[subproject_index])
      {
        auto& dependant_subproject = project.subprojects[dependant_index];

        //estd::log("PP: {}, {}.", subproject.name, dependant_subproject.name);

        auto& dependant_includes = dependant_subproject.includes;
        dependant_includes.insert(dependant_includes.end(), subproject_includes.begin(), subproject_includes.end());

        auto& dependant_dependencies = dependant_subproject.dependencies;
        dependant_dependencies.push_back(dependency.c_str());
        dependant_dependencies.insert(dependant_dependencies.end(), subproject_dependencies.begin(), subproject_dependencies.end());

        auto& depndency_state = states[dependant_index];
        depndency_state.rank = states[subproject_index].rank + 1;
        depndency_state.supplied++;

        const bool supplied_last_dependency = depndency_state.supplied == dependencies_lists[dependant_index].size();
        if (supplied_last_dependency) processing_queue.push(dependant_index);
      }
    }

#pragma warning "Linking logic optimization possible, pass dependencies and rank the configuration."
    std::sort(project.subprojects.begin(), project.subprojects.end(), [&name_mapping, &states](const subproject_configuration& lhs, const subproject_configuration& rhs)
      {
        const std::size_t lhs_ranks_index = name_mapping[lhs.name];
        const std::size_t rhs_ranks_index = name_mapping[rhs.name];
        return states[lhs_ranks_index].rank < states[rhs_ranks_index].rank;
      });

    estd::log("\nOrdering: ");
    for (std::size_t subproject_index = 0; subproject_index < subprojects_count; ++subproject_index)
    {
      const auto& subproject = project.subprojects[subproject_index];
      estd::log("[{}] [{}] Dependencies:", subproject.name.c_str(), states[name_mapping[subproject.name]].rank);
      for (const auto& dependency : subproject.dependencies)
      {
        estd::log("{}.", dependency.c_str());
      }
    }

    //std::exit(2);

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
        path.append(g_cli_parameters.get_root_path());
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
        path.append(g_cli_parameters.get_root_path());
        path.append(include->c_str());

        project.includes.push_back(path.c_str());
      }

      if (const std::string* precompile_header = reader.precompile_header())
      {
        estd::stack_string_1024 path;
        path.append(g_cli_parameters.get_root_path());
        path.append(precompile_header->c_str());

        const bool path_is_not_present = !std::filesystem::exists(path.c_str());
        if (path_is_not_present) estd::throw_error<std::invalid_argument>("Cound't find a precompile header [{}].", path.c_str());

        const bool is_directory = std::filesystem::is_directory(path.c_str());
        if (is_directory) estd::throw_error<std::invalid_argument>("Expected precompile header [{}] to be a file not a directory.", path.c_str());

        project.precompile_header = path;
      }

      if (const std::string* artifact = reader.artifact_type())
      {
        if ((*artifact) == "Executable")
        {
          project.artifact_type = artifact_types::excutable;
        }
        else if ((*artifact) == "StaticLibary")
        {
          project.artifact_type = artifact_types::static_library;
        }
        else if ((*artifact) == "DynamicLibrary")
        {
          project.artifact_type = artifact_types::dynamic_library;
        }
        else
        {
          estd::throw_error<std::invalid_argument>("Failed to parse artifact type [{}].", artifact->c_str());
        }
      }
      else
      {
        project.artifact_type = artifact_types::static_library;
      }

#pragma warning "Clould add parsing of artifact type based on extension."
      if (const std::string* name = reader.artifact_name())
      {
        estd::log("FOUND: [{}]", project.name.c_str());

        estd::stack_string_512 path = g_cli_parameters.get_project_path();
        path.append(*name);

        project.artifact_name = path;
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
