#pragma once

#include "project.h"
#include "build.h"

struct instructions_description
{
  modifiers_configuration modifiers;
  project_configuration project;
  builds_configurations builds;
};