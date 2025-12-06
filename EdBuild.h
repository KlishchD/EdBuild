#pragma once

#include <atomic>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <locale>
#include <memory>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <iostream>
#include <cstdint>
#include <format>
#include <thread>
#include <cstdlib>
#include <print>
#include <cstdio>
#include <tuple>
#include <utility>
#include <map>
#include <set>
#include <queue>
#include <stdio.h>

#include "dependencies\nlohmann_json\single_include\\nlohmann\\json.hpp"

// ---------- Error handling -------------------

namespace estd
{
  template <typename... args_types>
  inline void log(const std::format_string<args_types...>& format, args_types... args);
  inline void log(const char* string);
}

// Small note from me to me) due to exploration of std::format.
// std::format expects format_string which has only consteval constructor.
// Which means function has to receive format_string that will be constructed at compile time
// rather than here during runtime.
template <typename error_type, typename... args_types>
inline void throw_error(const std::format_string<args_types...> format, args_types... args)
{
  throw error_type(std::vformat(format.get(), std::make_format_args(args...)));
}

template <typename object_type, typename function_type, typename... args_types>
inline void try_or_log_on_fail(object_type* object, function_type function, args_types... args)
{
  try
  {
    (object->*function)(args...);
  }
  catch (const std::exception& exception)
  {
    estd::log(exception.what());
    exit(1);
  }
}

template <typename... args_types>
inline void assert_condition(bool condition, const std::format_string<args_types...>& format, args_types... args)
{
  constexpr bool assertions_enabled = true;
  if constexpr (assertions_enabled)
  {
    if (!condition)
    {
      throw_error<std::logic_error>(format, args...);
    }
  }
}

inline const char* fetch_error_message_friendly(int32_t error_code)
{
  constexpr uint32_t message_capacity = 256;
  char* message = new char[message_capacity];
  errno_t error = strerror_s(message, message_capacity, error_code);

  if (error)
  {
    throw_error<std::logic_error>("Failed to fetch error message with code {}.", error);
  }

  return message ? message : "Failed to fetch error message.";
}

// ---------------------------------------------

// ---------- Strings --------------------------

struct strings
{
  static constexpr uint32_t max_parameter_name_length = 256;
  static constexpr uint32_t max_parameter_length = 256;
  static constexpr uint32_t max_path_length = 260;
  static constexpr uint32_t default_local_string_legth = 512;

  static constexpr const char* root_path_parameter_name = "-Root";
  static constexpr const char* target_parameter_name = "-Target";
  static constexpr const char* platform_parameter_name = "-Platform";
  static constexpr const char* intermediate_parameter_name = "-Intermediate";
  static constexpr const char* thread_parameter_name = "-Threads";

  static constexpr const char* instructions_path = "instructions.json";

  static constexpr const char* expected_parameter_format = "Expected cli parameter: [{}].";

  template <uint32_t size = default_local_string_legth>
  static inline char* string()
  {
    return new char[size] { 0 };
  }

  static inline void concat_inline(char* result, uint32_t result_size, const char* str)
  {
    uint32_t str_len = strlen(str);
    assert_condition(str_len < result_size, "Concatenation result exceeds local string size.");
    memcpy(result, str, str_len);
  }

  template <uint32_t size = default_local_string_legth>
  static inline const char* concat(const char* lhs, const char* rhs)
  {
    const uint32_t lhs_size = strlen(lhs);
    const uint32_t rhs_size = strlen(rhs);

    char* result = string<default_local_string_legth>();
    concat_inline(result, default_local_string_legth, lhs);
    concat_inline(result + lhs_size, default_local_string_legth, rhs);
    return result;
  }

  static inline void free(const char* string)
  {
    if (!string)
    {
      throw_error<std::invalid_argument>("Can not deallocate nullptr string.");
    }

    delete[] string;
  }
};

// ---------- Logging --------------------------

// TODO: Try to remove copy cost when passing std::string as argument.

namespace estd
{
  template<std::size_t capacity>
  class stack_string;

  using stack_string_512 = stack_string<512>;
  using stack_string_1024 = stack_string<1024>;
  using stack_string_2048 = stack_string<2048>;
  using stack_string_4096 = stack_string<4096>;
  using stack_string_8192 = stack_string<8192>;
  using stack_string_16384 = stack_string<16384>;
  using stack_string_32768 = stack_string<32768>;

  template <std::size_t capacity = 8192>
    [[nodiscard]] stack_string<capacity> vformat_stack(const std::string_view _Fmt, const std::format_args _Args) {
    assert_condition(capacity >= _Fmt.size() + _Args._Estimate_required_capacity(), "Exceeded capacity of stack format [{}>{}].", _Fmt.size() + _Args._Estimate_required_capacity(), capacity);
    stack_string<capacity> _Str;
    std::vformat_to(std::back_insert_iterator{ _Str }, _Fmt, _Args);
    return _Str;
  }


#pragma "Platform specific code."
  template <typename result_string_type, bool enable_debug_logging = true>
  class shell final
  {
  public:
    template <typename input_string_type>
    result_string_type run(input_string_type command)
    {
      if constexpr (enable_debug_logging)
      {
        estd::log("Executing command: {}.", command.c_str());
      }

      FILE* pipe = _popen(command.c_str(), "r");
      if (!pipe) throw_error<std::runtime_error>("Failed to open shell pipe.");
  
      char buffer[result_string_type::get_static_capacity()] = { 0 };
  
      std::size_t total_length = 0;
      char* start = buffer;
  
      while (true)
      {
        char* result = fgets(start, result_string_type::get_static_capacity() - total_length - 1, pipe);
        if (!result) break;
  
        std::size_t length = strnlen(start, result_string_type::get_static_capacity() - total_length - 1);
        start += length;
        total_length += length;
  
        if (length == 0) break;
      }
  
      if (ferror(pipe))
      {
        throw_error<std::runtime_error>("Failed to read from shell pipe.");
      }
  
      _pclose(pipe);
  
      return buffer;
    }
  };

  template <std::size_t threads_limit, typename job_type>
  void async_execute(job_type job, std::size_t threads_count)
  {
    assert_condition(threads_count < threads_limit, "Exceeded maximum threads count limit [{}>{}].", threads_count, threads_limit);

    std::array<std::thread, threads_limit> threads;

    for (std::size_t index = 0; index < threads_count; ++index)
    {
      threads[index] = std::move(std::thread(job));
    }

    for (std::size_t index = 0; index < threads_count; ++index)
    {
      threads[index].join();
    }
  }

  template <std::size_t threads_limit, template <typename> typename collection, typename command_string_type>
  void async_shell_execute(const collection<command_string_type>& commands, std::size_t threads_count)
  {
    std::atomic<std::size_t> vacant_job_index(0);
    auto job = [&vacant_job_index, &commands]()
      {
        while (true)
        {
          std::size_t index = vacant_job_index.fetch_add(1);
          if (index >= commands.size()) break;

          shell<stack_string_512> local_shell;
          local_shell.run(commands[index]);
        }
      };

    const std::size_t corrected_threads_count = std::min(commands.size(), threads_count);
    async_execute<threads_limit>(job, corrected_threads_count);
  }

  template <typename... args_types>
  inline void log(const std::format_string<args_types...>& format, args_types... args)
  {
    constexpr bool logging_enabled = true;
    if constexpr (logging_enabled)
    {
      std::cout << estd::vformat_stack(format.get(), std::make_format_args(args...)) << "\n";
    }
  }

  inline void log(const char* string)
  {
    log("{}", string);
  }
}

// ---------------------------------------------

// ---------- Memory --------------------------

struct memory_report
{
  uint32_t allocated;
  uint32_t deallocated;
  uint32_t times_allocated;
  uint32_t times_deallocated;
  
  bool active;
  std::mutex mutex;

  memory_report()
  {
    clean();
    active = false;
  }

  void clean()
  {
    allocated = 0;
    deallocated = 0;
    times_allocated = 0;
    times_deallocated = 0;
  }

  void dump() const
  {
    estd::log("Allocated={}", allocated);
    estd::log("TimesAllocated={}", times_allocated);
    estd::log("Deallocated={}", deallocated);
    estd::log("TimesDeallocated={}", times_deallocated);
  }

  void validate() const
  {
    if (allocated != deallocated)
    {
      throw_error<std::logic_error>("Memory leak detected !!!");
    }
  }

  void activate()
  {
    active = true;
  }

  void deactivate()
  {
    active = false;
  }

  void allocate(size_t size)
  {
    if (!active) return;

    std::lock_guard _(mutex);
    allocated += size;
    ++times_allocated;
  }

  void deallocate(size_t size)
  {
    if (!active) return;
   
    std::lock_guard _(mutex);
    deallocated += size;
    ++times_deallocated;
  }
} g_memory_report;

void* operator new(size_t size)
{
  if (size == 0)
  {
    throw_error<std::invalid_argument>("Can not allocate 0 bytes of memory.");
  }

  g_memory_report.allocate(size);

  uint32_t* header = reinterpret_cast<uint32_t*>(malloc(size + sizeof(uint32_t)));
  if (!header)
  {
    throw_error<std::logic_error>("Failed to allocate {} bytes.", size);
  }

  (*header) = size;
  return reinterpret_cast<void*>(header + 1);
}

void operator delete(void* data) noexcept
{
  uint32_t* header = reinterpret_cast<uint32_t*>(data) - 1;

  g_memory_report.deallocate(*header);
  free(header);
}

template <typename type>
inline void deallocate_vector(std::vector<type>& vector)
{
  std::vector<type> _;
  vector.swap(_);
}

namespace estd
{
  template <typename type, std::size_t capacity>
  struct stack_allocator
  {
    using value_type = type;

    template <class _Other>
    struct rebind {
      using other = stack_allocator<_Other, capacity>;
    };

    stack_allocator() = default;
    stack_allocator(const stack_allocator& other) {}

    value_type* allocate(std::size_t size)
    {
      assert_condition(capacity == size, "Expected size to be equal to capacity for static string allocation [{}!={}].", capacity, size);
      return data;
    }

    void deallocate(value_type* ptr, std::size_t size)
    {
      assert_condition(capacity == size, "Expected size to be equal to capacity for static string allocation. [{}!={}]", capacity, size);
    }
  protected:
    value_type data[capacity];
  };

  template<std::size_t capacity>
  class stack_string : public std::basic_string<char, std::char_traits<char>, estd::stack_allocator<char, capacity>>
  {
    using super = std::basic_string<char, std::char_traits<char>, estd::stack_allocator<char, capacity>>;
  public:
    static inline constexpr std::size_t get_static_capacity()
    {
      return capacity;
    }

    stack_string() : super()
    {
      constexpr std::size_t small_string_capacity = 16;
      super::reserve(capacity - small_string_capacity);
    }

    stack_string(const char* begin, const char* end) : stack_string()
    {
      super::append(begin, end);
    }

    stack_string(const char* intial_value) : stack_string()
    {
      super::append(intial_value);
    }

    stack_string(const stack_string& other) : stack_string()
    {
      super::append(other.c_str());
    }

    stack_string(stack_string&& other) : stack_string()
    {
      super::append(other.c_str());
    }

    stack_string& operator=(const stack_string& other)
    {
      super::clear();
      super::append(other.c_str());
      return *this;
    }

    stack_string& operator=(stack_string&& other)
    {
      super::clear();
      super::append(other.c_str());
      return *this;
    }
  };

  template<typename string_type>
  void remove_file_extension(string_type& path)
  {
    while (path.size() && path.back() != '.') path.pop_back();
    if (path.size()) path.pop_back();
  }

  template<typename input_string_type, typename output_string_type>
  void remove_file_extension(const input_string_type& input, const output_string_type& output)
  {
    output = input;
    remove_file_extension(output);
  }

  template <typename string_type>
  void replace_extension(string_type& path, const char* new_extension)
  {
    remove_file_extension(path);
    path.append(new_extension);
  }

  template<typename filename_string_type, typename path_string_type>
  void append_filename(const filename_string_type& filename, path_string_type& path)
  {
#pragma warning "Platform dependent code"

    if (filename.empty()) return;

    std::size_t filename_begin_index = filename.size() - 1;
    while (filename_begin_index > 0 && filename[filename_begin_index] != '\\')
    {
      --filename_begin_index;
    }

    for (std::size_t index = filename_begin_index; index < filename.size() && filename[index] != '.'; ++index)
    {
      path.push_back(filename[index]);
    }
  }

}

// ---------------------------------------------

// ---------- Inputs ---------------------------

struct cli_parameter
{
  const char* name;
  const char* value;
};

class cli_parameters
{
public:
  cli_parameters()
  { }

  void initialize(int32_t count, const char** parameters)
  {
    assert_condition(count % 2, "Expected even number of parameters.");

    m_parameters.reserve(count / 2);
    for (int32_t i = 1; i < count; i += 2)
    {
      m_parameters.emplace_back(parameters[i], parameters[i + 1]);
    }

    test_parameter<false>(strings::root_path_parameter_name);
    test_parameter<false>(strings::target_parameter_name);
    test_parameter<false>(strings::platform_parameter_name);
    test_parameter<false>(strings::intermediate_parameter_name);
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

  uint32_t get_threads_count() const
  {
    return atoi(find(strings::thread_parameter_name)  );
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
    deallocate_vector(m_parameters);
  }
protected:
  template<bool test_integer = false>
  void test_parameter(const char* name)
  {
    const char* parameter = find(name);
    assert_condition(parameter, strings::expected_parameter_format, name);

    if constexpr (test_integer)
    {
      uint32_t length = strnlen(parameter, strings::max_parameter_length);
      for (uint32_t i = 0; i < length; ++i)
      {
        if (!std::isdigit(parameter[i]))
        {
          throw_error<std::logic_error>("Expected threads count to be a number.");
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
    deallocate_vector(m_enums);
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
  assert_condition(result, "Was not able to find target {}.", g_cli_parameters.get_target());
  return result;
}

class platform final : public named_enum_type
{
public:
  enum type
  {
    windows
  };

  consteval platform(char marker, const char* name, type type) : named_enum_type(marker, name, static_cast<int32_t>(type))
  {
  }
};

using platforms_list = named_enums_list<platform, platform::type>;

inline platforms_list& platforms()
{
  static platforms_list list;

  if (list.empty())
  {
    static constexpr platform windows = { 'W', "Windows", platform::windows };

    list.append(&windows);
  }

  return list;
}

inline const platform* active_platform()
{
  static const platform* result = platforms().find(g_cli_parameters.get_platform());
  assert_condition(result, "Was not able to find platform {}.", g_cli_parameters.get_platform());
  return result;
}

// ---------------------------------------------

// ---------- IO -------------------------------

using json_object = nlohmann::json;

template <bool perform_seek_to_file_start>
uint32_t fetch_file_size(FILE* file)
{
  if (!file)
  {
    throw_error<std::logic_error>("Attempted to fetch size of nullptr file.");
  }

  int32_t error;

  if constexpr (perform_seek_to_file_start)
  {
    error = fseek(file, 0, SEEK_SET);

    if (error)
    {
      throw_error<std::logic_error>("Failed to jump to the begining of the file with error code ({}) and message ({}).", error, fetch_error_message_friendly(error));
    }
  }

  error = fseek(file, 0, SEEK_END);

  if (error)
  {
    throw_error<std::logic_error>("Failed to jump to the end of the file with error code ({}) and message ({}).", error, fetch_error_message_friendly(error));
  }

  uint32_t file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  return file_size;
}

template <typename string_type>
void read_file_full(const string_type& absolute_path, char** out_buffer, uint32_t& out_file_size)
{
  FILE* file;
  fopen_s(&file, absolute_path.c_str(), "rb");

  if (!file)
  {
    throw_error<std::logic_error>("Failed to open file {}.", absolute_path);
  }

  uint32_t file_size = fetch_file_size<false>(file);
  if (file_size == 0)
  {
    throw_error<std::logic_error>("File is empty {}.", absolute_path);
  }

  estd::log("Loading {} with size of {}.", absolute_path.c_str(), file_size);

  char* buffer = new char[file_size];
  uint32_t read = fread(buffer, sizeof(char), file_size, file);

  if (read != file_size)
  {
    if (feof(file))
    {
      throw_error<std::logic_error>("Read file size ({}) happened to different from actual one ({}).", file_size, read);
    }
    else if (uint32_t error_code = ferror(file))
    {
      const char* error_message = fetch_error_message_friendly(error_code);
      throw_error<std::logic_error>("Failed to read the whole file with a code ({}) and a reason ({}).", error_code, error_message);
    }
  }

  (*out_buffer) = buffer;
  out_file_size = file_size;
}

json_object read_json(const char* relative_path)
{
  estd::stack_string_1024 absolute_path;
  absolute_path += g_cli_parameters.get_root_path();
  absolute_path += relative_path;

  char* buffer = nullptr;
  uint32_t buffer_size = 0;
  read_file_full(absolute_path, &buffer, buffer_size);

  json_object object = nlohmann::json::parse(buffer, buffer + buffer_size);

  delete[] buffer;

  return object; 
}

// --------------------------------------------------------

// ---------- Compiler logic -------------------------------

enum class compiler_options : int8_t
{
  language_standard = 0,
  waringings_level,
  disable_warnings
};

// TODO: Consider pointers or moving.
struct option_description
{
  compiler_options type;
  std::string value;
};

struct define_description
{
  std::string key;
  std::string value;
};

using option_descriptions = std::vector<option_description>;
using define_descriptions = std::vector<define_description>;
using dependecy_libraries = std::vector<std::string>;
using source_files = std::vector<std::string>;
using include_files = std::vector<std::string>;

enum class artifact_types : uint8_t
{
  excutable = 0,
  static_library,
  dynamic_library
};

struct subproject_configuration
{
  std::string name;

  option_descriptions options;
  define_descriptions defines;
  dependecy_libraries dependencies;

  source_files sources;
  include_files includes;
  std::string precompile_header;

  artifact_types artifact_type;
  std::string artifact_name;

  bool has_precompile_header() const
  {
    return precompile_header.size();
  }

  bool is_precompiled() const
  {
    return artifact_name.size();
  }
};

using subproject_configurations = std::vector<subproject_configuration>;

struct project_configuration
{
  struct resource_view
  {
    const project_configuration& project;
    std::size_t subproject_index;

    const std::string& get_subproject_name() const
    {
      return project.subprojects[subproject_index].name;
    }
  };

  struct source_view : public resource_view
  {
    std::size_t source_index;

    const std::string& get_path() const
    {
      return project.subprojects[subproject_index].sources[source_index];
    }

    bool has_precompiler_header() const
    {
      return project.subprojects[subproject_index].has_precompile_header();
    }
  };

  struct precompile_header_view : public resource_view
  {
    const std::string& get_path() const
    {
      return project.subprojects[subproject_index].precompile_header;
    }

    operator bool () const
    {
      return project.subprojects[subproject_index].has_precompile_header();
    }
  };

  subproject_configurations subprojects;
  option_descriptions options;
  define_descriptions defines;
  std::string name;

  source_view get_source_view(std::size_t subproject, std::size_t source) const
  {
    // TODO: Add assert.
    return { *this, subproject, source };
  }

  precompile_header_view get_precompile_header_view(std::size_t subproject) const
  {
    // TODO: Add assert.
    return { *this, subproject };
  }
};

using source_view = project_configuration::source_view;
using precompile_header_view = project_configuration::precompile_header_view;
using dependencies_list = std::vector<const std::string*>;

class compiler_interpreter
{
public:
  using command_string = estd::stack_string_2048;
  using commands_list = std::vector<command_string>;
  
  using compilation_dependency = estd::stack_string_512;
  using compilation_dependenices = std::vector<compilation_dependency>;

  void compile(const project_configuration& project)
  {
    setup(project);

    std::size_t source_commands_count = 0;
    std::size_t header_commands_count = 0;
    for (const auto& subproject : project.subprojects)
    {
      if (subproject.is_precompiled()) continue;
      source_commands_count += subproject.sources.size();
      header_commands_count += subproject.has_precompile_header();
    }

    commands_list source_commands, header_commands, database_entry_commands;
    source_commands.reserve(source_commands_count);
    header_commands.reserve(header_commands_count);
    database_entry_commands.reserve(source_commands_count + header_commands_count);

    #pragma warning "URVO"
    for (std::size_t subproject_index = 0; subproject_index < project.subprojects.size(); ++subproject_index)
    {
      const auto& subproject = project.subprojects[subproject_index];
      if (subproject.is_precompiled()) continue;

      if (precompile_header_view view = project.get_precompile_header_view(subproject_index))
      {
        if (needs_recompilation(view))
        {
          command_string compilation_command = compute_precompile_header_command(view);
          header_commands.push_back(std::move(compilation_command));

          command_string database_entry_command = compute_database_entry_command(view);
          database_entry_commands.push_back(std::move(database_entry_command));
        }
      }

      const std::size_t sources_count = project.subprojects[subproject_index].sources.size();
      for (std::size_t source_index = 0; source_index < sources_count; ++source_index)
      {
        source_view view = project.get_source_view(subproject_index, source_index);

        if (needs_recompilation(view))
        {
          command_string compilation_command = compute_source_command(view);
          source_commands.push_back(std::move(compilation_command));

          command_string database_entry_command = compute_database_entry_command(view);
          database_entry_commands.push_back(std::move(database_entry_command));
        }
      }
    }

    //estd::async_shell_execute<32>(header_commands, g_cli_parameters.get_threads_count());
    //estd::async_shell_execute<32>(source_commands, g_cli_parameters.get_threads_count());
    //estd::async_shell_execute<32>(database_entry_commands, g_cli_parameters.get_threads_count());

    std::string database;

    constexpr std::size_t max_expected_entry_size = 2048;
    database.reserve((source_commands_count + header_commands_count) * max_expected_entry_size + 2);

    database.append("[\n");

    for (std::size_t subproject_index = 0; subproject_index < project.subprojects.size(); ++subproject_index)
    {
      const auto& subproject = project.subprojects[subproject_index];
      if (subproject.is_precompiled()) continue;

      if (precompile_header_view view = project.get_precompile_header_view(subproject_index))
      {
        command_string database_entry_path = get_output_path(view);
        database_entry_path.append(".dbe");

        if (std::filesystem::exists(database_entry_path.c_str()))
        {
          append_file_data(database_entry_path, database);
          database.push_back('\n');
        }
      }

      const std::size_t sources_count = project.subprojects[subproject_index].sources.size();
      for (std::size_t source_index = 0; source_index < sources_count; ++source_index)
      {
        source_view view = project.get_source_view(subproject_index, source_index);

        command_string database_entry_path = get_output_path(view);
        database_entry_path.append(".dbe");

        //estd::log("ENTRY: {}.", database_entry_path.c_str());

        if (std::filesystem::exists(database_entry_path.c_str()))
        {
          append_file_data(database_entry_path, database);
          database.push_back('\n');
        }
      }
    }

    database.push_back(']');

    command_string database_path = g_cli_parameters.get_intermediate_path();
    database_path.append("database.json");
    dump_to_file(database_path, database);

    clear();
  }

  virtual ~compiler_interpreter() = default;

protected:
  virtual void setup(const project_configuration& project) = 0;
  virtual void clear() = 0;

  virtual command_string compute_source_command(const source_view& view) = 0;
  virtual command_string compute_database_entry_command(const source_view& view) = 0;
  virtual bool needs_recompilation(const source_view& view) = 0;

  virtual command_string compute_precompile_header_command(const precompile_header_view& view) = 0;
  virtual command_string compute_database_entry_command(const precompile_header_view& view) = 0;
  virtual bool needs_recompilation(const precompile_header_view& view) = 0;

  template <typename view_type>
  inline command_string get_output_path(const view_type& view) const
  {
    command_string result = g_cli_parameters.get_intermediate_path();
    result.append(view.get_subproject_name());
    append_filename(view.get_path(), result);

    return result;
  }

  void append_file_data(const command_string& filepath, std::string& store)
  {
    std::ifstream file(filepath.c_str(), std::ios_base::in);

    command_string line;
    while (std::getline(file, line))
    {
      store.append(line);
    }
  }

  void dump_to_file(const command_string& filepath, const std::string& data)
  {
    std::ofstream file(filepath.c_str(), std::ios_base::out);
    file << data;
  }
};

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

    for (const auto& subprojects : project.subprojects)
    {
      command_string path = g_cli_parameters.get_intermediate_path();
      path.append(subprojects.name);

      if (!std::filesystem::exists(path.c_str()))
      {
        estd::log("Created missing directory: {}.", path.c_str());
        std::filesystem::create_directories(path.c_str());
      }
    }
  }

  virtual void clear() override
  {
    project_suffix.clear();
    subproject_suffixes.clear();
  }

  virtual command_string compute_source_command(const source_view& view) override
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

  virtual command_string compute_precompile_header_command(const precompile_header_view& view) override
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

  virtual command_string compute_database_entry_command(const source_view& view) override
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

  virtual command_string compute_database_entry_command(const precompile_header_view& view) override
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

  virtual bool needs_recompilation(const precompile_header_view& view) override
  {
#pragma warning "Platform specific code"
    return needs_recompilation_internal(view, ".pch");
  }

  virtual bool needs_recompilation(const source_view& view) override
  {
#pragma warning "Platform specific code"
    return needs_recompilation_internal(view, ".obj");
  }
  
  template <typename view_type>
  bool needs_recompilation_internal(const view_type& view, const char* extension)
  {
    command_string dependencies_list_path = get_output_path(view);
    dependencies_list_path.append(".deps");

    bool update_dependencies = false;
    if (std::filesystem::exists(dependencies_list_path.c_str()))
    {
      auto dependency_update_time = std::filesystem::last_write_time(dependencies_list_path.c_str());
      auto source_update_time = std::filesystem::last_write_time(view.get_path());
      update_dependencies = dependency_update_time < source_update_time;
    }
    else
    {
      update_dependencies = true;
    }

    if (update_dependencies)
    {
      command_string command = "clang++ -MM ";
      command.append(view.get_path());
      command.append(project_suffix);
      command.append(subproject_suffixes[view.subproject_index]);
      command.append(" -MF ");
      command.append(dependencies_list_path);

      estd::shell<estd::stack_string_512> local_shell;
      local_shell.run(command);
    }

    command_string target_path = get_output_path(view);
    target_path.append(extension);

    if (!std::filesystem::exists(target_path.c_str())) return true;
    const auto compilation_time = std::filesystem::last_write_time(target_path.c_str());

    std::ifstream file(dependencies_list_path.c_str(), std::ios_base::in);
    estd::stack_string_512 line;

    while (std::getline(file, line))
    {
      std::size_t space_pre_word = line.find(' ');
      if (space_pre_word == std::string::npos) break;

      std::size_t word_start = line.find_first_of("qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM", space_pre_word);
      if (word_start == std::string::npos) break;

      std::size_t word_end = line.find_first_of(" \n\0", word_start);
      if (word_end == std::string::npos) word_end = line.size();

      std::string_view path_view(line.c_str() + word_start, line.c_str() + word_end);
      const auto update_time = std::filesystem::last_write_time(path_view);
      if (compilation_time < update_time) return true;

      //estd::log("DEPDENCY: [{}].", path_view);
    }

    //estd::log("[{}] FILE: {}, {}", update_dependencies, view.get_path().c_str(), view.get_subproject_name().c_str());

    return false;
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
    case compiler_options::language_standard:
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

      throw_error<std::invalid_argument>("Provided C++ standard [{}] is not supported.", option.value);

      break;
    }
    case compiler_options::waringings_level:
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

      throw_error<std::invalid_argument>("Provided warnings level is not supported [{}].", option.value);

      break;
    }
    case compiler_options::disable_warnings:
    {
      if (option.value == "1") return "-w";
      throw_error<std::invalid_argument>("Provided disable warnings value is not supported [{}].", option.value);

      break;
    }

    default:
      throw_error<std::invalid_argument>("Provided option is not supported [{}].", static_cast<uint8_t>(option.type));
      break;
    }

    return "";
  }

  inline command_string convert_define(const define_description& define) const
  {
    command_string result;
    result.append("-D").append(define.key);
    if(define.value.size()) result.append("=").append(define.value);
    return result;
  }
private:
  command_string project_suffix;
  commands_list subproject_suffixes;
  commands_list precompile_headers_suffixes;
};

class linker_interpreter
{
public:
  using command_string = estd::stack_string_4096;

  void link(const project_configuration& project)
  {
    std::vector<command_string> link_commands;
    link_commands.reserve(project.subprojects.size());

    for (const auto& subproject : project.subprojects)
    {
      if (subproject.is_precompiled()) continue;
      command_string command = compute_link_command(subproject);
      link_commands.push_back(std::move(command));
    }

    //estd::async_shell_execute<32>(link_commands, 1); // g_cli_parameters.get_threads_count()
  }
protected:
  virtual command_string compute_link_command(const subproject_configuration& subproject) = 0;
};

class llvm_linker_interpreter final : public linker_interpreter
{
public:
protected:
  virtual command_string compute_link_command(const subproject_configuration& subproject) override
  {
    if (subproject.artifact_type != artifact_types::static_library) return "";
    return compute_static_library_link_command(subproject);
  }

  command_string compute_static_library_link_command(const subproject_configuration& subproject)
  {
    command_string result = "lld-link ";
    
#pragma warning "Platform dependent code."
#pragma warning "Make one varargs function for path composition, it will allow to hide platform dependent code and make it easier to read."
    for (const auto& source : subproject.sources)
    {
      result.append(g_cli_parameters.get_intermediate_path());
      result.append(subproject.name);
      estd::append_filename(source, result);
      result.append(".obj ");
    }

    for (const auto& dependency_library : subproject.dependencies)
    {
      result.append(dependency_library);
      result.push_back(' ');
    }

    result.append("/out:");
    result.append(g_cli_parameters.get_intermediate_path());
    result.append(subproject.name);
    result.append("\\");
    result.append(subproject.name);
    result.append(".lib");

    return result;
  }
};

// ---------------------------------------------------------

// ---------- Input Parsing --------------------------------

class input_reader
{
public:
  // TODO: Consider collapsing these.
  struct option_data
  {
    const std::string* name;
    const std::string* value;
  };
  
  struct define_data
  {
    const std::string* name;
    const std::string* value;
  };

  virtual bool has_next_option() const = 0;
  virtual std::optional<option_data> next_option() = 0;

  virtual bool has_next_define() const = 0;
  virtual std::optional<define_data> next_define() = 0;

  virtual bool has_next_source() const = 0;
  virtual const std::string* next_source() = 0;

  virtual bool has_next_include() const = 0;
  virtual const std::string* next_include() = 0;

  virtual bool has_next_dependency() const = 0;
  virtual const std::string* next_dependency() = 0;

  virtual const std::string* precompile_header() const = 0;
  virtual const std::string* artifact_type() const = 0;
  virtual const std::string* artifact_name() const = 0;

  virtual const std::string* project_name() const = 0;

  virtual bool is_subproject_valid() const = 0;
  virtual bool has_next_subproject() const = 0;
  virtual void next_subproject() = 0;
};

template <typename value_type>
const value_type* fetch_value(const nlohmann::json& input, const char* value_name)
{
  if (!input.contains(value_name)) return nullptr;
  return input[value_name].get_ptr<const value_type*>();
}

template <typename value_type>
const value_type* fetch_value(const nlohmann::json& input, std::size_t index)
{
  if (index >= input.size()) return nullptr;
  return input[index].get_ptr<const value_type*>();
}

class json_reader : public input_reader
{
public:
  json_reader(const json_object& data) : data(data), subproject_index(0), option_index(0), define_index(0), source_index(0), include_index(0), dependency_index(0)
  { }

  virtual bool has_next_option() const override
  {
    const json_object& project = get_current_project();
    if (!project.contains("Options")) return false;

    const json_object& options = project["Options"];
    return options.size() > option_index;
  }

  virtual std::optional<option_data> next_option() override
  {
    const json_object& project = get_current_project();
    if (!project.contains("Options")) return std::nullopt;

    const json_object& options = project["Options"];
    if (option_index >= options.size()) return std::nullopt;

    const json_object& option_object = options[option_index];
    ++option_index;

    const std::string* platforms = fetch_value<std::string>(option_object, "Platforms");
    if (platforms && !active_platform()->match(platforms->c_str())) return std::nullopt;

    const std::string* targets = fetch_value<std::string>(option_object, "Targets");
    if (targets && !active_target()->match(targets->c_str())) return std::nullopt;

    const std::string* value = fetch_value<std::string>(option_object, "Value");
    assert_condition(value, "Failed to fetch Value parameter from an option.");

    const std::string* name = fetch_value<std::string>(option_object, "Name");
    assert_condition(name, "Failed to fetch Name parameter from an option.");

    return option_data { name, value };
  }

  virtual bool has_next_define() const override
  {
    const json_object& project = get_current_project();
    if (!project.contains("Defines")) return false;

    const json_object& defines = project["Defines"];
    return defines.size() > define_index;
  }

  virtual std::optional<define_data> next_define() override
  {
    const json_object& project = get_current_project();
    if (!project.contains("Defines")) return std::nullopt;

    const json_object& defines = project["Defines"];
    if (define_index >= defines.size()) return std::nullopt;

    const json_object& define_object = defines[define_index];
    ++define_index;

    const std::string* platforms = fetch_value<std::string>(define_object, "Platforms");
    if (platforms && !active_platform()->match(platforms->c_str())) return std::nullopt;

    const std::string* targets = fetch_value<std::string>(define_object, "Targets");
    if (targets && !active_target()->match(targets->c_str())) return std::nullopt;

    const std::string* value = fetch_value<std::string>(define_object, "Value");
    assert_condition(value, "Failed to fetch Value parameter from an define.");

    const std::string* name = fetch_value<std::string>(define_object, "Name");
    assert_condition(name, "Failed to fetch Name parameter from an define.");

    return define_data { name, value };
  }

  virtual bool has_next_source() const override
  {
    if (subproject_index == 0) return false;
    
    const json_object& project = get_current_project();
    if (!project.contains("Sources")) return false;

    const json_object& sources = project["Sources"];
    return sources.size() > source_index;
  }

  virtual const std::string* next_source() override
  {
    if (subproject_index == 0) return nullptr;

    const json_object& project = get_current_project();
    if (!project.contains("Sources")) return nullptr;

    const json_object& sources = project["Sources"];
    return fetch_value<std::string>(sources, source_index++);
  }

  virtual bool has_next_include() const override
  {
    if (subproject_index == 0) return false;

    const json_object& project = get_current_project();
    if (!project.contains("Includes")) return false;

    const json_object& includes = project["Includes"];
    return includes.size() > include_index;
  }

  virtual const std::string* next_include() override
  {
    if (subproject_index == 0) return nullptr;

    const json_object& project = get_current_project();
    if (!project.contains("Includes")) return nullptr;

    const json_object& includes = project["Includes"];
    return fetch_value<std::string>(includes, include_index++);
  }

  virtual bool has_next_dependency() const override
  {
    if (subproject_index == 0) return false;

    const json_object& project = get_current_project();
    if (!project.contains("Dependencies")) return false;

    const json_object& dependencies = project["Dependencies"];
    return dependencies.size() > dependency_index;
  }

  virtual const std::string* next_dependency() override
  {
    if (subproject_index == 0) return nullptr;

    const json_object& project = get_current_project();
    if (!project.contains("Dependencies")) return nullptr;

    const json_object& dependencies = project["Dependencies"];
    return fetch_value<std::string>(dependencies, dependency_index++);
  }

  virtual const std::string* precompile_header() const override
  {
    if (subproject_index == 0) return nullptr;

    const json_object& project = get_current_project();
    if (!project.contains("PrecompileHeader")) return nullptr;

    return fetch_value<std::string>(project, "PrecompileHeader");
  }

  virtual const std::string* artifact_type() const override
  {
    if (subproject_index == 0) return nullptr;

    const json_object& project = get_current_project();
    return fetch_value<std::string>(project, "ArtifactType");
  }

  virtual const std::string* artifact_name() const override
  {
    if (subproject_index == 0) return nullptr;

    const json_object& project = get_current_project();
    return fetch_value<std::string>(project, "ArtifactName");
  }

  virtual const std::string* project_name() const override
  {
    const json_object& project = subproject_index == 0 ? data : get_current_project();
    return fetch_value<std::string>(project, "Name");
  }

  virtual void next_subproject() override
  {
    ++subproject_index;
    reset();
  }

  virtual bool is_subproject_valid() const override
  {
    return subproject_index <= data["Projects"].size();
  }

  virtual bool has_next_subproject() const override
  {
    return data["Projects"].size() >= subproject_index;
  }
protected:
  inline void reset()
  {
    option_index = 0;
    define_index = 0;
    source_index = 0;
    include_index = 0;
    dependency_index = 0;
  }

  inline const json_object& get_current_project() const
  {
    return subproject_index == 0 ? data["Global"] : data["Projects"][subproject_index - 1];
  }
protected:
  const json_object& data;
  std::size_t subproject_index;
  std::size_t option_index;
  std::size_t define_index;
  std::size_t source_index;
  std::size_t include_index;
  std::size_t dependency_index;
};

class compiler_input_parser
{
public:
  using option_parser = compiler_options(*)(const std::string& option_name);

  void register_option_parser(option_parser parser)
  {
    option_parsers.push_back(parser);
  }

  project_configuration parse(input_reader& reader) const
  {
    std::vector<dependencies_list> dependencies_lists;
    dependencies_list temporary_dependencies;

    project_configuration project;
    parse_project(reader, project, temporary_dependencies);
    reader.next_subproject();

    // TODO: Can reserve here.
    while (reader.is_subproject_valid())
    {
      subproject_configuration subproject;
      parse_project(reader, subproject, temporary_dependencies);

      project.subprojects.push_back(std::move(subproject));
      dependencies_lists.push_back(std::move(temporary_dependencies));

      reader.next_subproject();
    }

    const std::size_t subprojects_count = dependencies_lists.size();

    std::map<std::string, std::size_t> name_mapping;
    for (std::size_t subproject_index = 0; subproject_index < subprojects_count; ++subproject_index)
    {
      const auto& subproject = project.subprojects[subproject_index];
      name_mapping[subproject.name] = subproject_index;
    }

    std::vector<std::vector<std::size_t>> inverse_dependencies_lists;
    inverse_dependencies_lists.resize(subprojects_count);
    
    for (std::size_t subproject_index = 0; subproject_index < subprojects_count; ++subproject_index)
    {
      for (const std::string* dependency_name : dependencies_lists[subproject_index])
      {
        estd::log("PP: {}, {}.", project.subprojects[subproject_index].name, *dependency_name);

        const std::size_t dependency_index = name_mapping[*dependency_name];
        inverse_dependencies_lists[dependency_index].push_back(subproject_index);
      }
    }

    struct node_state
    {
      std::size_t rank = 0;
      std::size_t supplied = 0;
    };

    std::vector<node_state> states(subprojects_count);
    std::queue<std::size_t> processing_queue;

    for (std::size_t subproject_index = 0; subproject_index < subprojects_count; ++subproject_index)
    {
      const bool is_leaf_project = dependencies_lists[subproject_index].empty();
      if (is_leaf_project)
      {
        processing_queue.push(subproject_index);
      }
    }

    while (processing_queue.size())
    {
      const std::size_t subproject_index = processing_queue.front();
      processing_queue.pop();

      const auto& subproject = project.subprojects[subproject_index];
      const auto& subproject_includes = subproject.includes;
      const auto& subproject_dependencies = subproject.dependencies;
      //estd::log("Processing: {}, {}.", subproject.name, inverse_dependencies_lists[subproject_index].size());

#pragma warning "Redundant copy."
      estd::stack_string_512 dependency;
      if (subproject.artifact_name.size())
      {
        dependency = subproject.artifact_name.c_str();
      }
      else
      {
        dependency.append(g_cli_parameters.get_intermediate_path());
        dependency.append(subproject.name);
        dependency.push_back('\\');
        dependency.append(subproject.name);
        dependency.append(".lib");
      }

      for (const std::size_t dependant_index : inverse_dependencies_lists[subproject_index])
      {
        auto& dependant_subproject = project.subprojects[dependant_index];

        //estd::log("PP: {}, {}.", subproject.name, dependant_subproject.name);

        auto& dependant_includes = dependant_subproject.includes;
        dependant_includes.insert(dependant_includes.end(), subproject_includes.begin(), subproject_includes.end());

        auto& dependant_dependencies = dependant_subproject.dependencies;
        dependant_dependencies.push_back(dependency.c_str());
        dependant_dependencies.insert(dependant_dependencies.end(), subproject_dependencies.begin(), subproject_dependencies.end());

        auto& depndency_state = states[dependant_index];
        depndency_state.rank = states[subproject_index].rank + 1;
        depndency_state.supplied++;

        const bool supplied_last_dependency = depndency_state.supplied == dependencies_lists[dependant_index].size();
        if (supplied_last_dependency) processing_queue.push(dependant_index);
      }
    }
    
#pragma warning "Linking logic optimization possible, pass dependencies and rank the configuration."
    std::sort(project.subprojects.begin(), project.subprojects.end(), [&name_mapping, &states](const subproject_configuration& lhs, const subproject_configuration& rhs)
      {
        const std::size_t lhs_ranks_index = name_mapping[lhs.name];
        const std::size_t rhs_ranks_index = name_mapping[rhs.name];
        return states[lhs_ranks_index].rank < states[rhs_ranks_index].rank;
      });

    estd::log("\nOrdering: ");
    for (std::size_t subproject_index = 0; subproject_index < subprojects_count; ++subproject_index)
    {
      const auto& subproject = project.subprojects[subproject_index];
      estd::log("{} ranks {}.", subproject.name.c_str(), states[name_mapping[subproject.name]].rank);
      for (const auto& include : subproject.includes)
      {
        estd::log("{}.", include.c_str());
      }
    }

    //std::exit(2);

    return project;
  }
protected:
  inline option_description parse_option(const input_reader::option_data& input) const
  {
    compiler_options type = static_cast<compiler_options>(-1);
    for (const auto& parser : option_parsers)
    {
      compiler_options result = parser(*input.name);
      if (static_cast<int32_t>(result) >= 0)
      {
        type = result;
        break;
      }
    }

    assert_condition(static_cast<int32_t>(type) >= 0, "Failed to parse option [{}].", input.name->c_str());
    return option_description{ type, *input.value };
  }

  inline define_description parse_define(const input_reader::define_data& input) const
  {
    return define_description{ *input.name, *input.value };
  }

  template <typename project_type>
  inline void parse_project(input_reader& reader, project_type& project, dependencies_list& dependencies) const
  {
    project.name = *reader.project_name();

    while (reader.has_next_option())
    {
      std::optional<input_reader::option_data> option = reader.next_option();
      if (option.has_value()) project.options.push_back(std::move(parse_option(option.value())));
    }

    while (reader.has_next_define())
    {
      std::optional<input_reader::define_data> define = reader.next_define();
      if (define.has_value()) project.defines.push_back(std::move(parse_define(define.value())));
    }

    if constexpr (std::is_same_v<project_type, subproject_configuration>)
    {
      while (reader.has_next_source())
      {
        const std::string* source = reader.next_source();
        if (!source) continue;

        estd::stack_string_1024 path;
        path.append(g_cli_parameters.get_root_path());
        path.append(source->c_str());

        if (std::filesystem::is_directory(path.c_str()))
        {
          project.includes.push_back(path.c_str());
        }

        append_all_files(path, ".cpp", project.sources);
      }

      while (reader.has_next_include())
      {
        const std::string* include = reader.next_include();
        if (!include) continue;

        estd::stack_string_1024 path;
        path.append(g_cli_parameters.get_root_path());
        path.append(include->c_str());

        project.includes.push_back(path.c_str());
      }

      if (const std::string* precompile_header = reader.precompile_header())
      {
        estd::stack_string_1024 path;
        path.append(g_cli_parameters.get_root_path());
        path.append(precompile_header->c_str());

        const bool path_is_not_present = !std::filesystem::exists(path.c_str());
        if (path_is_not_present) throw_error<std::invalid_argument>("Cound't find a precompile header [{}].", path.c_str());

        const bool is_directory = std::filesystem::is_directory(path.c_str());
        if (is_directory) throw_error<std::invalid_argument>("Expected precompile header [{}] to be a file not a directory.", path.c_str());

        project.precompile_header = path;
      }

      if (const std::string* artifact = reader.artifact_type())
      {
        if ((*artifact) == "Executable")
        {
          project.artifact_type = artifact_types::excutable;
        }
        else if ((*artifact) == "StaticLibary")
        {
          project.artifact_type = artifact_types::static_library;
        }
        else if ((*artifact) == "DynamicLibrary")
        {
          project.artifact_type = artifact_types::dynamic_library;
        }
        else
        {
          throw_error<std::invalid_argument>("Failed to parse artifact type [{}].", artifact->c_str());
        }
      }
      else
      {
        project.artifact_type = artifact_types::static_library;
      }

#pragma "Clould add parsing of artifact type based on extension."
      if (const std::string* name = reader.artifact_name())
      {
        estd::log("FOUND: [{}]", project.name.c_str());

        estd::stack_string_512 path = g_cli_parameters.get_intermediate_path();
        path.append(*name);

        project.artifact_name = path;
      }

      while (reader.has_next_dependency())
      {
        const std::string* dependency = reader.next_dependency();
        dependencies.push_back(dependency);
      }
    }
  }

  template <typename destination_type>
  inline void append_all_files(const estd::stack_string_1024& path, const char* extension, destination_type& destination) const
  {
    // TODO: This does way to many allocations, need to explore custom wrappers, or accept it as part of file manipulation life.
    const bool path_is_not_present = !std::filesystem::exists(path.c_str());
    if (path_is_not_present) throw_error<std::invalid_argument>("Cound't find a source [{}].", path.c_str());

    const bool is_file = !std::filesystem::is_directory(path.c_str());
    if (is_file) { destination.emplace_back(path.c_str()); return; }
    
    for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(path.c_str()))
    {
      const std::filesystem::path& path = entry.path();
      if (path.extension().compare(extension)) continue;
      destination.push_back(std::move(path.string()));
    }
  }
protected:
  std::vector<option_parser> option_parsers;
};

// ---------------------------------------------
