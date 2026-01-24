#include "platforms_registry.h"

void platform_builder::commit()
{
  registry.register_platform(*this);
}
