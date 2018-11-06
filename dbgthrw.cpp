

// dbgthrw.cpp

// debug throwing stuff

#include <stddef.h>
#include <assert.h>
#include <alloc.h>
#include <stdexcept>
#include "bienutil/bienutil.h"
#include "bienutil/_util.h"
#include "bienutil/_dbgthrw.h"

#ifndef __NDEBUG_THROW

__BIENUTIL_BEGIN_NAMESPACE

_throw_static_base	_throw_object_base::ms_tsb;

__BIENUTIL_END_NAMESPACE

#endif !__NDEBUG_THROW
