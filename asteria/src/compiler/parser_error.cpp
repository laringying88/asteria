// This file is part of Asteria.
// Copyleft 2018 - 2019, LH_Mouse. All wrongs reserved.

#include "../precompiled.hpp"
#include "parser_error.hpp"
#include "../utilities.hpp"

namespace Asteria {

Parser_Error::~Parser_Error()
  {
  }

void Parser_Error::do_compose_message()
  {
    // Reuse the string.
    rocket::tinyfmt_str fmt;
    this->m_what.clear();
    fmt.set_string(rocket::move(this->m_what));
    // Write the status code in digital form.
    fmt << "asteria parser error " << this->m_stat << " at ";
    // Append the source location.
    if(this->m_line > 0) {
      fmt << "line " << this->m_line << ", offset " << this->m_offset << ", length " << this->m_length;
    }
    else {
      fmt << "the end of input data";
    }
    // Append the status description.
    fmt << ": " << describe_parser_status(this->m_stat);
    // Set the string.
    this->m_what = fmt.extract_string();
  }

static_assert(rocket::conjunction<std::is_nothrow_copy_constructible<Parser_Error>,
                                  std::is_nothrow_move_constructible<Parser_Error>,
                                  std::is_nothrow_copy_assignable<Parser_Error>,
                                  std::is_nothrow_move_assignable<Parser_Error>>::value, "");

}  // namespace Asteria
