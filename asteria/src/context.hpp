// This file is part of Asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#ifndef ASTERIA_CONTEXT_HPP_
#define ASTERIA_CONTEXT_HPP_

#include "fwd.hpp"
#include "reference.hpp"

namespace Asteria
{

class Context
  {
  private:
    const Sptr<Context> m_parent_opt;
    const bool m_feigned;

    Dictionary<Reference> m_named_refs;

  public:
    Context(Sptr<Context> parent_opt, bool feigned)
      : m_parent_opt(std::move(parent_opt)), m_feigned(feigned)
      {
      }
    ~Context();

    Context(const Context &) = delete;
    Context & operator=(const Context &) = delete;

  public:
    Sptr<Context> get_parent_opt() const noexcept
      {
        return m_parent_opt;
      }
    bool is_feigned() const noexcept
      {
        return m_feigned;
      }

    const Reference * get_named_reference_opt(const String &name) const noexcept
      {
        return m_named_refs.get(name);
      }
    Reference * get_named_reference_opt(const String &name) noexcept
      {
        return m_named_refs.get(name);
      }
    Reference * set_named_reference(const String &name, Reference &&ref)
      {
        return m_named_refs.set(name, std::move(ref)).first;
      }

    // N.B. If a named reference is found, this function materialize it. Otherwise `nullptr` is returned.
    Sptr<Variable> get_variable_opt(const String &name);
  };

}

#endif
