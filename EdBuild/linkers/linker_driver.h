#pragma once

#include "linker_translator.h"

class linker_driver
{
public:
  virtual void create_artifacts() = 0;
  virtual void prepare() = 0;

  virtual void generate_linking_commands(commands_partitions& partitions) = 0;
  virtual ~linker_driver() = default;
};

template <linker_translator translator_type>
class direct_linking_driver : public linker_driver
{
public:
  direct_linking_driver(const platform& active_platform, project_configuration& project)
    : active_platform(active_platform), project(project), translator(active_platform)
  {
    owned_subprojects.reserve(project.subprojects.size());
    for (std::size_t subproject_index{ 0 }; subproject_index < project.subprojects.size(); ++subproject_index)
    {
      owned_subprojects.push_back(subproject_index);
    }
  }

  virtual void create_artifacts() override
  {
    bool project_symbols_enabled = false;
    for (const auto& option : project.options)
    {
      if (option.type == builder_options::generate_symbols_database)
      {
        project_symbols_enabled = option.value == "1";
        break;
      }
    }

    for (std::size_t subproject_index : owned_subprojects)
    {
      auto& subproject = project.subprojects[subproject_index];
      if (subproject.is_preproced()) continue;

      auto& artifact = subproject.artifact;
      switch (artifact.type)
      {
      case artifact_types::static_library:
      {
        artifact.static_library().append(subproject.output_path);
        artifact.static_library().append(subproject.name);
        artifact.static_library().append(active_platform.get_static_library_extension());
        break;
      }
      case artifact_types::dynamic_library:
      {
        artifact.import_library().append(subproject.output_path);
        artifact.import_library().append(subproject.name);
        artifact.import_library().append(active_platform.get_static_library_extension());

        artifact.dynamic_library().append(subproject.output_path);
        artifact.dynamic_library().append(subproject.name);
        artifact.dynamic_library().append(active_platform.get_dynamic_library_extension());

        break;
      }
      case artifact_types::excutable:
      {
        artifact.executable().append(subproject.output_path);
        artifact.executable().append(subproject.name);
        artifact.executable().append(active_platform.get_executable_extension());
        break;
      }
      default:
        estd::no_default("Linker driver failed to create artefact for subproject: [{}].", subproject.name.c_str());
      }

      bool symbols_needed = false;

      const bool symbols_supported = subproject.artifact.type != artifact_types::static_library;
      if (symbols_supported)
      {
        symbols_needed = project_symbols_enabled;

        if (!symbols_needed)
        {
          for (const auto& option : subproject.options)
          {
            if (option.type == builder_options::generate_symbols_database)
            {
              symbols_needed = option.value == "1";
              break;
            }
          }
        }
      }

      if (symbols_needed)
      {
        artifact.symbols_database().append(subproject.output_path);
        artifact.symbols_database().append(subproject.name);
        artifact.symbols_database().append(active_platform.get_symbols_database_extension());
      }
    }
  }

  virtual void prepare() override
  {
    // Intentionally left empty.
  }

  virtual void generate_linking_commands(commands_partitions& partitions) override
  {
#pragma message("Could improve performance by grouping static libraries in one partition.")
    for (std::size_t subproject_index : owned_subprojects)
    {
      artifact_view view = project.get_artifact_view(subproject_index);

      if (view.description->preproduced) continue;

      estd::log("{}Linking entry{}: [{}].", estd::colors::green(), estd::colors::reset(), view.subproject_name);

      command_string command;
      translator.compute_linking_command(view, command);

      for (const auto& option : project.options)
      {
        translator.append_option(option, command);
      }

      const auto& subproject = project.subprojects[subproject_index];
      for (const auto& option : subproject.options)
      {
        translator.append_option(option, command);
      }

      command_description result;
      result.value = std::move(command);
      result.alias = "Linking ";
      result.alias.append(view.subproject_name);

      commands_partition partition;
      partition.parser = translator.create_parser();
      partition.commands.push_back(std::move(result));

      partitions.push_back(std::move(partition));
    }
  }
protected:
  const platform& active_platform;

  project_configuration& project;
  ownership_list owned_subprojects;
  translator_type translator;
};