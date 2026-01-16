#include "strings.h"
#include "logging.h"

int32_t estd::stoi(const char* start, const char* end)
{
  const uint32_t width = end - start;
  assert_condition(width < 10, "Number width [{:{}}] is too big for 32 bit integer.", start, width);

  bool negative = false;
  if (start[0] == '-')
  {
    ++start;
    negative = true;

    assert_condition(start != end, "Number must have at least one digit.");
  }

  int64_t store = 0;
  for (const char* it = start; it != end; ++it)
  {
    assert_condition(std::isdigit(*it), "Number [{:{}}]must consist of only digits.", start, width);

    store = (store * 10LL) + (*it - '0');
  }

  assert_condition(store > std::numeric_limits<int32_t>::min(), "Number [{}] is too small to be 32 bit integer.", store);
  assert_condition(store < std::numeric_limits<int32_t>::max(), "Number [{}] is too big to be 32 bit integer.", store);

  return negative ? -store : store;
}
