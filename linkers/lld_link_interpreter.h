#pragma once

#include "linker_interpreter.h"

class llvm_linker_interpreter final : public linker_interpreter
{
public:
protected:
  virtual command_string compute_link_command(const subproject_configuration& subproject) override
  {
    if (subproject.artifact_type != artifact_types::static_library) return "";
    return compute_static_library_link_command(subproject);
  }

  command_string compute_static_library_link_command(const subproject_configuration& subproject)
  {
    command_string result = "lld-link ";

#pragma warning "Platform dependent code."
#pragma warning "Make one varargs function for path composition, it will allow to hide platform dependent code and make it easier to read."
    for (const auto& source : subproject.sources)
    {
      result.append(g_cli_parameters.get_intermediate_path());
      result.append(subproject.name);
      estd::append_filename(source, result);
      result.append(".obj ");
    }

    for (const auto& dependency_library : subproject.dependencies)
    {
      result.append(dependency_library);
      result.push_back(' ');
    }

    result.append("/out:");
    result.append(g_cli_parameters.get_intermediate_path());
    result.append(subproject.name);
    result.append("\\");
    result.append(subproject.name);
    result.append(".lib");

    return result;
  }
};