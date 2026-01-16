#pragma once

#include "compilers/compiler_orchestrator.h"
#include "linkers/linker_orchestrator.h"
#include "builder_cache.h"

class builder
{
public:
  struct configuration
  {
    configuration(const tools_registry& tools) : tools(tools)
    { }

    const tools_registry& tools;
    bool ignore_builder_updates = false;
    bool generate_compilation_database = true;
  };

  builder(const configuration& config)
    : config(config),
    cache(cli().get_platform(), cli().get_target())
  {
  }

  void build(project_configuration& project)
  {
    compiler_orchestrator compilers(project, config.tools);
    linker_orchestrator linkers(project, config.tools);

    // Data is parsed, setting up and optimizing subproject.
    create_artifacts(compilers, linkers, project);
    organize_dependencies(project);

    // Project is optimized, setting up orchestrators.
    orchestrators_preparation(compilers, linkers);

    // Building preparations.
    setup_directories(project);

    // Compiling.
    compilation_preparations(compilers, project);
    update_dependencies(compilers, project);
    filter(compilers, project);

    auto compilation_output = compile(compilers, project);
    if (compilation_output.size())
    {
      std::sort(compilation_output.begin(), compilation_output.end(),
        [](const compilation_result& left, const compilation_result& right) {
          return static_cast<uint32_t>(left.type) < static_cast<uint32_t>(right.type);
        });

      estd::log("\n{}Compilations results{}:", estd::colors::yellow(), estd::colors::reset());
      for (const auto& output : compilation_output)
      {
        const char* name = get_type_name(output.type);
        const char* color = get_type_color(output.type);

        estd::log("{}{:7}{} [{:4}:{:4}] {:50}: {}",
          color, name, estd::colors::reset(),
          output.line, output.column,
          output.file.c_str(),
          output.message.c_str());
      }
    }

    estd::log("\n{}Starting Linking{}:", estd::colors::yellow(), estd::colors::reset());

    // Linking.
    link(linkers, project);

    cache.update_cache(project);

    // Composition.
    generate_builds(project);

    // Utilities.
    if (config.generate_compilation_database)
    {
      assemble_commands_database(compilers, project);
    }
  }

protected:
  void create_artifacts(compiler_orchestrator& compilers, linker_orchestrator& linkers, const project_configuration& project)
  {
    compilers.create_artifacts();
    linkers.create_artifacts();

    estd::log("{}Artifacts{}:", estd::colors::yellow(), estd::colors::reset());
    for (const auto& subproject : project.subprojects)
    {
      const auto& artifact = subproject.artifact;

      const char* status = artifact.preproduced ? "preproduced" : "generated";
      const char* output = artifact.field2.c_str();
      const char* input = artifact.field1.size() ? artifact.field1.c_str() : "None";
      estd::log("{}{}{}: {}, {}, {}, {}.", estd::colors::green(), subproject.name.c_str(), estd::colors::reset(), get_type_name(artifact.type), status, output, input);
    }
  }

  void organize_dependencies(project_configuration& project)
  {
    estd::log("\n{}Distributing dependencies.{}", estd::colors::yellow(), estd::colors::reset());

    // Set up graph.
    const std::size_t subprojects_count = project.subprojects.size();
    std::vector<std::size_t> supplies(subprojects_count, 0);
    std::queue<std::size_t> distributors;

    for (std::size_t subproject_index{ 0 }; subproject_index < subprojects_count; ++subproject_index)
    {
      auto& subproject = project.subprojects[subproject_index];
      subproject.original_rank = subproject_index;
      subproject.rank = 0;

      const bool is_leaf_project = subproject.dependencies_count == 0;
      if (is_leaf_project) distributors.push(subproject_index);
    }

    // Traverse and distribute needed dependencies.
    while (distributors.size())
    {
      const std::size_t subproject_index = distributors.front();
      distributors.pop();

      const auto& distributor = project.subprojects[subproject_index];
      const auto& distributor_includes = distributor.includes;
      const auto& distributor_dependencies = distributor.artifact_dependencies;

      estd::log("{}Processing distributor{}: {}, {}.", estd::colors::green(), estd::colors::reset(), distributor.name.c_str(), distributor.dependants.size());

      #pragma message("Redundant copy.")
      for (const std::size_t dependant_index : distributor.dependants)
      {
        auto& dependant = project.subprojects[dependant_index];
        auto& dependant_includes = dependant.includes;
        auto& dependant_dependencies = dependant.artifact_dependencies;

        estd::log("{}Processing dependant{}: {}.", estd::colors::green(), estd::colors::reset(), dependant.name.c_str());

        dependant_includes.insert(dependant_includes.end(), distributor_includes.begin(), distributor_includes.end());
        dependant_dependencies.insert(dependant_dependencies.end(), distributor_dependencies.begin(), distributor_dependencies.end());
        dependant_dependencies.push_back(distributor.artifact);

        const std::size_t supplied = ++supplies[dependant_index];
        const bool can_become_distributor = supplied == dependant.dependencies_count;
        if (can_become_distributor)
        {
          dependant.rank = distributor.rank + 1;
          distributors.push(dependant_index);
        }
      }
    }

    // Clean up dependencies.
    for (auto& subproject : project.subprojects)
    {
      auto& includes = subproject.includes;
      std::sort(includes.begin(), includes.end());

      auto includes_it = std::unique(includes.begin(), includes.end());
      includes.erase(includes_it, includes.end());

      auto& dependencies = subproject.artifact_dependencies;
      std::sort(dependencies.begin(), dependencies.end(),
        [](const artifact_description& left, const artifact_description& right)
        {
          if (left.field1 == right.field1) return left.field2 < right.field2;
          return left.field1 < right.field1;
        });

      auto dependencies_it = std::unique(dependencies.begin(), dependencies.end(),
        [](const artifact_description& left, const artifact_description& right) {
          return left.field1 == right.field1 && left.field2 == right.field2;
        });

      dependencies.erase(dependencies_it, dependencies.end());
    }

    // Reorder to help orchestrators.
    #pragma message("Linking logic optimization possible, pass dependencies and rank the configuration.")
    std::sort(project.subprojects.begin(), project.subprojects.end(),
      [](const subproject_configuration& left, const subproject_configuration& right)
        { return left.rank < right.rank; });

    // Log state.
    estd::log("\n{}Reordered subprojects{}: ", estd::colors::yellow(), estd::colors::reset());
    for (const auto& subproject : project.subprojects)
    {
      estd::log("{}{}{} was {} and became {} with {} dependencies and {} includes.",
        estd::colors::green(),
        subproject.name.c_str(),
        estd::colors::reset(),
        subproject.original_rank, subproject.rank,
        subproject.artifact_dependencies.size(), subproject.includes.size());
    }

    estd::log("\n{}Dependencies{}: ", estd::colors::yellow(), estd::colors::reset());
    for (const auto& subproject : project.subprojects)
    {
      const char* description = subproject.artifact_dependencies.size() ? "" : "None.";
      estd::log("{}{}{}: {}", estd::colors::green(), subproject.name.c_str(), estd::colors::reset(), description);
      for (const auto& artifact : subproject.artifact_dependencies)
      {
        estd::log("[{}] - [{}].", artifact.field1.c_str(), artifact.field2.c_str());
      }
    }
  }

  void setup_directories(const project_configuration& project)
  {
    estd::log("\n{}Intermedite directory setup{}:", estd::colors::yellow(), estd::colors::reset());
    for (const auto& subprojects : project.subprojects)
    {
      if (subprojects.is_preproced()) continue;

      command_string path = cli().get_intermediate_path();
      path.append(subprojects.name);

      if (std::filesystem::exists(path.c_str()))
      {
        estd::log("{}Intermediate directory detected{}: [{}].", estd::colors::green(), estd::colors::reset(), path.c_str());
      }
      else
      {
        std::filesystem::create_directories(path.c_str());
        estd::log("{}Intermediate directory created{}: [{}].", estd::colors::green(), estd::colors::reset(), path.c_str());
      }
    }

    estd::log("\n{}Builds directory setup{}:", estd::colors::yellow(), estd::colors::reset());
    for (const auto& build : project.builds)
    {
#pragma message("Platform dependant code.")
      command_string path = cli().get_builds_path();
      path.append(build.name);
      path.append("\\");

      if (std::filesystem::exists(path.c_str()))
      {
        std::filesystem::remove_all(path.c_str());
        estd::log("{}Build directory cleared up{}: [{}].", estd::colors::green(), estd::colors::reset(), path.c_str());
      }

      std::filesystem::create_directories(path.c_str());
      estd::log("{}Build directory created{}: [{}].", estd::colors::green(), estd::colors::reset(), path.c_str());
    }
  }

  void orchestrators_preparation(compiler_orchestrator& compilers, linker_orchestrator& linkers)
  {
    estd::log("\nPreparing orchestrators.");
    compilers.prepare();
    linkers.prepare();
  }

  void compilation_preparations(compiler_orchestrator& orchestrator, project_configuration& project)
  {
    compilables_count = 0;
    for (const auto& subrpoject : project.subprojects)
    {
      compilables_count += subrpoject.get_compilables_count();
    }

    estd::log("\n{}Compilables detected{}: {}.\n", estd::colors::yellow(), estd::colors::reset(), compilables_count);
  }
  
  void update_dependencies(compiler_orchestrator& orchestrator, project_configuration& project)
  {
    commands_list dependencies_list_commands = orchestrator.generate_dependencies_update_commands();
    estd::log("\n{}Dependency list commands count{}: {}.\n", estd::colors::yellow(), estd::colors::reset(), dependencies_list_commands.size());

    estd::async_shell_execute<32>(dependencies_list_commands, cli().get_threads_count());
  }

  void filter(compiler_orchestrator& orchestrator, project_configuration& project)
  {
    estd::log("{}Performing per compiler filtering.{}\n", estd::colors::yellow(), estd::colors::reset());

    std::string builder_path = estd::fetch_executable_path();
    const auto builder_update_time = std::filesystem::last_write_time(builder_path);
    const bool builder_was_updated = cache.is_build_outdated(builder_update_time);

    if (builder_was_updated)
    {
      project.status.set_builder_was_updated();
    }
    else if (cache.is_hash_outdated(project))
    {
      project.status.set_project_hash_mismatch();
    }
    else
    {
      for (auto& subproject : project.subprojects)
      {
        if (cache.is_hash_outdated(subproject))
        {
          subproject.status.set_subproject_hash_mismatch();
        }
      }
    }

    estd::log("");
    orchestrator.perform_compilation_filtering();
    estd::log("");
  }

  compilation_results_list compile(compiler_orchestrator& orchestrator, project_configuration& project)
  {
    commands_partitions partitions = orchestrator.generate_compilation_commands();

    estd::log("\n{}Compilation partitions count{}: {}.", estd::colors::yellow(), estd::colors::reset(), partitions.size());
    for (std::size_t partition_index{ 0 }; partition_index < partitions.size(); ++partition_index)
    {
      const auto& partition = partitions[partition_index];
      estd::log("{}Compilation partition{}: {} - {}.", estd::colors::green(), estd::colors::reset(), partition_index, partition.get_commands_count());
    }

    estd::log("");

    compilation_results_list results;

    for (std::size_t partition_index{ 0 }; partition_index < partitions.size(); ++partition_index)
    {
      const auto& partition = partitions[partition_index];

      compiler_output_parser& parser = *std::static_pointer_cast<compiler_output_parser>(partition.parser);
      parser.set_output_store(results);
      parser.set_threads_count(cli().get_threads_count());
      parser.set_execution_policy(execution_policy::stop_on_error);

      estd::log("{}Partition {}{}:", estd::colors::yellow(), estd::colors::reset(), partition_index);
      estd::async_shell_execute<32>(partition.commands, parser, cli().get_threads_count());

      parser.clean_up();
    }

    return results;
  }

  void assemble_commands_database(compiler_orchestrator& orchestrator, project_configuration& project)
  {
    commands_list database_entry_commands = orchestrator.generate_database_entry_commands();
    estd::log("Database entries commands count: {}.\n");

    estd::async_shell_execute<32>(database_entry_commands, cli().get_threads_count());

    std::string database;

    constexpr std::size_t max_expected_entry_size = 2048;
    database.reserve(compilables_count * max_expected_entry_size + 2);

    database.append("[\n");

    for (compilable_view view : project.get_compilables())
    {
      command_string database_entry_path = get_output_path(view);
      database_entry_path.append(".dbe");

      //estd::log("ENTRY: {}.", database_entry_path.c_str());

      if (std::filesystem::exists(database_entry_path.c_str()))
      {
        append_file_data(database_entry_path, database);
        database.push_back('\n');
      }
    }

    database.push_back(']');

    command_string database_path = cli().get_intermediate_path();
    database_path.append("database.json");
    dump_to_file(database_path, database);
  }

  void link(linker_orchestrator& linkers, project_configuration& project)
  {
    commands_partitions partitions = linkers.generate_linking_commands();

    estd::log("\n{}Linking partitions count{}: {}.", estd::colors::yellow(), estd::colors::reset(), partitions.size());
    for (std::size_t partition_index{ 0 }; partition_index < partitions.size(); ++partition_index)
    {
      const auto& partition = partitions[partition_index];
      estd::log("{}Linking partition{}: {} - {}.", estd::colors::green(), estd::colors::reset(), partition_index, partition.get_commands_count());
    }

    estd::log("");

    for (std::size_t partition_index{ 0 }; partition_index < partitions.size(); ++partition_index)
    {
      const auto& partition = partitions[partition_index];
      estd::log("{}Partition{} {}:", estd::colors::yellow(), estd::colors::reset(), partition_index);
      estd::async_shell_execute<32>(partition.commands, cli().get_threads_count());
    }
  }

  void generate_builds(const project_configuration& project)
  {
    for (const auto& build : project.builds)
    {
#pragma message("Platform dependant code.")
      estd::stack_string_512 build_path_string = cli().get_builds_path();
      build_path_string.append(build.name);
      build_path_string.append("\\");

      estd::log("Generating [{}] build.", build.name.c_str());

      std::filesystem::path build_path = build_path_string.c_str();

      auto name_predicate = [&search_name = build.subproject_name](const subproject_configuration& subproject)
        { return subproject.name == search_name; };

      const auto& subprojects = project.subprojects;
      auto subproject_it = std::find_if(subprojects.begin(), subprojects.end(), name_predicate);

      estd::assert_condition(subproject_it != subprojects.end(),
        "Failed to find subproject [{}] for build [{}] creation.",
        build.subproject_name.c_str(), build.name.c_str());

      const auto& subproject = *subproject_it;

      for (const auto& artifact : subproject.artifact_dependencies)
      {
        const bool needs_moving = artifact.type == artifact_types::dynamic_library;
        if (needs_moving)
        {
          std::filesystem::path artifact_path = artifact.dynamic_library();
          std::filesystem::copy(artifact_path, build_path);

          const bool needs_symbols = artifact.symbols_database().size();
          if (needs_symbols)
          {
            std::filesystem::path artifact_path = artifact.symbols_database();
            std::filesystem::copy(artifact_path, build_path);
          }
        }
      }

      const bool needs_moving = subproject.artifact.type != artifact_types::static_library;
      if (needs_moving)
      {
        const auto& artifact = subproject.artifact;

        std::filesystem::path artifact_path = artifact.output();
        std::filesystem::copy(artifact_path, build_path);

        const bool needs_symbols = artifact.symbols_database().size();
        if (needs_symbols)
        {
          std::filesystem::path artifact_path = artifact.symbols_database();
          std::filesystem::copy(artifact_path, build_path);
        }
      }
    }
  }

protected:
  const configuration& config;
  builder_cache cache;

  std::string build_entry;

  std::size_t compilables_count;

  bool instructions_were_updated;
};
