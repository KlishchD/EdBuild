#pragma once

namespace instructions
{
  estd::json load(const estd::path& path, const estd::path& includes_absolute);
  estd::json load(const estd::path& path);
}