#pragma once

#include <cstdint>
#include <array>

namespace estd
{
  constexpr inline std::size_t crc_table_size = 256;
  using crc_table = std::array<uint32_t, crc_table_size>;

  // https://gist.github.com/timepp/1f678e200d9e0f2a043a9ec6b3690635
  const crc_table& calculate_crc32_table();

  inline uint32_t crc32_append(uint32_t initial, const void* data, size_t byte_size)
  {
    const crc_table& table = calculate_crc32_table();

    uint32_t c = initial ^ 0xFFFFFFFF;

    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);
    for (size_t byte_index = 0; byte_index < byte_size; ++byte_index)
    {
      c = table[(c ^ bytes[byte_index]) & 0xFF] ^ (c >> 8);
    }

    return c ^ 0xFFFFFFFF;
  }

  template <typename string_type>
  uint32_t crc32_append_string(uint32_t initial, const string_type& string)
  {
    const auto* data = reinterpret_cast<const void*>(string.data());
    return crc32_append(initial, data, string.size());
  }

  template <typename enum_type>
  uint32_t crc32_append_enum(uint32_t initial, enum_type value)
  {
    uint32_t casted = static_cast<uint32_t>(value);
    return crc32_append(initial, &casted, sizeof(uint32_t));
  }

  template <typename generic_type>
  uint32_t crc32_append_generic(uint32_t initial, const generic_type& value)
  {
    const auto* data = reinterpret_cast<const void*>(&value);
    return crc32_append(initial, data, sizeof(generic_type));
  }
}