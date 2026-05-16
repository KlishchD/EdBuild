#include "EdBuild.h"
#include "compilers/compiler_translator.h"
#include "compilation_results_printer.h"

namespace building
{
  namespace output
  {
    bool test_source(error_types type, sources filter)
    {
      return uint32_t(type) & uint32_t(filter);
    }

    printer::printer()
      : source(sources::all),
      method(methods::minimal),
      ordering(orderings::original)
    {

    }

    printer& printer::set_ordering(orderings new_ordering)
    {
      ordering = new_ordering;
      return *this;
    }

    printer& printer::set_sources(sources new_source)
    {
      source = new_source;
      return *this;
    }

    printer& printer::set_method(methods new_method)
    {
      method = new_method;
      return *this;
    }

    printer& printer::print(compilation_results_list& results)
    {
      const bool is_reorderin_needed = ordering != orderings::original;
      if (is_reorderin_needed)
      {
        std::sort(results.begin(), results.end(),
          [this](const compilation_result& left, const compilation_result& right)
          {
            switch (ordering)
            {
            case orderings::errors_first: return static_cast<uint32_t>(left.type) < static_cast<uint32_t>(right.type);
            case orderings::errors_last: return static_cast<uint32_t>(left.type) < static_cast<uint32_t>(right.type);
            default: estd::no_default("Ordering type [{}] is not supported.", static_cast<uint32_t>(ordering));
            }

            return false;
          });
      }

      uint32_t notes = 0;
      uint32_t warrnings = 0;
      uint32_t errors = 0;
      uint32_t unknowns = 0;

      estd::log("\n{}Compilations results{}:", estd::colors::yellow(), estd::colors::reset());
      for (const auto& result : results)
      {
        const char* name = get_type_name(result.type);
        const char* color = get_type_color(result.type);

        if (test_source(result.type, source))
        {
          estd::log("{}{:7}{} [{:4}:{:4}] {:50}: {}",
            color, name, estd::colors::reset(),
            result.line, result.column,
            result.file.c_str(),
            result.message.c_str());
        }

        switch (result.type)
        {
        case error_types::error: ++errors; break;
        case error_types::warning: ++warrnings; break;
        case error_types::note: ++notes; break;
        default: ++unknowns; break;
        }
      }

      estd::log("{}Errors{}: {}. {}Warrnings{}: {}. {}Notes{}: {}. {}Unknowns{}: {}.",
        estd::colors::red(), estd::colors::reset(), errors,
        estd::colors::yellow(), estd::colors::reset(), warrnings,
        estd::colors::blue(), estd::colors::reset(), notes,
        estd::colors::reset(), estd::colors::reset(), unknowns
      );

      return *this;
    }
  }
}
