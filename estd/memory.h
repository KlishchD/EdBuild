#pragma once

namespace estd
{
  struct memory_report
  {
    uint32_t allocated;
    uint32_t deallocated;
    uint32_t times_allocated;
    uint32_t times_deallocated;

    bool active;
    std::mutex mutex;

    memory_report()
    {
      clean();
      active = false;
    }

    void clean()
    {
      allocated = 0;
      deallocated = 0;
      times_allocated = 0;
      times_deallocated = 0;
    }

    void dump() const
    {
      log("Allocated={}", allocated);
      log("TimesAllocated={}", times_allocated);
      log("Deallocated={}", deallocated);
      log("TimesDeallocated={}", times_deallocated);
    }

    void validate() const
    {
      if (allocated != deallocated)
      {
        throw_error<std::logic_error>("Memory leak detected !!!");
      }
    }

    void activate()
    {
      active = true;
    }

    void deactivate()
    {
      active = false;
    }

    void allocate(size_t size)
    {
      if (!active) return;

      std::lock_guard _(mutex);
      allocated += size;
      ++times_allocated;
    }

    void deallocate(size_t size)
    {
      if (!active) return;

      std::lock_guard _(mutex);
      deallocated += size;
      ++times_deallocated;
    }
  };

  template <typename type>
  inline void deallocate_vector(std::vector<type>& vector)
  {
    std::vector<type> _;
    vector.swap(_);
  }
}