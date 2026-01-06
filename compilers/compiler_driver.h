#pragma once

#include "compiler_driver_cache.h"

// Driver knows which projects it can work on and performs operations only on them.
class compiler_driver
{
public:
  virtual void create_artifacts() = 0;
  virtual void prepare() = 0;

  virtual void generate_dependencies_update_commands(commands_list& list) const = 0;
  virtual void perform_compilation_filtering() = 0;

  virtual void generate_compilation_commands(commands_partitions& lists) const = 0;
  virtual void generate_database_entry_commands(commands_list& list) const = 0;

  virtual ~compiler_driver() = default;
};

template <compiler_translator translator_type>
class caching_compiler_driver : public compiler_driver
{
public:
  caching_compiler_driver(project_configuration& project) : project(project), translator(), cache(project, translator)
  {
    owned_subprojects.reserve(project.subprojects.size());
    for (std::size_t subproject_index{ 0 }; subproject_index < project.subprojects.size(); ++subproject_index)
    {
      owned_subprojects.push_back(subproject_index);
    }
  }

  virtual void create_artifacts() override
  {
    // Intentionally ignoring it.
  }

  virtual void prepare() override
  {
    cache.build();
  }

  virtual void generate_dependencies_update_commands(commands_list& list) const override 
  {
    for (std::size_t subproject_index : owned_subprojects)
    {
      for (compilable_view view : project.get_compilables(subproject_index))
      {
        if (!view.is_source) continue;

        estd::log("{}Dependency entry{}: [{}] [{}].", estd::colors::green(), estd::colors::reset(), view.subproject_name, view.path);

        command_string list_path = get_output_path(view);
        list_path.append(".deps");

#pragma message("List could be updated by updating dependencies themselves, need to handle this as well.")
        const bool list_exists = std::filesystem::exists(list_path.c_str());
        if (list_exists)
        {
          const auto list_update_time = std::filesystem::last_write_time(list_path.c_str());
          const auto source_update_time = std::filesystem::last_write_time(view.path);
          const bool is_up_to_date = list_update_time > source_update_time;
          if (is_up_to_date) continue;
        }

        command_string command = translator.compute_dependecies_list_update_command(view);
        cache.append_sufixes(view.subproject_index, false, command);

        command_description result;
        result.value = std::move(command);
        result.alias = "Generating dependencies ";
        result.alias.append(view.path);

        list.push_back(std::move(result));
      }
    }
  }
  
  virtual void perform_compilation_filtering() override
  {
    for (std::size_t subproject_index : owned_subprojects)
    {
      const auto& subproject = project.subprojects[subproject_index];

      for (compilable_view view : project.get_compilables(subproject_index))
      {
        if (project.status.is_filtered())
        {
          (*view.status) = project.status;
        }
        else if (subproject.status.is_filtered())
        {
          (*view.status) = subproject.status;
        }
        else
        {
          do
          {
            command_string target_path = get_output_path(view);
            target_path.append(view.extension);

            const bool object_file_is_not_present = !std::filesystem::exists(target_path.c_str());
            if (object_file_is_not_present) { view.status->set_object_files_is_not_present(); break; }

            const auto compilation_time = std::filesystem::last_write_time(target_path.c_str());
            const auto compilable_update_time = std::filesystem::last_write_time(view.path);
            const bool compilable_was_updated = compilable_update_time > compilation_time;
            if (compilable_was_updated) { view.status->set_compilable_was_updated(); break; }

            command_string dependencies_list_path = get_output_path(view);
            dependencies_list_path.append(".deps");

            std::ifstream file(dependencies_list_path.c_str(), std::ios_base::in);
            estd::stack_string_512 line;

            bool dependencies_were_not_updated = true;
            while (std::getline(file, line) && dependencies_were_not_updated)
            {
              std::filesystem::file_time_type update_time = translator.parse_update_time(line);
              dependencies_were_not_updated = compilation_time > update_time;
            }

            if (!dependencies_were_not_updated) { view.status->set_dependencies_were_updated(); break; }
          } while (false);
        }

        estd::log("{}Filtering entry{}: [{}], [{}], [{}].", estd::colors::green(), estd::colors::reset(), view.subproject_name, view.path, view.status->get_reason().c_str());
      }
    }
  }
  
  virtual void generate_compilation_commands(commands_partitions& partitions) const override
  {
    auto parser = translator.create_parser();
    partitions[0].parser = parser;
    partitions[1].parser = parser;

    for (std::size_t subproject_index : owned_subprojects)
    {
      for (compilable_view view : project.get_compilables(subproject_index))
      {
        if (view.status->is_filtered())
        {
          estd::log("{}Compilation entry{}: [{}] [{}].", estd::colors::green(), estd::colors::reset(), view.subproject_name, view.path);

          command_string command = translator.compute_compilation_command(view);
          cache.append_sufixes(view.subproject_index, view.is_source, command);

          command_description result;
          result.value = std::move(command);
          result.alias = "Compiling ";
          if (!view.is_source)
          {
            result.alias.append("PCH ");
          }
          result.alias.append(view.path);

          commands_list& commands = partitions[view.is_source].commands;
          commands.push_back(std::move(result));
        }
      }
    }
  }

  virtual void generate_database_entry_commands(commands_list& list) const override
  {
    for (std::size_t subproject_index : owned_subprojects)
    {
      for (compilable_view view : project.get_compilables(subproject_index))
      {
        if (view.status->is_filtered())
        {
          estd::log("Database entry: [{}] [{}].", view.subproject_name, view.path);

          command_string command = translator.compute_database_entry_command(view);
          cache.append_sufixes(view.subproject_index, view.is_source, command);

          command_description result;
          result.value = std::move(command);
          result.alias = "Generating database entry for ";
          result.alias.append(view.path);

          list.push_back(std::move(result));
        }
      }
    }
  }
protected:
  project_configuration& project;
  ownership_list owned_subprojects;

  translator_type translator;
  compiler_translator_cache<translator_type> cache;
};