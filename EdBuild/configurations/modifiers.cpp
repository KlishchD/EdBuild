#include "modifiers.h"

uint32_t option_description::get_hash() const
{
  uint32_t hash = 0;

  hash = estd::crc32_append_enum(hash, type);
  hash = estd::crc32_append_string(hash, value);

  return hash;
}

uint32_t define_description::get_hash() const
{
  uint32_t hash = 0;

  hash = estd::crc32_append_string(hash, key);
  hash = estd::crc32_append_string(hash, value);

  return hash;
}