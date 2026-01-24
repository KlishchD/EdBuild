inline const char* get_type_name(artifact_types type)
{
  switch (type)
  {
  case artifact_types::excutable: return "Executable";
  case artifact_types::static_library: return "StaticLibrary";
  case artifact_types::dynamic_library: return "DynamicLibrary";
  default: return "None";
  }
}

inline compilables_forward_iterator::compilables_forward_iterator() : project(nullptr), subproject_index(0), compilable_index(0)
{

}

inline compilables_forward_iterator::compilables_forward_iterator(project_configuration* project) : project(project)
{
  move_to_first_item(0);
}

inline compilables_forward_iterator::compilables_forward_iterator(project_configuration* project, std::size_t subproject_index) : project(project), subproject_index(subproject_index)
{
  move_to_first_item(subproject_index);
}

inline compilables_forward_iterator::compilables_forward_iterator(project_configuration* project, std::size_t subproject_index, std::size_t compilable_index) : project(project), subproject_index(subproject_index), compilable_index(compilable_index)
{

}

inline compilables_forward_iterator::compilables_forward_iterator(const compilables_forward_iterator& other) : project(other.project), subproject_index(other.subproject_index), compilable_index(other.compilable_index)
{

}

inline compilables_forward_iterator& compilables_forward_iterator::operator=(const compilables_forward_iterator& other)
{
  project = other.project;
  subproject_index = other.subproject_index;
  compilable_index = other.compilable_index;
  return *this;
}

inline compilable_view compilables_forward_iterator::operator*()
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

inline compilable_view compilables_forward_iterator::operator*() const
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

inline compilables_forward_iterator compilables_forward_iterator::operator++(int)
{
  compilables_forward_iterator result = *this;
  this->operator++();
  return result;
}

inline compilables_forward_iterator& compilables_forward_iterator::operator++()
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

inline bool compilables_forward_iterator::operator==(const compilables_forward_iterator& other) const
{
  return project == other.project && subproject_index == other.subproject_index && compilable_index == other.compilable_index;
}

inline void compilables_forward_iterator::move_to_first_item(std::size_t subrpoject_start_index)
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

inline compilables_list project_configuration::get_compilables()
{
  return compilables_list{ const_cast<project_configuration*>(this) };
}

inline compilables_list project_configuration::get_compilables(std::size_t subproject_index)
{
  return compilables_list{ const_cast<project_configuration*>(this), subproject_index, subproject_index + 1 };
}


inline artifacts_forward_iterator::artifacts_forward_iterator() : project(nullptr), subproject_index(0)
{

}

inline artifacts_forward_iterator::artifacts_forward_iterator(project_configuration* project) : project(project)
{

}

inline artifacts_forward_iterator::artifacts_forward_iterator(project_configuration* project, std::size_t subproject_index) : project(project), subproject_index(subproject_index)
{

}

inline artifacts_forward_iterator::artifacts_forward_iterator(const artifacts_forward_iterator& other) : project(other.project), subproject_index(other.subproject_index)
{

}

inline artifact_view artifacts_forward_iterator::operator*() const
{
  //estd::log("[{}].", project->subprojects[subproject_index].name.c_str());
  estd::assert_condition(subproject_index < project->subprojects.size(), "Artifacts: Subproject index [{}] is out of the bound [{}].", subproject_index, project->subprojects.size());
  return project->get_artifact_view(subproject_index);
}

inline artifact_view artifacts_forward_iterator::operator*()
{
  //estd::log("[{}].", project->subprojects[subproject_index].name.c_str());
  estd::assert_condition(subproject_index < project->subprojects.size(), "Artifacts: Subproject index [{}] is out of the bound [{}].", subproject_index, project->subprojects.size());
  return project->get_artifact_view(subproject_index);
}

inline artifacts_forward_iterator& artifacts_forward_iterator::operator++()
{
  estd::assert_condition(subproject_index < project->subprojects.size(), "Artifacts: Subproject index [{}] is out of the bound [{}].", subproject_index, project->subprojects.size());
  ++subproject_index;
  return *this;
}

inline artifacts_forward_iterator artifacts_forward_iterator::operator++(int)
{
  artifacts_forward_iterator result = *this;
  this->operator++();
  return result;
}

inline bool artifacts_forward_iterator::operator==(const artifacts_forward_iterator& other) const
{
  return project == other.project && subproject_index == other.subproject_index;
}

inline artifacts_forward_iterator& artifacts_forward_iterator::operator=(const artifacts_forward_iterator& other)
{
  project = other.project;
  subproject_index = other.subproject_index;
  return *this;
}

inline bool project_configuration::has_precompile_header(std::size_t subproject_index) const
{
  estd::assert_condition(subproject_index < subprojects.size(), "Attempted to check out of bound subproject [{}] for precompile header, subprojects [{}].", subproject_index, subprojects.size());
  return subprojects[subproject_index].has_precompile_header();
}

inline compilable_view project_configuration::get_source_view(std::size_t subproject_index, std::size_t source_index) const
{
  compilable_view result{ };

  if (subproject_index < subprojects.size())
  {
    auto& subproject = subprojects[subproject_index];
    if (source_index < subproject.sources.size())
    {
#pragma message("Plaftform dependant code.")
      result.path = subproject.sources[source_index].c_str();
      result.output_path = subproject.output_path.c_str();

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

inline compilable_view project_configuration::get_source_view(std::size_t subproject_index, std::size_t source_index)
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

inline compilable_view project_configuration::get_precompile_header_view(std::size_t subproject_index) const
{
  compilable_view result{ };

  if (subproject_index < subprojects.size())
  {
    auto& subproject = subprojects[subproject_index];
    if (subproject.precompile_header.is_present())
    {
#pragma message("Plaftform dependant code.")
      result.path = subproject.precompile_header.c_str();
      result.output_path = subproject.output_path.c_str();

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

inline compilable_view project_configuration::get_precompile_header_view(std::size_t subproject_index)
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

inline artifact_view project_configuration::get_artifact_view(std::size_t subproject_index)
{
  const auto& subproject = subprojects[subproject_index];

  artifact_view view;
  view.description = &subproject.artifact;
  view.dependencies = &subproject.artifact_dependencies;
  view.compilables = get_compilables(subproject_index);
  view.subproject_name = subproject.name.c_str();

  return view;
}

inline artifacts_list project_configuration::get_artifacts() const
{
  return artifacts_list{ const_cast<project_configuration*>(this) };
}