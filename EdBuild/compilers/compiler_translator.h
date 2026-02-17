#pragma once

#include "configurations/project.h"

template <typename tested_type>
concept compiler_translator = requires(tested_type object, command_string& command) {
  { object.append_option(option_description(), command) } -> std::same_as<void>;
  { object.append_define(define_description(), command) } -> std::same_as<void>;
  { object.append_include(estd::path(), command) } -> std::same_as<void>;
  { object.append_precompile_header(compilable_view(), command) } -> std::same_as<void>;
  { object.compute_dependecies_list_update_command(compilable_view()) } -> std::same_as<command_string>;
  { object.compute_compilation_command(compilable_view()) } -> std::same_as<command_string>;
  { object.compute_database_entry_command(compilable_view()) } -> std::same_as<command_string>;
  { object.parse_update_time(estd::stack_string_512()) } -> std::same_as<std::filesystem::file_time_type>;
  { object.create_parser() } -> std::same_as<command_output_parser_ptr>;
};

struct compilation_result
{
  std::string file;
  std::string message;

  std::string include_trail;
  std::string auxiliary;

  uint32_t line;
  uint32_t column;

  error_types type;

  inline operator bool() const
  {
    return message.size();
  }
};

using compilation_results_list = std::vector<compilation_result>;

class compiler_output_parser : public command_output_parser
{
public:
  virtual void set_output_store(compilation_results_list& list) = 0;
  virtual void set_threads_count(uint32_t threads_count) = 0;
  virtual void clean_up() = 0;
};