#ifndef __BIENUTIL_H
#define __BIENUTIL_H

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// bienutil.h

#define __BIENUTIL_USE_NAMESPACE ns_bienutil // always use namespaces.

#if !defined( __BIENUTIL_USE_NAMESPACE )
#define __BIENUTIL_GLOBALNAMESPACE
#endif

#define __STD std
#define _BIEN_NOTHROW noexcept
#define _BIEN_TRY try
#define _BIEN_CATCH_ALL catch(...)
#define _BIEN_THROW(x) throw x
#define _BIEN_RETHROW throw
#define _BIEN_UNWIND(action) catch(...) { action; throw; }

#ifdef __BIENUTIL_GLOBALNAMESPACE
#define __BIENUTIL_BEGIN_NAMESPACE
#define __BIENUTIL_END_NAMESPACE
#define __BIENUTIL_USING_NAMESPACE
#define __BIENUTIL_NAMESPACE
#else //__BIENUTIL_GLOBALNAMESPACE
#ifndef __BIENUTIL_USE_NAMESPACE
#define __BIENUTIL_USE_NAMESPACE ns_bienutil
#endif //__BIENUTIL_USE_NAMESPACE
#define __BIENUTIL_BEGIN_NAMESPACE namespace __BIENUTIL_USE_NAMESPACE { using namespace std;
#define __BIENUTIL_END_NAMESPACE }
#define __BIENUTIL_USING_NAMESPACE using namespace __BIENUTIL_USE_NAMESPACE;
#define __BIENUTIL_NAMESPACE __BIENUTIL_USE_NAMESPACE::
#endif //__BIENUTIL_GLOBALNAMESPACE

#include <cstring>

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t)&((TYPE*)0)->MEMBER)
#endif // offsetof

#endif //!__BIENUTIL_H
