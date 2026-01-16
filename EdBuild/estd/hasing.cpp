#include "hasing.h"

const estd::crc_table& estd::calculate_crc32_table()
{
  static crc_table table;

  static bool initalized = false;
  if (!initalized)
  {
    uint32_t polynomial = 0xEDB88320;
    for (uint32_t i = 0; i < crc_table_size; i++)
    {
      uint32_t c = i;
      for (size_t j = 0; j < 8; j++)
      {
        if (c & 1) {
          c = polynomial ^ (c >> 1);
        }
        else {
          c >>= 1;
        }
      }
      table[i] = c;
    }

    initalized = true;
  }

  return table;
}
