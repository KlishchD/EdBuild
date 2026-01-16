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
    parser.register_option_parser([](const std::string& name) { return name == "C++" ? compiler_options::language_standard : static_cast<compiler_options>(-1); });
    parser.register_option_parser([](const std::string& name) { return name == "DisableWarnings" ? compiler_options::disable_warnings : static_cast<compiler_options>(-1); });

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


// 
// class Project;
// 
// std::string ExecuteConsoleCommand(const std::string& command)
// {
//   FILE* pipe = _popen(command.c_str(), "r");
//   if (!pipe) throw_error<std::logic_error>("Failed to open a pipe for command {}.", command);
// 
//   std::string result;
//   result.reserve(2048);
//     
//   constexpr uint32_t bufferSize = 256;
//   char buffer[bufferSize];
// 
//   while (fgets(buffer, bufferSize, pipe))
//   {
//     result.append(buffer);
//   }
// 
//   enum ExecutionCodes
//   {
//     Success = 0,
//     ErrorWithReason = -1
//   };
// 
//   const int32_t status = _pclose(pipe);
// 
//   switch (status)
//   {
//   case Success: return result;
//   case ErrorWithReason:
//   {
//     constexpr uint32_t capacity = 1024;
//     char message[capacity];
// 
//     if (strerror_s(message, errno))
//     {
//       throw_error<std::logic_error>("Failed to execute command with a reason [{}]", std::string_view(message, capacity));
//     }
//     else
//     {
//       throw_error<std::logic_error>("Failed to execute command but was not able to fetch a reason.");
//     }
//   }
//   default: throw_error<std::logic_error>("Failed to execute command, with no reason provided but with code {}.", status);
//   }
// }
// 
// void ParseDependencies(const nlohmann::json& json, std::vector<uint32_t>& indices);
// void ParseDefines(const nlohmann::json& json, std::vector<uint32_t>& indices);
// void ParseOptions(const nlohmann::json& json, std::vector<uint32_t>& indices);
// void ParseSources(const nlohmann::json& json, Project* project, uint32_t projectIndex);
// const std::string* GetIfPresent(const std::string& name, const nlohmann::json& json);
// 
// inline const Platform* g_ActivePlatform = nullptr;
// 
// 
// struct Define
// {
//   std::string Name;
//   std::string Value;
// };
// 
// struct Option
// {
//   std::string Name;
//   std::string Value;
// };
// 
// struct Source
// {
//   std::string Path;
//   std::string Output;
// };
// 
// class Project
// {
// public:
//   enum ProjectStatus
//   {
//     NotCompiled,
//     Compiled
//   };
// 
//   enum ProjectCompilationType
//   {
//     Executable,
//     DynamicLibrary,
//     StaticLibrary
//   };
//   
//   Project(const nlohmann::json& project, uint32_t index)
//   {
//     m_DataMutex = std::make_unique<std::mutex>();
// 
//     m_Name = project["Name"];
// 
//     if (project.contains("Library"))
//     {
//       m_Status = ProjectStatus::Compiled;
//       for (const std::string& library : project["Library"])
//       {
//         std::filesystem::path path = *g_Parameters.Find("-Root") + library;
//         if (std::filesystem::exists(path))
//         {
//           m_Libraries.push_back(path.string());
//         }
//         else
//         {
//           throw_error<std::logic_error>("Failed to find library {}", path.string());
//         }
//       }
//     }
//     else
//     {
//       m_Status = ProjectStatus::NotCompiled;
// 
//       ParseSources(project, this, index);
// 
//       if (project.contains("PrecompileHeader"))
//       {
//         const std::string& header = project["PrecompileHeader"];
//         m_PrecompileHeader = *g_Parameters.GetRootPath() + header;
//         if (!std::filesystem::exists(m_PrecompileHeader) || std::filesystem::is_directory(m_PrecompileHeader))
//         {
//           throw_error<std::logic_error>("Provided precompile header is not present or is a directory {}.", m_PrecompileHeader);
//         }
//       }
//       
//       ParseDependencies(project, m_Dependencies);
//       ParseDefines(project, m_Defines);
//       ParseOptions(project, m_Options);
//     }
// 
//     if (project.contains("Includes"))
//     {
//       for (const std::string& includes : project["Includes"])
//       {        
//         std::filesystem::path path = *g_Parameters.Find("-Root") + includes;
//         if (std::filesystem::exists(path) && std::filesystem::is_directory(path))
//         {
//           m_SourceDirectories.push_back(path.string());
//         }
//         else
//         {
//           throw_error<std::logic_error>("Failed to find includes path {}", path.string());
//         }
//       }
//     }
// 
//     if (project.contains("Type"))
//     {
//       const std::string& type = project["Type"];
//       if (type == "E")
//       {
//         m_Type = Executable;
//       }
//       else if (type == "S")
//       {
//         m_Type = StaticLibrary;
//       }
//       else if (type == "D")
//       {
//         m_Type = DynamicLibrary;
//       }
//       else
//       {
//         throw_error<std::logic_error>("Project compilation type is not supported expected 'ESD' but recieved {}.", type);
//       }
//     }
//     else
//     {
//       m_Type = StaticLibrary;
//     }
//   }
// 
//   const std::string& GetName() const
//   {
//     return m_Name;
//   }
// 
//   const std::vector<uint32_t>& GetDependencies() const
//   {
//     return m_Dependencies;
//   }
// 
//   const std::vector<uint32_t>& GetDefines() const
//   {
//     return m_Defines;
//   }
// 
//   const std::vector<uint32_t>& GetOptions() const
//   {
//     return m_Options;
//   }
// 
//   const std::vector<std::string>& GetIncludes() const
//   {
//     return m_SourceDirectories;
//   }
// 
//   const std::vector<Source>& GetSources() const
//   {
//     return m_Sources;
//   }
// 
//   const std::string& GetPrecompileHeader() const
//   {
//     return m_PrecompileHeader;
//   }
// 
//   bool NeedsCompilation() const
//   {
//     return m_Status == ProjectStatus::NotCompiled;
//   }
// 
//   void AddSource(const std::string& source)
//   {
//     std::string filename = std::filesystem::path(source).filename().replace_extension(".obj").string();
//     std::string output = *g_Parameters.GetIntermediatePath() + m_Name + "\\" + filename;
// 
//     std::lock_guard<std::mutex> _(*m_DataMutex.get());
//     m_Sources.emplace_back(source, output);
//   }
// 
//   void AddSourceDirectory(const std::string& directory)
//   {
//     m_SourceDirectories.push_back(directory);
//   }
// protected:
//   std::string m_Name;
//   std::vector<std::string> m_Libraries;
//   std::vector<std::string> m_SourceDirectories;
//   std::vector<Source> m_Sources;
//   std::vector<uint32_t> m_Dependencies;
//   std::vector<uint32_t> m_Defines;
//   std::vector<uint32_t> m_Options;
//   std::string m_PrecompileHeader;
//   ProjectStatus m_Status;
//   ProjectCompilationType m_Type;
// 
//   std::unique_ptr<std::mutex> m_DataMutex;
// };
// 
// class ResourceManager
// {
// public:
//   int32_t AddDefine(const std::string& name, const std::string& value, const std::string* platforms, const std::string* targets)
//   {
//     if (targets && !g_ActiveTarget->Match(*targets)) return -1;
//     if (platforms && !g_ActivePlatform->Match(*platforms)) return -1;
//     m_Defines.emplace_back(name, value);
//     return m_Defines.size() - 1;
//   }
// 
//   int32_t AddOption(const std::string& name, const std::string& value, const std::string* platforms, const std::string* targets)
//   {
//     if (targets && !g_ActiveTarget->Match(*targets)) return -1;
//     if (platforms && !g_ActivePlatform->Match(*platforms)) return -1;
//     m_Options.emplace_back(name, value);
//     return m_Options.size() - 1;
//   }
// 
//   int32_t AddProject(const nlohmann::json& description)
//   {
//     m_Projects.emplace_back(description, m_Projects.size());
//     return m_Projects.size() - 1;
//   }
// 
//   uint32_t GetProjectsCount() const
//   {
//     return m_Projects.size();
//   }
// 
//   int32_t FindProject(const std::string& name) const
//   {
//     for (int32_t i = 0; i < m_Projects.size(); ++i)
//     {
//       const Project& project = m_Projects[i];
//       if (project.GetName() == name)
//       {
//         return i;
//       }
//     }
// 
//     return -1;
//   }
// 
//   Project* GetProject(int32_t index)
//   {
//     if (index >= 0 && index < m_Projects.size())
//     {
//       return &m_Projects[index];
//     }
//     else
//     {
//       return nullptr;
//     }
//   }
// 
//   Project* GetProject(const std::string& name)
//   {
//     int32_t index = FindProject(name);
//     return index >= 0 ? &m_Projects[index] : nullptr;
//   }
// 
//   const std::vector<Project>& GetProjects() const
//   {
//     return m_Projects;
//   }
//   
//   const Define* GetDefine(int32_t index) const
//   {
//     if (index >= 0 && index < m_Defines.size())
//     {
//       return &m_Defines[index];
//     }
//     else
//     {
//       return nullptr;
//     }
//   }
// 
//   const Option* GetOption(int32_t index) const
//   {
//     if (index >= 0 && index < m_Options.size())
//     {
//       return &m_Options[index];
//     }
//     else
//     {
//       return nullptr;
//     }
//   }
// protected:
//   std::vector<Define> m_Defines;
//   std::vector<Option> m_Options;
//   std::vector<Project> m_Projects;
// } g_Resources;
// 
// class ValidationJob
// {
// public:
//   ValidationJob(const std::string& source, uint32_t projectIndex) : m_Source(source), m_ProjectIndex(projectIndex)
//   {}
// 
//   void Execute()
//   {
//     std::cout << "Validation job: " << m_Source << std::endl;
//     g_Resources.GetProject(m_ProjectIndex)->AddSource(m_Source);
//   }
// protected:
//   std::string m_Source;
//   uint32_t m_ProjectIndex;
// };
// 
// class JobManager
// {
// public:
//   void AddValidationJob(const std::string& source, uint32_t projectIndex)
//   {
//     if (source.size())
//     {
//       m_ValidationJobs.emplace_back(source, projectIndex);
//     }
//     else
//     {
//       throw_error<std::logic_error>("Can not create validation job for empty source");
//     }
//   }
// 
//   void RunValidationJobs(uint32_t threads)
//   {
//     Run(m_ValidationJobs, threads);  
//   }
// protected:
//   template <typename TaskClass>
//   void Run(std::vector<TaskClass>& tasks, uint32_t count)
//   {
//     std::atomic_uint32_t taskIndex;
//     auto task = [&tasks, &taskIndex]()
//     {
//       uint32_t index = taskIndex.fetch_add(1);
// 
//       while (index < tasks.size())
//       {
//         tasks[index].Execute();
//         index = taskIndex.fetch_add(1);
//       }
//     };
// 
//     std::vector<std::thread> threads;
//     for (uint32_t i = 0; i < count; ++i)
//     {
//       std::thread thread(task);
//       threads.push_back(std::move(thread));
//     }
// 
//     for (std::thread& thread : threads)
//     {
//       thread.join();
//     }
//   }
//   
// protected:
//   std::vector<ValidationJob> m_ValidationJobs;
// } g_Jobs;
// 
// 
// void ParseDependencies(const nlohmann::json& json, std::vector<uint32_t>& indices)
// {
//   if (!json.contains("Dependencies")) return;
// 
//   for (const std::string& dependencie : json["Dependencies"])
//   {
//     const int32_t index = g_Resources.FindProject(dependencie);
//     const Project* project = g_Resources.GetProject(index);
//     if (project)
//     {
//       indices.push_back(index);
//       
//       for (uint32_t index : project->GetDependencies())
//       {
//         indices.push_back(index);
//       }
//     }
//     else
//     {
//       throw_error<std::logic_error>("Failed to find dependencie {}", dependencie);
//     }
//   }
// }
// 
// void ParseDefines(const nlohmann::json& json, std::vector<uint32_t>& indices)
// {
//   if (!json.contains("Defines")) return;
//   
//   for (const nlohmann::json& define : json["Defines"])
//   {
//     const std::string* platforms = GetIfPresent("Platforms", define);
//     const std::string* targets = GetIfPresent("Targets", define);
//     int32_t index = g_Resources.AddDefine(define["Name"], define["Value"], platforms, targets);
//     // TODO: I am not sure if it makes sense but we could do some deduplication here if we will see many duplicates among the projects.
//     if (index >= 0)
//     {
//       indices.push_back(index);
//     }
//   }
// }
// 
// void ParseOptions(const nlohmann::json& json, std::vector<uint32_t>& indices)
// {
//   if (!json.contains("Options")) return;
//   
//   for (const nlohmann::json& option : json["Options"])
//   {
//     const std::string* platforms = GetIfPresent("Platforms", option);
//     const std::string* targets = GetIfPresent("Targets", option);
//     int32_t index = g_Resources.AddOption(option["Name"], option["Value"], platforms, targets);
//     // TODO: I am not sure if it makes sense but we could do some deduplication here if we will see many duplicates among the projects.
//     if (index >= 0)
//     {
//       indices.push_back(index);
//     }
//   }
// }
// 
// void AddDefines(const std::vector<uint32_t>& defines, std::string& suffix)
// {
//   // Could be better to assemble them right away.
//   // But could need some kind of class or method that
//   // would get all the needed data in a proper command parts.
//   for (uint32_t index : defines)
//   {
//     const Define* define = g_Resources.GetDefine(index);
// 
//     suffix += " -D" + define->Name;
//     if (define->Value.size())
//     {
//       suffix += "=" + define->Value;
//     }
//   }
// }
// 
// void AddOptions(const std::vector<uint32_t>& options, std::string& suffix)
// {
//   for (uint32_t index : options)
//   {
//     const Option* option = g_Resources.GetOption(index);
// 
//     if (option->Name == "C++")
//     {
//       if (option->Value == "23")
//       {
//         suffix += " -std=c++23 ";
//       }
//       else
//       {
//         throw_error<std::logic_error>("C++23 is only supported C++ option.");
//       }
//     }
//     else if (option->Name == "DisableWarnings")
//     {
//       if (option->Value == "1")
//       {
//         suffix += " -w ";
//       }
//       else if (option->Value == "0")
//       {
//         
//       }
//       else
//       {
//         throw_error<std::logic_error>("DisableWarnings options can be only 0 or 1 but provided {}.", option->Value);
//       }
//     }
//     else
//     {
//       throw_error<std::logic_error>("Unsupported compiler option {}={}.", option->Name, option->Value);
//     }
//   }
// }
// 
// void AddIncludes(const std::vector<std::string>& includes, std::string& suffix)
// {
//   for (const std::string& include : includes)
//   {
//     suffix += " -I " + include; 
//   }
// }
// 
// // bool DoesFileNeedsRecompilation(const std::string& file, const std::string& output)
// // {
// //   std::filesystem::path outputPath = output;
// //   if (!std::filesystem::exists(outputPath))
// //   {
// //     return true;
// //   }
// 
// //   std::chrono::time_point<std::chrono::file_clock> compiledTime = std::filesystem::last_write_time(outputPath);
// //   std::chrono::time_point<std::chrono::file_clock> editTime = std::filesystem::last_write_time(file);
// //   return editTime > compiledTime;
// // }
// 
// // Shitty designe here to pass both, that is why there should be no such separations.
// void ParseSources(const nlohmann::json& description, Project* project, uint32_t projectIndex)
// {
//   for (const std::string& source : description["Sources"])
//   {
//     std::filesystem::path path = *g_Parameters.Find("-Root") + source;
// 
//     if (!std::filesystem::exists(path))
//     {
//       throw_error<std::logic_error>("Cound't find a source {}", path.string());
//     }
// 
//     if (std::filesystem::is_directory(path))
//     {
//       for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(path))
//       {
//         const std::filesystem::path& path = entry.path();
//         if (path.extension().compare(".cpp") == 0)
//         {
//           g_Jobs.AddValidationJob(path.string(), projectIndex);
//         }
//       }
// 
//       project->AddSourceDirectory(path.string());
//     }
//     else
//     {
//       g_Jobs.AddValidationJob(path.string(), projectIndex);
//     }
//   }
// }
// 
// void RunCompilations(const std::vector<std::string>& commands, uint32_t threads)
// {
//   std::atomic_uint32_t pendingCommandIndex;
//   auto task = [&pendingCommandIndex, &commands]()
//   {
//     uint32_t index = pendingCommandIndex.fetch_add(1);
// 
//     while (index < commands.size())
//     {
//       const std::string& command = commands[index];
// 
//       std::println("[{}] Compiling {}", index, command);
// 
//       int code = system(command.c_str());
//       if (code != 0)
//       {
//         std::println("[{}] Compilation failed {}", index, command);
//         exit(code);
//       }
//       else
//       {
//         std::println("[{}] Compilation succeeded {}", index, command);
//       }
// 
//       index = pendingCommandIndex.fetch_add(1);
//     }
//   };
// 
//   std::chrono::time_point<std::chrono::system_clock> startTime = std::chrono::system_clock::now();
// 
//   std::vector<std::thread> compilations;
//   for (uint32_t i = 0; i < threads; ++i)
//   {
//     std::thread thread(task);
//     compilations.push_back(std::move(thread));
//   }
// 
//   for (std::thread& thread : compilations)
//   {
//     thread.join();
//   }
// 
//   std::chrono::time_point<std::chrono::system_clock> endTime = std::chrono::system_clock::now();
// 
//   float compilationTimeSeconds = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count() / 1000000.0f;
//   std::println("Compilation took {} seconds", compilationTimeSeconds);
// }
//const std::string* GetIfPresent(const std::string& name, const nlohmann::json& json)
//{
//  return json.contains(name) ? json[name].get_ptr<const std::string*>() : nullptr;
//}
/*

    class consumer
    {
    public:
      consumer(const project_configuration& project) : project(project), consumed_subprojects(0)
      { }

      bool can_consume() const
      {
        return consumed_subprojects < project.subprojects.size();
      }

      const subproject_configuration& consume()
      {
        return project.subprojects[consumed_subprojects++];
      }

    private:
      const project_configuration& project;
      std::size_t consumed_subprojects;
    } consumer_;

    class context
    {
      bool project_parsed;
      bool subprojects_parsed;
      bool project_interpreted;
    } context_;

    while (!context_.subprojects_parsed)
    {
      while (consumer_.can_consume())
      {

      }
    }
*/

/*estd::log("\nParse test is starts here: ");
while (reader.is_subproject_valid())
{
  estd::log("Parsing project: {}.", reader.project_name()->c_str());

  while (reader.has_next_option())
  {
    std::optional<input_reader::option_data> option = reader.next_option();
    if (!option.has_value())
    {
      estd::log("Options was not parsed!");
    }
    else
    {
      estd::log("Option [{}]=[{}].", option.value().name->c_str(), option.value().value->c_str());
    }
  }

  while (reader.has_next_define())
  {
    std::optional<input_reader::define_data> define = reader.next_define();
    if (!define.has_value())
    {
      estd::log("Define was not parsed!");
    }
    else
    {
      estd::log("Define [{}]=[{}].", define.value().name->c_str(), define.value().value->c_str());
    }
  }

  reader.next_subproject();
}
estd::log("Parse test has ended here.\n");*/
//       estd::log("Includes: ");
//       for (const auto& include : subprojcet.includes)
//       {
//         estd::log("{}", include.c_str());
//       }
// 
//       estd::log("Sources: ");
//       for (const auto& source : subprojcet.sources)
//       {
//         estd::log("{}", source.c_str());
//       }

// 
//     std::string name = "C++";
//     std::string value = "11";
// 
//     input_reader::option_data data = { };
//     data.name = &name;
//     data.value = &value;

//     option_description option = parser.parse_option(data).value();
//     estd::log("Option: {}.", static_cast<uint32_t>(option.type));
// 

// 
//     command_string test_defines = interpreter.convert_single(define_description{ "TEST", "VALUE" });
//     estd::log("Defines test = [{}]", test_defines.c_str());
//  
//     command_string test_options = interpreter.convert_single(option_description{ compiler_options::language_standard, "17" });
//     estd::log("Options test = [{}]", test_options.c_str());
//  
//     command_string combined_test;
//     interpreter.extend_list(define_description{ "TEST", "VALUE" }, combined_test);
//     interpreter.extend_list(option_description{ compiler_options::language_standard, "17" }, combined_test);
//     estd::log("Combined test = [{}]", combined_test.c_str());
//     estd::log("Combined size = [{}]", test_defines.size() + test_options.size() + combined_test.size());
// // 
//     project_configuration project;
//     project.subprojects.emplace_back( option_descriptions { { compiler_options::waringings_level, "default" } }, define_descriptions { { "TEST1", "VALUE1"} });
//     project.subprojects.emplace_back( option_descriptions { { compiler_options::waringings_level, "none" } }, define_descriptions { { "TEST2", "VALUE2" } });
//     project.subprojects.emplace_back( option_descriptions { { compiler_options::language_standard, "20" } }, define_descriptions { { "TEST3", "VALUE3" } });
// 
//     //option_description option = parse_option(instructions["Global"]["Options"][0]).value();
// 
//     project.options.push_back(option);
// 
     //command_string extended_combined_test;
     //interpreter.extend_list(project, extended_combined_test);
     //estd::log("Extneded combined test = [{}]", extended_combined_test.c_str());
// 
//   
//   string global_deinfes;
//   vector<Define> local_defines;
// 
//   string global_options;
//   vector<string> local_options;
// 
//   class compiler_state_parser
//   {
//     void parse(string key, string value, string sink)
//     {
//       sink.append(convert_key(key));
//       sink.push_back(' ');
//       sink.append(conver_value(value));
//     }
//   };
// 
//   class compiler_state
//   {
//     compiler_state(parser);
// 
//     void push_global(string key, string value)
//     {
//       g_parser.parse(type, key, value, global_options);
//     }
// 
//     void push_local(string key, string value, uint32_t local_index)
//     {
//       g_parser.parse(type, key, value, local_state[local_index]);
//     }
// 
//     state_type type;
//     string global_state;
//     vector<string> local_state;
//   };

//   nlohmann::json instructions;
// 
//  
//   try
//   {
//     instructions = ReadJson("instructions.json");
//   }
//   catch(const std::exception& exception)
//   {
//     std::cout << exception.what();
//     return 1;
//   }
// 
//   std::vector<uint32_t> globalDefines;
//   std::vector<uint32_t> globalOptions;
//   if (instructions.contains("Global"))
//   {
//     const nlohmann::json& globlas = instructions["Global"];
//     ParseDefines(globlas, globalDefines);
//     ParseOptions(globlas, globalOptions);
//   }
// 
//   try
//   {
//     for (const nlohmann::json& description : instructions["Projects"])
//     {
//       g_Resources.AddProject(description);
//     }
//   }
//   catch(const std::exception& exception)
//   {
//     std::cout << exception.what();
//     return 1;
//   }
// 
//   for (const Project& project : g_Resources.GetProjects())
//   {
//     const std::filesystem::path path = *g_Parameters.GetIntermediatePath() + project.GetName();
//     std::filesystem::create_directory(path);
//   }
// 
//   g_Jobs.RunValidationJobs(g_Parameters.GetThreadsCount());
//   
//   std::vector<std::string> precompileHeadersCommands;
//   std::vector<std::string> compilationCommands;
//   for (const Project& project : g_Resources.GetProjects())
//   {
//     if (project.NeedsCompilation())
//     {
//       std::string prefix = "clang++ -c ";
//       std::string suffix = " ";
// 
//       AddDefines(globalDefines, suffix);
//       AddDefines(project.GetDefines(), suffix);
//       AddIncludes(project.GetIncludes(), suffix);
// 
//       for (uint32_t index : project.GetDependencies())
//       {
//         const Project* dependencie = g_Resources.GetProject(index);
//         AddIncludes(dependencie->GetIncludes(), suffix);
//       }
// 
//       AddOptions(globalOptions, suffix);
//       AddOptions(project.GetOptions(), suffix);
// 
//       const std::string& precompileHeader = project.GetPrecompileHeader();
//       if (precompileHeader.size())
//       {
//         const std::string filename = std::filesystem::path(precompileHeader).filename().replace_extension(".pch").string();
//         const std::string output = *g_Parameters.GetIntermediatePath() + project.GetName() + "\\" + filename;
//         precompileHeadersCommands.push_back(prefix + " -o " + output + " " + precompileHeader + suffix);
//         prefix += "-include-pch " + output + " ";
//       }
// 
//       for (const Source& source : project.GetSources())
//       {
//         compilationCommands.push_back(prefix + source.Path + " -o " + source.Output + suffix);
//       }
//     }
//     else
//     {
//       // TODO: Add copy command for the libs.
//     }
//   }
// 
//   RunCompilations(precompileHeadersCommands, g_Parameters.GetThreadsCount());
//   RunCompilations(compilationCommands, g_Parameters.GetThreadsCount());
