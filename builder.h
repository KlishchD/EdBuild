#pragma once

#include "compilers/compiler_orchestrator.h"

class builder
{
public:
  builder() 
  {
#pragma warning "Platform dependent code."

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

    builder_was_updated = executable_update_time > intermediate_update_time;
  }

  void build(project_configuration& project)
  {
    setup_directories(project);

    compiler_orchestrator orchestrator(project);
    compilation_preparations(orchestrator, project);
    update_dependencies(orchestrator, project);
    filter(orchestrator, project);
    compile(orchestrator, project);
    assemble_commands_database(orchestrator, project);

  }
protected:
  void setup_directories(const project_configuration& project)
  {
    estd::log("Directory setup:");

    for (const auto& subprojects : project.subprojects)
    {
      if (subprojects.is_precompiled()) continue;

      command_string path = g_cli_parameters.get_intermediate_path();
      path.append(subprojects.name);

      if (std::filesystem::exists(path.c_str()))
      {
        estd::log("Directory detected: [{}].", path.c_str());
      }
      else
      {
        std::filesystem::create_directories(path.c_str());
        estd::log("Directory created: [{}].", path.c_str());
      }
    }

    estd::log("");
  }

  void compilation_preparations(compiler_orchestrator& orchestrator, project_configuration& project)
  {
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
    commands_list compilation_commands = orchestrator.generate_compilation_commands();
    estd::log("Compilations commands count: {}.\n", compilation_commands.size());

    estd::async_shell_execute<32>(compilation_commands, g_cli_parameters.get_threads_count());
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

protected:
  bool builder_was_updated;
  std::size_t compilables_count;
};
