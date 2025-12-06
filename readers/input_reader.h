#pragma once

class input_reader
{
public:
  // TODO: Consider collapsing these.
  struct option_data
  {
    const std::string* name;
    const std::string* value;
  };

  struct define_data
  {
    const std::string* name;
    const std::string* value;
  };

  virtual bool has_next_option() const = 0;
  virtual std::optional<option_data> next_option() = 0;

  virtual bool has_next_define() const = 0;
  virtual std::optional<define_data> next_define() = 0;

  virtual bool has_next_source() const = 0;
  virtual const std::string* next_source() = 0;

  virtual bool has_next_include() const = 0;
  virtual const std::string* next_include() = 0;

  virtual bool has_next_dependency() const = 0;
  virtual const std::string* next_dependency() = 0;

  virtual const std::string* precompile_header() const = 0;
  virtual const std::string* artifact_type() const = 0;
  virtual const std::string* artifact_name() const = 0;

  virtual const std::string* project_name() const = 0;

  virtual bool is_subproject_valid() const = 0;
  virtual bool has_next_subproject() const = 0;
  virtual void next_subproject() = 0;
};