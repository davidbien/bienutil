#ifndef ___UTIL_H__BIEN__
#define ___UTIL_H__BIEN__

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _util.h

#include <type_traits>
#include <variant>
#include "_booltyp.h"

#if (defined(__GNUC__) && !defined(__clang__))
#define GCC_COMPILER 1
#endif

#ifndef __ICL
#define __MSC_INLINE __INLINE
#else //!__ICL
#define __MSC_INLINE
#endif //!__ICL

#define _ppmacroxstr( s ) #s
#define ppmacroxstr( s ) _ppmacroxstr(s)

__BIENUTIL_BEGIN_NAMESPACE

// The maximum amount that we will allocate using alloca:
// Allow 512KB on the stack. I used to allocate like 30MB under Windows. Could allow this to be more but want to see how things go.
static const size_t vknbyMaxAllocaSize = ( 1ull << 19 );


// this is an abstract class that is used to produce an error for the programmer:
struct ___semantic_error_object
{
  virtual void  error() = 0;
};

// Test whether some types are the same:
template < class t_Ty1, class t_Ty2 >
struct __IsSameType
{
  typedef std::false_type  _TyF;
};

template < class t_Ty >
struct __IsSameType< t_Ty, t_Ty >
{
  typedef std::true_type   _TyF;
};

template < class t_Ty1, class t_Ty2 >
struct __AssertSameType;
template < class t_Ty >
struct __AssertSameType< t_Ty, t_Ty >
{
  typedef std::true_type   _TyF;
};

#define __ASSERT_SAME_TYPE( _t1, _t2 ) static_assert( std::is_same_v< _t1, _t2 > )

// Whether to allow a const/non-const object reference to be transfered to
//  a const/non-const:
template < class _TyFromConst, class _TyToConst > struct __TransferConst;
template <> struct __TransferConst< std::true_type, std::true_type > 
{ 
  typedef std::true_type _TyF;
};
template <> struct __TransferConst< std::false_type, std::true_type > 
{
  typedef std::true_type _TyF;
};
template <> struct __TransferConst< std::false_type, std::false_type >
{ 
  typedef std::true_type _TyF;
};
#define __TRANSFER_CONST2( _f1, _f2 ) typedef typename __TransferConst< _f1, _f2 >::_TyF
#define __TRANSFER_CONST( _f1, _f2 )  typedef typename __TransferConst< _f1, _f2 >::_TyF _TyFoo

// Assertion type - __assert_bool<false> is undefined so will result in compile error.
template < bool _f >  struct __assert_bool;
template <> struct __assert_bool<true>  
{ 
  typedef std::true_type _TyF;
};

#ifndef NDEBUG
#ifdef __ICL
// Intel doesn't allow operations among template arguments.
#define __ASSERT_BOOL2( _f )    typedef typename __assert_bool< _f >::_TyF
#define __ASSERT_BOOL( _f ) static constexpr bool __fAssertBool = (_f); \
  typedef typename __assert_bool< _fAssertBool##__LINE__ >::_TyF  _TyFooAssertBool;
#else //__ICL
#define __ASSERT_BOOL2( _f )    typedef typename __assert_bool< _f >::_TyF
#define __ASSERT_BOOL( _f ) typedef typename __assert_bool< (_f) >::_TyF  _TyFooAssertBool;
#endif 
#else //!NDEBUG
#define __ASSERT_BOOL( _f )
#endif //!NDEBUG


// Assertion logic for boolean type ( i.e. not boolean const variable ):
template < class t_TyFBoolType > struct __assert_bool_type;
template <> struct __assert_bool_type< std::true_type >
{ 
  typedef std::true_type _TyF;
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

// Note that these to templates represent the standard pattern "overload" for variants.
template< class... Ts > 
struct _VisitHelpOverloadFCall : Ts... { using Ts::operator()...; };
// explicit deduction guide (not needed as of C++20)
template<class... Ts> _VisitHelpOverloadFCall(Ts...) -> _VisitHelpOverloadFCall<Ts...>;

// unique_variant will produce a set of unique types from a set of possibly repeated types.
// This is useful in many situations.
template <typename T, typename... Ts>
struct filter_duplicates { using type = T; };

template <template <typename...> class C, typename... Ts, typename U, typename... Us>
struct filter_duplicates<C<Ts...>, U, Us...>
    : std::conditional_t<(std::is_same_v<U, Ts> || ...)
                       , filter_duplicates<C<Ts...>, Us...>
                       , filter_duplicates<C<Ts..., U>, Us...>> {};

template <typename T>
struct unique_variant;

template <typename... Ts>
struct unique_variant<std::variant<Ts...>> : filter_duplicates<std::variant<>, Ts...> {};

template <typename T>
using unique_variant_t = typename unique_variant<T>::type;

// Concatenation of types onto a variadic template:
// concatenator_list: Takes a parameter pack at the end and produces the new variadic type.
template <typename t_TyT1, typename ... >
struct concatenator_list;
template < template < typename ... > class t_T, typename... Args0, typename... Args1 >
struct concatenator_list<t_T<Args0...>, Args1...> {
  using type = t_T<Args0..., Args1...>;
};

// concatenator_pack: Takes a variadic template and concatenates its types to produce the new variadic type.
template <typename t_TyT1, typename t_TyT2 >
struct concatenator_pack;
template < template < typename ... > class t_T1, typename... Args0, template < typename ... > class t_T2, typename... Args1 >
struct concatenator_pack<t_T1<Args0...>, t_T2< Args1...> > {
  using type = t_T1<Args0..., Args1...>;
};

template < class t_Ty, size_t t_kN >
constexpr size_t DimensionOf( t_Ty rgt[ t_kN ] )
{
  return t_kN;
}

// https://stackoverflow.com/questions/21028299/is-this-behavior-of-vectorresizesize-type-n-under-c11-and-boost-container/21028912#21028912
// https://en.cppreference.com/w/cpp/container/vector/resize
// Allocator adaptor that interposes construct() calls to
// convert value initialization into default initialization.
template <typename T, typename A=std::allocator<T>>
class default_init_allocator : public A {
  typedef std::allocator_traits<A> a_t;
public:
  template <typename U> struct rebind {
    using other =
      default_init_allocator<
        U, typename a_t::template rebind_alloc<U>
      >;
  };

  using A::A;

  template <typename U>
  void construct(U* ptr)
    noexcept(std::is_nothrow_default_constructible<U>::value) {
    ::new(static_cast<void*>(ptr)) U;
  }
  template <typename U, typename...Args>
  void construct(U* ptr, Args&&... args) {
    a_t::construct(static_cast<A&>(*this),
                   ptr, std::forward<Args>(args)...);
  }
};

__BIENUTIL_END_NAMESPACE

#endif //___UTIL_H__BIEN__
