#pragma once

#include "project.h"

class linker_interpreter
{
public:
  using command_string = estd::stack_string_4096;

  void link(const project_configuration& project)
  {
    std::vector<command_string> link_commands;
    link_commands.reserve(project.subprojects.size());

    for (const auto& subproject : project.subprojects)
    {
      if (subproject.is_precompiled()) continue;
      command_string command = compute_link_command(subproject);
      link_commands.push_back(std::move(command));
    }

    //estd::async_shell_execute<32>(link_commands, 1); // g_cli_parameters.get_threads_count()
  }
protected:
  virtual command_string compute_link_command(const subproject_configuration& subproject) = 0;
};
