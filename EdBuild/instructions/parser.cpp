#include "EdBuild.h"
#include "configurations/instructions.h"
#include "platforms_registry.h"
#include "targets_registry.h"

#include "parser.h"

namespace instructions
{
  using dependencies_list = std::vector<const char*>;
  using dependencies_lists = std::vector<dependencies_list>;
  using name_map = std::map<name_string, std::size_t>;

  struct parser_context
  {
    parser_context(const estd::path& project_path,
      const platform& active_platform, const target& active_target,
      const option_parsers_list& parsers)
      : project_path(project_path),
      active_platform(active_platform), active_target(active_target),
      parsers(parsers)
    { }

    const estd::path& project_path;

    const platform& active_platform;
    const target& active_target;
  
    const option_parsers_list& parsers;
  };

  void parse_options(const parser_context& context, const estd::json& source, options_list& destination)
  {
    const bool has_options = source.contains("Options");
    if (!has_options) return;

    const auto& options = source["Options"];
    if (!options.is_array()) return;

    destination.reserve(options.size());

    for (const auto& option : options)
    {
      const char* name = estd::fetch_c_str(option, "Name");
      estd::assert_condition(name, "Failed to fetch 'Name' parameter from an option.");

      const char* value = estd::fetch_c_str(option, "Value");
      estd::assert_condition(value, "Failed to fetch 'Value' parameter from an option [{}].", name);

      const char* platforms = estd::fetch_c_str(option, "Platforms");
      const bool platform_filtered_out = platforms && !context.active_platform.match(platforms);
      if (platform_filtered_out) continue;

      const char* targets = estd::fetch_c_str(option, "Targets");
      const bool target_filtered_out = targets && !context.active_target.match(targets);
      if (target_filtered_out) continue;

      destination.emplace_back();
      auto& parsed = destination.back();

      int8_t type = -1;
      for (const auto& parser : context.parsers)
      {
        type = static_cast<int8_t>(parser(std::string(name)));
        if (type >= 0) break;
      }

      estd::assert_condition(type >= 0, "Failed to parse an option [{}].", name);

      parsed.value = value;
      parsed.type = static_cast<builder_options>(type);
    }
  }

  void parse_defines(const parser_context& context, const estd::json& source, defines_list& destination)
  {
    const bool has_defines = source.contains("Defines");
    if (!has_defines) return;

    const auto& defines = source["Defines"];
    if (!defines.is_array()) return;

    destination.reserve(defines.size());

    for (const auto& define : defines)
    {
      const char* name = estd::fetch_c_str(define, "Name");
      estd::assert_condition(name, "Failed to fetch 'Name' parameter from a define.");

      const char* value = estd::fetch_c_str(define, "Value");
      estd::assert_condition(value, "Failed to fetch 'Value' parameter from a define [{}].", name);

      const char* platforms = estd::fetch_c_str(define, "Platforms");
      const bool platform_filtered_out = platforms && !context.active_platform.match(platforms);
      if (platform_filtered_out) continue;

      const char* targets = estd::fetch_c_str(define, "Targets");
      const bool target_filtered_out = targets && !context.active_target.match(targets);
      if (target_filtered_out) continue;

      destination.emplace_back();

      auto& parsed = destination.back();
      parsed.name = name;
      parsed.value = value;
    }
  }

  void parse_modifiers(const parser_context& context, const estd::json& source, modifiers_configuration& destination)
  {
    const bool has_modifiers = source.contains("Modifiers");
    if (!has_modifiers) return;

    const auto& modifiers = source["Modifiers"];
    if (!modifiers.is_array()) return;

    for (const auto& modifier : modifiers)
    {
      if (!modifier.is_object()) continue;

      const char* name = estd::fetch_c_str(modifier, "Name");
      estd::assert_condition(name, "No name was provided for a modifer [{}].", destination.options.size());

      options_modifier options;
      options.name = name;
      parse_options(context, modifier, options.options);

      defines_modifier defines;
      defines.name = name;
      parse_defines(context, modifier, defines.options);
    
      destination.options.push_back(std::move(options));
      destination.defines.push_back(std::move(defines));
    }
  }
  
  void parse_globals(const parser_context& context, const estd::json& source, project_configuration& destination)
  {
    const bool has_global_modifiers = source.contains("Globals");
    if (!has_global_modifiers) return;

    const auto& globals = source["Globals"];
    if (!globals.is_object()) return;

    parse_options(context, globals, destination.options);
    parse_defines(context, globals, destination.defines);
  }

  void parse_sources(const parser_context& context, const estd::json& source, sources_list& sources_destination, includes_list& includes_destination)
  {
    const bool has_sources = source.contains("Sources");
    if (!has_sources) return;

    const auto& sources = source["Sources"];
    if (!sources.is_array()) return;

    for (const auto& source : sources)
    {
      const char* source_path = estd::fetch_c_str(source);
      if (!source_path) continue;

      estd::path path = source_path;
      if (path.is_relative())
      {
        path = context.project_path;
        path.append(source_path);
      }

      estd::assert_condition(path.exists(), "Provided source path [{}] doesn't exist.", path);

#pragma message("C++ compiler hardcode.")
      const char* source_extension = ".cpp";

      if (path.is_directory())
      {
        includes_destination.push_back(path);

        for (const std::filesystem::directory_entry& entry : path.iterate_recursively())
        {
          const std::filesystem::path& path = entry.path();
          const bool extension_match = path.extension().compare(source_extension) == 0;
          if (extension_match) sources_destination.emplace_back(path.string().c_str());
        }
      }
      else
      {
        sources_destination.push_back(path);
      }
    }
  }

  void parse_includes(const parser_context& context, const estd::json& source, includes_list& destination)
  {
    const bool has_includes = source.contains("Includes");
    if (!has_includes) return;

    const auto& includes = source["Includes"];
    if (!includes.is_array()) return;

    for (const auto& include : includes)
    {
      const char* include_path = estd::fetch_c_str(include);
      if (!include_path) continue;

      estd::path path = include_path;
      if (path.is_relative())
      {
        path = context.project_path;
        path.append(include_path);
      }

      estd::assert_condition(path.exists(), "Provided include include path [{}] doesn't exist.", path);

      destination.push_back(std::move(path));
    }
  }

  void parse_precompile_header(const parser_context& context, const estd::json& source, compilable_description& destination)
  {
    const bool has_precompile_header = source.contains("PrecompileHeader");
    if (!has_precompile_header) return;

    const char* precompile_header_path = estd::fetch_c_str(source, "PrecompileHeader");
    if (!precompile_header_path) return;

    estd::path& path = destination.path;
    path
      .append(context.project_path)
      .append(precompile_header_path);

    estd::assert_condition(path.exists(), "Provided precompile header path [{}] doesn't exist.", path);
    estd::assert_condition(path.is_file(), "Provided precompile header path [{}] is not file.", path);
  }

  void parse_artifacts(const parser_context& context, const estd::json& source, const char* project_name, artifact_description& destination)
  {
    const bool is_artifact_preproduced = source.contains("Artifact");
    const char* artifact_type = estd::fetch_c_str(source, "ArtifactType");

    estd::assert_condition(int32_t(is_artifact_preproduced) + int32_t(bool(artifact_type)) != 2,
      "Provided preproduced artifacts while requested to produce artifact for the project [{}].", project_name);

    if (is_artifact_preproduced)
    {
      const auto& artifact = source["Artifact"];

      const char* static_library = estd::fetch_c_str(artifact, "StaticLibrary");
      const char* import_library = estd::fetch_c_str(artifact, "ImportLibrary");
      const char* dynamic_library = estd::fetch_c_str(artifact, "DynamicLibrary");
      const char* executable = estd::fetch_c_str(artifact, "Executable");

      destination.preproduced = true;

      const bool preproduced_static_library = static_library;
      const bool preproduced_dynamic_library = dynamic_library;
      const bool preproduced_executable = executable;

      estd::assert_condition(int32_t(preproduced_static_library) + int32_t(preproduced_dynamic_library) + int32_t(preproduced_executable) == 1,
        "Provided multiple incompetable preproduced artifacts for a project [{}].", project_name);

      if (preproduced_static_library) destination.type = artifact_types::static_library;
      if (preproduced_dynamic_library) destination.type = artifact_types::dynamic_library;
      if (preproduced_executable) destination.type = artifact_types::excutable;

      switch (destination.type)
      {
      case artifact_types::static_library:
      {
        destination.static_library().append(context.project_path);
        destination.static_library().append(static_library);
        break;
      }
      case artifact_types::dynamic_library:
      {
        if (import_library)
        {
          destination.import_library().append(context.project_path);
          destination.import_library().append(import_library);
        }

        destination.dynamic_library().append(context.project_path);
        destination.dynamic_library().append(dynamic_library);

        break;
      }
      case artifact_types::excutable:
      {
        destination.executable().append(context.project_path);
        destination.executable().append(executable);
        break;
      }
      default: estd::no_default("Failed to match preproduced artifact type [{}].", static_cast<int32_t>(destination.type)); break;
      }
    }

    if (artifact_type)
    {
      destination.preproduced = false;
      destination.type = artifact_types::static_library;

      const bool is_executable = std::strcmp(artifact_type, "Executable") == 0;
      const bool is_dynamic_library = std::strcmp(artifact_type, "DynamicLibrary") == 0;
      const bool is_static_library = std::strcmp(artifact_type, "StaticLibary") == 0;

      if (is_executable) destination.type = artifact_types::excutable;
      if (is_dynamic_library) destination.type = artifact_types::dynamic_library;
      if (is_static_library) destination.type = artifact_types::static_library;
    }

    if (!is_artifact_preproduced && !artifact_type)
    {
      destination.preproduced = false;
      destination.type = artifact_types::static_library;
    }

    const char* resources_path = estd::fetch_c_str(source, "Resources");
    if (resources_path)
    {
      destination.resources.append(context.project_path);
      destination.resources.append(resources_path);
    }
  }

  void parse_dependencies(const parser_context& context, const estd::json& source, dependencies_list& destination)
  {
    const bool has_dependencies = source.contains("Dependencies");
    if (!has_dependencies) return;

    const auto& dependencies = source["Dependencies"];
    if (!dependencies.is_array()) return;

    for (const auto& dependency : dependencies)
    {
      const char* dependency_name = estd::fetch_c_str(dependency);
      if (dependency_name) destination.push_back(dependency_name);
    }
  }

  void parse_projects(const parser_context& context, const estd::json& source, name_map& mapping, project_configuration& destination)
  {
    const bool has_projects = source.contains("Projects");
    if (!has_projects) return;

    const auto& projects = source["Projects"];
    if (!projects.is_array()) return;

    auto& subprojects = destination.subprojects;
    subprojects.reserve(projects.size());

    dependencies_lists subprojects_dependencies;

    for (const auto& project : projects)
    {
      if (!project.is_object()) continue;

      const char* name = estd::fetch_c_str(project, "Name");
      estd::assert_condition(name, "No name was provided for a project [{}].", subprojects.size());

      subprojects.emplace_back();

      auto& subproject = subprojects.back();
      subproject.name = name;

      mapping[name] = subprojects.size() - 1;

      parse_options(context, project, subproject.options);
      parse_defines(context, project, subproject.defines);

      parse_sources(context, project, subproject.sources, subproject.includes);
      parse_includes(context, project, subproject.includes);
      parse_precompile_header(context, project, subproject.precompile_header);

      parse_artifacts(context, project, name, subproject.artifact);

      subprojects_dependencies.emplace_back();
      dependencies_list& dependencies = subprojects_dependencies.back();

      parse_dependencies(context, project, dependencies);
    }

    const std::size_t subprojects_count = subprojects_dependencies.size();
    for (std::size_t subproject_index = 0; subproject_index < subprojects_count; ++subproject_index)
    {
      auto& subproject = subprojects[subproject_index];
      subproject.dependencies_count = subprojects_dependencies[subproject_index].size();

      for (const char* dependency_name : subprojects_dependencies[subproject_index])
      {
        estd::log("PP: {}, {}.", subprojects[subproject_index].name, dependency_name);

        const std::size_t dependency_index = mapping[dependency_name];
        auto& dependency_subrproject = subprojects[dependency_index];
        dependency_subrproject.dependants.push_back(subproject_index);
      }
    }
  }

  void parse_builds(const parser_context& context, const estd::json& source, const name_map& mapping, const subproject_configurations& projects, builds_configurations& destinations)
  {
    const bool has_builds = source.contains("Builds");
    if (!has_builds) return;

    const auto& builds = source["Builds"];
    if (!builds.is_array()) return;

    destinations.reserve(builds.size());
    for (const auto& build : builds)
    {
      if (!build.is_object()) continue;

      const char* name = estd::fetch_c_str(build, "Name");
      estd::assert_condition(name, "No name was provided for the build [{}].", destinations.size());

      const char* project = estd::fetch_c_str(build, "Project");
      estd::assert_condition(project, "No project was provided for the build [{}].", name);
      estd::assert_condition(mapping.count(project), "No project [{}] was found as a base for build [{}].", project, name);

      destinations.emplace_back();

      const auto project_index = mapping.at(project);

      auto& destination = destinations.back();
      destination.name = name;
      destination.subproject = &projects[project_index];
      
      parse_options(context, build, destination.options);
      parse_defines(context, build, destination.defines);
    }
  }

  description parse(const parser_inputs& inputs)
  {
    estd::log("Parsing instructions.");

    const auto& source = inputs.source;

    description result;

    project_configuration& project = result.project;
    builds_configurations& builds = result.builds;
    modifiers_configuration& modifiers = result.modifiers;

    std::map<name_string, std::size_t> name_mapping;
    dependencies_lists subprojects_dependencies;

    parser_context context{ inputs.project_path, *inputs.active_platform, *inputs.active_target, inputs.option_parsers };

    parse_modifiers(context, source, modifiers);
    parse_globals(context, source, project);

    name_map mapping;
    parse_projects(context, source, mapping, project);
    parse_builds(context, source, mapping, project.subprojects, builds);

    return result;
  }
}

