#pragma once

#include "compiler_driver.h"

// Orchestrator knows about all the drivers and manages them.
// Also it will be the one to decide partitions for distributed compilation.
class compiler_orchestrator
{
public:
#pragma message("Add drivers conflicts detection, based on projects interests.")
  compiler_orchestrator(project_configuration& project, const tools_registry& tools) : project(project), compilables_count(0)
  {
    tools.create_drivers(project, drivers);

    for (const auto& subproject : project.subprojects)
    {
      compilables_count += subproject.get_compilables_count();
    }
  }

  commands_list generate_dependencies_update_commands() 
  {
    commands_list list;
    list.reserve(compilables_count);

    for (const auto& driver : drivers)
    {
      driver->generate_dependencies_update_commands(list);
    }

    return list;
  }

  void perform_compilation_filtering()
  {
    for (const auto& driver : drivers)
    {
      driver->perform_compilation_filtering();
    }
  }

  commands_paritions generate_compilation_commands()
  {
    commands_paritions lists(2);

#pragma message("Will need to generalize this, when time will come to add distribution.")
    lists[0].reserve(project.subprojects.size());
    lists[1].reserve(compilables_count);

    for (const auto& driver : drivers)
    {
      driver->generate_compilation_commands(lists);
    }

    return lists;
  }
  
  commands_list generate_database_entry_commands()
  {
    commands_list list;
    list.reserve(compilables_count);

    for (const auto& driver : drivers)
    {
      driver->generate_database_entry_commands(list);
    }

    return list;
  }

protected:
  const project_configuration& project;
  drivers_list<compiler_driver> drivers;
  std::size_t compilables_count;
};