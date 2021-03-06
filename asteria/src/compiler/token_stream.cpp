// This file is part of Asteria.
// Copyleft 2018 - 2020, LH_Mouse. All wrongs reserved.

#include "../precompiled.hpp"
#include "token_stream.hpp"
#include "enums.hpp"
#include "token.hpp"
#include "parser_error.hpp"
#include "../utils.hpp"

namespace asteria {
namespace {

class Line_Reader
  {
  private:
    tinybuf* m_cbuf;
    cow_string m_file;

    size_t m_line = 0;
    size_t m_off = 0;
    cow_string m_str;

  public:
    explicit
    Line_Reader(tinybuf& xcbuf, const cow_string& xfile, size_t xline)
      : m_cbuf(&xcbuf), m_file(xfile), m_line(xline - 1)
      { }

  public:
    ASTERIA_NONCOPYABLE_DESTRUCTOR(Line_Reader)
      = default;

    tinybuf&
    cbuf()
      const noexcept
      { return *(this->m_cbuf);  }

    const cow_string&
    file()
      const noexcept
      { return this->m_file;  }

    int
    line()
      const noexcept
      { return static_cast<int>(this->m_line);  }

    int
    offset()
      const noexcept
      { return static_cast<int>(this->m_off);  }

    Source_Location
    tell()
      const noexcept
      { return Source_Location(this->file(), this->line(), this->offset());  }

    bool
    next_line()
      {
        // Clear the current line.
        this->m_str.clear();
        this->m_off = 0;

        // Buffer a line.
        int ch;
        while(((ch = this->m_cbuf->getc()) != EOF) && (ch != '\n'))
          this->m_str.push_back(static_cast<char>(ch));

        // Fail if the last line is empty and EOF is encountered.
        if(this->m_str.empty() && (ch == EOF))
          return false;

        // Accept the line.
        // Increment the line number if a line has been read successfully.
        this->m_line++;
        return true;
      }

    size_t
    navail()
      const noexcept
      { return this->m_str.size() - this->m_off;  }

    const char*
    data(size_t add = 0)
      const
      {
        if(add > this->navail())
          ASTERIA_THROW("Attempt to seek past end of line (`$1` + `$2` > `$3`)",
                        this->m_off, add, this->m_str.size());
        return this->m_str.data() + (this->m_off + add);
      }

    char
    peek(size_t add = 0)
      const noexcept
      {
        if(add > this->navail())
          return 0;
        return this->m_str[this->m_off + add];
      }

    void
    consume(size_t add)
      {
        if(add > this->navail())
          ASTERIA_THROW("Attempt to seek past end of line (`$1` + `$2` > `$3`)",
                        this->m_off, add, this->m_str.size());
        this->m_off += add;
      }

    void
    rewind(size_t off = 0)
      {
        if(off > this->m_str.size())
          ASTERIA_THROW("Invalid offset within current line (`$1` > `$2`)",
                        off, this->m_str.size());
        this->m_off = off;
      }
  };

template<typename XTokenT>
bool
do_push_token(cow_vector<Token>& tokens, Line_Reader& reader, size_t tlen,
              XTokenT&& xtoken)
  {
    tokens.emplace_back(reader.tell(), tlen, ::std::forward<XTokenT>(xtoken));
    reader.consume(tlen);
    return true;
  }

bool
do_may_infix_operators_follow(cow_vector<Token>& tokens)
  {
    // Infix operators are not allowed at the beginning.
    if(tokens.empty())
      return false;

    // Infix operators may follow if the keyword denotes a value or reference.
    const auto& p = tokens.back();
    if(p.is_keyword())
      return ::rocket::is_any_of(p.as_keyword(),
                 { keyword_null, keyword_true, keyword_false, keyword_this });

    // Infix operators may follow if the punctuator can terminate an expression.
    if(p.is_punctuator())
      return ::rocket::is_any_of(p.as_punctuator(),
                 { punctuator_inc, punctuator_dec, punctuator_head, punctuator_tail,
                   punctuator_parenth_cl, punctuator_bracket_cl, punctuator_brace_cl });

    // Infix operators can follow.
    return true;
  }

size_t
do_mask_length(size_t& tlen, Line_Reader& reader, uint8_t mask)
  {
    size_t mlen = 0;
    while(is_cctype(reader.peek(tlen), mask)) {
      mlen += 1;
      tlen += 1;
    }
    return mlen;
  }

cow_string&
do_collect_digits(cow_string& tstr, size_t& tlen, Line_Reader& reader, uint8_t mask)
  {
    for(;;) {
      // Skip a digit separator.
      char c = reader.peek(tlen);
      if(c == '`') {
        tlen += 1;
        continue;
      }

      // Stop at an unwanted character.
      if(!is_cctype(c, mask))
        break;

      // Collect a digit.
      tstr += c;
      tlen += 1;
    }
    return tstr;
  }

bool
do_accept_numeric_literal(cow_vector<Token>& tokens, Line_Reader& reader,
                          bool integers_as_reals)
  {
    // numeric-literal ::=
    //   number-sign-opt ( binary-literal | decimal-literal | hexadecimal-literal )
    //   exponent-suffix-opt
    // number-sign-opt ::=
    //   PCRE([+-]?)
    // binary-literal ::=
    //   PCRE(0[bB]([01]`?)+(\.([01]`?)+))
    // decimal-literal ::=
    //   PCRE(([0-9]`?)+(\.([0-9]`?)+))
    // hexadecimal-literal ::=
    //   PCRE(0[xX]([0-9A-Fa-f]`?)+(\.([0-9A-Fa-f]`?)+))
    // exponent-suffix-opt ::=
    //   decimal-exponent-suffix | binary-exponent-suffix | ""
    // decimal-exponent-suffix ::=
    //   PCRE([eE][-+]?([0-9]`?)+)
    // binary-exponent-suffix ::=
    //   PCRE([pP][-+]?([0-9]`?)+)
    cow_string tstr;
    size_t tlen = 0;

    // These specify what are allowed in each individual part of the literal.
    double sign = 1;
    bool has_point = false;
    uint8_t mmask = cctype_digit;
    uint8_t expch = 'e';

    // Look for an explicit sign symbol.
    switch(reader.peek(tlen)) {
      case '+':
        tstr = sref("+");
        tlen += 1;
        break;

      case '-':
        tstr = sref("-");
        tlen += 1;
        sign = -1;
        break;
    }

    // If a sign symbol exists in a context where an infix operator is allowed, it is
    // treated as the latter.
    if((tlen != 0) && do_may_infix_operators_follow(tokens))
      return false;

    switch(reader.peek(tlen)) {
      case 'n':
      case 'N': {
        if(do_mask_length(tlen, reader, cctype_namei | cctype_digit) != 3)
          return false;

        auto sptr = reader.data() + tlen - 3;
        if((sptr[1] != 'a') || (sptr[2] != sptr[0]))  // `nan` or `NaN`
          return false;

        Token::S_real_literal xtoken;
        xtoken.val = ::std::copysign(::std::numeric_limits<V_real>::quiet_NaN(), sign);
        return do_push_token(tokens, reader, tlen, ::std::move(xtoken));
      }

      case 'i':
      case 'I': {
        if(do_mask_length(tlen, reader, cctype_namei | cctype_digit) != 8)
          return false;

        auto sptr = reader.data() + tlen - 8;
        if(!mem_equal(sptr + 1, "nfinity", 7))  // `infinity` or `Infinity`
          return false;

        Token::S_real_literal xtoken;
        xtoken.val = ::std::copysign(::std::numeric_limits<V_real>::infinity(), sign);
        return do_push_token(tokens, reader, tlen, ::std::move(xtoken));
      }

      case '0':
        tstr += reader.peek(tlen);
        tlen += 1;

        // Check the radix identifier.
        if(::rocket::is_any_of(reader.peek(tlen) | 0x20, { 'b', 'x' })) {
          tstr += reader.peek(tlen);
          tlen += 1;

          // Accept the radix identifier.
          mmask = cctype_xdigit;
          expch = 'p';
        }

        // Fallthrough
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
        break;

      default:
        return false;
    }

    // Accept the longest string composing the integral part.
    do_collect_digits(tstr, tlen, reader, mmask);

    // Check for a radix point. If one exists, the fractional part shall follow.
    if(reader.peek(tlen) == '.') {
      tstr += reader.peek(tlen);
      tlen += 1;

      // Accept the fractional part.
      has_point = true;
      do_collect_digits(tstr, tlen, reader, mmask);
    }

    // Check for the exponent.
    if((reader.peek(tlen) | 0x20) == expch) {
      tstr += reader.peek(tlen);
      tlen += 1;

      // Check for an optional sign symbol.
      if(::rocket::is_any_of(reader.peek(tlen), { '+', '-' })) {
        tstr += reader.peek(tlen);
        tlen += 1;
      }

      // Accept the exponent.
      do_collect_digits(tstr, tlen, reader, cctype_digit);
    }

    // Accept numeric suffixes.
    // Note, at the moment we make no use of such suffixes, so any suffix will
    // definitely cause parser errors.
    do_collect_digits(tstr, tlen, reader, cctype_alpha | cctype_digit);

    // Convert the token to a literal.
    // We always parse the literal as a floating-point number.
    ::rocket::ascii_numget numg;
    const char* bp = tstr.c_str();
    const char* ep = bp + tstr.size();
    if(!numg.parse_F(bp, ep))
      throw Parser_Error(parser_status_numeric_literal_invalid, reader.tell(), tlen);

    if(bp != ep)
      throw Parser_Error(parser_status_numeric_literal_suffix_invalid,
                         reader.tell(), tlen);

    // It is cast to an integer only when `integers_as_reals` is `false` and it does not
    // contain a radix point.
    if(!integers_as_reals && !has_point) {
      // Try casting the value to an `integer`.
      Token::S_integer_literal xtoken;
      numg.cast_I(xtoken.val, INT64_MIN, INT64_MAX);

      // Check for errors. Note that integer casts never underflows.
      if(numg.overflowed())
        throw Parser_Error(parser_status_integer_literal_overflow, reader.tell(), tlen);

      if(numg.inexact())
        throw Parser_Error(parser_status_integer_literal_inexact, reader.tell(), tlen);

      if(!numg)
        throw Parser_Error(parser_status_numeric_literal_invalid, reader.tell(), tlen);

      // Accept the integral value.
      return do_push_token(tokens, reader, tlen, ::std::move(xtoken));
    }
    else {
      // Try casting the value to a `real`.
      Token::S_real_literal xtoken;
      numg.cast_F(xtoken.val, -DBL_MAX, DBL_MAX);

      // Check for errors. Note that integer casts are never inexact.
      if(numg.overflowed())
        throw Parser_Error(parser_status_real_literal_overflow, reader.tell(), tlen);

      if(numg.underflowed())
        throw Parser_Error(parser_status_real_literal_underflow, reader.tell(), tlen);

      if(!numg)
        throw Parser_Error(parser_status_numeric_literal_invalid, reader.tell(), tlen);

      // Accept the real value.
      return do_push_token(tokens, reader, tlen, ::std::move(xtoken));
    }
  }

struct Prefix_Comparator
  {
    template<typename ElementT>
    bool
    operator()(const ElementT& lhs, const ElementT& rhs)
      const noexcept
      { return ::rocket::char_traits<char>::compare(
                   lhs.str, rhs.str, ::rocket::size(lhs.str)) < 0;  }

    template<typename ElementT>
    bool
    operator()(char lhs, const ElementT& rhs)
      const noexcept
      { return ::rocket::char_traits<char>::lt(lhs, rhs.str[0]);  }

    template<typename ElementT>
    bool
    operator()(const ElementT& lhs, char rhs)
      const noexcept
      { return ::rocket::char_traits<char>::lt(lhs.str[0], rhs);  }
  };

template<typename ElementT, size_t N>
pair<ElementT*, ElementT*>
do_prefix_range(ElementT (&table)[N], char ch)
  {
    Prefix_Comparator comp;
#ifdef ROCKET_DEBUG
    ROCKET_ASSERT(::std::is_sorted(table, table + N, comp));
#endif
    return ::std::equal_range(table, table + N, ch, comp);
  }

struct Punctuator_Element
  {
    char str[6];
    Punctuator punct;
  }
constexpr s_punctuators[] =
  {
    { "!",     punctuator_notl        },
    { "!=",    punctuator_cmp_ne      },
    { "%",     punctuator_mod         },
    { "%=",    punctuator_mod_eq      },
    { "&",     punctuator_andb        },
    { "&&",    punctuator_andl        },
    { "&&=",   punctuator_andl_eq     },
    { "&=",    punctuator_andb_eq     },
    { "(",     punctuator_parenth_op  },
    { ")",     punctuator_parenth_cl  },
    { "*",     punctuator_mul         },
    { "*=",    punctuator_mul_eq      },
    { "+",     punctuator_add         },
    { "++",    punctuator_inc         },
    { "+=",    punctuator_add_eq      },
    { ",",     punctuator_comma       },
    { "-",     punctuator_sub         },
    { "--",    punctuator_dec         },
    { "-=",    punctuator_sub_eq      },
    { "->",    punctuator_arrow       },
    { ".",     punctuator_dot         },
    { "...",   punctuator_ellipsis    },
    { "/",     punctuator_div         },
    { "/=",    punctuator_div_eq      },
    { ":",     punctuator_colon       },
    { "::",    punctuator_scope       },
    { ";",     punctuator_semicol     },
    { "<",     punctuator_cmp_lt      },
    { "<<",    punctuator_sla         },
    { "<<<",   punctuator_sll         },
    { "<<<=",  punctuator_sll_eq      },
    { "<<=",   punctuator_sla_eq      },
    { "<=",    punctuator_cmp_lte     },
    { "<=>",   punctuator_spaceship   },
    { "=",     punctuator_assign      },
    { "==",    punctuator_cmp_eq      },
    { ">",     punctuator_cmp_gt      },
    { ">=",    punctuator_cmp_gte     },
    { ">>",    punctuator_sra         },
    { ">>=",   punctuator_sra_eq      },
    { ">>>",   punctuator_srl         },
    { ">>>=",  punctuator_srl_eq      },
    { "?",     punctuator_quest       },
    { "?=",    punctuator_quest_eq    },
    { "?\?",   punctuator_coales      },
    { "?\?=",  punctuator_coales_eq   },
    { "[",     punctuator_bracket_op  },
    { "[$]",   punctuator_tail        },
    { "[^]",   punctuator_head        },
    { "]",     punctuator_bracket_cl  },
    { "^",     punctuator_xorb        },
    { "^=",    punctuator_xorb_eq     },
    { "{",     punctuator_brace_op    },
    { "|",     punctuator_orb         },
    { "|=",    punctuator_orb_eq      },
    { "||",    punctuator_orl         },
    { "||=",   punctuator_orl_eq      },
    { "}",     punctuator_brace_cl    },
    { "~",     punctuator_notb        },
  };

bool
do_accept_punctuator(cow_vector<Token>& tokens, Line_Reader& reader)
  {
    // For two elements X and Y, if X is in front of Y, then X is potential a prefix
    // of Y. Traverse the range backwards to prevent premature matches, as a token is
    // defined to be the longest valid character sequence.
    auto r = do_prefix_range(s_punctuators, reader.peek());
    while(r.first != r.second) {
      const auto& cur = *--(r.second);

      size_t tlen = ::std::strlen(cur.str);
      if(reader.navail() < tlen)
        continue;

      if(!mem_equal(reader.data(), cur.str, tlen))
        continue;

      Token::S_punctuator xtoken = { cur.punct };
      return do_push_token(tokens, reader, tlen, ::std::move(xtoken));
    }

    // No punctuator has been accepted.
    return false;
  }

bool
do_accept_string_literal(cow_vector<Token>& tokens, Line_Reader& reader, char head,
                         bool escapable)
  {
    // string-literal ::=
    //   escape-string-literal | noescape-string-literal
    // escape-string-literal ::=
    //   PCRE("([^\\]|(\\([abfnrtveZ0'"?\\/]|(x[0-9A-Fa-f]{2})|(u[0-9A-Fa-f]{4})|
    //         (U[0-9A-Fa-f]{6}))))*?")
    // noescape-string-literal ::=
    //   PCRE('[^']*?')
    if(reader.peek() != head)
      return false;

    // Get a string literal.
    size_t tlen = 1;
    cow_string val;
    for(;;) {
      // Read a character.
      char next = reader.peek(tlen);
      if(next == 0)
        throw Parser_Error(parser_status_string_literal_unclosed,
                           reader.tell(), tlen);

      tlen += 1;

      // Check it.
      if(next == head) {
        // The end of this string is encountered. Finish.
        break;
      }
      if(!escapable || (next != '\\')) {
        // This character does not start an escape sequence. Copy it as is.
        val.push_back(next);
        continue;
      }

      // Translate this escape sequence.
      // Read the next charactter.
      next = reader.peek(tlen);
      if(next == 0)
        throw Parser_Error(parser_status_escape_sequence_incomplete,
                           reader.tell(), tlen);
      tlen += 1;

      // Translate it.
      int xcnt = 0;
      switch(next) {
        case '\'':
        case '\"':
        case '\\':
        case '?':
        case '/':
          val.push_back(next);
          break;

        case 'a':
          val.push_back('\a');
          break;

        case 'b':
          val.push_back('\b');
          break;

        case 'f':
          val.push_back('\f');
          break;

        case 'n':
          val.push_back('\n');
          break;

        case 'r':
          val.push_back('\r');
          break;

        case 't':
          val.push_back('\t');
          break;

        case 'v':
          val.push_back('\v');
          break;

        case '0':
          val.push_back('\0');
          break;

        case 'Z':
          val.push_back('\x1A');
          break;

        case 'e':
          val.push_back('\x1B');
          break;

        case 'U':
          xcnt += 2;
          // Fallthrough
        case 'u':
          xcnt += 2;
          // Fallthrough
        case 'x': {
          // How many hex digits are there?
          xcnt += 2;

          // Read hex digits.
          char32_t cp = 0;
          for(int i = 0;  i < xcnt;  ++i) {
            // Read a hex digit.
            char c = reader.peek(tlen);
            if(c == 0)
              throw Parser_Error(parser_status_escape_sequence_incomplete,
                                 reader.tell(), tlen);

            if(!is_cctype(c, cctype_xdigit))
              throw Parser_Error(parser_status_escape_sequence_invalid_hex,
                                 reader.tell(), tlen);

            // Accumulate this digit.
            tlen += 1;
            uint32_t dval = static_cast<uint8_t>(c);
            dval |= 0x20;

            cp *= 16;
            cp += (dval <= '9') ? (dval - '0') : (dval - 'a' + 10);
          }

          if(next == 'x') {
            // Write the character verbatim.
            val.push_back(static_cast<char>(cp));
          }
          else {
            // Write a Unicode code point.
            if(!utf8_encode(val, cp))
              throw Parser_Error(parser_status_escape_utf_code_point_invalid,
                                 reader.tell(), tlen);
          }
          break;
        }

        default:
          throw Parser_Error(parser_status_escape_sequence_unknown, reader.tell(), tlen);
      }
    }

    Token::S_string_literal xtoken = { ::std::move(val) };
    return do_push_token(tokens, reader, tlen, ::std::move(xtoken));
  }

struct Keyword_Element
  {
    char str[10];
    Keyword kwrd;
  }
constexpr s_keywords[] =
  {
    { "__abs",     keyword_abs       },
    { "__ceil",    keyword_ceil      },
    { "__floor",   keyword_floor     },
    { "__fma",     keyword_fma       },
    { "__global",  keyword_global    },
    { "__iceil",   keyword_iceil     },
    { "__ifloor",  keyword_ifloor    },
    { "__iround",  keyword_iround    },
    { "__isinf",   keyword_isinf     },
    { "__isnan",   keyword_isnan     },
    { "__itrunc",  keyword_itrunc    },
    { "__round",   keyword_round     },
    { "__sign",    keyword_sign      },
    { "__sqrt",    keyword_sqrt      },
    { "__trunc",   keyword_trunc     },
    { "__vcall",   keyword_vcall     },
    { "and",       keyword_and       },
    { "assert",    keyword_assert    },
    { "break",     keyword_break     },
    { "case",      keyword_case      },
    { "catch",     keyword_catch     },
    { "const",     keyword_const     },
    { "continue",  keyword_continue  },
    { "countof",   keyword_countof   },
    { "default",   keyword_default   },
    { "defer",     keyword_defer     },
    { "do",        keyword_do        },
    { "each",      keyword_each      },
    { "else",      keyword_else      },
    { "false",     keyword_false     },
    { "for",       keyword_for       },
    { "func",      keyword_func      },
    { "if",        keyword_if        },
    { "import",    keyword_import    },
    { "not",       keyword_not       },
    { "null",      keyword_null      },
    { "or",        keyword_or        },
    { "ref",       keyword_ref       },
    { "return",    keyword_return    },
    { "switch",    keyword_switch    },
    { "this",      keyword_this      },
    { "throw",     keyword_throw     },
    { "true",      keyword_true      },
    { "try",       keyword_try       },
    { "typeof",    keyword_typeof    },
    { "unset",     keyword_unset     },
    { "var",       keyword_var       },
    { "while",     keyword_while     },
  };

bool
do_accept_identifier_or_keyword(cow_vector<Token>& tokens, Line_Reader& reader,
                                bool keywords_as_identifiers)
  {
    // identifier ::=
    //   PCRE([A-Za-z_][A-Za-z_0-9]*)
    if(!is_cctype(reader.peek(), cctype_namei))
      return false;

    // Check for keywords if not otherwise disabled.
    size_t tlen = 0;
    do_mask_length(tlen, reader, cctype_namei | cctype_digit);

    if(!keywords_as_identifiers) {
      auto r = do_prefix_range(s_keywords, reader.peek());
      while(r.first != r.second) {
        const auto& cur = *(r.first++);

        if(::std::strlen(cur.str) != tlen)
          continue;

        if(!mem_equal(reader.data(), cur.str, tlen))
          continue;

        Token::S_keyword xtoken = { cur.kwrd };
        return do_push_token(tokens, reader, tlen, ::std::move(xtoken));
      }
    }

    // Accept a plain identifier.
    Token::S_identifier xtoken = { cow_string(reader.data(), tlen) };
    return do_push_token(tokens, reader, tlen, ::std::move(xtoken));
  }

}  // namespace

Token_Stream::
~Token_Stream()
  {
  }

Token_Stream&
Token_Stream::
reload(const cow_string& file, int line, tinybuf& cbuf)
  {
    // Tokens are parsed and stored here in normal order.
    // We will have to reverse this sequence before storing it into `*this` if
    // it is accepted. The storage may be reused.
    cow_vector<Token> tokens;
    tokens.swap(this->m_rtoks);
    tokens.clear();

    // Save the position of an unterminated block comment.
    opt<pair<Source_Location, size_t>> bcomm;

    // Read source code line by line.
    Line_Reader reader(cbuf, file, static_cast<size_t>(line));
    while(reader.next_line()) {
      // Discard the first line if it looks like a shebang.
      if((reader.line() == line) && (::std::strncmp(reader.data(), "#!", 2) == 0))
        continue;

      // Check for conflict markers.
      if((reader.navail() >= 7)
         && ::rocket::is_any_of(reader.peek(), { '<', '|', '=', '>' })
         && ::std::all_of(reader.data() + 1, reader.data() + 7,
                          [&](char ch) { return ch == reader.peek();  }))
        throw Parser_Error(parser_status_conflict_marker_detected, reader.tell(), 7);

      // Ensure this line is a valid UTF-8 string.
      while(reader.navail() != 0) {
        // Decode a code point.
        char32_t cp;
        auto tptr = reader.data();
        if(!utf8_decode(cp, tptr, reader.navail()))
          throw Parser_Error(parser_status_utf8_sequence_invalid,
                             reader.tell(), reader.navail());

        // Disallow plain null characters in source data.
        if(cp == 0)
          throw Parser_Error(parser_status_null_character_disallowed, reader.tell(), 1);

        // Accept this code point.
        reader.consume(static_cast<size_t>(tptr - reader.data()));
      }

      // Re-scan this line from the beginning.
      reader.rewind();

      // Break this line down into tokens.
      while(reader.navail() != 0) {
        // Are we inside a block comment?
        if(bcomm) {
          // Search for the terminator of this block comment.
          auto tptr = ::std::strstr(reader.data(), "*/");
          if(!tptr)
            break;

          // Finish this comment and resume from the end of it.
          bcomm.reset();
          reader.consume(static_cast<size_t>(tptr + 2 - reader.data()));
          continue;
        }

        // Read a character.
        if(is_cctype(reader.peek(), cctype_space)) {
          // Skip a space.
          reader.consume(1);
          continue;
        }

        if(reader.peek() == '/') {
          if(reader.peek(1) == '/') {
            // Start a line comment. Discard all remaining characters in this line.
            break;
          }
          if(reader.peek(1) == '*') {
            // Start a block comment.
            bcomm.emplace(reader.tell(), size_t(2));
            reader.consume(2);
            continue;
          }
        }

        bool token_got = do_accept_numeric_literal(tokens, reader,
                                              this->m_opts.integers_as_reals) ||
                         do_accept_punctuator(tokens, reader) ||
                         do_accept_string_literal(tokens, reader, '\"', true) ||
                         do_accept_string_literal(tokens, reader, '\'',
                                        this->m_opts.escapable_single_quotes) ||
                         do_accept_identifier_or_keyword(tokens, reader,
                                        this->m_opts.keywords_as_identifiers);
        if(!token_got)
          throw Parser_Error(parser_status_token_character_unrecognized,
                             reader.tell(), 1);
      }
    }

    // Fail if a block comment was not closed.
    // A block comment may straddle multiple lines. We just mark the
    // first line here.
    if(bcomm)
      throw Parser_Error(parser_status_block_comment_unclosed,
                         bcomm->first, bcomm->second);

    // Reverse the token sequence and accept it.
    ::std::reverse(tokens.mut_begin(), tokens.mut_end());
    this->m_rtoks = ::std::move(tokens);
    return *this;
  }

}  // namespace asteria
