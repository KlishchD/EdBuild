#pragma once

#include "configurations/project.h"

template <typename tested_object>
concept linker_translator = requires(tested_object object, command_string& command) {
  { object.append_option(option_description(), command) } -> std::same_as<void>;
  { object.compute_linking_command(artifact_view(), command) } -> std::same_as<void>;
  { object.create_parser() } -> std::same_as<command_output_parser_ptr>;
};