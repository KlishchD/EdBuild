#pragma once

#include "EdBuild.h"

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

  uint32_t get_hash() const;
};

struct define_description
{
  std::string key;
  std::string value;

  uint32_t get_hash() const;
};

class filter_status
{
  enum
  {
    object_files_is_not_present = 1,
    dependencies_were_updated,
    builder_was_updated,
    compilable_was_updated,
    project_hash_mismatch,
    subproject_hash_mismatch
  };
public:
  filter_status();
  filter_status(const filter_status& new_status);
  filter_status(filter_status&& new_status) noexcept;

  inline std::string get_reason() const
  {
    switch (status)
    {
    case object_files_is_not_present: return "Object file is not present.";
    case dependencies_were_updated: return "Dependencies were updated.";
    case builder_was_updated: return "Builder was updated.";
    case compilable_was_updated: return "Compilable file itself was updated.";
    case project_hash_mismatch: return "Project has different hash.";
    case subproject_hash_mismatch: return "Subproject has different hash.";
    default:
      return "None";
    }
  }

  inline uint16_t get_status() const { return status; }
  inline bool is_not_filtered() const { return status == 0; }
  inline bool is_filtered() const { return status; }
  inline operator bool() const { return status; }

  inline filter_status& operator=(uint16_t new_status)
  {
    status = new_status;
    return *this;
  }

  inline filter_status& operator=(const filter_status& new_status)
  {
    status = new_status.status;
    return *this;
  }

  inline filter_status& operator=(filter_status&& new_status)
  {
    status = new_status.status;
    new_status.status = 0;
    return *this;
  }

  inline void set_object_files_is_not_present() { status = object_files_is_not_present; }
  inline void set_dependencies_were_updated() { status = dependencies_were_updated; }
  inline void set_builder_was_updated() { status = builder_was_updated; }
  inline void set_compilable_was_updated() { status = compilable_was_updated; }
  inline void set_project_hash_mismatch() { status = project_hash_mismatch; }
  inline void set_subproject_hash_mismatch() { status = subproject_hash_mismatch; }
private:
  uint16_t status;
};

struct compilable_description
{
  std::string path;
  filter_status status;

  compilable_description();

  template <typename string_type>
  compilable_description(const string_type& path) : path(path), status() {}

  compilable_description(const compilable_description& other);
  compilable_description(compilable_description&& other)  noexcept;

  compilable_description& operator=(const compilable_description& other);
  compilable_description& operator=(compilable_description&& other) noexcept;

  inline bool is_present() const { return path.size(); }
  inline bool is_not_present() const { return path.empty(); }

  inline const char* get_c_path() const { return path.c_str(); }
  inline uint32_t get_hash() const { return estd::crc32_append_string(0, path); }
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

inline const char* get_type_name(artifact_types type);

struct artifact_description
{
  // Workaround, because you can not make a nice union with std::strings.
  std::string field1;
  std::string field2;
  std::string resources;

  artifact_types type;
  bool preproduced;

  inline const std::string& input() const { return type == artifact_types::static_library ? static_library() : import_library(); }
  inline const std::string& output() const { return field2; }

  inline const std::string& import_library() const { return field1; };
  inline std::string& import_library() { return field1; };

  inline void import_library(const std::string& path) { field1 = path; }
  inline void import_library(std::string&& path) { field1 = std::move(path); }

  inline const std::string& dynamic_library() const { return field2; };
  inline std::string& dynamic_library() { return field2; };

  inline void dynamic_library(const std::string& path) { field2 = path; }
  inline void dynamic_library(std::string&& path) { field2 = std::move(path); }

  inline const std::string& static_library() const { return field2; };
  inline std::string& static_library() { return field2; };

  inline void static_library(const std::string& path) { field2 = path; }
  inline void static_library(std::string&& path) { field2 = std::move(path); }

  inline const std::string& executable() const { return field2; };
  inline std::string& executable() { return field2; };

  inline void executable(const std::string& path) { field2 = path; }
  inline void executable(std::string&& path) { field2 = std::move(path); }
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

  filter_status status;

  uint32_t get_hash() const;

  inline bool has_precompile_header() const { return precompile_header.is_present(); }

  inline bool is_preproced() const { return artifact.preproduced; }
  inline bool produces_artifact() const { return !artifact.preproduced; }
  inline bool preproduced_artifact() const { return artifact.preproduced; }

  inline std::size_t get_compilables_count() const
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

  inline operator bool() const { return !!path; }
};

struct build_configuration
{
  std::string name;
  std::string subproject_name;
};

using build_configurations = std::vector<build_configuration>;

struct project_configuration;
class compilables_forward_iterator final
{
public:
  using difference_type = int64_t;
  using value_type = compilable_view;

  compilables_forward_iterator();
  compilables_forward_iterator(project_configuration* project);
  compilables_forward_iterator(project_configuration* project, std::size_t subproject_index);
  compilables_forward_iterator(project_configuration* project, std::size_t subproject_index, std::size_t compilable_index);
  compilables_forward_iterator(const compilables_forward_iterator& other);
  compilables_forward_iterator& operator=(const compilables_forward_iterator& other);

  inline compilable_view operator*() const;
  inline compilable_view operator*();

  inline compilables_forward_iterator& operator++();
  inline compilables_forward_iterator operator++(int);

  inline bool operator==(const compilables_forward_iterator& other) const;
private:
  inline void move_to_first_item(std::size_t subrpoject_start_index);

private:
  project_configuration* project;
  std::size_t subproject_index;
  std::size_t compilable_index;
};
static_assert(std::forward_iterator<compilables_forward_iterator>);

#pragma message("Add a const version.")
struct compilables_list
{
  compilables_list();
  compilables_list(project_configuration* project);
  compilables_list(project_configuration* project, std::size_t begin_subroject_index, std::size_t end_subproject_index);
  compilables_list(const compilables_list& other);
  compilables_list(compilables_list&& other) noexcept;

  compilables_list& operator=(const compilables_list& other);
  compilables_list& operator=(compilables_list&& other) noexcept;

  inline compilables_forward_iterator begin() { return { project, begin_subroject_index }; }
  inline compilables_forward_iterator end() { return { project, end_subproject_index }; }

  inline compilables_forward_iterator begin() const { return { project, begin_subroject_index }; }
  inline compilables_forward_iterator end() const { return { project, end_subproject_index }; }
private:
  project_configuration* project;
  std::size_t begin_subroject_index;
  std::size_t end_subproject_index;
};

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

  artifacts_forward_iterator();
  artifacts_forward_iterator(project_configuration* project);
  artifacts_forward_iterator(project_configuration* project, std::size_t subproject_index);
  artifacts_forward_iterator(const artifacts_forward_iterator& other);

  inline artifacts_forward_iterator& operator=(const artifacts_forward_iterator& other);

  inline artifact_view operator*() const;
  inline artifact_view operator*();

  inline artifacts_forward_iterator& operator++();
  inline artifacts_forward_iterator operator++(int);

  inline bool operator==(const artifacts_forward_iterator& other) const;
private:
  project_configuration* project;
  std::size_t subproject_index;
};

static_assert(std::forward_iterator<artifacts_forward_iterator>);

class artifacts_list
{
public:
  artifacts_list(project_configuration* project);

  artifacts_forward_iterator begin();
  artifacts_forward_iterator end();
protected:
  project_configuration* project;
};

struct project_configuration
{
  std::string name;

  subproject_configurations subprojects;
  option_descriptions options;
  define_descriptions defines;

  build_configurations builds;

  filter_status status;

  uint32_t get_hash() const;

  inline bool has_precompile_header(std::size_t subproject_index) const;

  inline compilable_view get_source_view(std::size_t subproject_index, std::size_t source_index) const;
  inline compilable_view get_source_view(std::size_t subproject_index, std::size_t source_index);

  inline compilable_view get_precompile_header_view(std::size_t subproject_index) const;
  inline compilable_view get_precompile_header_view(std::size_t subproject_index);

  inline compilables_list get_compilables();
  inline compilables_list get_compilables(std::size_t subproject_index);

  inline artifact_view get_artifact_view(std::size_t subproject_index);
  inline artifacts_list get_artifacts() const;
};

#include "project.inl"