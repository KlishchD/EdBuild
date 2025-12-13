#pragma once

#include "project.h"

class compiler_interpreter
{
public:
  using command_string = estd::stack_string_2048;
  using commands_list = std::vector<command_string>;

  using compilation_dependency = estd::stack_string_512;
  using compilation_dependenices = std::vector<compilation_dependency>;

  void compile(const project_configuration& project)
  {
    setup(project);

    for (const auto& subprojects : project.subprojects)
    {
      if (subprojects.is_precompiled()) continue;

      command_string path = g_cli_parameters.get_intermediate_path();
      path.append(subprojects.name);

      if (!std::filesystem::exists(path.c_str()))
      {
        estd::log("Created missing directory: {}.", path.c_str());
        std::filesystem::create_directories(path.c_str());
      }
    }

    std::size_t source_commands_count = 0;
    std::size_t header_commands_count = 0;
    for (const auto& subproject : project.subprojects)
    {
      if (subproject.is_precompiled()) continue;
      source_commands_count += subproject.sources.size();
      header_commands_count += subproject.has_precompile_header();
    }

    commands_list source_commands, header_commands;
    source_commands.reserve(source_commands_count);
    header_commands.reserve(header_commands_count);

    commands_list database_entry_commands, dependencies_list_commands;
    database_entry_commands.reserve(source_commands_count + header_commands_count);
    dependencies_list_commands.reserve(source_commands_count + header_commands_count);

    for (compilable_view view : project.get_compilables())
    {
      if (are_dependencies_outdated(view))
      {
        command_string update_command = compute_dependecies_list_update_command(view);
        dependencies_list_commands.push_back(std::move(update_command));
      }
    }

    estd::async_shell_execute<32>(dependencies_list_commands, g_cli_parameters.get_threads_count());

#pragma warning "URVO"
    for (compilable_view view : project.get_compilables())
    {
      if (needs_recompilation(view))
      {
        command_string compilation_command = compute_compilation_command(view);

        auto& compilation_command_destiantion = view.is_source ? source_commands : header_commands;
        compilation_command_destiantion.push_back(compilation_command);

        command_string database_entry_command = compute_database_entry_command(view);
        database_entry_commands.push_back(std::move(database_entry_command));
      }
    }

    estd::async_shell_execute<32>(header_commands, g_cli_parameters.get_threads_count());
    estd::async_shell_execute<32>(source_commands, g_cli_parameters.get_threads_count());
    estd::async_shell_execute<32>(database_entry_commands, g_cli_parameters.get_threads_count());
    
    std::string database;
    
    constexpr std::size_t max_expected_entry_size = 2048;
    database.reserve((source_commands_count + header_commands_count) * max_expected_entry_size + 2);
    
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

    clear();
  }

  virtual ~compiler_interpreter() = default;

protected:
  virtual void setup(const project_configuration& project) = 0;
  virtual void clear() = 0;

  virtual command_string compute_dependecies_list_update_command(const compilable_view& view) const = 0;
  virtual command_string compute_compilation_command(const compilable_view& view) const = 0;
  virtual command_string compute_database_entry_command(const compilable_view& view) const = 0;

  virtual std::filesystem::file_time_type parse_update_time(const estd::stack_string_512& dependency_line) const = 0;

  inline bool are_dependencies_outdated(const compilable_view& view) const
  {
    command_string database_path = get_output_path(view);
    database_path.append(".deps");

    if (!std::filesystem::exists(database_path.c_str())) return true;
    
    auto dependency_update_time = std::filesystem::last_write_time(database_path.c_str());
    auto source_update_time = std::filesystem::last_write_time(view.path);
    return dependency_update_time < source_update_time;
  }

  inline bool needs_recompilation(const compilable_view& view) const
  {
    command_string target_path = get_output_path(view);
    target_path.append(view.extension);
    
    const bool object_file_is_not_present = !std::filesystem::exists(target_path.c_str());
    if (object_file_is_not_present) return true;

    command_string dependencies_list_path = get_output_path(view);
    dependencies_list_path.append(".deps");

    std::ifstream file(dependencies_list_path.c_str(), std::ios_base::in);
    estd::stack_string_512 line;

    const auto compilation_time = std::filesystem::last_write_time(target_path.c_str());
    while (std::getline(file, line))
    {
      std::filesystem::file_time_type update_time = parse_update_time(line);
      if (compilation_time < update_time) return true;
    }

    return false;
  }

  inline command_string get_output_path(const compilable_view& view) const
  {
    command_string result = g_cli_parameters.get_intermediate_path();
    result.append(view.subproject_name);
    append_filename(view.path, result);

    return result;
  }

  void append_file_data(const command_string& filepath, std::string& store)
  {
    std::ifstream file(filepath.c_str(), std::ios_base::in);

    command_string line;
    while (std::getline(file, line))
    {
      store.append(line);
    }
  }

  void dump_to_file(const command_string& filepath, const std::string& data)
  {
    std::ofstream file(filepath.c_str(), std::ios_base::out);
    file << data;
  }
};
