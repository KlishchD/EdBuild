#pragma once

#include "project.h"

template <typename tested_type>
concept compiler_translator = requires(tested_type object, command_string& command) {
  { object.append_option(option_description(), command) } -> std::same_as<void>;
  { object.append_define(define_description(), command) } -> std::same_as<void>;
  { object.append_include(std::string(), command) } -> std::same_as<void>;
  { object.append_precompile_header(compilable_view(), command) } -> std::same_as<void>;
  { object.compute_dependecies_list_update_command(compilable_view()) } -> std::same_as<command_string>;
  { object.compute_compilation_command(compilable_view()) } -> std::same_as<command_string>;
  { object.compute_database_entry_command(compilable_view()) } -> std::same_as<command_string>;
  { object.parse_update_time(estd::stack_string_512()) } -> std::same_as<std::filesystem::file_time_type>;
};