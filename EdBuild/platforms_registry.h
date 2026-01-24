#pragma once

#include "EdBuild.h"
#include "estd/types.h"

class platforms_registry;

class platform_builder
{
  friend class platform;
public:
  platform_builder(const char* name, char marker, platforms_registry& registry) 
    : name(name), marker(marker), registry(registry)
  {
  }

  platform_builder& set_preprocessing_extension(const char* value)
  {
    preprocessing_extension = value;
    return *this;
  }

  platform_builder& set_object_extension(const char* value)
  {
    object_extension = value;
    return *this;
  }

  platform_builder& set_precompile_header_extension(const char* value)
  {
    precompile_header_extension = value;
    return *this;
  }

  platform_builder& set_static_library_extension(const char* value)
  {
    static_library_extension = value;
    return *this;
  }

  platform_builder& set_dynamic_library_extension(const char* value)
  {
    dynamic_library_extension = value;
    return *this;
  }

  platform_builder& set_executable_extension(const char* value)
  {
    executable_extension = value;
    return *this;
  }

  platform_builder& set_dependencies_extension(const char* value)
  {
    dependencies_extension = value;
    return *this;
  }

  platform_builder& set_database_extension(const char* value)
  {
    database_extension = value;
    return *this;
  }

  platform_builder& set_symbols_database_extension(const char* value)
  {
    symbols_database_extension = value;
    return *this;
  }

  void commit();
protected:
  platforms_registry& registry;

  const char* name;
  char marker;

  const char* preprocessing_extension;
  const char* object_extension;
  const char* precompile_header_extension;
  const char* static_library_extension;
  const char* dynamic_library_extension;
  const char* executable_extension;
  const char* dependencies_extension;
  const char* database_extension;
  const char* symbols_database_extension;
};

class platform : public estd::named_marker_type
{
public:
  platform(const platform_builder& builder)
    : estd::named_marker_type(builder.marker, builder.name),
    preprocessing_extension(builder.preprocessing_extension),
    object_extension(builder.object_extension),
    precompile_header_extension(builder.precompile_header_extension),
    static_library_extension(builder.static_library_extension),
    dynamic_library_extension(builder.dynamic_library_extension),
    executable_extension(builder.executable_extension),
    dependencies_extension(builder.dependencies_extension),
    database_extension(builder.database_extension),
    symbols_database_extension(builder.symbols_database_extension)
  { }

  const char* get_preprocessing_extension() const { return preprocessing_extension; }
  const char* get_object_extension() const { return object_extension; }
  const char* get_precompile_header_extension() const { return precompile_header_extension; }
  const char* get_static_library_extension() const { return static_library_extension; }
  const char* get_dynamic_library_extension() const { return dynamic_library_extension; }
  const char* get_executable_extension() const { return executable_extension; }
  const char* get_dependencies_extension() const { return dependencies_extension; }
  const char* get_database_extension() const { return database_extension; }
  const char* get_symbols_database_extension() const { return symbols_database_extension; }

  const char* get_compilable_extension(bool is_source) const
  {
    return is_source ? object_extension : precompile_header_extension;
  }
protected:
  const char* preprocessing_extension;
  const char* object_extension;
  const char* precompile_header_extension;
  const char* static_library_extension;
  const char* dynamic_library_extension;
  const char* executable_extension;
  const char* dependencies_extension;
  const char* database_extension;
  const char* symbols_database_extension;
};

class platforms_registry
{
public:
  platform_builder create_platform(const char* name, char marker)
  {
    return platform_builder(name, marker, *this);
  }

  void register_platform(const platform_builder& builder)
  {
    platforms.emplace_back(builder);
  }

  const platform* find(char marker) const
  {
    for (const auto& platform : platforms)
    {
      if (platform.match(marker)) return &platform;
    }

    return nullptr;
  }
protected:
  std::vector<platform> platforms;
};