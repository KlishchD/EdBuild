#pragma once

#include "EdBuild.h"

class builder_cache
{
public:
  builder_cache(const estd::path& intermediate_path, char platfrom, char target)
    : intermediate_path(intermediate_path)
  {
    entry_name.push_back(platfrom);
    entry_name.push_back(target);

    estd::path cache_path;
    cache_path
      .append(intermediate_path)
      .append("builder_cache.json");

    cache = estd::read_json(cache_path);
  }

  bool is_build_outdated(const file_time& builder_update_time) const
  {
    const bool entry_is_not_present = !cache.contains(entry_name);
    if (entry_is_not_present) { estd::log("Build entry not found in cache."); return true; }

    const estd::json& cache_entry = cache[entry_name];

    const bool build_is_not_present = !cache_entry.contains("BuildTime");
    if (build_is_not_present) { estd::log("Build time not found in cache."); return true; }
    
    int64_t time_point = cache_entry["BuildTime"];
    const file_time build_time = file_time(file_clock::duration(time_point));

    estd::log("{}Builder time{}: [{}].", estd::colors::green(), estd::colors::reset(), builder_update_time);
    estd::log("{}Build   time{}: [{}].", estd::colors::green(), estd::colors::reset(), build_time);

    return builder_update_time > build_time;
  }

  bool is_hash_outdated(const project_configuration& project) const
  {
    const bool entry_is_not_present = !cache.contains(entry_name);
    if (entry_is_not_present) { estd::log("Build entry was not found in cache."); return true; }

    const estd::json& cache_entry = cache[entry_name];

    const bool project_hash_is_not_present = !cache_entry.contains("ProjectHash");
    if (project_hash_is_not_present) { estd::log("Project has was not found in cache."); return true; }

    const uint32_t stored_hash = cache_entry["ProjectHash"];
    const uint32_t computed_hash = project.get_hash();

    estd::log("{}Hash{}: [Project], {} -> {}.", estd::colors::green(), estd::colors::reset(), stored_hash, computed_hash);

    return stored_hash != computed_hash;
  }

  bool is_hash_outdated(const subproject_configuration& subproject) const
  {
    const bool entry_is_not_present = !cache.contains(entry_name);
    if (entry_is_not_present) { estd::log("Build entry was not found in cache."); return true; }

    const estd::json& cache_entry = cache[entry_name];

    const char* subproject_name = subproject.name.c_str();

    const bool hashes_are_not_present = !cache_entry.contains("ProjectHashes");
    if (hashes_are_not_present) { estd::log("[{}] hashes list was not found in cache.", subproject_name); return true; }

    const estd::json& hashes = cache_entry["ProjectHashes"];

    const bool subproject_is_not_present = !hashes.contains(subproject_name);
    if (subproject_is_not_present) { estd::log("[{}] was not found in a hash cache list.", subproject_name); return true; }

    const uint32_t stored_hash = hashes[subproject_name];
    const uint32_t computed_hash = subproject.get_hash();

    estd::log("{}Hash{}: [{}], {} -> {}.", estd::colors::green(), estd::colors::reset(), subproject_name, stored_hash, computed_hash);

    return stored_hash != computed_hash;
  }

  void update_cache(const project_configuration& project)
  {
    estd::json& cache_entry = cache[entry_name];
    cache_entry.clear();

    const int64_t time_point = file_clock::now().time_since_epoch().count();
    cache_entry["BuildTime"] = time_point;

    cache_entry["ProjectHash"] = project.get_hash();

    estd::json& hashes = cache_entry["ProjectHashes"];
    for (const auto& subproject : project.subprojects)
    {
      hashes[subproject.name] = subproject.get_hash();
    }

    estd::path cache_path;
    cache_path
      .append(intermediate_path)
      .append("builder_cache.json");

    estd::write_json(cache_path, cache);

    estd::log("Builder cache was updated.");
  }
private:
  const estd::path& intermediate_path;

  estd::json cache;
  std::string entry_name;
};
