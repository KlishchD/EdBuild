#pragma once

#include "compilers/compiler_orchestrator.h"
#include "linkers/linker_orchestrator.h"

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

  builder(const configuration& config) : config(config), builder_was_updated(false)
  {
#pragma message("Platform dependent code.")

    if (!config.ignore_builder_updates) builder_was_updated = check_builder_update();
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
    compile(compilers, project);

    // Utilities.
    if (config.generate_compilation_database)
    {
      assemble_commands_database(compilers, project);
    }

    estd::log("\n\nStarting Linking:");

    // Linking.
    link(linkers, project);


    generate_builds(project);
  }

protected:
  bool check_builder_update()
  {
    constexpr std::size_t buffer_size = MAX_PATH;
    char executable_path[buffer_size];

    std::size_t size = GetModuleFileNameA(nullptr, executable_path, buffer_size);
    estd::assert_condition(size, "Failed to fetch executable path.");

    auto intermediate_update_time = std::filesystem::file_time_type::max();
    auto intermediate_iterator = std::filesystem::recursive_directory_iterator(g_cli_parameters.get_intermediate_path());
    for (const std::filesystem::directory_entry& entry : intermediate_iterator)
    {
      std::filesystem::path extension = entry.path().extension();
      //estd::log("[{}] <-> [{}]", entry.path().string().c_str(), extension.string().c_str());

      if (extension == ".obj" || extension == ".pch")
      {
        //estd::log("ENTERED!!!");
        intermediate_update_time = std::min(intermediate_update_time, entry.last_write_time());
      }
    }

    const auto executable_update_time = std::filesystem::last_write_time(executable_path);
    estd::log("Executable path:          [{}].", executable_path);
    estd::log("Intermediate update time: [{}].", intermediate_update_time);
    estd::log("Executalbe update time:   [{}].", executable_update_time);
    estd::log("");

    return executable_update_time > intermediate_update_time;
  }

  void create_artifacts(compiler_orchestrator& compilers, linker_orchestrator& linkers, const project_configuration& project)
  {
    compilers.create_artifacts();
    linkers.create_artifacts();

    estd::log("Artifacts:");
    for (const auto& subproject : project.subprojects)
    {
      const auto& artifact = subproject.artifact;

      const char* status = artifact.preproduced ? "preproduced" : "generated";
      const char* output = artifact.field2.c_str();
      const char* input = artifact.field1.size() ? artifact.field1.c_str() : "None";
      estd::log("{}: {}, {}, {}, {}.", subproject.name.c_str(), get_type_name(artifact.type), status, output, input);
    }
  }

  void organize_dependencies(project_configuration& project)
  {
    estd::log("\nDistributing dependencies.");

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

      estd::log("Processing distributor: {}, {}.", distributor.name.c_str(), distributor.dependants.size());

      #pragma message("Redundant copy.")
      for (const std::size_t dependant_index : distributor.dependants)
      {
        auto& dependant = project.subprojects[dependant_index];
        auto& dependant_includes = dependant.includes;
        auto& dependant_dependencies = dependant.artifact_dependencies;

        estd::log("Processing dependant: {}.", dependant.name.c_str());

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
    estd::log("\nReordered subprojects: ");
    for (const auto& subproject : project.subprojects)
    {
      estd::log("Subproject [{}] was {} and became {} with {} dependencies and {} includes.",
        subproject.name.c_str(),
        subproject.original_rank, subproject.rank,
        subproject.artifact_dependencies.size(), subproject.includes.size());
    }

    estd::log("\nDependencies: ");
    for (const auto& subproject : project.subprojects)
    {
      estd::log("{}:", subproject.name.c_str());
      for (const auto& artifact : subproject.artifact_dependencies)
      {
        estd::log("[{}] - [{}].", artifact.field1.c_str(), artifact.field2.c_str());
      }
    }
  }

  void setup_directories(const project_configuration& project)
  {
    estd::log("Directory setup:");

    for (const auto& subprojects : project.subprojects)
    {
      if (subprojects.is_preproced()) continue;

      command_string path = g_cli_parameters.get_intermediate_path();
      path.append(subprojects.name);

      if (std::filesystem::exists(path.c_str()))
      {
        estd::log("Intermediate directory detected: [{}].", path.c_str());
      }
      else
      {
        std::filesystem::create_directories(path.c_str());
        estd::log("Intermediate directory created: [{}].", path.c_str());
      }
    }

    for (const auto& build : project.builds)
    {
#pragma message("Platform dependant code.")
      command_string path = g_cli_parameters.get_builds_path();
      path.append(build.name);
      path.append("\\");

      if (std::filesystem::exists(path.c_str()))
      {
        std::filesystem::remove_all(path.c_str());
        estd::log("Build directory cleared up: [{}].", path.c_str());
      }

      std::filesystem::create_directories(path.c_str());
      estd::log("Build directory created: [{}].", path.c_str());
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

    estd::log("Compilables detected: {}.\n", compilables_count);

    if (builder_was_updated)
    {
      estd::log("Builder was updated, setting appropriate filtering status.\n");
      for (compilable_view view : project.get_compilables())
      {
        view.status->set_builder_was_updated();
      }
    }
  }
  
  void update_dependencies(compiler_orchestrator& orchestrator, project_configuration& project)
  {
    commands_list dependencies_list_commands = orchestrator.generate_dependencies_update_commands();
    estd::log("Dependency list commands count: {}.\n", dependencies_list_commands.size());

    estd::async_shell_execute<32>(dependencies_list_commands, g_cli_parameters.get_threads_count());
  }

  void filter(compiler_orchestrator& orchestrator, project_configuration& project)
  {
    if (builder_was_updated)
    {
      estd::log("Ignoring compiler filtering due to builder update which invalidated previous compilations.\n");
    }
    else
    {
      estd::log("Performing per compiler filtering.\n");
      orchestrator.perform_compilation_filtering();
      estd::log("");
    }
  }

  void compile(compiler_orchestrator& orchestrator, project_configuration& project)
  {
    commands_paritions partitions = orchestrator.generate_compilation_commands();

    estd::log("Compilation partitions count: {}.", partitions.size());
    for (std::size_t partition_index{ 0 }; partition_index < partitions.size(); ++partition_index)
    {
      estd::log("Compilation partition {} size: {}.", partition_index, partitions[partition_index].size());
    }

    estd::log("");

    for (std::size_t partition_index{ 0 }; partition_index < partitions.size(); ++partition_index)
    {
      estd::log("Partition {}:", partition_index);
      estd::async_shell_execute<32>(partitions[partition_index], g_cli_parameters.get_threads_count());
    }
  }

  void assemble_commands_database(compiler_orchestrator& orchestrator, project_configuration& project)
  {
    commands_list database_entry_commands = orchestrator.generate_database_entry_commands();
    estd::log("Database entries commands count: {}.\n");

    estd::async_shell_execute<32>(database_entry_commands, g_cli_parameters.get_threads_count());

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

    command_string database_path = g_cli_parameters.get_intermediate_path();
    database_path.append("database.json");
    dump_to_file(database_path, database);
  }

  void link(linker_orchestrator& linkers, project_configuration& project)
  {
    commands_paritions partition = linkers.generate_linking_commands();

    estd::log("Linking partitions count: {}.", partition.size());
    for (std::size_t partition_index{ 0 }; partition_index < partition.size(); ++partition_index)
    {
      const auto& commands = partition[partition_index];
      estd::log("Linking partition {} size: {}.", partition_index, commands.size());
    }

    estd::log("");

    for (std::size_t partition_index{ 0 }; partition_index < partition.size(); ++partition_index)
    {
      estd::log("Partition {}:", partition_index);
      estd::async_shell_execute<32>(partition[partition_index], g_cli_parameters.get_threads_count());
    }
  }

  void generate_builds(const project_configuration& project)
  {
    for (const auto& build : project.builds)
    {
#pragma message("Platform dependant code.")
      estd::stack_string_512 build_path_string = g_cli_parameters.get_builds_path();
      build_path_string.append(build.name);
      build_path_string.append("\\");

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
        }
      }

      const bool needs_moving = subproject.artifact.type != artifact_types::static_library;
      if (needs_moving)
      {
        std::filesystem::path artifact_path = subproject.artifact.output();
        std::filesystem::copy(artifact_path, build_path);
      }
    }
  }

protected:
  const configuration& config;
  bool builder_was_updated;
  std::size_t compilables_count;
};
