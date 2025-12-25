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
    list.append("lld-link ");

#pragma message("Platform dependency.")
    if (view.type == artifact_types::static_library)
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

    const char* output_extension = nullptr;
    switch (view.type)
    {
    case artifact_types::excutable: output_extension = active_platform()->get_exectuable_extension(); break;
    case artifact_types::static_library: output_extension = active_platform()->get_static_library_extension(); break;
    case artifact_types::dynamic_library: output_extension = active_platform()->get_dynamic_library_extension(); break;
    default:
    }

    list.append("/out:");
    list.append(g_cli_parameters.get_intermediate_path());
    list.append(view.subproject_name);
    list.append("\\");
    list.append(view.subproject_name);
    list.append(output_extension);

    const bool expects_dependencies = view.type != artifact_types::static_library;
    if (expects_dependencies)
    {
      list.push_back(' ');

      for (const auto& dependency_library : *view.dependencies)
      {
        list.append(dependency_library);
        list.push_back(' ');
      }

#pragma message("Platform dependency.")
      list.append("kernel32.lib user32.lib gdi32.lib winspool.lib ");
      list.append("comdlg32.lib advapi32.lib shell32.lib ole32.lib ");
      list.append("oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ");
    }
  }
};