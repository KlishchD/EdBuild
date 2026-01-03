#pragma once

#include "linker_translator.h"

class lld_linker_translator
{
public:
  void append_option(const option_description& option, command_string& list)
  {
  }

  void compute_linking_command(const artifact_view& view, command_string& list)
  {
    const artifact_description* artifact = view.description;

    list.append("lld-link ");

#pragma message("Platform dependency.")
    if (artifact->type == artifact_types::static_library)
    {
      list.append("/lib ");
    }
#pragma message("To be moved to options to allow user selection.")
    list.append("/subsystem:CONSOLE ");

#pragma message("Make one varargs function for path composition, it will allow to hide platform dependent code and make it easier to read.")
    for (compilable_view compilable : view.compilables)
    {
      list.append(g_cli_parameters.get_intermediate_path());
      list.append(compilable.subproject_name);
      estd::append_filename(compilable.path, list);
      list.append(active_platform()->get_object_extension());
      list.push_back(' ');
    }

    const bool is_executable = artifact->type == artifact_types::excutable;
    const bool has_resources = artifact->resources.size();
    if (is_executable && has_resources)
    {
      list.append(artifact->resources);
      list.push_back(' ');
    }

    list.append("/out:");
    list.append(artifact->output());

    const bool expects_dependencies = artifact->type != artifact_types::static_library;
    if (expects_dependencies)
    {
      list.push_back(' ');

      for (const auto& dependency_artifact : *view.dependencies)
      {
        list.append(dependency_artifact.input());
        list.push_back(' ');
      }

#pragma message("Platform dependency.")
      list.append("kernel32.lib user32.lib gdi32.lib winspool.lib ");
      list.append("comdlg32.lib advapi32.lib shell32.lib ole32.lib ");
      list.append("oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ");
    }
  }

  command_output_parser_ptr create_parser()
  {
    return nullptr;
  }
};