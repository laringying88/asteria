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
      sref(__FILE__), __LINE__, sref(R"__(
///////////////////////////////////////////////////////////////////////////////

        const chars = "0123456789abcdefghijklmnopqrstuvwxyz";
        // We presume these random strings will never match any real files.
        var dname = ".filesystem-test_dir_" + std.string.implode(std.array.shuffle(std.string.explode(chars)));
        var fname = ".filesystem-test_file_" + std.string.implode(std.array.shuffle(std.string.explode(chars)));

        assert std.filesystem.get_information(dname) == null;
        assert std.filesystem.get_information(fname) == null;

        assert std.filesystem.dir_create(dname) == 1;
        assert std.filesystem.get_information(dname).b_dir == true;
        assert std.filesystem.dir_create(dname) == 0;

        std.filesystem.file_append(dname + "/f1", "1");
        std.filesystem.file_append(dname + "/f2", "2");
        assert std.filesystem.dir_create(dname + "/f3") == 1;
        std.filesystem.file_append(dname + "/f3/a", "3");
        assert std.filesystem.dir_create(dname + "/f4") == 1;
        assert std.filesystem.dir_create(dname + "/f4/f5") == 1;
        std.filesystem.file_append(dname + "/f4/f5/a", "4");
        std.filesystem.file_append(dname + "/f4/f5/b", "5");
        assert std.array.sort(std.array.copy_keys(std.filesystem.dir_list(dname))) == ["f1","f2","f3","f4"];

        assert std.filesystem.remove_recursive(dname + "/f1") == 1;
        assert std.filesystem.remove_recursive(dname + "/f1") == 0;

        std.filesystem.move_from(dname + "/f5", dname + "/f2");
        try { std.filesystem.move_from(dname + "/f5", dname + "/f2");  assert false;  }
          catch(e) { assert std.string.find(e, "Assertion failure") == null;  }
        assert std.array.sort(std.array.copy_keys(std.filesystem.dir_list(dname))) == ["f3","f4","f5"];

        try { std.filesystem.file_remove(dname);  assert false;  }
          catch(e) { assert std.string.find(e, "Assertion failure") == null;  }
        try { std.filesystem.dir_remove(dname);  assert false;  }
          catch(e) { assert std.string.find(e, "Assertion failure") == null;  }
        assert std.filesystem.remove_recursive(dname) == 8;
        assert std.filesystem.dir_remove(dname) == 0;

        try { std.filesystem.file_read("/nonexistent") == null;  assert false;  }
          catch(e) { assert std.string.find(e, "Assertion failure") == null;  }
        std.filesystem.file_append(fname, "@@@@$$", true);  // "@@@@$$"
        assert std.filesystem.get_information(fname).b_dir == false;
        assert std.filesystem.get_information(fname).n_size == 6;
        std.filesystem.file_write(fname, "hello");  // "hello"
        assert std.filesystem.get_information(fname).n_size == 5;
        std.filesystem.file_write(fname, 3, "HELLO");  // "helHELLO"
        assert std.filesystem.get_information(fname).n_size == 8;
        std.filesystem.file_write(fname, 5, "#");  // "helHE#"
        assert std.filesystem.get_information(fname).n_size == 6;

        std.filesystem.file_append(fname, "??");  // "helHE#??"
        assert std.filesystem.get_information(fname).n_size == 8;
        std.filesystem.file_append(fname, "!!");  // "helHE#??!!"
        assert std.filesystem.get_information(fname).n_size == 10;
        try { std.filesystem.file_append(fname, "!!", true);  assert false;  }
          catch(e) { assert std.string.find(e, "Assertion failure") == null;  }

        assert std.filesystem.file_read(fname) == "helHE#??!!";
        assert std.filesystem.file_read(fname, 2) == "lHE#??!!";
        assert std.filesystem.file_read(fname, 1000) == "";
        assert std.filesystem.file_read(fname, 2, 1000) == "lHE#??!!";
        assert std.filesystem.file_read(fname, 2, 3) == "lHE";

        std.filesystem.file_copy_from(fname + ".2", fname);
        assert std.filesystem.file_read(fname + ".2") == "helHE#??!!";

        var data = "";
        var appender = func(off, str) { data += str;  };
        try { std.filesystem.file_stream("/nonexistent", appender);  assert false;  }
          catch(e) { assert std.string.find(e, "Assertion failure") == null;  }
        assert std.filesystem.file_stream(fname, appender) == 10;
        assert data == "helHE#??!!";
        data = "";
        assert std.filesystem.file_stream(fname, appender, 2) == 8;
        assert data == "lHE#??!!";
        data = "";
        assert std.filesystem.file_stream(fname, appender, 1000) == 0;
        assert data == "";
        data = "";
        assert std.filesystem.file_stream(fname, appender, 2, 1000) == 8;
        assert data == "lHE#??!!";
        data = "";
        assert std.filesystem.file_stream(fname, appender, 2, 3) == 3;
        assert data == "lHE";

        try { std.filesystem.dir_create(fname);  assert false;  }
          catch(e) { assert std.string.find(e, "Assertion failure") == null;  }
        assert std.filesystem.file_remove(fname) == 1;
        assert std.filesystem.file_remove(fname) == 0;
        assert std.filesystem.file_remove(fname + ".2") == 1;

        assert std.filesystem.dir_create(fname) == 1;
        try { std.filesystem.file_remove(fname);  assert false;  }
          catch(e) { assert std.string.find(e, "Assertion failure") == null;  }
        assert std.filesystem.dir_remove(fname) == 1;

///////////////////////////////////////////////////////////////////////////////
      )__"));
    Global_Context global;
    code.execute(global);
  }
