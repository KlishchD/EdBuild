#pragma once

#include "linker_translator.h"

class linker_driver
{
public:
  virtual void generate_linking_commands(commands_paritions& lists) = 0;
  virtual ~linker_driver() = default;
};

template <linker_translator translator_type>
class direct_linking_driver : public linker_driver
{
public:
  direct_linking_driver(project_configuration& project) : project(project), translator()
  {

  }

  virtual void generate_linking_commands(commands_paritions& lists) override
  {
#pragma message("Could improve performance by grouping static libraries in one partition.")
    for (artifact_view view : project.get_artifacts())
    {
      if (view.preproduced) continue;

      estd::log("Linking entry: [{}].", view.subproject_name);

      command_string command;
      translator.compute_linking_command(view, command);
      lists.push_back({ command });
    }
  }
protected:
  project_configuration& project;
  translator_type translator;
};