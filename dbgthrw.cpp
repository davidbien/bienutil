
//          Copyright David Lawrence Bien 1997 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// dbgthrw.cpp

// debug throwing stuff

#include <stdexcept>
#include "syslogmgr.h"
#include "bienutil.h"
#include "jsonobjs.h"
#include "_util.h"
#include "_dbgthrw.h"

#ifndef __NDEBUG_THROW

__BIENUTIL_BEGIN_NAMESPACE

_throw_static_base	_throw_object_base::ms_tsb;

__BIENUTIL_END_NAMESPACE

#endif //!__NDEBUG_THROW
