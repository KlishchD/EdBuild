#pragma once

class input_reader
{
public:
  // TODO: Consider collapsing these.
  struct option_data
  {
    const std::string* name = nullptr;
    const std::string* value = nullptr;
    const std::string* platforms = nullptr;
    const std::string* targets = nullptr;
  };

  struct define_data
  {
    const std::string* name = nullptr;
    const std::string* value = nullptr;
    const std::string* platforms = nullptr;
    const std::string* targets = nullptr;
  };

  virtual bool is_project_name_present() const = 0;
  virtual bool is_globals_list_present() const = 0;
  virtual bool is_projects_list_present() const = 0;
  virtual bool is_builds_list_present() const = 0;

  virtual bool has_next_option() const = 0;
  virtual option_data next_option() = 0;

  virtual bool has_next_define() const = 0;
  virtual define_data next_define() = 0;

  virtual bool has_next_source() const = 0;
  virtual const std::string* next_source() = 0;

  virtual bool has_next_include() const = 0;
  virtual const std::string* next_include() = 0;

  virtual bool has_next_dependency() const = 0;
  virtual const std::string* next_dependency() = 0;

  virtual const std::string* precompile_header() const = 0;

  virtual const std::string* artifact_type() const = 0;
  virtual const std::string* resources() const = 0;

  virtual const std::string* static_library() const = 0;
  virtual const std::string* import_library() const = 0;
  virtual const std::string* dynamic_library() const = 0;
  virtual const std::string* executable() const = 0;

  virtual const std::string* project_name() const = 0;

  virtual bool is_subproject_valid() const = 0;
  virtual bool has_next_subproject() const = 0;
  virtual void next_subproject() = 0;

  virtual bool has_next_build() const = 0;
  virtual bool next_build() = 0;

  virtual const std::string* build_name() const = 0;
  virtual const std::string* build_subproject_name() const = 0;
};