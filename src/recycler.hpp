// This file is part of Asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#ifndef ASTERIA_RECYCLER_HPP_
#define ASTERIA_RECYCLER_HPP_

#include "fwd.hpp"

namespace Asteria {

class Recycler {
public:
	enum : std::size_t {
		defragmentation_threshold_initial    = 200,
		defragmentation_threshold_increment  = 100,
	};

private:
	Wptr_vector<Variable> m_weak_variables;
	std::size_t m_defragmentation_threshold;

public:
	Recycler()
		: m_weak_variables(), m_defragmentation_threshold(defragmentation_threshold_initial)
	{ }
	~Recycler();

	Recycler(const Recycler &) = delete;
	Recycler &operator=(const Recycler &) = delete;

public:
	void adopt_variable(Spcref<Variable> variable_opt);
	void defragment(bool aggressive = false) noexcept;
};

}

#endif
