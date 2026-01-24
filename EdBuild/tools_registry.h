#pragma once

#include "EdBuild.h"
#include "compilers/compiler_driver.h"
#include "linkers/linker_driver.h"

class platform;

template <typename driver_type>
using drivers_list = std::vector<std::unique_ptr<driver_type>>;
using compiler_drivers_list = drivers_list<compiler_driver>;
using linker_drivers_list = drivers_list<linker_driver>;

template <typename T>
concept is_compiler_driver = std::is_base_of_v<compiler_driver, T>;

template <typename driver_type>
using driver_creator = std::function<std::unique_ptr<driver_type>(const platform&, project_configuration&)>;
using compiler_driver_creator = driver_creator<compiler_driver>;
using linker_driver_creator = driver_creator<linker_driver>;

template <typename T>
concept is_linker_driver = std::is_base_of_v<linker_driver, T>;

struct tools_registry
{
public:
  template <typename driver_type>
  void register_driver()
  {
    if constexpr (is_compiler_driver<driver_type>)
    {
      compilers.push_back([](const platform& active_platform, project_configuration& project) { return std::make_unique<driver_type>(active_platform, project); });
    }
    else if constexpr (is_linker_driver<driver_type>)
    {
      linkers.push_back([](const platform& active_platform, project_configuration& project) { return std::make_unique<driver_type>(active_platform, project); });
    }
    else
    {
      static_assert(0, "Driver type is not recognized by tools registry.");
    }
  }

  template <typename driver_type>
  void create_drivers(const platform& active_platform, project_configuration& project, drivers_list<driver_type>& drivers) const
  {
    if constexpr (is_compiler_driver<driver_type>)
    {
      for (const auto& creator : compilers)
      {
        drivers.push_back(creator(active_platform, project));
      }
    }
    else if constexpr (is_linker_driver<driver_type>)
    {
      for (const auto& creator : linkers)
      {
        drivers.push_back(creator(active_platform, project));
      }
    }
    else
    {
      static_assert(0, "Driver type is not recognized by tools registry.");
    }
  }
private:
  std::vector<compiler_driver_creator> compilers;
  std::vector<linker_driver_creator> linkers;
};