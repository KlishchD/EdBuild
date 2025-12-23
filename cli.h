#pragma once

struct cli_parameter
{
  const char* name;
  const char* value;
};

class cli_parameters
{
public:
  cli_parameters()
  {
  }

  void initialize(int32_t count, const char** parameters)
  {
    estd::assert_condition(count % 2, "Expected even number of parameters.");

    m_parameters.reserve(count / 2);
    for (int32_t i = 1; i < count; i += 2)
    {
      m_parameters.emplace_back(parameters[i], parameters[i + 1]);
    }

    test_parameter<false>(strings::root_path_parameter_name);
    test_parameter<false>(strings::target_parameter_name);
    test_parameter<false>(strings::platform_parameter_name);
    test_parameter<false>(strings::intermediate_parameter_name);
    test_parameter<false>(strings::project_parameter_name);
    test_parameter<true>(strings::thread_parameter_name);
  }

  const char* find(const char* name) const
  {
    for (const cli_parameter& parameter : m_parameters)
    {
      if (strncmp(parameter.name, name, strings::max_parameter_name_length) == 0)
      {
        return parameter.value;
      }
    }

    return {};
  }

  bool contains(const char* name) const
  {
    return find(name) != nullptr;
  }

  inline const char* get_root_path() const
  {
    return find(strings::root_path_parameter_name);
  }

  char get_target() const
  {
    return find(strings::target_parameter_name)[0];
  }

  char get_platform() const
  {
    return find(strings::platform_parameter_name)[0];
  }

  const char* get_intermediate_path() const
  {
    return find(strings::intermediate_parameter_name);
  }

  const char* get_project_path() const
  {
    return find(strings::project_parameter_name);
  }

  uint32_t get_threads_count() const
  {
    return atoi(find(strings::thread_parameter_name));
  }

  bool ignore_builder_update() const
  {
    const char* status = find(strings::ignore_builder_update_name);
    return status ? status[0] == '1' : false;
  }

  void dump_parameters() const
  {
    estd::log("CLI parameters: ");
    for (const cli_parameter& parameter : m_parameters)
    {
      estd::log("{}={}", parameter.name, parameter.value);
    }
  }

  void clean()
  {
    // Need to really deallocate a vector to ensure memory leak check will not be triggered for nothing.
    estd::deallocate_vector(m_parameters);
  }
protected:
  template<bool test_integer = false>
  void test_parameter(const char* name)
  {
    const char* parameter = find(name);
    estd::assert_condition(parameter, "Expected cli parameter: [{}].", name);

    if constexpr (test_integer)
    {
      uint32_t length = strnlen(parameter, strings::max_parameter_length);
      for (uint32_t i = 0; i < length; ++i)
      {
        if (!std::isdigit(parameter[i]))
        {
          estd::throw_error<std::logic_error>("Expected threads count to be a number.");
        }
      }
    }
  }
protected:
  std::vector<cli_parameter> m_parameters;
} g_cli_parameters;

// ---------------------------------------------

// ---------- common types ---------------------

class marked_type
{
public:
  consteval marked_type(char marker) : m_marker(marker)
  {

  }

  inline bool match(char marker) const
  {
    return m_marker == marker;
  }

  inline bool match(const char* markers) const
  {
    const uint32_t length = strlen(markers);
    for (uint32_t i = 0; i < length; ++i)
    {
      if (markers[i] == m_marker)
      {
        return true;
      }
    }

    return false;
  }

  void dump() const
  {
    estd::log("Marker={}", m_marker);
  }
protected:
  char m_marker;
};

class named_enum_type : public marked_type
{
public:
  consteval named_enum_type(char marker, const char* name, int32_t value) : marked_type(marker), m_name(name), m_value(value)
  {
  }

  const char* get_name() const
  {
    return m_name;
  }

  const int32_t get_value() const
  {
    return m_value;
  }

  void dump() const
  {
    marked_type::dump();
    estd::log("Name={}", m_name);
    estd::log("Value={}", m_value);
  }
protected:
  const char* m_name;
  int32_t m_value;
};

template <typename enum_type, typename type_type>
class named_enums_list
{
public:
  named_enums_list()
  {
    m_enums.reserve(5);
  }

  void append(const enum_type* value)
  {
    m_enums.push_back(value);
  }

  bool empty() const
  {
    return m_enums.empty();
  }

  const enum_type* find(char marker) const
  {
    for (const enum_type* value : m_enums)
    {
      if (value->match(marker))
      {
        return value;
      }
    }

    return nullptr;
  }

  void clean()
  {
    estd::deallocate_vector(m_enums);
  }
protected:
  std::vector<const enum_type*> m_enums;
};

class target final : public named_enum_type
{
public:
  enum type
  {
    debug,
    base,
    release
  };

  consteval target(char marker, const char* name, type type) : named_enum_type(marker, name, static_cast<int32_t>(type))
  {
  }
};

using targets_list = named_enums_list<target, target::type>;

inline targets_list& targets()
{
  static targets_list list;

  if (list.empty())
  {
    static constexpr target debug = { 'D', "Debug", target::debug };
    static constexpr target base = { 'B', "Base", target::base };
    static constexpr target release = { 'R', "Release", target::release };

    list.append(&debug);
    list.append(&base);
    list.append(&release);
  }

  return list;
}

inline const target* active_target()
{
  static const target* result = targets().find(g_cli_parameters.get_target());
  estd::assert_condition(result, "Was not able to find target {}.", g_cli_parameters.get_target());
  return result;
}

class platform final : public named_enum_type
{
public:
  enum type
  {
    windows
  };

  consteval platform(char marker, const char* name, type type,
    const char* preprocessing_extension,
    const char* object_extension, const char* precompile_header_extension,
    const char* static_library_extension, const char* dynamic_library_extension, const char* exectuable_extension,
    const char* dependencies_extension, const char* database_extension)
    : named_enum_type(marker, name, static_cast<int32_t>(type)),
    preprocessing_extension(preprocessing_extension),
    object_extension(object_extension),
    precompile_header_extension(precompile_header_extension),
    static_library_extension(static_library_extension),
    dynamic_library_extension(dynamic_library_extension),
    exectuable_extension(exectuable_extension),
    dependencies_extension(dependencies_extension),
    database_extension(database_extension)
  {
  }

  // Some of these extensions are cross platform but I wanted to have them customizable :)
  const char* get_preprocessing_extension() const { return preprocessing_extension; }
  const char* get_object_extension() const { return object_extension; }
  const char* get_precompile_header_extension() const { return precompile_header_extension; }
  const char* get_static_library_extension() const { return static_library_extension; }
  const char* get_dynamic_library_extension() const { return dynamic_library_extension; }
  const char* get_exectuable_extension() const { return exectuable_extension; }
  const char* get_dependencies_extension() const { return dependencies_extension; }
  const char* get_database_extension() const { return database_extension; }
protected:
  const char* preprocessing_extension;
  const char* object_extension;
  const char* precompile_header_extension;
  const char* static_library_extension;
  const char* dynamic_library_extension;
  const char* exectuable_extension;
  const char* dependencies_extension;
  const char* database_extension;
};

using platforms_list = named_enums_list<platform, platform::type>;

inline platforms_list& platforms()
{
  static platforms_list list;

  if (list.empty())
  {
    static constexpr platform windows = {
      'W',
      "Windows",
      platform::windows,
      ".i", ".obj", ".pch",
      ".lib", ".dll", ".exe",
      ".deps", ".dbe"
    };

    list.append(&windows);
  }

  return list;
}

inline const platform* active_platform()
{
  static const platform* result = platforms().find(g_cli_parameters.get_platform());
  estd::assert_condition(result, "Was not able to find platform {}.", g_cli_parameters.get_platform());
  return result;
}