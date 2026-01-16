#pragma once

#include "tools_registry.h"

class linker_orchestrator
{
public:
  linker_orchestrator(project_configuration& project, const tools_registry& tools) : project(project)
  {
    tools.create_drivers(project, drivers);
  }

  void create_artifacts()
  {
    for (auto& driver : drivers)
    {
      driver->create_artifacts();
    }
  }

  void prepare()
  {
    for (auto& driver : drivers)
    {
      driver->prepare();
    }
  }

  commands_partitions generate_linking_commands()
  {
    commands_partitions partitions;
    partitions.reserve(project.subprojects.size());

    for (auto& driver : drivers)
    {
      driver->generate_linking_commands(partitions);
    }

    return partitions;
  }

protected:
  const project_configuration& project;
  drivers_list<linker_driver> drivers;
};