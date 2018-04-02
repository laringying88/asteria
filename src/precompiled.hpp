// This file is part of asteria.
// Copyleft 2018, LH_Mouse. All wrongs reserved.

#ifndef ASTERIA_PRECOMPILED_HPP_
#define ASTERIA_PRECOMPILED_HPP_

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <array>
#include <string>
#include <iterator>
#include <algorithm>
#include <utility>
#include <stdexcept>
#include <functional>
#include <iosfwd>
#include <iomanip>
#include <ostream>
#include <memory>
#include <tuple>
#include <type_traits>

#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <climits>
#include <cctype>

#include <boost/container/vector.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/variant.hpp>

namespace Asteria {

template<typename ElementT>
using Sptr = std::shared_ptr<ElementT>;
template<typename ElementT>
using Spref = const Sptr<ElementT> &;

}

#endif
