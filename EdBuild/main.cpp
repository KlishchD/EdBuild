#include "EdBuild.h"

#include "tools_registry.h"
#include "platforms_registry.h"
#include "targets_registry.h"

#include "builder.h"
#include "readers/json_reader.h"
#include "parsers/input_parser.h"
#include "compilers/clang_translator.h"
#include "linkers/lld_link_translator.h"

void* operator new(size_t size)
{
  if (size == 0)
  {
    estd::throw_error<std::invalid_argument>("Can not allocate 0 bytes of memory.");
  }

  builder_memory_report().allocate(size);

  uint32_t* header = reinterpret_cast<uint32_t*>(malloc(size + sizeof(uint32_t)));
  if (!header)
  {
    estd::throw_error<std::logic_error>("Failed to allocate {} bytes.", size);
  }

  (*header) = size;
  return reinterpret_cast<void*>(header + 1);
}

void operator delete(void* data) noexcept
{
  if (data)
  {
    uint32_t* header = reinterpret_cast<uint32_t*>(data) - 1;

    builder_memory_report().deallocate(*header);
    free(header);
  }
}

int32_t main(int32_t count, const char** arguments)
{
  // clang++ -MM main.cpp

  builder_memory_report().activate();

  tools_registry tools;
  tools.register_driver<caching_compiler_driver<clang_cl_translator>>();
  tools.register_driver<direct_linking_driver<lld_linker_translator>>();

  platforms_registry platforms;
  platforms.create_platform("Windows", 'W')
    .set_preprocessing_extension(".i")
    .set_object_extension(".obj")
    .set_precompile_header_extension(".pch")
    .set_static_library_extension(".lib")
    .set_dynamic_library_extension(".dll")
    .set_executable_extension(".exe")
    .set_dependencies_extension(".deps")
    .set_database_extension(".dbe")
    .set_symbols_database_extension(".pdb")
    .commit();

  targets_registry targets;
  targets
    .create_target("Debug", 'D')
    .create_target("Release", 'R')
    .create_target("Base", 'B');

  using path_parameter = estd::console::path_parameter;
  using integer_parameter = estd::console::integer_parameter;
  using unsigned_integer_parameter = estd::console::unsigned_integer_parameter;
  using bool_parameter = estd::console::bool_parameter;
  using marker_parameter = estd::console::marker_parameter;

  builder::configuration configuration{ tools };

  estd::console::console console;
  console.add_parameter<path_parameter>("-Project", &configuration.project_path)
    .set_help("Sets a path to be perpended to all the relative project paths.")
    .set_mandatory(true)
    .set_directory(true);

  console.add_parameter<path_parameter>("-Intermediate", &configuration.intermediate_path)
    .set_help("Sets a path to the directory that will hold all the temporary intermediate data.")
    .set_mandatory(true)
    .set_directory(true);

  console.add_parameter<path_parameter>("-Builds", &configuration.builds_path)
    .set_help("Sets a path to the directory that builds will be composed at.")
    .set_mandatory(true)
    .set_directory(true);

  console.add_parameter<unsigned_integer_parameter>("-Threads", &configuration.threads)
    .set_help("Sets a maximum allowed number of threads to be used for building process.")
    .set_mandatory(true)
    .set_range(1, 32);

  console.add_parameter<bool_parameter>("-IgnoreBuilderUpdate", &configuration.ignore_builder_updates)
    .set_help("Disables build invalidation from the builder update check.");

  console.add_parameter<bool_parameter>("-GenerateCompilationDatabase", &configuration.generate_compilation_database)
    .set_help("Generates a compilation commands database after builds are generated.");

  char platform = '-';
  console.add_parameter<marker_parameter>("-Platform", &platform)
    .set_help("Sets a marker for a platform to compile the project for.")
    .set_madatory(true);

  char target = '-';
  console.add_parameter<marker_parameter>("-Target", &target)
    .set_help("Sets a marker for a target to compile.")
    .set_madatory(true);

  console.parse(count, arguments);
  console.verify_mandatory();

  configuration.platform = platforms.find(platform);
  assert_condition(configuration.platform, "Failed to find platform with marker {}.", platform);
  estd::log("Selected platform: [{}].", configuration.platform->get_name());

  configuration.target = targets.find(target);
  assert_condition(configuration.platform, "Failed to find target with marker {}.", target);
  estd::log("Selected target: [{}].", configuration.target->get_name());

  try
  {
    estd::path instructions_path;
    instructions_path
      .append(configuration.project_path)
      .append("instructions.json");

    estd::json instructions_source = estd::read_json(instructions_path);
    json_instructions_reader reader{ instructions_source };
    
    builder_input_parser parser{ *configuration.platform, *configuration.target};
    parser.register_option_parser([](const std::string& name) { return name == "C++" ? builder_options::language_standard : static_cast<builder_options>(-1); });
    parser.register_option_parser([](const std::string& name) { return name == "DisableWarnings" ? builder_options::disable_warnings : static_cast<builder_options>(-1); });
    parser.register_option_parser([](const std::string& name) { return name == "GenerateDebugInformation" ? builder_options::generate_debug_information : static_cast<builder_options>(-1); });
    parser.register_option_parser([](const std::string& name) { return name == "GenerateSymbolsDatabase" ? builder_options::generate_symbols_database : static_cast<builder_options>(-1); });

    instructions_description instructions = parser.parse(configuration.project_path, &reader);

    estd::log("Option modifiers: {}.", instructions.modifiers.options.size());
    estd::log("Define modifiers: {}.", instructions.modifiers.defines.size());
    estd::log("Subprojects: {}.", instructions.project.subprojects.size());
    estd::log("Builds: {}.", instructions.builds.size());

    builder instance{ configuration };
    instance.build(instructions);
  }
  catch (const std::exception& error)
  {
    estd::log(error.what());
    return 1;
  }

  builder_memory_report().deactivate();

  try
  {
    estd::log("\nMemory report:");
    builder_memory_report().dump();

    builder_memory_report().validate();
  }
  catch (const std::exception& error)
  {
    estd::log(error.what());
    return 1;
  }

  return 0;
}