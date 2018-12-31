#ifndef __BIENUTIL_H
#define __BIENUTIL_H

// bienutil.h

#define __BIENUTIL_USE_NAMESPACE ns_bienutil // always use namespaces.

#if !defined( _STLP_USE_NAMESPACES ) && !defined( __BIENUTIL_USE_NAMESPACE )
#define __BIENUTIL_GLOBALNAMESPACE
#endif

#ifdef _USE_STLPORT
#define __STD _STLP_STD
#ifdef _STLP_USE_EXCEPTIONS
#define _BIEN_USE_EXCEPTIONS
#endif
#define _BIEN_NOTHROW _STLP_NOTHROW
#define _BIEN_TRY _STLP_TRY
#define _BIEN_CATCH_ALL _STLP_CATCH_ALL
#define _BIEN_THROW(x) _STLP_THROW(x)
#define _BIEN_RETHROW _STLP_RETHROW
#define _BIEN_UNWIND(action) _STLP_UNWIND(action)
#else //_USE_STLPORT
#define __STD std
#define _BIEN_USE_EXCEPTIONS
#define _BIEN_NOTHROW noexcept
#define _BIEN_TRY try
#define _BIEN_CATCH_ALL catch(...)
#define _BIEN_THROW(x) throw x
#define _BIEN_RETHROW throw
#define _BIEN_UNWIND(action) catch(...) { action; throw; }
#endif //_USE_STLPORT

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

#endif //!__BIENUTIL_H
