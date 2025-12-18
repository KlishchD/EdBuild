#pragma once

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
    list.append(".pch ");
  }

  command_string compute_dependecies_list_update_command(const compilable_view& view) const
  {
    command_string command = "clang++ -MM ";
    command.append(view.path);
    command.append(" -MF ");
    command.append(get_output_path(view));
    command.append(".deps");
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
    result.append(".dbe ");
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
    list.append(".pch ");
  }

  command_string compute_dependecies_list_update_command(const compilable_view& view) const
  {
#pragma warning "Add exceptions logic as a separate option."
    command_string command = "clang-cl /TP /EHa /showIncludes:user /P ";
    command.append(view.path);
    command.append(" /Fi");
    command.append(get_output_path(view));
    command.append(".i");
    command.append(" 2> ");
    command.append(get_output_path(view));
    command.append(".deps");
    return command;
  }

  command_string compute_compilation_command(const compilable_view& view) const
  {
#pragma warning "Add exceptions logic as a separate option."
    command_string result = "clang-cl /TP /EHa /c ";

    if (!view.is_source)
    {
      result.append(view.path);
      while (result.size() && result.back() != '.')
      {
        result.pop_back();
      }

#pragma warning "This should probably be controllable."
      result.append("cpp");

      result.append(" /Yc");
    }

    result.append(view.path);

#pragma "File format mismatch issue."
    result.append(" /Fo");
    result.append(get_output_path(view));
    result.append(".obj");

    if (!view.is_source)
    {
      result.append(" /Fp");
      result.append(get_output_path(view));
      result.append(view.extension);
    }

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
    estd::throw_error<std::invalid_argument>("Provided disable warnings value is not supported [{}].", option.value);
    return "";
  }
};