#pragma once

#include "linker_translator.h"

class linker_driver
{
public:
  virtual void create_artifacts() = 0;
  virtual void prepare() = 0;

  virtual void generate_linking_commands(commands_paritions& lists) = 0;
  virtual ~linker_driver() = default;
};

template <linker_translator translator_type>
class direct_linking_driver : public linker_driver
{
public:
  direct_linking_driver(project_configuration& project) : project(project), translator()
  {
    owned_subprojects.reserve(project.subprojects.size());
    for (std::size_t subproject_index{ 0 }; subproject_index < project.subprojects.size(); ++subproject_index)
    {
      owned_subprojects.push_back(subproject_index);
    }
  }

  virtual void create_artifacts() override
  {
    for (std::size_t subproject_index : owned_subprojects)
    {
      auto& subproject = project.subprojects[subproject_index];
      if (subproject.is_preproced()) continue;

      auto& artifact = subproject.artifact;
      switch (artifact.type)
      {
      case artifact_types::static_library:
      {
        artifact.static_library().append(g_cli_parameters.get_intermediate_path());
        artifact.static_library().append(subproject.name);
        artifact.static_library().append("\\");
        artifact.static_library().append(subproject.name);
        artifact.static_library().append(active_platform()->get_static_library_extension());
        break;
      }
      case artifact_types::dynamic_library:
      {
        artifact.import_library().append(g_cli_parameters.get_intermediate_path());
        artifact.import_library().append(subproject.name);
        artifact.static_library().append("\\");
        artifact.import_library().append(subproject.name);
        artifact.import_library().append(active_platform()->get_static_library_extension());

        artifact.dynamic_library().append(g_cli_parameters.get_intermediate_path());
        artifact.dynamic_library().append(subproject.name);
        artifact.static_library().append("\\");
        artifact.dynamic_library().append(subproject.name);
        artifact.dynamic_library().append(active_platform()->get_dynamic_library_extension());

        break;
      }
      case artifact_types::excutable:
      {
        artifact.executable().append(g_cli_parameters.get_intermediate_path());
        artifact.executable().append(subproject.name);
        artifact.static_library().append("\\");
        artifact.executable().append(subproject.name);
        artifact.executable().append(active_platform()->get_exectuable_extension());
        break;
      }
      default:
        estd::no_default("Linker driver failed to create artefact for subproject: [{}].", subproject.name.c_str());
      }
    }
  }

  virtual void prepare() override
  {
    // Intentionally left empty.
  }

  virtual void generate_linking_commands(commands_paritions& lists) override
  {
#pragma message("Could improve performance by grouping static libraries in one partition.")
    for (std::size_t subproject_index : owned_subprojects)
    {
      artifact_view view = project.get_artifact_view(subproject_index);

      if (view.description->preproduced) continue;

      estd::log("{}Linking entry{}: [{}].", estd::colors::green(), estd::colors::reset(), view.subproject_name);

      command_string command;
      translator.compute_linking_command(view, command);
      lists.push_back({ command });
    }
  }
protected:
  project_configuration& project;
  ownership_list owned_subprojects;
  translator_type translator;
};