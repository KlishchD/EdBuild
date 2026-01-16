#include "EdBuild.h"

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

  try
  {
    cli().initialize(count, arguments);
    cli().dump_parameters();
  
    estd::log("\nActive target: ");
    active_target()->dump();
  
    estd::log("\nActive platform: ");
    active_platform()->dump();

    estd::stack_string_512 instructions_path = cli().get_project_path();
    instructions_path.append(strings::instructions_path);
    estd::json instructions = estd::read_json(instructions_path);

    json_reader reader { instructions };
    
    compiler_input_parser parser;
    parser.register_option_parser([](const std::string& name) { return name == "C++" ? builder_options::language_standard : static_cast<builder_options>(-1); });
    parser.register_option_parser([](const std::string& name) { return name == "DisableWarnings" ? builder_options::disable_warnings : static_cast<builder_options>(-1); });
    parser.register_option_parser([](const std::string& name) { return name == "GenerateDebugInformation" ? builder_options::generate_debug_information : static_cast<builder_options>(-1); });
    parser.register_option_parser([](const std::string& name) { return name == "GenerateSymbolsDatabase" ? builder_options::generate_symbols_database : static_cast<builder_options>(-1); });

    project_configuration project = parser.parse(reader);
    estd::log("\nProject name: {}.", project.name.c_str());
    estd::log("Defines: {}.", project.defines.size());
    estd::log("Options: {}.", project.options.size());
    estd::log("Subprojects: {}.", project.subprojects.size());
    estd::log("");

    tools().register_driver<caching_compiler_driver<clang_cl_translator>>();
    tools().register_driver<direct_linking_driver<lld_linker_translator>>();

    builder::configuration configuration(tools());
    configuration.ignore_builder_updates = cli().ignore_builder_update();
    configuration.generate_compilation_database = false;

    builder instance{ configuration };
    instance.build(project);

    targets().clean();
    platforms().clean();
      
    cli().clean();
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

#pragma message("Checkout multithreaded builds.")

  return 0;
}