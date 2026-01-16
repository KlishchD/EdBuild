#pragma once

#include <fstream>

namespace estd
{
  template <bool perform_seek_to_file_start>
  uint32_t fetch_file_size(FILE* file)
  {
    if (!file)
    {
      throw_error<std::logic_error>("Attempted to fetch size of nullptr file.");
    }

    int32_t error;

    if constexpr (perform_seek_to_file_start)
    {
      error = fseek(file, 0, SEEK_SET);

      if (error)
      {
        throw_error<std::logic_error>("Failed to jump to the begining of the file with error code ({}) and message ({}).", error, fetch_error_message_friendly(error));
      }
    }

    error = fseek(file, 0, SEEK_END);

    if (error)
    {
      throw_error<std::logic_error>("Failed to jump to the end of the file with error code ({}) and message ({}).", error, fetch_error_message_friendly(error));
    }

    uint32_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    return file_size;
  }

  template <typename string_type>
  void read_file_full(const string_type& absolute_path, char** out_buffer, uint32_t& out_file_size)
  {
    #pragma message("This should be done using C++ api, no need for C here.")

    FILE* file;
    fopen_s(&file, absolute_path.c_str(), "rb");

    if (!file)
    {
      throw_error<std::logic_error>("Failed to open file {}.", absolute_path);
    }

    uint32_t file_size = fetch_file_size<false>(file);
    if (file_size == 0)
    {
      throw_error<std::logic_error>("File is empty {}.", absolute_path);
    }

    log("Loading {} with size of {}.", absolute_path.c_str(), file_size);

    char* buffer = new char[file_size];
    uint32_t read = fread(buffer, sizeof(char), file_size, file);

    if (read != file_size)
    {
      if (feof(file))
      {
        throw_error<std::logic_error>("Read file size ({}) happened to different from actual one ({}).", file_size, read);
      }
      else if (uint32_t error_code = ferror(file))
      {
        const char* error_message = fetch_error_message_friendly(error_code);
        throw_error<std::logic_error>("Failed to read the whole file with a code ({}) and a reason ({}).", error_code, error_message);
      }
    }

    fclose(file);

    (*out_buffer) = buffer;
    out_file_size = file_size;
  }
}