#pragma once

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

class filter_status
{
  enum
  {
    object_files_is_not_present = 1,
    dependencies_were_updated,
    builder_was_updated,
    compilable_was_updated
  };
public:
  filter_status() : status(0) {}
  filter_status(const filter_status& new_status) : status(new_status.status) {}
  filter_status(filter_status&& new_status) : status(new_status.status)
  {
    new_status.status = 0;
  }

  uint16_t get_status() const
  {
    return status;
  }

  std::string get_reason() const
  {
    switch (status)
    {
    case object_files_is_not_present: return "Object file is not present.";
    case dependencies_were_updated: return "Dependencies were updated.";
    case builder_was_updated: return "Builder was updated.";
    case compilable_was_updated: return "Compilable file itself was updated.";
    default:
      return "None";
    }
  }

  bool is_not_filtered() const
  {
    return status == 0;
  }

  bool is_filtered() const
  {
    return status;
  }

  operator bool() const
  {
    return status;
  }

  filter_status& operator=(uint16_t new_status)
  {
    status = new_status;
    return *this;
  }

  filter_status& operator=(const filter_status& new_status)
  {
    status = new_status.status;
    return *this;
  }

  filter_status& operator=(filter_status&& new_status)
  {
    status = new_status.status;
    new_status.status = 0;
    return *this;
  }

  void set_object_files_is_not_present()
  {
    status = object_files_is_not_present;
  }

  void set_dependencies_were_updated()
  {
    status = dependencies_were_updated;
  }

  void set_builder_was_updated()
  {
    status = builder_was_updated;
  }

  void set_compilable_was_updated()
  {
    status = compilable_was_updated;
  }
private:
  uint16_t status;
};

struct compilable_description
{
  std::string path;
  filter_status status;

  compilable_description() : path(), status() {}

  template <typename string_type>
  compilable_description(const string_type& path) : path(path), status() {}

  compilable_description(const compilable_description& other) : path(other.path), status(other.status)
  { }

  compilable_description(compilable_description&& other) : path(std::move(other.path)), status(std::move(other.status))
  { }

  compilable_description& operator=(const compilable_description& other)
  {
    path = other.path;
    status = other.status;
    return *this;
  }

  compilable_description& operator=(compilable_description&& other)
  {
    path = std::move(other.path);
    status = std::move(other.status);
    return *this;
  }

  bool is_present() const
  {
    return path.size();
  }

  bool is_not_present() const
  {
    return path.empty();
  }

  const char* get_c_path() const
  {
    return path.c_str();
  }
};

using option_descriptions = std::vector<option_description>;
using define_descriptions = std::vector<define_description>;
using source_files = std::vector<compilable_description>;
using include_files = std::vector<std::string>;
using dependats_list = std::vector<std::size_t>;

using dependant_subprojects_list = std::vector<std::size_t>;
using ownership_list = std::vector<std::size_t>;

enum class artifact_types : uint8_t
{
  excutable = 0,
  static_library,
  dynamic_library
};

const char* get_type_name(artifact_types type)
{
  switch (type)
  {
  case artifact_types::excutable: return "Executable";
  case artifact_types::static_library: return "StaticLibrary";
  case artifact_types::dynamic_library: return "DynamicLibrary";
  default: return "None";
  }
}

struct artifact_description
{
  // Workaround, because you can not make a nice union with std::strings.
  std::string field1;
  std::string field2;
  std::string resources;

  artifact_types type;
  bool preproduced;

  const std::string& input() const { return type == artifact_types::static_library ? static_library() : import_library(); }
  const std::string& output() const { return field2; }

  const std::string& import_library() const { return field1; };
  std::string& import_library() { return field1; };

  void import_library(const std::string& path) { field1 = path; }
  void import_library(std::string&& path) { field1 = std::move(path); }

  const std::string& dynamic_library() const { return field2; };
  std::string& dynamic_library() { return field2; };

  void dynamic_library(const std::string& path) { field2 = path; }
  void dynamic_library(std::string&& path) { field2 = std::move(path); }

  const std::string& static_library() const { return field2; };
  std::string& static_library() { return field2; };

  void static_library(const std::string& path) { field2 = path; }
  void static_library(std::string&& path) { field2 = std::move(path); }

  const std::string& executable() const { return field2; };
  std::string& executable() { return field2; };

  void executable(const std::string& path) { field2 = path; }
  void executable(std::string&& path) { field2 = std::move(path); }
};

using artifact_dependencies_list = std::vector<artifact_description>;

struct subproject_configuration
{
  std::string name;

  option_descriptions options;
  define_descriptions defines;

  source_files sources;
  include_files includes;
  compilable_description precompile_header;

  dependant_subprojects_list dependants;
  artifact_dependencies_list artifact_dependencies;

  std::size_t rank;
  std::size_t original_rank;
  std::size_t dependencies_count;

  artifact_description artifact;

  bool has_precompile_header() const { return precompile_header.is_present(); }

  bool is_preproced() const { return artifact.preproduced; }
  bool produces_artifact() const { return !artifact.preproduced; }
  bool preproduced_artifact() const { return artifact.preproduced; }

  std::size_t get_compilables_count() const
  {
    if (is_preproced()) return 0;
    return sources.size() + has_precompile_header();
  }
};

using subproject_configurations = std::vector<subproject_configuration>;

struct compilable_view
{
  const char* path = nullptr;
  const char* extension = nullptr;

  const char* subproject_name = nullptr;
  std::size_t subproject_index = -1;
  std::size_t compilable_index = -1;

  filter_status* status;

  bool is_source = false;
  bool has_precompile_header = false;

  operator bool() const
  {
    return !!path;
  }
};

struct build_configuration
{
  std::string name;
  std::string subproject_name;
};

using build_configurations = std::vector<build_configuration>;

struct project_configuration
{
  std::string name;

  subproject_configurations subprojects;
  option_descriptions options;
  define_descriptions defines;

  build_configurations builds;

  bool has_precompile_header(std::size_t subproject_index) const
  {
    estd::assert_condition(subproject_index < subprojects.size(), "Attempted to check out of bound subproject [{}] for precompile header, subprojects [{}].", subproject_index, subprojects.size());
    return subprojects[subproject_index].has_precompile_header();
  }

  compilable_view get_source_view(std::size_t subproject_index, std::size_t source_index) const
  {
    compilable_view result{ };

    if (subproject_index < subprojects.size())
    {
      auto& subproject = subprojects[subproject_index];
      if (source_index < subproject.sources.size())
      {
#pragma message("Plaftform dependant code.")
        result.path = subproject.sources[source_index].get_c_path();
        result.extension = ".obj";

        result.subproject_name = subproject.name.c_str();
        result.subproject_index = subproject_index;
        result.compilable_index = source_index + 1;

        result.status = nullptr;

        result.is_source = true;
        result.has_precompile_header = subproject.precompile_header.is_not_present();
      }
    }

    return result;
  }

  compilable_view get_source_view(std::size_t subproject_index, std::size_t source_index)
  {
    compilable_view result = std::as_const(*this).get_source_view(subproject_index, source_index);

    if (subproject_index < subprojects.size())
    {
      auto& subproject = subprojects[subproject_index];
      if (source_index < subproject.sources.size())
      {
        result.status = &subproject.sources[source_index].status;
      }
    }

    return result;
  }

  compilable_view get_precompile_header_view(std::size_t subproject_index) const
  {
    compilable_view result{ };

    if (subproject_index < subprojects.size())
    {
      auto& subproject = subprojects[subproject_index];
      if (subproject.precompile_header.is_present())
      {
#pragma message("Plaftform dependant code.")
        result.path = subproject.precompile_header.get_c_path();
        result.extension = ".pch";

        result.subproject_name = subproject.name.c_str();
        result.subproject_index = subproject_index;
        result.compilable_index = 0;

        result.status = nullptr;

        result.is_source = false;
        result.has_precompile_header = false;
      }
    }

    return result;
  }

  compilable_view get_precompile_header_view(std::size_t subproject_index)
  {
    compilable_view result = std::as_const(*this).get_precompile_header_view(subproject_index);

    if (subproject_index < subprojects.size())
    {
      auto& subproject = subprojects[subproject_index];
      if (subproject.precompile_header.is_present())
      {
        result.status = &subproject.precompile_header.status;
      }
    }

    return result;
  }

  class compilables_forward_iterator final
  {
  public:
    using difference_type = int64_t;
    using value_type = compilable_view;

    compilables_forward_iterator() : project(nullptr), subproject_index(0), compilable_index(0)
    {
    }

    compilables_forward_iterator(project_configuration* project)
      : project(project)
    {
      move_to_first_item(0);
    }

    compilables_forward_iterator(project_configuration* project, std::size_t subproject_index)
      : project(project), subproject_index(subproject_index)
    {
      move_to_first_item(subproject_index);
    }

    compilables_forward_iterator(project_configuration* project, std::size_t subproject_index, std::size_t compilable_index)
      : project(project), subproject_index(subproject_index), compilable_index(compilable_index)
    {
    }

    compilables_forward_iterator(const compilables_forward_iterator& other)
      : project(other.project), subproject_index(other.subproject_index), compilable_index(other.compilable_index)
    {
    }

    compilables_forward_iterator& operator=(const compilables_forward_iterator& other)
    {
      project = other.project;
      subproject_index = other.subproject_index;
      compilable_index = other.compilable_index;
      return *this;
    }

    compilable_view operator*() const
    {
      //estd::log("[{}] [{}] [{}].", subproject_index, project->subprojects[subproject_index].name.c_str(), compilable_index);

      estd::assert_condition(subproject_index < project->subprojects.size(), "Compilables: Subproject index [{}] is out of the bound [{}].", subproject_index, project->subprojects.size());

      const auto& subproject = project->subprojects[subproject_index];
      const bool has_precompile_header = subproject.precompile_header.is_present();
      estd::assert_condition(compilable_index != 0 || has_precompile_header, "Compilable index [{}] is not available as subproject [{}] has no precompile headers.", compilable_index, subproject.name.c_str());

      const std::size_t compilables_count = subproject.sources.size() + 1;
      estd::assert_condition(compilable_index < compilables_count, "Compilable index [{}] is out of the bounds in subrpoject [{}] with compiblables count [{}].", compilable_index, subproject.name.c_str(), compilables_count);

      compilable_view view;

      if (compilable_index == 0) view = project->get_precompile_header_view(subproject_index);
      else view = project->get_source_view(subproject_index, compilable_index - 1);

      return view;
    }

    compilable_view operator*()
    {
      //estd::log("[{}] [{}] [{}].", subproject_index, project->subprojects[subproject_index].name.c_str(), compilable_index);

      estd::assert_condition(subproject_index < project->subprojects.size(), "Compilables: Subproject index [{}] is out of the bound [{}].", subproject_index, project->subprojects.size());

      const auto& subproject = project->subprojects[subproject_index];
      const bool has_precompile_header = subproject.precompile_header.is_present();
      estd::assert_condition(compilable_index != 0 || has_precompile_header, "Compilable index [{}] is not available as subproject [{}] has no precompile headers.", compilable_index, subproject.name.c_str());

      const std::size_t compilables_count = subproject.sources.size() + 1;
      estd::assert_condition(compilable_index < compilables_count, "Compilable index [{}] is out of the bounds in subrpoject [{}] with compiblables count [{}].", compilable_index, subproject.name.c_str(), compilables_count);

      compilable_view view;

      if (compilable_index == 0) view = project->get_precompile_header_view(subproject_index);
      else view = project->get_source_view(subproject_index, compilable_index - 1);

      return view;
    }

    compilables_forward_iterator& operator++()
    {
      estd::assert_condition(subproject_index < project->subprojects.size(), "Compilables: Subproject index [{}] is out of the bound [{}].", subproject_index, project->subprojects.size());
      const auto& subproject = project->subprojects[subproject_index];

      const bool only_precompile_header = compilable_index == 0 && subproject.sources.empty();
      const bool all_sources = compilable_index == subproject.sources.size();
      const bool exhausted_compilables = only_precompile_header || all_sources;

      //estd::log("[{}] [{}] [{}] [{}]", subproject.name.c_str(), compilable_index, subproject.sources.size(), exhausted_compilables);

      if (exhausted_compilables)
      {
        while (subproject_index < project->subprojects.size())
        {
          ++subproject_index;

          if (subproject_index < project->subprojects.size())
          {
            const auto& next_subrpoject = project->subprojects[subproject_index];
            if (next_subrpoject.is_preproced()) continue;

            const bool has_compilables = next_subrpoject.get_compilables_count();
            if (has_compilables) break;
          }
        }

        if (subproject_index < project->subprojects.size())
        {
          const auto& next_subrpoject = project->subprojects[subproject_index];
          compilable_index = next_subrpoject.precompile_header.is_not_present();
        }
        else
        {
          compilable_index = 0;
        }
      }
      else
      {
        ++compilable_index;
      }

      return *this;
    }

    compilables_forward_iterator operator++(int)
    {
      compilables_forward_iterator result = *this;
      this->operator++();
      return result;
    }

    bool operator==(const compilables_forward_iterator& other) const
    {
      return project == other.project && subproject_index == other.subproject_index && compilable_index == other.compilable_index;
    }
  private:
    void move_to_first_item(std::size_t subrpoject_start_index)
    {
      for (std::size_t index{ subrpoject_start_index }; index < project->subprojects.size(); ++index)
      {
        const auto& subproject = project->subprojects[index];

        if (subproject.precompile_header.is_present())
        {
          subproject_index = index;
          compilable_index = 0;
          return;
        }

        if (subproject.sources.size())
        {
          subproject_index = index;
          compilable_index = 1;
          return;
        }
      }

      subproject_index = project->subprojects.size();
      compilable_index = 0;
    }

  private:
    project_configuration* project;
    std::size_t subproject_index;
    std::size_t compilable_index;
  };

  static_assert(std::forward_iterator<compilables_forward_iterator>);

#pragma message("Add a const version.")
  struct compilables_list
  {
    compilables_list()
      : project(nullptr), begin_subroject_index(0), end_subproject_index(0)
    { }

    compilables_list(project_configuration* project)
      : project(project), begin_subroject_index(0), end_subproject_index(project->subprojects.size())
    { }

    compilables_list(project_configuration* project, std::size_t begin_subroject_index, std::size_t end_subproject_index)
      : project(project), begin_subroject_index(begin_subroject_index), end_subproject_index(end_subproject_index)
    { }

    compilables_list(const compilables_list& other)
      : project(other.project), begin_subroject_index(other.begin_subroject_index), end_subproject_index(other.end_subproject_index)
    { }

    compilables_list(compilables_list&& other)
      : project(other.project), begin_subroject_index(other.begin_subroject_index), end_subproject_index(other.end_subproject_index)
    {
      other.project = nullptr;
      other.begin_subroject_index = 0;
      other.end_subproject_index = 0;
    }

    compilables_list& operator=(const compilables_list& other)
    {
      project = other.project;
      begin_subroject_index = other.begin_subroject_index;
      end_subproject_index = other.end_subproject_index;
      return *this;
    }

    compilables_list& operator=(compilables_list&& other)
    {
      project = other.project;
      begin_subroject_index = other.begin_subroject_index;
      end_subproject_index = other.end_subproject_index;

      other.project = nullptr;
      other.begin_subroject_index = 0;
      other.end_subproject_index = 0;

      return *this;
    }

    compilables_forward_iterator begin() { return { project, begin_subroject_index }; }
    compilables_forward_iterator end() { return { project, end_subproject_index }; }

    compilables_forward_iterator begin() const { return { project, begin_subroject_index }; }
    compilables_forward_iterator end() const { return { project, end_subproject_index }; }
  private:
    project_configuration* project;
    std::size_t begin_subroject_index;
    std::size_t end_subproject_index;
  };

  compilables_list get_compilables()
  {
    return compilables_list{ const_cast<project_configuration*>(this) };
  }

  compilables_list get_compilables(std::size_t subproject_index)
  {
    return compilables_list{ const_cast<project_configuration*>(this), subproject_index, subproject_index + 1 };
  }

#pragma message("Strange list ptr.")
  struct artifact_view
  {
    const artifact_description* description;
    const artifact_dependencies_list* dependencies;
    compilables_list compilables;
    const char* subproject_name;
  };

  class artifacts_forward_iterator final
  {
  public:
    using difference_type = int64_t;
    using value_type = artifact_view;

    artifacts_forward_iterator() : project(nullptr), subproject_index(0)
    { }

    artifacts_forward_iterator(project_configuration* project) : project(project)
    { }

    artifacts_forward_iterator(project_configuration* project, std::size_t subproject_index)
      : project(project), subproject_index(subproject_index)
    { }

    artifacts_forward_iterator(const artifacts_forward_iterator& other)
      : project(other.project), subproject_index(other.subproject_index)
    { }

    artifacts_forward_iterator& operator=(const artifacts_forward_iterator& other)
    {
      project = other.project;
      subproject_index = other.subproject_index;
      return *this;
    }

    artifact_view operator*() const
    {
      //estd::log("[{}].", project->subprojects[subproject_index].name.c_str());
      estd::assert_condition(subproject_index < project->subprojects.size(), "Artifacts: Subproject index [{}] is out of the bound [{}].", subproject_index, project->subprojects.size());
      return project->get_artifact_view(subproject_index);
    }

    artifact_view operator*()
    {
      //estd::log("[{}].", project->subprojects[subproject_index].name.c_str());
      estd::assert_condition(subproject_index < project->subprojects.size(), "Artifacts: Subproject index [{}] is out of the bound [{}].", subproject_index, project->subprojects.size());
      return project->get_artifact_view(subproject_index);
    }

    artifacts_forward_iterator& operator++()
    {
      estd::assert_condition(subproject_index < project->subprojects.size(), "Artifacts: Subproject index [{}] is out of the bound [{}].", subproject_index, project->subprojects.size());
      ++subproject_index;
      return *this;
    }

    artifacts_forward_iterator operator++(int)
    {
      artifacts_forward_iterator result = *this;
      this->operator++();
      return result;
    }

    bool operator==(const artifacts_forward_iterator& other) const
    {
      return project == other.project && subproject_index == other.subproject_index;
    }
  private:
    project_configuration* project;
    std::size_t subproject_index;
  };

  static_assert(std::forward_iterator<artifacts_forward_iterator>);

  class artifacts_list
  {
  public:
    artifacts_list(project_configuration* project) : project(project)
    {
    }

    artifacts_forward_iterator begin() { return { project, 0 }; }
    artifacts_forward_iterator end() { return { project, project->subprojects.size() }; }
  protected:
    project_configuration* project;
  };

  artifact_view get_artifact_view(std::size_t subproject_index)
  {
    const auto& subproject = subprojects[subproject_index];

    artifact_view view;
    view.description = &subproject.artifact;
    view.dependencies = &subproject.artifact_dependencies;
    view.compilables = get_compilables(subproject_index);
    view.subproject_name = subproject.name.c_str();

    return view;
  }

  artifacts_list get_artifacts() const
  {
    return artifacts_list{ const_cast<project_configuration*>(this) };
  }
};

#pragma message("Header only problems.")
using compilables_list = project_configuration::compilables_list;
using artifact_view = project_configuration::artifact_view;