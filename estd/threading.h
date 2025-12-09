#pragma once

namespace estd
{
  template <std::size_t threads_limit, typename job_type>
  void async_execute(job_type job, std::size_t threads_count)
  {
    assert_condition(threads_count < threads_limit, "Exceeded maximum threads count limit [{}>{}].", threads_count, threads_limit);

    std::array<std::thread, threads_limit> threads;

    for (std::size_t index = 0; index < threads_count; ++index)
    {
      threads[index] = std::move(std::thread(job));
    }

    for (std::size_t index = 0; index < threads_count; ++index)
    {
      threads[index].join();
    }
  }

  template <std::size_t threads_limit, template <typename> typename collection, typename command_string_type>
  void async_shell_execute(const collection<command_string_type>& commands, std::size_t threads_count)
  {
    std::atomic<std::size_t> vacant_job_index(0);
    auto job = [&vacant_job_index, &commands]()
      {
        while (true)
        {
          std::size_t index = vacant_job_index.fetch_add(1);
          if (index >= commands.size()) break;

          shell<stack_string_512> local_shell;
          local_shell.run(commands[index]);
        }
      };

    const std::size_t corrected_threads_count = std::min(commands.size(), threads_count);
    async_execute<threads_limit>(job, corrected_threads_count);
  }
}