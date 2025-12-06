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

    std::size_t source_commands_count = 0;
    std::size_t header_commands_count = 0;
    for (const auto& subproject : project.subprojects)
    {
      if (subproject.is_precompiled()) continue;
      source_commands_count += subproject.sources.size();
      header_commands_count += subproject.has_precompile_header();
    }

    commands_list source_commands, header_commands, database_entry_commands;
    source_commands.reserve(source_commands_count);
    header_commands.reserve(header_commands_count);
    database_entry_commands.reserve(source_commands_count + header_commands_count);

#pragma warning "URVO"
    for (std::size_t subproject_index = 0; subproject_index < project.subprojects.size(); ++subproject_index)
    {
      const auto& subproject = project.subprojects[subproject_index];
      if (subproject.is_precompiled()) continue;

      if (precompile_header_view view = project.get_precompile_header_view(subproject_index))
      {
        if (needs_recompilation(view))
        {
          command_string compilation_command = compute_precompile_header_command(view);
          header_commands.push_back(std::move(compilation_command));

          command_string database_entry_command = compute_database_entry_command(view);
          database_entry_commands.push_back(std::move(database_entry_command));
        }
      }

      const std::size_t sources_count = project.subprojects[subproject_index].sources.size();
      for (std::size_t source_index = 0; source_index < sources_count; ++source_index)
      {
        source_view view = project.get_source_view(subproject_index, source_index);

        if (needs_recompilation(view))
        {
          command_string compilation_command = compute_source_command(view);
          source_commands.push_back(std::move(compilation_command));

          command_string database_entry_command = compute_database_entry_command(view);
          database_entry_commands.push_back(std::move(database_entry_command));
        }
      }
    }

    //estd::async_shell_execute<32>(header_commands, g_cli_parameters.get_threads_count());
    //estd::async_shell_execute<32>(source_commands, g_cli_parameters.get_threads_count());
    //estd::async_shell_execute<32>(database_entry_commands, g_cli_parameters.get_threads_count());

    std::string database;

    constexpr std::size_t max_expected_entry_size = 2048;
    database.reserve((source_commands_count + header_commands_count) * max_expected_entry_size + 2);

    database.append("[\n");

    for (std::size_t subproject_index = 0; subproject_index < project.subprojects.size(); ++subproject_index)
    {
      const auto& subproject = project.subprojects[subproject_index];
      if (subproject.is_precompiled()) continue;

      if (precompile_header_view view = project.get_precompile_header_view(subproject_index))
      {
        command_string database_entry_path = get_output_path(view);
        database_entry_path.append(".dbe");

        if (std::filesystem::exists(database_entry_path.c_str()))
        {
          append_file_data(database_entry_path, database);
          database.push_back('\n');
        }
      }

      const std::size_t sources_count = project.subprojects[subproject_index].sources.size();
      for (std::size_t source_index = 0; source_index < sources_count; ++source_index)
      {
        source_view view = project.get_source_view(subproject_index, source_index);

        command_string database_entry_path = get_output_path(view);
        database_entry_path.append(".dbe");

        //estd::log("ENTRY: {}.", database_entry_path.c_str());

        if (std::filesystem::exists(database_entry_path.c_str()))
        {
          append_file_data(database_entry_path, database);
          database.push_back('\n');
        }
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

  virtual command_string compute_source_command(const source_view& view) = 0;
  virtual command_string compute_database_entry_command(const source_view& view) = 0;
  virtual bool needs_recompilation(const source_view& view) = 0;

  virtual command_string compute_precompile_header_command(const precompile_header_view& view) = 0;
  virtual command_string compute_database_entry_command(const precompile_header_view& view) = 0;
  virtual bool needs_recompilation(const precompile_header_view& view) = 0;

  template <typename view_type>
  inline command_string get_output_path(const view_type& view) const
  {
    command_string result = g_cli_parameters.get_intermediate_path();
    result.append(view.get_subproject_name());
    append_filename(view.get_path(), result);

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
