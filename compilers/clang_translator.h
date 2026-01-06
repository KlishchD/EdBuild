#pragma once

#include "compiler_translator.h"

class clang_output_parser : public compiler_output_parser
{
  struct feature
  {
    const char* start = nullptr;
    const char* end = nullptr;
    uint32_t aux = 0;

    inline operator bool() const
    {
      return start && end;
    }
  };

public:
  clang_output_parser()
    : result_mutex(), result(nullptr), thread_result(), policy(execution_policy::stop_on_error), active(true) {
  }

  virtual void set_output_store(compilation_results_list& list) override
  {
    result = &list;
  }

  virtual void set_threads_count(uint32_t threads_count) override
  {
    thread_result.resize(threads_count);
  }

  virtual void parse(const char* line, std::size_t length, const void* cookie) override
  {
    estd::assert_condition(result, "Attempting to parse compilation results without providing output storage.");

    const std::size_t thread_index = reinterpret_cast<std::size_t>(cookie);
    estd::assert_condition(thread_index < thread_result.size(),
      "Using parser with more {} than expected threads {}.",
      thread_index, thread_result.size());

    compilation_result& local_result = thread_result[thread_index];

    feature type_feature = find_type_feature(line, length);
    feature include_feature = find_include_feature(line, length);

    const bool has_error = bool(local_result);
    const bool new_error_detected = bool(type_feature) || bool(include_feature);
    if (new_error_detected && has_error) commit_thread_result(thread_index);

    if (include_feature)
    {
      local_result.include_trail.append(line, length - 1);
      return;
    }

    if (type_feature)
    {
      local_result.type = static_cast<error_types>(type_feature.aux);

      feature path_feature = find_path_feature(line, length, type_feature);
      if (path_feature)
      {
        local_result.file.append(path_feature.start, path_feature.end - path_feature.start);

        feature line_feature = find_line_feature(line, length, path_feature);
        feature comlumn_feature = find_column_feature(line, length, line_feature);

        if (line_feature) local_result.line = estd::stoi(line_feature.start, line_feature.end);
        if (comlumn_feature) local_result.column = estd::stoi(comlumn_feature.start, comlumn_feature.end);
      }

      feature message_feature = find_message_feature(line, length, type_feature);
      local_result.message.append(message_feature.start, message_feature.end - message_feature.start);
      estd::capitalize_inline(local_result.message[0]);

      return;
    }

    local_result.auxiliary.append(line, length);

    //estd::log("Thread {} got line [{:.{}}], {}, [{}].", thread_index, line, length, length, error_was_appened);
  }

  virtual void set_execution_policy(execution_policy new_policy) override
  {
    policy = new_policy;
  }

  virtual bool can_proceed() const override
  {
    return !result || active;
  }

  virtual void clean_up() override
  {
    for (compilation_result& local_result : thread_result)
    {
      if (local_result)
      {
        result->push_back(std::move(local_result));
      }
    }

    active = true;
    result = nullptr;
  }
protected:
  feature find_type_feature(const char* line, std::size_t length) const
  {
    feature result;

    struct pattern
    {
      const char* line;
      uint32_t value;
    };

    pattern patterns[] = {
      { "error:", static_cast<uint32_t>(error_types::error) },
      { "warning:", static_cast<uint32_t>(error_types::warning) },
      { "note:", static_cast<uint32_t>(error_types::note) },
    };

    for (const auto& pattern : patterns)
    {
      if (const char* match = strstr(line, pattern.line))
      {
        result.start = match;
        result.end = match + strlen(pattern.line);
        result.aux = pattern.value;
        break;
      }
    }

    return result;
  }

  feature find_include_feature(const char* line, std::size_t length) const
  {
    feature result;

    if (strstr(line, "In file included from"))
    {
      result.start = line;
      result.end = line + length;
    }

    return result;
  }

  feature find_path_feature(const char* line, std::size_t length, const feature& type_feature) const
  {
    feature result;

    const bool path_omited = line == type_feature.start;
    if (path_omited) return result;

    result.start = line;
    result.end = line;

    while (result.end < type_feature.start)
    {
      const char* possible_end = strstr(result.end, "\\");
      if (!possible_end || possible_end >= type_feature.start) break;

      result.end = possible_end + 1;
    }

    result.end = strstr(result.end, "(");

    return result;
  }

  feature find_line_feature(const char* line, std::size_t length, const feature& path_feature)
  {
    feature result;

    if (path_feature.end[0] == '(')
    {
      result.start = path_feature.end + 1;
      result.end = strstr(path_feature.end, ",");
    }

    return result;
  }

  feature find_column_feature(const char* line, std::size_t length, const feature& line_feature)
  {
    feature result;

    if (line_feature)
    {
      result.start = line_feature.end + 1;
      result.end = strstr(line_feature.end, ")");
    }

    return result;
  }

  feature find_message_feature(const char* line, std::size_t length, const feature& type_feature)
  {
    feature result;

    result.start = type_feature.end + 1;
    result.end = line + length;

    return result;
  }

  void commit_thread_result(std::size_t thread_index)
  {
    compilation_result& local_result = thread_result[thread_index];

    switch (policy)
    {
    case execution_policy::stop_on_error: active &= static_cast<uint32_t>(local_result.type) > static_cast<uint32_t>(error_types::error); break;
    case execution_policy::stop_on_warning: active &= static_cast<uint32_t>(local_result.type) > static_cast<uint32_t>(error_types::warning); break;
    case execution_policy::stop_on_note: active &= static_cast<uint32_t>(local_result.type) > static_cast<uint32_t>(error_types::note); break;
    case execution_policy::stop_on_any: active = false; break;

    case execution_policy::disregard_all:
    default:
      break;
    }

    std::lock_guard _(result_mutex);
    result->push_back(std::move(local_result));
  }

protected:
  std::mutex result_mutex;
  compilation_results_list* result;

  std::vector<compilation_result> thread_result;

  execution_policy policy;
  bool active;
};

class clang_translator
{
public:
  void append_option(const option_description& option, command_string& list) const
  {
    command_string local_result = convert_option(option);
    if (local_result.empty()) return;

    list.push_back(' ');
    list.append(local_result);
  }

  void append_define(const define_description& define, command_string& list) const
  {
    list.append(" -D");
    list.append(define.key);

    if (define.value.size())
    {
      list.append("=");
      list.append(define.value);
    }
  }

  void append_include(const std::string& include, command_string& list) const
  {
    if (include.empty()) return;

    list.append(" -I ");
    list.append(include);
  }

  void append_precompile_header(const compilable_view& view, command_string& list) const
  {
    if (!view || view.is_source) return;

    list.append(" -include-pch ");
    list.append(get_output_path(view));
    list.append(active_platform()->get_precompile_header_extension());
    list.append(" ");
  }

  command_string compute_dependecies_list_update_command(const compilable_view& view) const
  {
    command_string command = "clang++ -MM ";
    command.append(view.path);
    command.append(" -MF ");
    command.append(get_output_path(view));
    command.append(active_platform()->get_dependencies_extension());
    return command;
  }

  command_string compute_compilation_command(const compilable_view& view) const 
  {
    command_string result = "clang++";

    result.append(" -c ");
    result.append(view.path);

    if (!view.is_source)
    {
      result.append(" -Xclang -emit-pch ");
    }

    result.append(" -o ");
    result.append(get_output_path(view));
    result.append(view.extension);

    result.append(" 2>&1");

    return result;
  }

  command_string compute_database_entry_command(const compilable_view& view) const 
  {
    command_string output_path = get_output_path(view);

    command_string result = "clang++ -c ";
    result.append(view.path);
    result.append(" -o ");
    result.append(get_output_path(view));
    result.append(view.extension);
    result.append(" -MJ ");
    result.append(output_path);
    result.append(active_platform()->get_database_extension());
    result.append(" ");
    return result;
  }

  std::filesystem::file_time_type parse_update_time(const estd::stack_string_512& dependency_line) const
  {
    std::size_t space_pre_word = dependency_line.find(' ');
    if (space_pre_word == std::string::npos) return std::filesystem::file_time_type(std::filesystem::file_time_type::duration(0));

    std::size_t word_start = dependency_line.find_first_of("qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM", space_pre_word);
    if (word_start == std::string::npos) return std::filesystem::file_time_type(std::filesystem::file_time_type::duration(0));

    std::size_t word_end = dependency_line.find_first_of(" \n\0", word_start);
    if (word_end == std::string::npos) word_end = dependency_line.size();

    std::string_view path_view(dependency_line.c_str() + word_start, dependency_line.c_str() + word_end);
    return std::filesystem::last_write_time(path_view);
  }

  command_output_parser_ptr create_parser() const
  {
    return std::make_shared<clang_output_parser>();
  }
private:
  inline command_string convert_option(const option_description& option) const
  {
    switch (option.type)
    {
    case compiler_options::language_standard: return convert_language_standard_option(option);
    case compiler_options::waringings_level: return convert_warnings_level_option(option);
    case compiler_options::disable_warnings: return convert_disable_warnings_option(option);
    default:
      estd::throw_error<std::invalid_argument>("Provided option is not supported [{}].", static_cast<uint8_t>(option.type));
      break;
    }

    return "";
  }

  inline command_string convert_language_standard_option(const option_description& option) const
  {
    constexpr const char* supported_standards[][2] = {
        { "11", "-std=c++11" },
        { "17", "-std=c++17" },
        { "20", "-std=c++20" },
        { "23", "-std=c++23" }
    };

    for (const auto& standard : supported_standards)
    {
      if (option.value == standard[0])
      {
        return standard[1];
      }
    }

    estd::throw_error<std::invalid_argument>("Provided C++ standard [{}] is not supported.", option.value);

    return "";
  }

  inline command_string convert_warnings_level_option(const option_description& option) const
  {
    constexpr const char* supported_levels[][2] = {
        { "none", "-w" } ,
        { "default", "" },
    };

    for (const auto& level : supported_levels)
    {
      if (option.value == level[0])
      {
        return level[1];
      }
    }

    estd::throw_error<std::invalid_argument>("Provided warnings level is not supported [{}].", option.value);

    return "";
  }

  inline command_string convert_disable_warnings_option(const option_description& option) const
  {
    if (option.value == "1") return "-w";
    if (option.value == "0") return "";
    estd::throw_error<std::invalid_argument>("Provided disable warnings value is not supported [{}].", option.value);
    return "";
  }
};

class clang_cl_translator
{
public:
  void append_option(const option_description& option, command_string& list) const
  {
    command_string local_result = convert_option(option);
    if (local_result.empty()) return;

    list.push_back(' ');
    list.append(local_result);
  }

  void append_define(const define_description& define, command_string& list) const
  {
    list.append(" /D");
    list.append(define.key);

    if (define.value.size())
    {
      list.append("=");
      list.append(define.value);
    }
  }

  void append_include(const std::string& include, command_string& list) const
  {
    if (include.empty()) return;

    list.append(" /I ");
    list.append(include);
  }

  void append_precompile_header(const compilable_view& view, command_string& list) const
  {
    if (!view || view.is_source) return;

    list.append(" /Yu");
    list.append(view.path);

    list.append(" /Fp");
    list.append(get_output_path(view));
    list.append(active_platform()->get_precompile_header_extension());
    list.append(" ");
  }

  command_string compute_dependecies_list_update_command(const compilable_view& view) const
  {
#pragma message("Add exceptions logic as a separate option.")
    command_string command = "clang-cl /TP /EHa /showIncludes:user /P ";
    command.append(view.path);
    command.append(" /Fi");
    command.append(get_output_path(view));
    command.append(active_platform()->get_preprocessing_extension());
    command.append(" 2> ");
    command.append(get_output_path(view));
    command.append(active_platform()->get_dependencies_extension());
    return command;
  }

  command_string compute_compilation_command(const compilable_view& view) const
  {
#pragma message("Add exceptions logic as a separate option.")
    command_string result = "clang-cl /TP /EHa /c ";

    if (!view.is_source)
    {
      result.append(view.path);
      while (result.size() && result.back() != '.')
      {
        result.pop_back();
      }

#pragma message("This should probably be controllable.")
      result.append("cpp");

      result.append(" /Yc");
    }

    result.append(view.path);

#pragma message("File format mismatch issue.")
    result.append(" /Fo");
    result.append(get_output_path(view));
    result.append(active_platform()->get_object_extension());

    if (!view.is_source)
    {
      result.append(" /Fp");
      result.append(get_output_path(view));
      result.append(view.extension);
    }

    result.append(" 2>&1");

    return result;
  }

  command_string compute_database_entry_command(const compilable_view& view) const
  {
    estd::throw_error<std::logic_error>("Database is generation is not supported for clang-cl.");
    return "";
  }

  std::filesystem::file_time_type parse_update_time(const estd::stack_string_512& dependency_line) const
  {
    std::size_t note_first_part = dependency_line.find(':');
    if (note_first_part == std::string::npos) return std::filesystem::file_time_type(std::filesystem::file_time_type::duration(0));

    std::size_t note_second_part = dependency_line.find(':', note_first_part + 1);
    if (note_second_part == std::string::npos) return std::filesystem::file_time_type(std::filesystem::file_time_type::duration(0));

    std::size_t word_start = dependency_line.find_first_of("qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM", note_second_part);
    if (word_start == std::string::npos) return std::filesystem::file_time_type(std::filesystem::file_time_type::duration(0));

    std::size_t word_end = dependency_line.find_first_of("\n\0", word_start);
    if (word_end == std::string::npos) word_end = dependency_line.size();

    //estd::log("FOUND: [{}]", std::string_view(dependency_line.begin() + word_start, dependency_line.begin() + word_end));

    std::string_view path_view(dependency_line.c_str() + word_start, dependency_line.c_str() + word_end);
    return std::filesystem::last_write_time(path_view);
  }

  command_output_parser_ptr create_parser() const
  {
    return std::make_shared<clang_output_parser>();
  }
private:
  inline command_string convert_option(const option_description& option) const
  {
    switch (option.type)
    {
    case compiler_options::language_standard: return convert_language_standard_option(option);
    case compiler_options::waringings_level: return convert_warnings_level_option(option);
    case compiler_options::disable_warnings: return convert_disable_warnings_option(option);
    default:
      estd::throw_error<std::invalid_argument>("Provided option is not supported [{}].", static_cast<uint8_t>(option.type));
      break;
    }

    return "";
  }

  inline command_string convert_language_standard_option(const option_description& option) const
  {
    constexpr const char* supported_standards[][2] = {
        { "11", "/std:c++11" },
        { "17", "/std:c++17" },
        { "20", "/std:c++20" }
    };

    for (const auto& standard : supported_standards)
    {
      if (option.value == standard[0])
      {
        return standard[1];
      }
    }

    estd::throw_error<std::invalid_argument>("Provided C++ standard [{}] is not supported.", option.value);

    return "";
  }

  inline command_string convert_warnings_level_option(const option_description& option) const
  {
    constexpr const char* supported_levels[][2] = {
        { "none", "/W0" } ,
        { "default", "" },
    };

    for (const auto& level : supported_levels)
    {
      if (option.value == level[0])
      {
        return level[1];
      }
    }

    estd::throw_error<std::invalid_argument>("Provided warnings level is not supported [{}].", option.value);

    return "";
  }

  inline command_string convert_disable_warnings_option(const option_description& option) const
  {
    if (option.value == "1") return "/W0";
    if (option.value == "0") return "";
    estd::throw_error<std::invalid_argument>("Provided disable warnings value is not supported [{}].", option.value);
    return "";
  }
};