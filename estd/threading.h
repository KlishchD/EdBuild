#pragma once

namespace estd
{
  template <typename tested_type>
  concept task_exectuion_controller = requires (tested_type object)
  {
    { object.can_proceed() } -> std::same_as<bool>;
  };

  template <typename tested_type>
  concept shell_async_parser = shell_output_parser<tested_type> && task_exectuion_controller<tested_type>;

  template <typename tested_type>
  concept async_execution_handler = requires(tested_type object, std::size_t task_index)
  {
    { object.aquire_task(task_index) } -> std::same_as<bool>;
    { object.execute(std::size_t(), std::size_t()) } -> std::same_as<void>;
  };

  struct default_async_shell_parser
  {
    void parse(const char* line, std::size_t length, const void* cookie)
    { /* Intentionally left empty. */ }

    bool can_proceed() const
    {
      return true;
    }
  };

  template <shell_async_parser parser_type, template <typename> typename collection, typename command_type>
  class async_shell_execution_handler
  {
  public:
    async_shell_execution_handler(const collection<command_type>& commands, parser_type& parser)
      : commands(commands), tasks_counter(0), parser(parser)
    { }

    bool aquire_task(std::size_t& index)
    {
      if (!parser.can_proceed()) return false;

      index = tasks_counter.fetch_add(1);
      return index < commands.size();
    }

    void execute(std::size_t command_index, std::size_t thread_index)
    {
      const auto& command = commands[command_index];

      if (command.alias.size())
      {
        estd::log("{}", command.alias.c_str());
      }
      else
      {
        estd::log("{}Executing command{}: {}.", colors::green(), colors::reset(), command.value.c_str());
      }

      shell local_shell;
      local_shell.run(command.value, parser, reinterpret_cast<const void*>(thread_index));
    }

    parser_type& get_parser()
    {
      return parser;
    }
  protected:
    const collection<command_type>& commands;
    std::atomic<std::size_t> tasks_counter;
    parser_type& parser;
  };

  template <std::size_t threads_limit, typename task_type>
  void async_execute(task_type task, std::size_t threads_count)
  {
    assert_condition(threads_count < threads_limit, "Exceeded maximum threads count limit [{}>{}].", threads_count, threads_limit);

    std::array<std::thread, threads_limit> threads;

    for (std::size_t index{ 0 }; index < threads_count; ++index)
    {
      threads[index] = std::move(std::thread(task, index));
    }

    for (std::size_t index{ 0 }; index < threads_count; ++index)
    {
      threads[index].join();
    }
  }

  template <std::size_t threads_limit, async_execution_handler handler_type>
  void async_managed_execute(handler_type& handler, std::size_t threads_count)
  {
    auto task = [&handler](std::size_t thread_index)
      {
        std::size_t task_index;
        while (handler.aquire_task(task_index))
        {
          handler.execute(task_index, thread_index);
        }
      };

    async_execute<threads_limit>(task, threads_count);
  }

  template <std::size_t threads_limit, typename parser_type, template <typename> typename collection, typename command_type>
  void async_shell_execute(const collection<command_type>& commands, parser_type& parser, std::size_t threads_count)
  {
    async_shell_execution_handler handler(commands, parser);

    const std::size_t corrected_threads_count = std::min(commands.size(), threads_count);
    async_managed_execute<threads_limit>(handler, corrected_threads_count);
  }

  template <std::size_t threads_limit, template <typename> typename collection, typename command_type>
  void async_shell_execute(const collection<command_type>& commands, std::size_t threads_count)
  {
    default_async_shell_parser parser;
    async_shell_execution_handler handler(commands, parser);

    const std::size_t corrected_threads_count = std::min(commands.size(), threads_count);
    async_managed_execute<threads_limit>(handler, corrected_threads_count);
  }
}