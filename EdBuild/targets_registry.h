#pragma once

#include "EdBuild.h"
#include "estd/types.h"

class target : public estd::named_marker_type
{
public:
  target(const char* name, char marker)
    : estd::named_marker_type(marker, name)
  { }
};

class targets_registry
{
public:
  targets_registry& create_target(const char* name, char marker)
  {
    targets.emplace_back(name, marker);
    return *this;
  }

  const target* find(char marker)
  {
    for (const target& target : targets)
    {
      if (target.match(marker)) return &target;
    }

    return nullptr;
  }
protected:
  std::vector<target> targets;
};