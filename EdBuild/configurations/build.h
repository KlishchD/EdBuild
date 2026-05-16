#pragma once

#include "project.h"

struct build_configuration
{
  name_string name;
  estd::path output_path;

  const subproject_configuration* subproject;
  options_list options;
  defines_list defines;
};

using builds_configurations = std::vector<build_configuration>;