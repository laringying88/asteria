// This file is part of Asteria.
// Copyleft 2018 - 2020, LH_Mouse. All wrongs reserved.

#include "../precompiled.hpp"
#include "bindings_gc.hpp"
#include "argument_reader.hpp"
#include "simple_binding_wrapper.hpp"
#include "../runtime/global_context.hpp"
#include "../runtime/generational_collector.hpp"
#include "../utilities.hpp"

namespace Asteria {

Iopt std_gc_tracked_count(Global& global, Ival generation)
  {
    auto gc_gen = static_cast<GC_Generation>(::rocket::clamp(generation,
                        static_cast<Ival>(gc_generation_newest), static_cast<Ival>(gc_generation_oldest)));
    if(gc_gen != generation) {
      return nullopt;
    }
    // Get the collector.
    const auto& coll = global.generational_collector()->get_collector(gc_gen);
    // Get the current number of variables being tracked.
    size_t count = coll.count_tracked_variables();
    return static_cast<int64_t>(count);
  }

Iopt std_gc_get_threshold(Global& global, Ival generation)
  {
    auto gc_gen = static_cast<GC_Generation>(::rocket::clamp(generation,
                        static_cast<Ival>(gc_generation_newest), static_cast<Ival>(gc_generation_oldest)));
    if(gc_gen != generation) {
      return nullopt;
    }
    // Get the collector.
    const auto& coll = global.generational_collector()->get_collector(gc_gen);
    // Get the current threshold.
    uint32_t thres = coll.get_threshold();
    return static_cast<int64_t>(thres);
  }

Iopt std_gc_set_threshold(Global& global, Ival generation, Ival threshold)
  {
    auto gc_gen = static_cast<GC_Generation>(::rocket::clamp(generation,
                        static_cast<Ival>(gc_generation_newest), static_cast<Ival>(gc_generation_oldest)));
    if(gc_gen != generation) {
      return nullopt;
    }
    // Get the collector.
    auto& coll = global.generational_collector()->open_collector(gc_gen);
    // Set the threshold and return its old value.
    uint32_t thres = coll.get_threshold();
    coll.set_threshold(static_cast<uint32_t>(::rocket::clamp(threshold, 0, INT32_MAX)));
    return static_cast<int64_t>(thres);
  }

Ival std_gc_collect(Global& global, Iopt generation_limit)
  {
    auto gc_limit = gc_generation_oldest;
    // Unlike others, this function does not fail if `generation_limit` is out of range.
    if(generation_limit) {
      gc_limit = static_cast<GC_Generation>(::rocket::clamp(*generation_limit,
                       static_cast<Ival>(gc_generation_newest), static_cast<Ival>(gc_generation_oldest)));
    }
    // Perform garbage collection up to the generation specified.
    size_t nvars = global.generational_collector()->collect_variables(gc_limit);
    return static_cast<int64_t>(nvars);
  }

void create_bindings_gc(Oval& result, API_Version /*version*/)
  {
    //===================================================================
    // `std.gc.tracked_count()`
    //===================================================================
    result.insert_or_assign(::rocket::sref("tracked_count"),
      Fval(::rocket::make_refcnt<Simple_Binding_Wrapper>(
        // Description
        ::rocket::sref(
          "\n"
          "`std.gc.count_tracked(generation)`\n"
          "\n"
          "  * Gets the number of variables that are being tracked by the\n"
          "    collector for `generation`. Valid values for `generation` are\n"
          "    `0`, `1` and `2`.\n"
          "\n"
          "  * Returns the number of variables being tracked. This value is\n"
          "    only informative. If `generation` is not valid, `null` is\n"
          "    returned.\n"
        ),
        // Definition
        [](cow_vector<Reference>&& args, Reference&& /*self*/, Global& global) -> Value  {
          Argument_Reader reader(::rocket::sref("std.gc.tracked_count"), ::rocket::ref(args));
          // Parse arguments.
          Ival generation;
          if(reader.I().g(generation).F()) {
            // Call the binding function.
            return std_gc_tracked_count(global, ::rocket::move(generation));
          }
          // Fail.
          reader.throw_no_matching_function_call();
        })
      ));
    //===================================================================
    // `std.gc.get_threshold()`
    //===================================================================
    result.insert_or_assign(::rocket::sref("get_threshold"),
      Fval(::rocket::make_refcnt<Simple_Binding_Wrapper>(
        // Description
        ::rocket::sref(
          "\n"
          "`std.gc.get_threshold(generation)`\n"
          "\n"
          "  * Gets the threshold of the collector for `generation`. Valid\n"
          "    values for `generation` are `0`, `1` and `2`.\n"
          "\n"
          "  * Returns the threshold. If `generation` is not valid, `null` is\n"
          "    returned.\n"
        ),
        // Definition
        [](cow_vector<Reference>&& args, Reference&& /*self*/, Global& global) -> Value  {
          Argument_Reader reader(::rocket::sref("std.gc.get_threshold"), ::rocket::ref(args));
          // Parse arguments.
          Ival generation;
          if(reader.I().g(generation).F()) {
            // Call the binding function.
            return std_gc_get_threshold(global, ::rocket::move(generation));
          }
          // Fail.
          reader.throw_no_matching_function_call();
        })
      ));
    //===================================================================
    // `std.gc.set_threshold()`
    //===================================================================
    result.insert_or_assign(::rocket::sref("set_threshold"),
      Fval(::rocket::make_refcnt<Simple_Binding_Wrapper>(
        // Description
        ::rocket::sref(
          "\n"
          "`std.gc.set_threshold(generation, threshold)`\n"
          "\n"
          "  * Sets the threshold of the collector for `generation` to\n"
          "    `threshold`. Valid values for `generation` are `0`, `1` and\n"
          "    `2`. Valid values for `threshold` range from `0` to an\n"
          "    unspecified positive integer; overlarge values are capped\n"
          "    silently without failure. A larger `threshold` makes garbage\n"
          "    collection run less often but slower. Setting `threshold` to\n"
          "    `0` ensures all unreachable variables be collected immediately.\n"
          "\n"
          "  * Returns the threshold before the call. If `generation` is not\n"
          "    valid, `null` is returned.\n"
        ),
        // Definition
        [](cow_vector<Reference>&& args, Reference&& /*self*/, Global& global) -> Value  {
          Argument_Reader reader(::rocket::sref("std.gc.set_threshold"), ::rocket::ref(args));
          // Parse arguments.
          Ival generation;
          Ival threshold;
          if(reader.I().g(generation).g(threshold).F()) {
            // Call the binding function.
            return std_gc_set_threshold(global, ::rocket::move(generation), ::rocket::move(threshold));
          }
          // Fail.
          reader.throw_no_matching_function_call();
        })
      ));
    //===================================================================
    // `std.gc.collect()`
    //===================================================================
    result.insert_or_assign(::rocket::sref("collect"),
      Fval(::rocket::make_refcnt<Simple_Binding_Wrapper>(
        // Description
        ::rocket::sref(
          "\n"
          "`std.gc.collect([generation_limit])`\n"
          "\n"
          "  * Performs garbage collection on all generations including and\n"
          "    up to `generation_limit`. If it is absent, all generations are\n"
          "    collected.\n"
          "\n"
          "  * Returns the number of variables that have been collected in\n"
          "    total.\n"
        ),
        // Definition
        [](cow_vector<Reference>&& args, Reference&& /*self*/, Global& global) -> Value  {
          Argument_Reader reader(::rocket::sref("std.gc.collect"), ::rocket::ref(args));
          // Parse arguments.
          Iopt generation_limit;
          if(reader.I().g(generation_limit).F()) {
            // Call the binding function.
            return std_gc_collect(global, ::rocket::move(generation_limit));
          }
          // Fail.
          reader.throw_no_matching_function_call();
        })
      ));
    //===================================================================
    // End of `std.gc`
    //===================================================================
  }

}  // namespace Asteria
