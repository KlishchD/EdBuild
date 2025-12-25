#pragma once

class linker_orchestrator
{
public:
  linker_orchestrator(project_configuration& project, const tools_registry& tools) : project(project)
  {
    tools.create_drivers(project, drivers);
  }

  commands_paritions generate_linking_commands()
  {
    commands_paritions partitions;
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