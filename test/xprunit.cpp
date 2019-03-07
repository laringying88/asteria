// This file is part of Asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#include "_test_init.hpp"
#include "../asteria/src/syntax/xprunit.hpp"
#include "../asteria/src/runtime/global_context.hpp"
#include "../asteria/src/runtime/executive_context.hpp"
#include "../asteria/src/runtime/air_node.hpp"
#include "../asteria/src/runtime/reference_stack.hpp"
#include "../asteria/src/runtime/analytic_context.hpp"

using namespace Asteria;

int main()
  {
    Global_Context global;
    Executive_Context ctx(nullptr);
    Source_Location sloc(rocket::sref("test"), 42);

    const auto cond = global.create_variable();
    cond->reset(sloc, D_null(), false);
    ctx.open_named_reference(rocket::sref("cond")) = Reference_Root::S_variable { cond };

    const auto dval = global.create_variable();
    ctx.open_named_reference(rocket::sref("dval")) = Reference_Root::S_variable { dval };
    dval->reset(sloc, D_real(1.5), false);

    const auto ival = global.create_variable();
    ctx.open_named_reference(rocket::sref("ival")) = Reference_Root::S_variable { ival };
    ival->reset(sloc, D_integer(3), false);

    const auto aval = global.create_variable();
    aval->reset(sloc, D_array(), false);
    ctx.open_named_reference(rocket::sref("aval")) = Reference_Root::S_variable { aval };

    // Plain: aval[1] = !cond ? (dval++ + 0.25) : (ival * "hello,");
    // RPN:   aval 1 [] cond ! ?: =                    ::= expr
    //                         |\-- dval ++ 0.25 +     ::= branch_true
    //                         \--- ival "hello," *    ::= branch_false
    Cow_Vector<Xprunit> branch_true;
    {
      branch_true.emplace_back(Xprunit::S_named_reference { rocket::sref("dval") });
      branch_true.emplace_back(Xprunit::S_operator_rpn { Xprunit::xop_postfix_inc, false });
      branch_true.emplace_back(Xprunit::S_literal { D_real(0.25) });
      branch_true.emplace_back(Xprunit::S_operator_rpn { Xprunit::xop_infix_add, false });
    }
    Cow_Vector<Xprunit> branch_false;
    {
      branch_false.emplace_back(Xprunit::S_named_reference { rocket::sref("ival") });
      branch_false.emplace_back(Xprunit::S_literal { D_string("hello,") });
      branch_false.emplace_back(Xprunit::S_operator_rpn { Xprunit::xop_infix_mul, false });
    }
    Cow_Vector<Xprunit> nodes;
    {
      nodes.emplace_back(Xprunit::S_named_reference { rocket::sref("aval") });
      nodes.emplace_back(Xprunit::S_literal { D_integer(1) });
      nodes.emplace_back(Xprunit::S_operator_rpn { Xprunit::xop_postfix_at, false });
      nodes.emplace_back(Xprunit::S_named_reference { rocket::sref("cond") });
      nodes.emplace_back(Xprunit::S_operator_rpn { Xprunit::xop_prefix_notl, false });
      nodes.emplace_back(Xprunit::S_branch { std::move(branch_true), std::move(branch_false), false });
      nodes.emplace_back(Xprunit::S_operator_rpn { Xprunit::xop_infix_assign, false });
    }
    Cow_Vector<Air_Node> expr_code;
    Analytic_Context actx(&ctx);
    rocket::for_each(nodes, [&](const Xprunit &node) { node.generate_code(expr_code, actx);  });

    Reference_Stack stack;
    rocket::for_each(expr_code, [&](const Air_Node &node) { node.execute(stack, ctx, rocket::sref("dummy_function"), global);  });
    ASTERIA_TEST_CHECK(stack.size() == 1);
    auto value = dval->get_value();
    ASTERIA_TEST_CHECK(value.check<D_real>() == 2.5);
    value = ival->get_value();
    ASTERIA_TEST_CHECK(value.check<D_integer>() == 3);
    value = aval->get_value();
    ASTERIA_TEST_CHECK(value.check<D_array>().at(1).check<D_real>() == 1.75);
    value = stack.top().read();
    ASTERIA_TEST_CHECK(value.check<D_real>() == 1.75);

    cond->open_value() = D_integer(42);
    rocket::for_each(expr_code, [&](const Air_Node &node) { node.execute(stack, ctx, rocket::sref("dummy_function"), global);  });
    ASTERIA_TEST_CHECK(stack.size() == 2);
    value = dval->get_value();
    ASTERIA_TEST_CHECK(value.check<D_real>() == 2.5);
    value = ival->get_value();
    ASTERIA_TEST_CHECK(value.check<D_integer>() == 3);
    value = aval->get_value();
    ASTERIA_TEST_CHECK(value.check<D_array>().at(1).check<D_string>() == "hello,hello,hello,");
    value = stack.top().read();
    ASTERIA_TEST_CHECK(value.check<D_string>() == "hello,hello,hello,");
 }