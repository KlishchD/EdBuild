#pragma once

#include "compiler_driver.h"

using compiler_drivers_list = std::vector<std::unique_ptr<compiler_driver>>;

#pragma warning "Revisit compilers registry."
struct compilers_registry
{
public:
  using driver_creator = std::function<std::unique_ptr<compiler_driver>(project_configuration&)>;

  template <typename driver_type>
  void register_driver()
  {
    creators.push_back([](project_configuration& project) { return std::make_unique<driver_type>(project); });
  }

  void create_dirvers(project_configuration& project, compiler_drivers_list& drivers)
  {
    for (const auto& creator : creators)
    {
      drivers.push_back(creator(project));
    }
  }
private:
  std::vector<driver_creator> creators;
} g_compilers_registry;

// Orchestrator knows about all the drivers and manages them.
// Also it will be the one to decide paritions for distributed compilation.
class compiler_orchestrator
{
public:
#pragma warning "Add drivers conflicts detection, based on projects interests."
#pragma warning "Is it a good idea to have registry public this way?"
  compiler_orchestrator(project_configuration& project) : compilables_count(0)
  {
    g_compilers_registry.create_dirvers(project, drivers);

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

  commands_list generate_compilation_commands()
  {
    commands_list list;
    list.reserve(compilables_count);

    for (const auto& driver : drivers)
    {
      driver->generate_compilation_commands(list);
    }

    return list;
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
  std::vector<std::unique_ptr<compiler_driver>> drivers;
  std::size_t compilables_count;
};