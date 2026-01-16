#include "platform.h"
#include <Windows.h>

std::string estd::fetch_executable_path()
{
  constexpr std::size_t buffer_size = MAX_PATH;
  char executable_path[buffer_size];

  std::size_t size = GetModuleFileNameA(nullptr, executable_path, buffer_size);
  estd::assert_condition(size, "Failed to fetch executable path.");

  return executable_path;
}
