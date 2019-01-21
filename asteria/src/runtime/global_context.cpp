// This file is part of Asteria.
// Copyleft 2018 - 2019, LH_Mouse. All wrongs reserved.

#include "../precompiled.hpp"
#include "global_context.hpp"
#include "generational_collector.hpp"
#include "variable.hpp"
#include "../utilities.hpp"

namespace Asteria {

Global_Context::~Global_Context()
  {
  }

void Global_Context::do_initialize()
  {
    // Initialize all components.
    const auto collector = rocket::make_refcnt<Generational_Collector>();
    this->do_tie_collector(collector);
    this->m_collector = collector;
    // Add standard library interfaces.
    D_object root;
    ASTERIA_DEBUG_LOG("TODO add std library");
    Reference_Root::S_constant ref_c = { std::move(root) };
    this->open_named_reference(rocket::sref("std")) = std::move(ref_c);
  }

RefCnt_Ptr<Variable> Global_Context::create_variable()
  {
    const auto collector = rocket::dynamic_pointer_cast<Generational_Collector>(this->m_collector);
    ROCKET_ASSERT(collector);
    return collector->create_variable();
  }

}
