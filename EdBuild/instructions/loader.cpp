#include "EdBuild.h"
#include "loader.h"

namespace instructions
{
  void set_base_path_single(estd::json& source, const estd::path& base)
  {
    if (!source.is_string()) return;
    estd::path path = estd::fetch_c_str(source);

    const bool is_absolute = path.is_absolute();
    if (is_absolute) return;

    const bool succeeded = path.set_base(base);
    if (!succeeded) { estd::log("Failed to set base {} for {}.", base, path); return; }

    source = path.c_str();
  }

  void set_base_path_multiple(estd::json& paths_list, const estd::path& base)
  {
    for (auto& source : paths_list)
    {
      set_base_path_single(source, base);
    }
  }

  estd::json load(const estd::path& path, const estd::path& includes_absolute)
  {
    estd::json result = estd::read_json(path);

    auto& destination = result["Projects"];

    const auto& includes = result["Includes"];
    for (const auto& include : includes)
    {
      estd::assert_condition(include.is_string(), "Instructions include path must be a string.");

      const char* relative_path = estd::fetch_c_str(include);

      estd::path include_path;
      include_path
        .append(includes_absolute)
        .append(relative_path);

      estd::log("{}Detected include{}: {}.", estd::colors::green(), estd::colors::reset(), include_path);

      const auto include_instructions = estd::read_json(include_path);

      const bool has_projects = include_instructions.contains("Projects");
      if (!has_projects) { estd::log("Ignoring include because there are no projects inside."); continue; }

      const auto& projects = include_instructions["Projects"];
      if (!projects.is_array()) { estd::log("Ignoring include because projects are not in a correct format."); continue; }

      const auto start_index = destination.size();

      for (const auto& project : projects)
      {
        const char* project_name = estd::fetch_c_str(project, "Name");
        estd::assert_condition(project_name,
          "Every project in the include file [{}] must have a name, [{}] has it missing.",
          include_path, destination.size() - start_index);

        estd::log("Project added: {}.", project_name);
        destination.push_back(project);
      }

      const auto end_index = destination.size();

      estd::path base_path;
      base_path
        .append(include_path)
        .pop();

      for (std::size_t index = start_index; index < end_index; ++index)
      {
        auto& project = destination[index];

        set_base_path_multiple(project["Sources"], base_path);
        set_base_path_multiple(project["Includes"], base_path);

        const bool has_artifact = project.contains("Artifact");
        if (has_artifact)
        {
          auto& artifact = project["Artifact"];

          set_base_path_single(artifact["StaticLibrary"], base_path);
          set_base_path_single(artifact["DynamicLibrary"], base_path);
          set_base_path_single(artifact["ImportLibrary"], base_path);
        }

        const bool has_precompile_header = project.contains("PrecompileHeader");
        if (has_precompile_header)
        {
          set_base_path_single(project["PrecompileHeader"], base_path);
        }
      }
    }

    //estd::log("{}.", result.dump(2));

    return result;
  }

  estd::json load(const estd::path& path)
  {
    estd::path includes_absolute;
    includes_absolute
      .append(path)
      .pop();

    return load(path, includes_absolute);
  }
}
