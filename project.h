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

struct compilable_view
{
  const char* path = nullptr;
  const char* extension = nullptr;

  const char* subproject_name = nullptr;
  std::size_t subproject_index = -1;
  std::size_t compilable_index = -1;

  bool is_source = false;
  bool has_precompile_header = false;

  operator bool() const
  {
    return !!path;
  }
};

struct project_configuration
{
  subproject_configurations subprojects;
  option_descriptions options;
  define_descriptions defines;
  std::string name;

  compilable_view get_source_view(std::size_t subproject_index, std::size_t source_index) const
  {
    compilable_view result{ };

    if (subproject_index < subprojects.size())
    {
      const auto& subproject = subprojects[subproject_index];
      if (source_index < subproject.sources.size())
      {
        result.path = subproject.sources[source_index].c_str();
        result.extension = ".obj";

        result.subproject_name = subproject.name.c_str();
        result.subproject_index = subproject_index;
        result.compilable_index = source_index + 1;

        result.is_source = true;
        result.has_precompile_header = !subproject.precompile_header.empty();
      }
    }

    return result;
  }

  compilable_view get_precompile_header_view(std::size_t subproject_index) const
  {
    compilable_view result{ };

    if (subproject_index < subprojects.size())
    {
      const auto& subproject = subprojects[subproject_index];
      if (subproject.precompile_header.size())
      {
        result.path = subproject.precompile_header.c_str();
        result.extension = ".pch";

        result.subproject_name = subproject.name.c_str();
        result.subproject_index = subproject_index;
        result.compilable_index = 0;

        result.is_source = false;
        result.has_precompile_header = false;
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

    compilables_forward_iterator(const project_configuration& project)
      : project(&project)
    {
      for (std::size_t index{ 0 }; index < project.subprojects.size(); ++index)
      {
        const auto& subproject = project.subprojects[index];

        if (subproject.precompile_header.size())
        {
          subproject_index = index;
          compilable_index = 0;
          break;
        }

        if (subproject.sources.size())
        {
          subproject_index = index;
          compilable_index = 1;
          break;
        }
      }
    }

    compilables_forward_iterator(const project_configuration& project, std::size_t subproject_index, std::size_t compilable_index)
      : project(&project), subproject_index(subproject_index), compilable_index(compilable_index)
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
      //estd::log("[{}] [{}]", project->subprojects[subproject_index].name.c_str(), compilable_index);

      estd::assert_condition(subproject_index < project->subprojects.size(), "Subproject index [{}] is out of the bound [{}].", subproject_index, project->subprojects.size());

      const auto& subproject = project->subprojects[subproject_index];
      const bool has_precompile_header = subproject.precompile_header.size();
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
      estd::assert_condition(subproject_index < project->subprojects.size(), "Subproject index [{}] is out of the bound [{}].", subproject_index, project->subprojects.size());
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
            if (next_subrpoject.is_precompiled()) continue;

            const bool has_compilables = next_subrpoject.precompile_header.size() || next_subrpoject.sources.size();
            if (has_compilables) break;
          }
        }

        if (subproject_index < project->subprojects.size())
        {
          const auto& next_subrpoject = project->subprojects[subproject_index];
          compilable_index = next_subrpoject.precompile_header.empty();
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
    const project_configuration* project;
    std::size_t subproject_index;
    std::size_t compilable_index;
  };

  static_assert(std::forward_iterator<compilables_forward_iterator>);

  struct compilables
  {
    compilables(const project_configuration& project) : project(project)
    { }

    compilables_forward_iterator begin() { return { project }; }
    compilables_forward_iterator end() { return { project, project.subprojects.size(), 0 }; }

  private:
    const project_configuration& project;
  };

  compilables get_compilables() const
  {
    return compilables { *this };
  }
};

using dependencies_list = std::vector<const std::string*>;