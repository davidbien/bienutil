#ifndef __TYPEMAP_H__
#define __TYPEMAP_H__

// _typemap.h

// simple character type mapping

#include <limits.h>

__BIENUTIL_BEGIN_NAMESPACE

template < class t_TyCharType >
struct _char_type_map;

template <>
struct _char_type_map< char >
{
  static const char ms_kcMin = CHAR_MIN;
  static const char ms_kcMax = CHAR_MAX;

  typedef char          _TyChar;
  typedef unsigned char _TyUnsigned;
  typedef signed char   _TySigned;
  typedef short         _TyLarger;
};

template <>
struct _char_type_map< unsigned char >
{
  static const unsigned char ms_kcMin = 0;
  static const unsigned char ms_kcMax = UCHAR_MAX;

  typedef char            _TyChar;
  typedef unsigned char   _TyUnsigned;
  typedef signed char     _TySigned;
  typedef unsigned short  _TyLarger;
};

template <>
struct _char_type_map< signed char >
{
  static const signed char ms_kcMin = SCHAR_MIN;
  static const signed char ms_kcMax = SCHAR_MAX;

  typedef char          _TyChar;
  typedef unsigned char _TyUnsigned;
  typedef signed char   _TySigned;
  typedef signed short  _TyLarger;
};

template <>
struct _char_type_map< wchar_t >
{
  static const wchar_t ms_kcMin = WCHAR_MIN;
  static const wchar_t ms_kcMax = WCHAR_MAX;

  typedef wchar_t       _TyChar;
  typedef wchar_t       _TyUnsigned;
  typedef signed short  _TySigned;
  typedef unsigned long _TyLarger;
};

template <>
struct _char_type_map< signed short >
{
  static const signed short ms_kcMin = SHRT_MIN;
  static const signed short ms_kcMax = SHRT_MAX;

  typedef wchar_t       _TyChar;
  typedef wchar_t       _TyUnsigned;
  typedef signed short  _TySigned;
  typedef signed long   _TyLarger;
};

template <>
struct _char_type_map< signed int >
{
  static const signed int ms_kcMin = INT_MIN;
  static const signed int ms_kcMax = INT_MAX;

  typedef unsigned int      _TyChar;
  typedef unsigned int      _TyUnsigned;
  typedef signed int        _TySigned;
	typedef signed long long  _TyLarger;
};

template <>
struct _char_type_map< unsigned int >
{
  static const unsigned int ms_kcMin = 0;
  static const unsigned int ms_kcMax = UINT_MAX;

  typedef unsigned int        _TyChar;
  typedef unsigned int        _TyUnsigned;
  typedef signed int          _TySigned;
	typedef unsigned long long  _TyLarger;
};

__BIENUTIL_END_NAMESPACE

#endif //__TYPEMAP_H__
