#pragma once

#include "compiler_interpreter.h"

class clang_interpreter final : public compiler_interpreter
{
protected:
  virtual void setup(const project_configuration& project) override
  {
    extend_list(project.options.begin(), project.options.end(), project_suffix);
    extend_list(project.defines.begin(), project.defines.end(), project_suffix);

    const auto& subprojects = project.subprojects;
    subproject_suffixes.reserve(subprojects.size());
    precompile_headers_suffixes.reserve(subprojects.size());

    for (std::size_t subproject_index = 0; subproject_index < project.subprojects.size(); ++subproject_index)
    {
      const auto& subproject = project.subprojects[subproject_index];

      command_string subproject_suffix;
      extend_list(subproject, subproject_suffix);
      subproject_suffixes.push_back(std::move(subproject_suffix));

      command_string precompile_header_suffix;
      extend_list(project.get_precompile_header_view(subproject_index), precompile_header_suffix);
      precompile_headers_suffixes.push_back(std::move(precompile_header_suffix));
    }
  }

  virtual void clear() override
  {
    project_suffix.clear();
    subproject_suffixes.clear();
  }

  virtual command_string compute_source_command(const source_view& view) const override
  {
#pragma warning "Platform specific code"
    command_string result = "clang++ -c ";
    result.append(view.get_path());
    result.append(" -o ");
    result.append(get_output_path(view));
    result.append(".obj");
    result.append(project_suffix);
    result.append(subproject_suffixes[view.subproject_index]);
    result.append(precompile_headers_suffixes[view.subproject_index]);
    return result;
  }

  virtual command_string compute_precompile_header_command(const precompile_header_view& view) const override
  {
#pragma warning "Platform specific code"
    command_string result = "clang++ -c ";
    result.append(view.get_path());
    result.append(" -o ");
    result.append(get_output_path(view));
    result.append(".pch");
    result.append(project_suffix);
    result.append(subproject_suffixes[view.subproject_index]);
    return result;
  }

  virtual command_string compute_database_entry_command(const source_view& view) const override
  {
#pragma warning "Platform specific code"
    command_string output_path = get_output_path(view);

    command_string result = "clang++ -c ";
    result.append(view.get_path());
    result.append(" -o ");
    result.append(get_output_path(view));
    result.append(".obj");
    result.append(project_suffix);
    result.append(subproject_suffixes[view.subproject_index]);
    result.append(" -MJ ");
    result.append(output_path);
    result.append(".dbe");
    return result;
  }

  virtual command_string compute_database_entry_command(const precompile_header_view& view) const override
  {
#pragma warning "Platform specific code"
    command_string output_path = get_output_path(view);

    command_string result = "clang++ -c ";
    result.append(view.get_path());
    result.append(" -o ");
    result.append(get_output_path(view));
    result.append(".pch");
    result.append(project_suffix);
    result.append(subproject_suffixes[view.subproject_index]);
    result.append(" -MJ ");
    result.append(output_path);
    result.append(".dbe");

    return result;
  }

  virtual command_string compute_dependecies_list_update_command(const source_view& view) const override
  {
    command_string command = "clang++ -MM ";
    command.append(view.get_path());
    command.append(project_suffix);
    command.append(subproject_suffixes[view.subproject_index]);
    command.append(" -MF ");
    command.append(get_output_path(view));
    command.append(".deps");
    return command;
  }

  virtual command_string compute_dependecies_list_update_command(const precompile_header_view& view) const override
  {
    command_string command = "clang++ -MM ";
    command.append(view.get_path());
    command.append(project_suffix);
    command.append(subproject_suffixes[view.subproject_index]);
    command.append(" -MF ");
    command.append(get_output_path(view));
    command.append(".deps");
    return command;
  }

  virtual std::filesystem::file_time_type parse_update_time(const estd::stack_string_512& dependency_line) const override
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

  void extend_list(const option_description& option, command_string& list) const
  {
    command_string local_result = convert_option(option);
    if (local_result.empty()) return;

    list.push_back(' ');
    list.append(local_result);
  }

  void extend_list(const define_description& define, command_string& list) const
  {
    command_string local_result = convert_define(define);
    if (local_result.empty()) return;

    list.push_back(' ');
    list.append(local_result);
  }

  void extend_list(const std::string& include, command_string& list) const
  {
    if (include.empty()) return;

    list.append(" -I ");
    list.append(include);
  }

  void extend_list(const precompile_header_view& view, command_string& list) const
  {
    if (!view) return;

    list.append(" -include-pch ");
    list.append(get_output_path(view));
    list.append(".pch ");
  }

  template <typename iterator_type>
  void extend_list(iterator_type begin, iterator_type end, command_string& list) const
  {
    for (; begin != end; ++begin)
    {
      extend_list(*begin, list);
    }
  }

  void extend_list(const subproject_configuration& subproject, command_string& list) const
  {
    extend_list(subproject.options.begin(), subproject.options.end(), list);
    extend_list(subproject.defines.begin(), subproject.defines.end(), list);
    extend_list(subproject.includes.begin(), subproject.includes.end(), list);
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
    estd::throw_error<std::invalid_argument>("Provided disable warnings value is not supported [{}].", option.value);
    return "";
  }

  inline command_string convert_define(const define_description& define) const
  {
    command_string result;
    result.append("-D").append(define.key);
    if (define.value.size()) result.append("=").append(define.value);
    return result;
  }
private:
  command_string project_suffix;
  commands_list subproject_suffixes;
  commands_list precompile_headers_suffixes;
};