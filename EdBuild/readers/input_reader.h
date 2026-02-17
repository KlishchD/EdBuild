#pragma once

struct modifier_data
{
  const char* name = nullptr;
  const char* value = nullptr;
  const char* platforms = nullptr;
  const char* targets = nullptr;
};

class globals_reader
{
public:
  virtual bool read_next_option(modifier_data& output) = 0;
  virtual bool read_next_define(modifier_data& output) = 0;
};

class modifier_list_reader
{
public:
  virtual const char* read_name() = 0;
  virtual std::size_t count() = 0;

  // Mutually exclusive.
  virtual bool read_next_option(modifier_data& output) = 0;
  virtual bool read_next_define(modifier_data& output) = 0;

  virtual bool next() = 0;
};

class projects_reader
{
public:
  virtual const char* read_name() = 0;
  virtual std::size_t count() = 0;

  virtual bool read_next_option(modifier_data& output) = 0;
  virtual bool read_next_define(modifier_data& output) = 0;

  virtual bool read_next_source(const char*& source_path) = 0;
  virtual bool read_next_include(const char*& include_path) = 0;

  virtual const char* read_precompile_header_path() = 0;

  virtual const char* read_artifact_type() = 0;
  virtual const char* read_resources_path() = 0;

  virtual const char* read_static_library_path() = 0;
  virtual const char* read_import_library_path() = 0;
  virtual const char* read_dynamic_library_path() = 0;
  virtual const char* read_executable_path() = 0;

  virtual bool read_next_dependency(const char*& path) = 0;

  virtual bool next() = 0;
};

class builds_reader
{
public:
  virtual const char* read_name() = 0;
  virtual std::size_t count() = 0;

  virtual bool read_next_option(modifier_data& output) = 0;
  virtual bool read_next_define(modifier_data& output) = 0;

  virtual const char* read_subprorject() = 0;

  virtual bool next() = 0;
};

class instructions_reader
{
public:
  virtual globals_reader* read_globals() = 0;
  virtual modifier_list_reader* read_options_modifiers() = 0;
  virtual modifier_list_reader* read_defines_modifiers() = 0;
  virtual projects_reader* read_projects() = 0;
  virtual builds_reader* read_builds() = 0;
};