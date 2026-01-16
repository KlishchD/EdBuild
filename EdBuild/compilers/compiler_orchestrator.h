#pragma once

#include "tools_registry.h"
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

  void prepare()
  {
    for (auto& driver : drivers)
    {
      driver->prepare();
    }
  }

  void create_artifacts()
  {
    for (auto& driver : drivers)
    {
      driver->create_artifacts();
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

  commands_partitions generate_compilation_commands()
  {
    commands_partitions partitions;

    commands_partition precompiler_headers_partition;
    precompiler_headers_partition.commands.reserve(project.subprojects.size());
    partitions.push_back(std::move(precompiler_headers_partition));

    commands_partition sources_partition;
    sources_partition.commands.reserve(compilables_count);
    partitions.push_back(std::move(sources_partition));

#pragma message("Will need to generalize this, when time will come to add distribution.")

    for (const auto& driver : drivers)
    {
      driver->generate_compilation_commands(partitions);
    }

    return partitions;
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