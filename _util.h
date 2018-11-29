#ifndef ___UTIL_H__BIEN__
#define ___UTIL_H__BIEN__

// _util.h

#include "_booltyp.h"

#ifdef __STL_USE_NAMESPACES
#define __STD_OR_GLOBAL_QUALIFIER std::
#else __STL_USE_NAMESPACES
#define __STD_OR_GLOBAL_QUALIFIER ::
#endif __STL_USE_NAMESPACES

#ifdef _MSC_VER
#define __INLINE __inline
#else _MSC_VER
#define __INLINE inline
#endif _MSC_VER

#ifndef __ICL
#define __MSC_INLINE __INLINE
#else !__ICL
#define __MSC_INLINE
#endif !__ICL

__BIENUTIL_BEGIN_NAMESPACE

// this is an abstract class that is used to produce an error for the programmer:
struct ___semantic_error_object
{
  virtual void  error() = 0;
};

// Test whether some types are the same:
template < class t_Ty1, class t_Ty2 >
struct __IsSameType
{
  typedef __false_type  _TyF;
};

template < class t_Ty >
struct __IsSameType< t_Ty, t_Ty >
{
  typedef __true_type   _TyF;
};

template < class t_Ty1, class t_Ty2 >
struct __AssertSameType;
template < class t_Ty >
struct __AssertSameType< t_Ty, t_Ty >
{
  typedef __true_type   _TyF;
};
#define __ASSERT_SAME_TYPE3( _t1, _t2, _Ty ) typedef typename __AssertSameType< _t1, _t2 >::_TyF _Ty
#define __ASSERT_SAME_TYPE( _t1, _t2 ) typedef typename __AssertSameType< _t1, _t2 >::_TyF _TyFooSameType

// Whether to allow a const/non-const object reference to be transfered to
//  a const/non-const:
template < class _TyFromConst, class _TyToConst > struct __TransferConst;
template <> struct __TransferConst< __true_type, __true_type > 
{ 
  typedef __true_type _TyF;
};
template <> struct __TransferConst< __false_type, __true_type > 
{
  typedef __true_type _TyF;
};
template <> struct __TransferConst< __false_type, __false_type >
{ 
  typedef __true_type _TyF;
};
#define __TRANSFER_CONST2( _f1, _f2 ) typedef typename __TransferConst< _f1, _f2 >::_TyF
#define __TRANSFER_CONST( _f1, _f2 )  typedef typename __TransferConst< _f1, _f2 >::_TyF _TyFoo

// Assertion type - __assert_bool<false> is undefined so will result in compile error.
template < bool _f >  struct __assert_bool;
template <> struct __assert_bool<true>  
{ 
  typedef __true_type _TyF;
};

#ifndef NDEBUG
#ifdef __ICL
// Intel doesn't allow operations among template arguments.
#define __ASSERT_BOOL2( _f )    typedef typename __assert_bool< _f >::_TyF
#define __ASSERT_BOOL( _f ) static constexpr bool __fAssertBool = (_f); \
  typedef typename __assert_bool< _fAssertBool##__LINE__ >::_TyF  _TyFooAssertBool;
#else __ICL
#define __ASSERT_BOOL2( _f )    typedef typename __assert_bool< _f >::_TyF
#define __ASSERT_BOOL( _f ) typedef typename __assert_bool< (_f) >::_TyF  _TyFooAssertBool;
#endif 
#else !NDEBUG
#define __ASSERT_BOOL( _f )
#endif !NDEBUG


// Assertion logic for boolean type ( i.e. not boolean const variable ):
template < class t_TyFBoolType > struct __assert_bool_type;
template <> struct __assert_bool_type< __true_type >
{ 
  typedef __true_type _TyF;
};

#define __ASSERT_BOOL_TYPE( _f )  typedef typename __assert_bool_type< _f >::_TyF

template < int t_i, class _TyType0, class _TyType1 > struct __select_type2;
template < class _TyType0, class _TyType1 > struct __select_type2<0, _TyType0, _TyType1>
{
  typedef _TyType0  _TyType;
};
template < class _TyType0, class _TyType1 > struct __select_type2<1, _TyType0, _TyType1>
{
  typedef _TyType1  _TyType;
};

__BIENUTIL_END_NAMESPACE

#endif ___UTIL_H__BIEN__
