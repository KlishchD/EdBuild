#pragma once

namespace instructions
{
  struct description
  {
    modifiers_configuration modifiers;
    project_configuration project;
    builds_configurations builds;
  };

  using option_parser = builder_options(*)(const std::string& option_name);
  using option_parsers_list = std::vector<option_parser>;


  struct parser_inputs
  {
    estd::json source;
    estd::path project_path;
    const platform* active_platform;
    const target* active_target;
    option_parsers_list option_parsers;
  };

  description parse(const parser_inputs& inputs);
}