#include "json.h"
#include "logging.h"

estd::json estd::read_json(const stack_string_512& path)
{
  char* buffer = nullptr;
  uint32_t buffer_size = 0;
  read_file_full(path, &buffer, buffer_size);

  json object = json::parse(buffer, buffer + buffer_size);

  delete[] buffer;

  return object;
}

void estd::write_json(const stack_string_512& path, const estd::json& object)
{
  std::ofstream file(path.c_str());
  file << object.dump(2);
}
