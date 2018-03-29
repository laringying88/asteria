/// This file is part of asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#ifndef ASTERIA_VARIABLE_HPP_
#define ASTERIA_VARIABLE_HPP_

#include "fwd.hpp"
#include "type_tuple.hpp"

namespace Asteria {

class Variable {
public:
	enum Type : unsigned {
		type_null      = -1u,
		type_boolean   =  0,
		type_integer   =  1,
		type_double    =  2,
		type_string    =  3,
		type_opaque    =  4,
		type_array     =  5,
		type_object    =  6,
		type_function  =  7,
	};
	using Types = Type_tuple< Boolean    //  0
	                        , Integer    //  1
	                        , Double     //  2
	                        , String     //  3
	                        , Opaque     //  4
	                        , Array      //  5
	                        , Object     //  6
	                        , Function   //  7
		>;

private:
	Types::rebind_as_variant m_variant;

public:
	template<typename ValueT, ASTERIA_UNLESS_IS_BASE_OF(Variable, ValueT)>
	Variable(ValueT &&value)
		: m_variant(std::forward<ValueT>(value))
	{ }

private:
	__attribute__((__noreturn__)) void do_throw_type_mismatch(Type expect) const;

public:
	Type get_type() const noexcept {
		return static_cast<Type>(m_variant.which());
	}

	template<Type expectT>
	const typename Types::at<expectT>::type *get_opt() const noexcept {
		return boost::get<typename Types::at<expectT>::type>(&m_variant);
	}
	template<Type expectT>
	typename Types::at<expectT>::type *get_opt() noexcept {
		return boost::get<typename Types::at<expectT>::type>(&m_variant);
	}
	template<typename ExpectT>
	const ExpectT *get_opt() const noexcept {
		return boost::get<ExpectT>(&m_variant);
	}
	template<typename ExpectT>
	ExpectT *get_opt() noexcept {
		return boost::get<ExpectT>(&m_variant);
	}

	template<Type expectT>
	const typename Types::at<expectT>::type &get() const {
		const auto ptr = get_opt<expectT>();
		if(!ptr){
			do_throw_type_mismatch(expectT);
		}
		return *ptr;
	}
	template<Type expectT>
	typename Types::at<expectT>::type &get(){
		const auto ptr = get_opt<expectT>();
		if(!ptr){
			do_throw_type_mismatch(expectT);
		}
		return *ptr;
	}
	template<typename ExpectT>
	const ExpectT &get() const {
		const auto ptr = get_opt<ExpectT>();
		if(!ptr){
			do_throw_type_mismatch(static_cast<Type>(Types::index_of<ExpectT>::value));
		}
		return *ptr;
	}
	template<typename ExpectT>
	ExpectT &get(){
		const auto ptr = get_opt<ExpectT>();
		if(!ptr){
			do_throw_type_mismatch(static_cast<Type>(Types::index_of<ExpectT>::value));
		}
		return *ptr;
	}

	template<typename ValueT>
	void set(ValueT &&value){
		m_variant = std::forward<ValueT>(value);
	}
};

extern const char *get_type_name(Variable::Type type) noexcept;

extern Variable::Type get_variable_type(const Variable *variable_opt) noexcept;
extern Variable::Type get_variable_type(const Shared_ptr<const Variable> &variable_opt) noexcept;
extern Variable::Type get_variable_type(const Value_ptr<Variable> &variable_opt) noexcept;

extern const char *get_variable_type_name(const Variable *variable_opt) noexcept;
extern const char *get_variable_type_name(const Shared_ptr<const Variable> &variable_opt) noexcept;
extern const char *get_variable_type_name(const Value_ptr<Variable> &variable_opt) noexcept;

extern void dump_variable_recursive(std::ostream &os, const Variable *variable_opt, unsigned indent_next = 0, unsigned indent_increment = 2);
extern void dump_variable_recursive(std::ostream &os, const Shared_ptr<const Variable> &variable_opt, unsigned indent_next = 0, unsigned indent_increment = 2);
extern void dump_variable_recursive(std::ostream &os, const Value_ptr<Variable> &variable_opt, unsigned indent_next = 0, unsigned indent_increment = 2);

extern std::ostream &operator<<(std::ostream &os, const Variable *variable_opt);
extern std::ostream &operator<<(std::ostream &os, const Shared_ptr<const Variable> &variable_opt);
extern std::ostream &operator<<(std::ostream &os, const Value_ptr<Variable> &variable_opt);

// This function is used to break dependency circles.
extern void dispose_variable_recursive(Variable *variable_opt) noexcept;

}

#endif
