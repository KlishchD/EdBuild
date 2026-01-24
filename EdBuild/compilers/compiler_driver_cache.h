#pragma once

#include "compiler_translator.h"

template <compiler_translator translator_type>
class compiler_translator_cache
{
public:
  compiler_translator_cache(const project_configuration& project, const translator_type& translator) : project(project), translator(translator)
  { }

  void clear()
  {
    project_suffix.clear();
    subproject_suffixes.clear();
    precompile_headers_suffixes.clear();
  }

  void build()
  {
    append_options(translator, project.options, project_suffix);
    append_defines(translator, project.defines, project_suffix);

    const auto& subprojects = project.subprojects;
    subproject_suffixes.reserve(subprojects.size());
    precompile_headers_suffixes.reserve(subprojects.size());

    for (std::size_t subproject_index = 0; subproject_index < project.subprojects.size(); ++subproject_index)
    {
      const auto& subproject = project.subprojects[subproject_index];

      command_string subproject_suffix;
      append_options(translator, subproject.options, subproject_suffix);
      append_defines(translator, subproject.defines, subproject_suffix);
      append_includes(translator, subproject.includes, subproject_suffix);

      subproject_suffixes.push_back(std::move(subproject_suffix));

      command_string precompile_header_suffix;

      compilable_view view = project.get_precompile_header_view(subproject_index);
      translator.append_precompile_header(view, precompile_header_suffix);

      precompile_headers_suffixes.push_back(std::move(precompile_header_suffix));
    }
  }

  void append_sufixes(std::size_t subproject_index, bool needs_precompile_header, command_string& list) const
  {
    list.append(project_suffix);
    list.append(subproject_suffixes[subproject_index]);
    if (needs_precompile_header) list.append(precompile_headers_suffixes[subproject_index]);
  }
private:
  void append_options(const translator_type& translator, const option_descriptions& options, command_string& list) const
  {
    for (const option_description& option : options)
    {
      translator.append_option(option, list);
    }
  }

  void append_defines(const translator_type& translator, const define_descriptions& defines, command_string& list) const
  {
    for (const define_description& define : defines)
    {
      translator.append_define(define, list);
    }
  }

  void append_includes(const translator_type& translator, const include_files& includes, command_string& list) const
  {
    for (const estd::path& include : includes)
    {
      translator.append_include(include, list);
    }
  }
private:
  const project_configuration& project;
  const translator_type& translator;

  command_string project_suffix;
  command_strings subproject_suffixes;
  command_strings precompile_headers_suffixes;
};