#include "project.h"

filter_status::filter_status() : status(0)
{

}

filter_status::filter_status(const filter_status& new_status) : status(new_status.status)
{

}

filter_status::filter_status(filter_status&& new_status) noexcept : status(new_status.status)
{
  new_status.status = 0;
}

compilable_description::compilable_description() : path(), status()
{

}

compilable_description::compilable_description(const compilable_description& other) : path(other.path), status(other.status)
{

}

compilable_description::compilable_description(compilable_description&& other) noexcept : path(std::move(other.path)), status(std::move(other.status))
{

}

compilable_description& compilable_description::operator=(compilable_description&& other) noexcept
{
  path = std::move(other.path);
  status = std::move(other.status);
  return *this;
}

compilable_description& compilable_description::operator=(const compilable_description& other)
{
  path = other.path;
  status = other.status;
  return *this;
}

uint32_t subproject_configuration::get_hash() const
{
  uint32_t hash = 0;
  hash = estd::crc32_append_string(hash, name);

  for (const auto& option : options)
  {
    const uint32_t option_hash = option.get_hash();
    hash = estd::crc32_append_generic(hash, option_hash);
  }

  for (const auto& define : defines)
  {
    const uint32_t define_hash = define.get_hash();
    hash = estd::crc32_append_generic(hash, define_hash);
  }

  for (const auto& include : includes)
  {
    hash = estd::crc32_append_string(hash, include);
  }

  const uint32_t precompile_header_hash = precompile_header.get_hash();
  hash = estd::crc32_append_generic(hash, precompile_header_hash);

  return hash;
}

uint32_t project_configuration::get_hash() const
{
  uint32_t hash = 0;

  for (const auto& option : options)
  {
    const uint32_t option_hash = option.get_hash();
    hash = estd::crc32_append_generic(hash, option_hash);
  }

  for (const auto& define : defines)
  {
    const uint32_t define_hash = define.get_hash();
    hash = estd::crc32_append_generic(hash, define_hash);
  }

  return hash;
}

compilables_list::compilables_list() : project(nullptr), begin_subroject_index(0), end_subproject_index(0)
{

}

compilables_list::compilables_list(project_configuration* project) : project(project), begin_subroject_index(0), end_subproject_index(project->subprojects.size())
{

}

compilables_list::compilables_list(project_configuration* project, std::size_t begin_subroject_index, std::size_t end_subproject_index) : project(project), begin_subroject_index(begin_subroject_index), end_subproject_index(end_subproject_index)
{

}

compilables_list::compilables_list(const compilables_list& other) : project(other.project), begin_subroject_index(other.begin_subroject_index), end_subproject_index(other.end_subproject_index)
{

}

compilables_list::compilables_list(compilables_list&& other) noexcept : project(other.project), begin_subroject_index(other.begin_subroject_index), end_subproject_index(other.end_subproject_index)
{
  other.project = nullptr;
  other.begin_subroject_index = 0;
  other.end_subproject_index = 0;
}

compilables_list& compilables_list::operator=(compilables_list&& other) noexcept
{
  project = other.project;
  begin_subroject_index = other.begin_subroject_index;
  end_subproject_index = other.end_subproject_index;

  other.project = nullptr;
  other.begin_subroject_index = 0;
  other.end_subproject_index = 0;

  return *this;
}

compilables_list& compilables_list::operator=(const compilables_list& other)
{
  project = other.project;
  begin_subroject_index = other.begin_subroject_index;
  end_subproject_index = other.end_subproject_index;
  return *this;
}

artifacts_list::artifacts_list(project_configuration* project) : project(project)
{

}

artifacts_forward_iterator artifacts_list::begin()
{
  return { project, 0 };
}

artifacts_forward_iterator artifacts_list::end()
{
  return { project, project->subprojects.size() };
}
