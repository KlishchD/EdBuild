#pragma once

namespace building
{
  namespace output
  {
    enum orderings
    {
      original,
      errors_first,
      errors_last
    };

    enum sources
    {
      errors = (1 << 0),
      warnings = (1 << 1),
      notes = (1 << 2),
      unknowns = (1 << 3),

      all = notes | warnings | errors | unknowns
    };

    enum methods
    {
      minimal,
      descriptive
    };

    class printer
    {
    public:
      printer();

      printer& set_ordering(orderings new_ordering);
      printer& set_sources(sources new_source);
      printer& set_method(methods new_method);
      printer& print(compilation_results_list& results) const;
    protected:
      sources source;
      methods method;
      orderings ordering;
    };
  }
}