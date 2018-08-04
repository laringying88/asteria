// This file is part of Asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#ifndef ROCKET_VARIANT_HPP_
#define ROCKET_VARIANT_HPP_

#include <type_traits> // so many...
#include <utility> // std::move(), std::forward(), std::declval(), std::swap()
#include <typeinfo>
#include "assert.hpp"
#include "throw.hpp"
#include "utilities.hpp"

namespace rocket
{

using ::std::common_type;
using ::std::is_convertible;
using ::std::decay;
using ::std::enable_if;
using ::std::is_nothrow_constructible;
using ::std::is_nothrow_assignable;
using ::std::is_nothrow_copy_constructible;
using ::std::is_nothrow_move_constructible;
using ::std::is_nothrow_copy_assignable;
using ::std::is_nothrow_move_assignable;
using ::std::is_nothrow_destructible;
using ::std::integral_constant;
using ::std::conditional;
using ::std::false_type;
using ::std::true_type;
using ::std::type_info;

template<typename ...elementsT>
class variant;

namespace details_variant
{
	template<unsigned indexT, typename ...typesT>
	struct type_getter
		// `type` is not defined.
	{
	};
	template<typename firstT, typename ...remainingT>
	struct type_getter<0, firstT, remainingT...>
		: common_type<firstT>
	{
	};
	template<unsigned indexT, typename firstT, typename ...remainingT>
	struct type_getter<indexT, firstT, remainingT...>
		: type_getter<indexT - 1, remainingT...>
	{
	};

	template<unsigned indexT, typename expectT, typename ...typesT>
	struct type_finder
		// `value` is not defined.
	{
	};
	template<unsigned indexT, typename expectT, typename firstT, typename ...remainingT>
	struct type_finder<indexT, expectT, firstT, remainingT...>
		: type_finder<indexT + 1, expectT, remainingT...>
	{
	};
	template<unsigned indexT, typename expectT, typename ...remainingT>
	struct type_finder<indexT, expectT, expectT, remainingT...>
		: integral_constant<unsigned, indexT>
	{
	};

	template<typename ...typesT>
	struct conjunction
		: true_type
	{
	};
	template<typename firstT, typename ...remainingT>
	struct conjunction<firstT, remainingT...>
		: conditional<firstT::value, conjunction<remainingT...>, false_type>::type
	{
	};

	template<typename expectT, typename ...typesT>
	struct has_type_recursive
		: false_type
	{
	};
	template<typename expectT, typename firstT, typename ...remainingT>
	struct has_type_recursive<expectT, firstT, remainingT...>
		: has_type_recursive<expectT, remainingT...>
	{
	};
	template<typename expectT, typename ...remainingT>
	struct has_type_recursive<expectT, expectT, remainingT...>
		: true_type
	{
	};
	template<typename expectT, typename ...elementsT, typename ...remainingT>
	struct has_type_recursive<expectT, variant<elementsT...>, remainingT...>
		: conditional<has_type_recursive<expectT, elementsT...>::value, true_type, has_type_recursive<expectT, remainingT...>>::type
	{
	};
	template<typename ...elementsT, typename ...remainingT>
	struct has_type_recursive<variant<elementsT...>, variant<elementsT...>, remainingT...>
		: true_type
	{
	};

	template<unsigned indexT, typename expectT, typename ...typesT>
	struct recursive_type_finder
		// `value` is not defined.
	{
	};
	template<unsigned indexT, typename expectT, typename firstT, typename ...remainingT>
	struct recursive_type_finder<indexT, expectT, firstT, remainingT...>
		: conditional<has_type_recursive<expectT, firstT>::value, integral_constant<unsigned, indexT>, recursive_type_finder<indexT + 1, expectT, remainingT...>>::type
	{
	};

	template<size_t firstT, size_t ...remainingT>
	struct max_of
		: max_of<firstT, max_of<remainingT...>::value>
	{
	};
	template<size_t firstT>
	struct max_of<firstT>
		: integral_constant<size_t, firstT>
	{
	};
	template<size_t firstT, size_t secondT>
	struct max_of<firstT, secondT>
		: integral_constant<size_t, !(firstT < secondT) ? firstT : secondT>
	{
	};

	template<typename ...elementsT>
	union basic_storage
	{
		char bytes[max_of<sizeof(elementsT)...>::value];
		alignas(max_of<alignof(elementsT)...>::value) char align;
	};

	namespace details_is_nothrow_swappable
	{
		using ::std::swap;

		template<typename typeT>
		struct is_nothrow_swappable
			: integral_constant<bool, noexcept(swap(::std::declval<typeT &>(), ::std::declval<typeT &>()))>
		{
		};
	}

	template<typename typeT>
	struct is_nothrow_swappable
		: details_is_nothrow_swappable::is_nothrow_swappable<typeT>
	{
	};

	template<typename ...elementsT>
	struct visit_helper
	{
		template<typename voidT, typename visitorT, typename ...paramsT>
		void operator()(voidT * /*stor*/, unsigned /*expect*/, visitorT &&/*visitor*/, paramsT &&.../*params*/) const
		{
			ROCKET_ASSERT_MSG(false, "The type index provided was out of range.");
		}
	};
	template<typename firstT, typename ...remainingT>
	struct visit_helper<firstT, remainingT...>
	{
		template<typename voidT, typename visitorT, typename ...paramsT>
		void operator()(voidT *stor, unsigned expect, visitorT &&visitor, paramsT &&...params) const
		{
			if(expect == 0){
				::std::forward<visitorT>(visitor)(reinterpret_cast<firstT *>(stor), ::std::forward<paramsT>(params)...);
			} else {
				visit_helper<remainingT...>()(stor, expect - 1, ::std::forward<visitorT>(visitor), ::std::forward<paramsT>(params)...);
			}
		}
	};

	struct visitor_copy_construct
	{
		template<typename elementT>
		void operator()(elementT *ptr, const void *src) const
		{
			noadl::construct_at(ptr, *(static_cast<const elementT *>(src)));
		}
	};
	struct visitor_move_construct
	{
		template<typename elementT>
		void operator()(elementT *ptr, void *src) const
		{
			noadl::construct_at(ptr, ::std::move(*(static_cast<const elementT *>(src))));
		}
	};
	struct visitor_copy_assign
	{
		template<typename elementT>
		void operator()(elementT *ptr, const void *src) const
		{
			*ptr = *(static_cast<const elementT *>(src));
		}
	};
	struct visitor_move_assign
	{
		template<typename elementT>
		void operator()(elementT *ptr, void *src) const
		{
			*ptr = ::std::move(*(static_cast<elementT *>(src)));
		}
	};
	struct visitor_destruct
	{
		template<typename elementT>
		void operator()(elementT *ptr) const
		{
			noadl::destruct_at(ptr);
		}
	};
	struct visitor_get_type_info
	{
		template<typename elementT>
		void operator()(const elementT *ptr, const type_info **ti) const
		{
			*ti = &(typeid(*ptr));
		}
	};
	struct visitor_wrapper
	{
		template<typename elementT, typename nextT>
		void operator()(elementT *ptr, nextT &&next) const
		{
			::std::forward<nextT>(next)(*ptr);
		}
	};
	struct visitor_swap
	{
		template<typename elementT, typename sourceT>
		void operator()(elementT *ptr, sourceT *src) const
		{
			noadl::adl_swap(*ptr, *(static_cast<elementT *>(src)));
		}
	};
}

template<typename ...elementsT>
class variant
{
	static_assert(details_variant::conjunction<is_nothrow_destructible<elementsT>...>::value, "Destructors of candidate types are not allowed to throw exceptions.");

public:
	template<unsigned indexT>
	struct at
		: details_variant::type_getter<indexT, elementsT...>
	{
	};
	template<typename elementT>
	struct index_of
		: details_variant::type_finder<0, elementT, elementsT...>
	{
	};

	template<typename ...addT>
	struct prepend
		: common_type<variant<addT..., elementsT...>>
	{
	};
	template<typename ...addT>
	struct append
		: common_type<variant<elementsT..., addT...>>
	{
	};

private:
	using storage = details_variant::basic_storage<elementsT...>;

private:
	unsigned m_turnout : 1;
	unsigned m_index : 31;
	storage m_buffers[2];

private:
	const void * do_get_front_buffer() const noexcept
	{
		const unsigned turnout = this->m_turnout;
		return this->m_buffers + turnout;
	}
	void * do_get_front_buffer() noexcept
	{
		const unsigned turnout = this->m_turnout;
		return this->m_buffers + turnout;
	}
	const void * do_get_back_buffer() const noexcept
	{
		const unsigned turnout = this->m_turnout;
		return this->m_buffers + (turnout ^ 1);
	}
	void * do_get_back_buffer() noexcept
	{
		const unsigned turnout = this->m_turnout;
		return this->m_buffers + (turnout ^ 1);
	}

	void do_set_up_new_buffer(unsigned index_new) noexcept
	{
		const unsigned turnout_old = this->m_turnout;
		this->m_turnout = (turnout_old ^ 1) & 1;
		const unsigned index_old = this->m_index;
		this->m_index = index_new & 0x7FFFFFFF;
		// Destroy the old buffer and poison its contents.
		details_variant::visit_helper<elementsT...>()(this->m_buffers + turnout_old, index_old,
		                                              details_variant::visitor_destruct());
#ifdef ROCKET_DEBUG
		::std::memset(this->m_buffers + turnout_old, '@', sizeof(storage));
#endif
	}

public:
	variant() noexcept(is_nothrow_constructible<typename details_variant::type_getter<0, elementsT...>::type>::value)
		: m_turnout(0)
	{
		// Value-initialize the first element.
		constexpr auto eindex = unsigned(0);
		using etype = typename details_variant::type_getter<eindex, elementsT...>::type;
		// Default-construct the first element in-place.
		const auto ptr = static_cast<etype *>(this->do_get_front_buffer());
		noadl::construct_at(ptr);
		this->m_index = 0;
	}
	template<typename elementT, typename enable_if<details_variant::has_type_recursive<typename decay<elementT>::type, elementsT...>::value>::type * = nullptr>
	variant(elementT &&elem)
		: m_turnout(0)
	{
		// This overload enables construction using the candidate of a nested variant.
		constexpr auto eindex = details_variant::recursive_type_finder<0, typename decay<elementT>::type, elementsT...>::value;
		using etype = typename details_variant::type_getter<eindex, elementsT...>::type;
		// Construct the element in-place.
		const auto ptr = static_cast<etype *>(this->do_get_front_buffer());
		noadl::construct_at(ptr, ::std::forward<elementT>(elem));
		this->m_index = eindex;
	}
	variant(const variant &other) noexcept(details_variant::conjunction<is_nothrow_copy_constructible<elementsT>...>::value)
		: m_turnout(0)
	{
		// Copy-construct the active element from `other`.
		details_variant::visit_helper<elementsT...>()(this->do_get_front_buffer(), other.m_index,
		                                              details_variant::visitor_copy_construct(), other.do_get_front_buffer());
		this->m_index = other.m_index;
	}
	variant(variant &&other) noexcept(details_variant::conjunction<is_nothrow_move_constructible<elementsT>...>::value)
		: m_turnout(0)
	{
		// Move-construct the active element from `other`.
		details_variant::visit_helper<elementsT...>()(this->do_get_front_buffer(), other.m_index,
		                                              details_variant::visitor_move_construct(), other.do_get_front_buffer());
		this->m_index = other.m_index;
	}
	template<typename elementT, typename enable_if<details_variant::has_type_recursive<typename decay<elementT>::type, elementsT...>::value>::type * = nullptr>
	variant & operator=(elementT &&elem)
	{
		// This overload, unlike `set()`, enables assignment using the candidate of a nested variant.
		constexpr auto eindex = details_variant::recursive_type_finder<0, typename decay<elementT>::type, elementsT...>::value;
		using etype = typename details_variant::type_getter<eindex, elementsT...>::type;
		if(this->m_index == eindex){
			// Assign the active element using perfect forwarding.
			const auto ptr = static_cast<etype *>(this->do_get_front_buffer());
			*ptr = ::std::forward<elementT>(elem);
			return *this;
		}
		// Construct the active element using perfect forwarding, then destroy the old element.
		const auto ptr = static_cast<etype *>(this->do_get_back_buffer());
		noadl::construct_at(ptr, ::std::forward<elementT>(elem));
		this->do_set_up_new_buffer(eindex);
		return *this;
	}
	variant & operator=(const variant &other) noexcept(details_variant::conjunction<is_nothrow_copy_assignable<elementsT>..., is_nothrow_copy_constructible<elementsT>...>::value)
	{
		if(this->m_index == other.m_index){
			// Copy-assign the active element from `other`
			details_variant::visit_helper<elementsT...>()(this->do_get_front_buffer(), other.m_index,
			                                              details_variant::visitor_copy_assign(), other.do_get_front_buffer());
			return *this;
		}
		// Copy-construct the active element from `other`, then destroy the old element.
		details_variant::visit_helper<elementsT...>()(this->do_get_back_buffer(), other.m_index,
		                                              details_variant::visitor_copy_construct(), other.do_get_front_buffer());
		this->do_set_up_new_buffer(other.m_index);
		return *this;
	}
	variant & operator=(variant &&other) noexcept(details_variant::conjunction<is_nothrow_move_assignable<elementsT>..., is_nothrow_move_constructible<elementsT>...>::value)
	{
		if(this->m_index == other.m_index){
			// Move-assign the active element from `other`
			details_variant::visit_helper<elementsT...>()(this->do_get_front_buffer(), other.m_index,
			                                              details_variant::visitor_move_assign(), other.do_get_front_buffer());
			return *this;
		}
		// Move-construct the active element from `other`, then destroy the old element.
		details_variant::visit_helper<elementsT...>()(this->do_get_back_buffer(), other.m_index,
		                                              details_variant::visitor_move_construct(), other.do_get_front_buffer());
		this->do_set_up_new_buffer(other.m_index);
		return *this;
	}
	~variant()
	{
		// Destroy the active element and poison all contents.
		details_variant::visit_helper<elementsT...>()(this->do_get_front_buffer(), this->m_index,
		                                              details_variant::visitor_destruct());
#ifdef ROCKET_DEBUG
		this->m_index = 0x6EEFDEAD;
		std::memset(m_buffers, '#', sizeof(m_buffers));
#endif
	}

public:
	unsigned index() const noexcept
	{
		ROCKET_ASSERT(this->m_index < sizeof...(elementsT));
		return this->m_index;
	}
	const type_info & type() const noexcept
	{
		auto ti = static_cast<const type_info *>(nullptr);
		details_variant::visit_helper<const elementsT...>()(this->do_get_front_buffer(), this->m_index,
		                                                    details_variant::visitor_get_type_info(), &ti);
		ROCKET_ASSERT(ti);
		return *ti;
	}
	template<typename elementT>
	const elementT * get() const noexcept
	{
		constexpr auto eindex = details_variant::type_finder<0, elementT, elementsT...>::value;
		using etype = typename details_variant::type_getter<eindex, elementsT...>::type;
		if(this->m_index != eindex){
			return nullptr;
		}
		return static_cast<const etype *>(this->do_get_front_buffer());
	}
	template<typename elementT>
	elementT * get() noexcept
	{
		constexpr auto eindex = details_variant::type_finder<0, elementT, elementsT...>::value;
		using etype = typename details_variant::type_getter<eindex, elementsT...>::type;
		if(this->m_index != eindex){
			return nullptr;
		}
		return static_cast<etype *>(this->do_get_front_buffer());
	}
	template<typename elementT>
	const elementT & as() const
	{
		const auto ptr = this->get<elementT>();
		if(!ptr){
			noadl::throw_invalid_argument("variant: The index requested is `%d` (`%s`), but the index currently active is `%d` (`%s`).",
			                              static_cast<int>(index_of<elementT>::value), typeid(elementT).name(), static_cast<int>(this->index()), this->type().name());
		}
		return *ptr;
	}
	template<typename elementT>
	elementT & as()
	{
		const auto ptr = this->get<elementT>();
		if(!ptr){
			noadl::throw_invalid_argument("variant: The index requested is `%d` (`%s`), but the index currently active is `%d` (`%s`).",
			                              static_cast<int>(index_of<elementT>::value), typeid(elementT).name(), static_cast<int>(this->index()), this->type().name());
		}
		return *ptr;
	}
	template<typename elementT, typename ...paramsT>
	elementT & emplace(paramsT &&...params) noexcept(is_nothrow_constructible<elementT, paramsT &&...>::value)
	{
		// This overload, unlike `operator=()`, does not accept the candidate of a nested variant.
		constexpr auto eindex = details_variant::type_finder<0, typename decay<elementT>::type, elementsT...>::value;
		using etype = typename details_variant::type_getter<eindex, elementsT...>::type;
		// Construct the active element using perfect forwarding, then destroy the old element.
		const auto ptr = static_cast<etype *>(this->do_get_back_buffer());
		noadl::construct_at(ptr, ::std::forward<paramsT>(params)...);
		this->do_set_up_new_buffer(eindex);
		return *ptr;
	}
	template<typename elementT>
	elementT & set(elementT &&elem) noexcept(details_variant::conjunction<is_nothrow_move_assignable<elementsT>..., is_nothrow_move_constructible<elementsT>...>::value)
	{
		// This overload, unlike `operator=()`, does not accept the candidate of a nested variant.
		constexpr auto eindex = details_variant::type_finder<0, typename decay<elementT>::type, elementsT...>::value;
		using etype = typename details_variant::type_getter<eindex, elementsT...>::type;
		if(this->m_index == eindex){
			// Assign the active element using perfect forwarding.
			const auto ptr = static_cast<etype *>(this->do_get_front_buffer());
			*ptr = ::std::forward<elementT>(elem);
			return *ptr;
		}
		// Construct the active element using perfect forwarding, then destroy the old element.
		const auto ptr = static_cast<etype *>(this->do_get_back_buffer());
		noadl::construct_at(ptr, ::std::forward<elementT>(elem));
		this->do_set_up_new_buffer(eindex);
		return *ptr;
	}

	template<typename visitorT>
	void visit(visitorT &&visitor) const
	{
		details_variant::visit_helper<elementsT...>()(this->do_get_front_buffer(), this->m_index,
		                                              details_variant::visitor_wrapper(), ::std::forward<visitorT>(visitor));
	}
	template<typename visitorT>
	void visit(visitorT &&visitor)
	{
		details_variant::visit_helper<elementsT...>()(this->do_get_front_buffer(), this->m_index,
		                                              details_variant::visitor_wrapper(), ::std::forward<visitorT>(visitor));
	}

	void swap(variant &other) noexcept(details_variant::conjunction<details_variant::is_nothrow_swappable<elementsT>..., is_nothrow_move_constructible<elementsT>...>::value)
	{
		if(this->m_index == other.m_index){
			// Swap the active elements.
			details_variant::visit_helper<elementsT...>()(this->do_get_front_buffer(), other.m_index,
			                                              details_variant::visitor_swap(), other.do_get_front_buffer());
			return;
		}
		// Move-construct the active element in `*this` from `other`.
		details_variant::visit_helper<elementsT...>()(this->do_get_back_buffer(), other.m_index,
		                                              details_variant::visitor_move_construct(), other.do_get_front_buffer());
		try {
			// Move-construct the active element in `other` from `*this`.
			details_variant::visit_helper<elementsT...>()(other.do_get_back_buffer(), this->m_index,
			                                              details_variant::visitor_move_construct(), this->do_get_front_buffer());
		} catch(...){
			// In case of an exception, the second object will not have been constructed.
			// Destroy the first object that has just been constructed, then rethrow the exception.
			details_variant::visit_helper<elementsT...>()(this->do_get_back_buffer(), other.m_index,
			                                              details_variant::visitor_destruct());
			throw;
		}
		// Destroy both elements.
		const unsigned this_index = this->m_index;
		this->do_set_up_new_buffer(other.m_index);
		other.do_set_up_new_buffer(this_index);
	}
};

template<typename ...elementsT>
void swap(variant<elementsT...> &lhs, variant<elementsT...> &other) noexcept(noexcept(lhs.swap(other)))
{
	lhs.swap(other);
}

}

#endif
