#pragma once

#include "input_reader.h"
#include "estd/json.h"

class json_reader : public input_reader
{
public:
  json_reader(const estd::json& data)
    : data(data),
    subproject_index(0),
    option_index(0), define_index(0),
    source_index(0), include_index(0),
    dependency_index(0),
    build_index(0)
  {
  }

  virtual bool has_next_option() const override
  {
    const estd::json& project = get_current_project();
    if (!project.contains("Options")) return false;

    const estd::json& options = project["Options"];
    return options.size() > option_index;
  }

  virtual std::optional<option_data> next_option() override
  {
    const estd::json& project = get_current_project();
    if (!project.contains("Options")) return std::nullopt;

    const estd::json& options = project["Options"];
    if (option_index >= options.size()) return std::nullopt;

    const estd::json& option_object = options[option_index];
    ++option_index;

    const std::string* platforms = estd::fetch_value<std::string>(option_object, "Platforms");
    if (platforms && !active_platform()->match(platforms->c_str())) return std::nullopt;

    const std::string* targets = estd::fetch_value<std::string>(option_object, "Targets");
    if (targets && !active_target()->match(targets->c_str())) return std::nullopt;

    const std::string* value = estd::fetch_value<std::string>(option_object, "Value");
    estd::assert_condition(value, "Failed to fetch Value parameter from an option.");

    const std::string* name = estd::fetch_value<std::string>(option_object, "Name");
    estd::assert_condition(name, "Failed to fetch Name parameter from an option.");

    return option_data{ name, value };
  }

  virtual bool has_next_define() const override
  {
    const estd::json& project = get_current_project();
    if (!project.contains("Defines")) return false;

    const estd::json& defines = project["Defines"];
    return defines.size() > define_index;
  }

  virtual std::optional<define_data> next_define() override
  {
    const estd::json& project = get_current_project();
    if (!project.contains("Defines")) return std::nullopt;

    const estd::json& defines = project["Defines"];
    if (define_index >= defines.size()) return std::nullopt;

    const estd::json& define_object = defines[define_index];
    ++define_index;

    const std::string* platforms = estd::fetch_value<std::string>(define_object, "Platforms");
    if (platforms && !active_platform()->match(platforms->c_str())) return std::nullopt;

    const std::string* targets = estd::fetch_value<std::string>(define_object, "Targets");
    if (targets && !active_target()->match(targets->c_str())) return std::nullopt;

    const std::string* value = estd::fetch_value<std::string>(define_object, "Value");
    estd::assert_condition(value, "Failed to fetch Value parameter from an define.");

    const std::string* name = estd::fetch_value<std::string>(define_object, "Name");
    estd::assert_condition(name, "Failed to fetch Name parameter from an define.");

    return define_data{ name, value };
  }

  virtual bool has_next_source() const override
  {
    if (subproject_index == 0) return false;

    const estd::json& project = get_current_project();
    if (!project.contains("Sources")) return false;

    const estd::json& sources = project["Sources"];
    return sources.size() > source_index;
  }

  virtual const std::string* next_source() override
  {
    if (subproject_index == 0) return nullptr;

    const estd::json& project = get_current_project();
    if (!project.contains("Sources")) return nullptr;

    const estd::json& sources = project["Sources"];
    return estd::fetch_value<std::string>(sources, source_index++);
  }

  virtual bool has_next_include() const override
  {
    if (subproject_index == 0) return false;

    const estd::json& project = get_current_project();
    if (!project.contains("Includes")) return false;

    const estd::json& includes = project["Includes"];
    return includes.size() > include_index;
  }

  virtual const std::string* next_include() override
  {
    if (subproject_index == 0) return nullptr;

    const estd::json& project = get_current_project();
    if (!project.contains("Includes")) return nullptr;

    const estd::json& includes = project["Includes"];
    return estd::fetch_value<std::string>(includes, include_index++);
  }

  virtual bool has_next_dependency() const override
  {
    if (subproject_index == 0) return false;

    const estd::json& project = get_current_project();
    if (!project.contains("Dependencies")) return false;

    const estd::json& dependencies = project["Dependencies"];
    return dependencies.size() > dependency_index;
  }

  virtual const std::string* next_dependency() override
  {
    if (subproject_index == 0) return nullptr;

    const estd::json& project = get_current_project();
    if (!project.contains("Dependencies")) return nullptr;

    const estd::json& dependencies = project["Dependencies"];
    return estd::fetch_value<std::string>(dependencies, dependency_index++);
  }

  virtual const std::string* precompile_header() const override
  {
    if (subproject_index == 0) return nullptr;

    const estd::json& project = get_current_project();
    if (!project.contains("PrecompileHeader")) return nullptr;

    return estd::fetch_value<std::string>(project, "PrecompileHeader");
  }

  virtual const std::string* artifact_type() const override
  {
    if (subproject_index == 0) return nullptr;

    const estd::json& project = get_current_project();
    return estd::fetch_value<std::string>(project, "ArtifactType");
  }

  virtual const std::string* resources() const override
  {
    if (subproject_index == 0) return nullptr;

    const estd::json& project = get_current_project();
    return estd::fetch_value<std::string>(project, "Resources");
  }

  virtual const std::string* static_library() const override
  {
    if (subproject_index == 0) return nullptr;

    const estd::json& project = get_current_project();
    if (!project.contains("Artifact")) return nullptr;

    const estd::json& artifact = project["Artifact"];
    return estd::fetch_value<std::string>(artifact, "StaticLibrary");
  }

  virtual const std::string* import_library() const override
  {
    if (subproject_index == 0) return nullptr;

    const estd::json& project = get_current_project();
    if (!project.contains("Artifact")) return nullptr;

    const estd::json& artifact = project["Artifact"];
    return estd::fetch_value<std::string>(artifact, "ImportLibrary");
  }

  virtual const std::string* dynamic_library() const override
  {
    if (subproject_index == 0) return nullptr;

    const estd::json& project = get_current_project();
    if (!project.contains("Artifact")) return nullptr;

    const estd::json& artifact = project["Artifact"];
    return estd::fetch_value<std::string>(artifact, "DynamicLibrary");
  }

  virtual const std::string* executable() const override
  {
    if (subproject_index == 0) return nullptr;

    const estd::json& project = get_current_project();
    if (!project.contains("Artifact")) return nullptr;

    const estd::json& artifact = project["Artifact"];
    return estd::fetch_value<std::string>(artifact, "Executable");
  }

  virtual const std::string* project_name() const override
  {
    const estd::json& project = subproject_index == 0 ? data : get_current_project();
    return estd::fetch_value<std::string>(project, "Name");
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

  virtual bool has_next_build() const override
  {
    return data["Builds"].size() >= build_index;
  }

  virtual bool next_build() override
  {
    ++build_index;
    return build_index <= data["Builds"].size();
  }

  virtual const std::string* build_name() const override
  {
    if (build_index == 0) return nullptr;

    const estd::json& builds = data["Builds"];
    if (build_index > builds.size()) return nullptr;

    const estd::json& build = builds[build_index - 1];
    return estd::fetch_value<std::string>(build, "Name");
  }

  virtual const std::string* build_subproject_name() const override
  {
    if (build_index == 0) return nullptr;

    const estd::json& builds = data["Builds"];
    if (build_index > builds.size()) return nullptr;

    const estd::json& build = builds[build_index - 1];
    return estd::fetch_value<std::string>(build, "Project");
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

  inline const estd::json& get_current_project() const
  {
    return subproject_index == 0 ? data["Global"] : data["Projects"][subproject_index - 1];
  }
protected:
  const estd::json& data;
  std::size_t subproject_index;
  std::size_t option_index;
  std::size_t define_index;
  std::size_t source_index;
  std::size_t include_index;
  std::size_t dependency_index;
  std::size_t build_index;
};