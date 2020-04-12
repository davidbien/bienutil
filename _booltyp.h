#ifndef __BOOLTYP_H
#define __BOOLTYP_H

// _booltyp.h

// Define the boolean types std::true_type and std::false_type and some related utilities.

#include <type_traits>

#ifdef _MSC_VER
#define __INLINE __inline
#else //_MSC_VER
#define __INLINE inline
#endif //_MSC_VER

__BIENUTIL_BEGIN_NAMESPACE

#if 0 // !defined( _STLP_STD ) && ( defined( __FreeBSD__ ) || defined(__APPLE__) || !defined( __GNUC__ ) ) // STLport defines these locally, FreeBSD's STL seems not to, gcc otherwise does.
struct std::true_type
{
};

struct std::false_type
{
};
#endif // 0 !_STLP_STD

#ifdef __GNUC__ // doesn't like templates declared like this.
// Type->boolean functions:
template < class t_TyBoolean >
bool  __FTrue( t_TyBoolean );

template <>
__INLINE bool  __FTrue( std::false_type )
{
  return false;
}
template <>
__INLINE bool  __FTrue( std::true_type )
{
  return true;
}

template < class t_TyBoolean >
bool  __FFalse( t_TyBoolean );

template <>
__INLINE bool  __FFalse( std::true_type )
{
  return false;
}
template <>
__INLINE bool  __FFalse( std::false_type )
{
  return true;
}
#endif //!__GNUC__

// Type->boolean types:
template < class _TyF > struct __type_to_bool
{
  static const bool ms_cfValue = true;
};
template < > struct __type_to_bool< std::false_type >
{
  static const bool ms_cfValue = false;
};

// Boolean->type types:

template < bool _f >  struct __boolean_type
{
  typedef std::true_type _type;
};
template <>
struct __boolean_type<false>
{
  typedef std::false_type  _type;
};

// Boolean operations:
template < bool t_fBoolean >
struct __boolean_not
{
  static const bool ms_cfValue = false;
};
template < >
struct __boolean_not< false >
{
  static const bool ms_cfValue = true;
};

template < bool t_fBoolean1, bool t_fBoolean2 >
struct __boolean_and
{
  static const bool ms_cfValue = false;
};
template < >
struct __boolean_and< true, true >
{
  static const bool ms_cfValue = true;
};

template < bool t_fBoolean1, bool t_fBoolean2 >
struct __boolean_or
{
  static const bool ms_cfValue = true;
};
template < >
struct __boolean_or< false, false >
{
  static const bool ms_cfValue = false;
};

template < bool t_fBoolean1, bool t_fBoolean2 >
struct __boolean_equals
{
  static const bool ms_cfValue = false;
};
template < >
struct __boolean_equals< false, false >
{
  static const bool ms_cfValue = true;
};
template < >
struct __boolean_equals< true, true >
{
  static const bool ms_cfValue = true;
};

// Boolean type operations:
template < class t_TyBoolean >
struct __booltyp_not
{
  typedef std::false_type  _value;
};
template < >
struct __booltyp_not< std::false_type >
{
  typedef std::true_type   _value;
};

template < class t_TyBool1, class t_TyBool2 >
struct __booltyp_and
{
  typedef std::false_type  _value;
};
template < >
struct __booltyp_and< std::true_type, std::true_type >
{
  typedef std::true_type   _value;
};

template < class t_TyBool1, class t_TyBool2 >
struct __booltyp_or
{
  typedef std::true_type   _value;
};
template < >
struct __booltyp_or< std::false_type, std::false_type >
{
  typedef std::false_type  _value;
};

template < class t_TyBool1, class t_TyBool2 >
struct __booltyp_equals
{
  typedef std::false_type    _value;
};
template < >
struct __booltyp_equals< std::false_type, std::false_type >
{
  typedef std::true_type     _value;
};
template < >
struct __booltyp_equals< std::true_type, std::true_type >
{
  typedef std::true_type     _value;
};

__BIENUTIL_END_NAMESPACE

#endif //__BOOLTYP_H










