// This file is part of Asteria.
// Copyleft 2018 - 2020, LH_Mouse. All wrongs reserved.

#include "utils.hpp"
#include "../src/simple_script.hpp"
#include "../src/runtime/global_context.hpp"

using namespace asteria;

int main()
  {
    Simple_Script code;
    code.reload_string(
///////////////////////////////////////////////////////////////////////////////

      sref(__FILE__), __LINE__, sref(R"__(
        func recur(n) {
          std.debug.logf("recur($1)", n + 1);
          return recur(n + 1) + 1;
        }
        return recur(0);

///////////////////////////////////////////////////////////////////////////////
      )__"));
    Global_Context global;
    ASTERIA_TEST_CHECK_CATCH(code.execute(global));
  }
