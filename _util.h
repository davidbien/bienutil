#pragma once

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _util.h

#include <type_traits>
#include <variant>
#include <stdint.h>
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

// REVIEW:<dbien>: use type_indentity_t to just transform a tuple into a variant (for instance).
// Multiplex templates with tuples and contain them in some type of variant/tuple/etc container.
template< template<class... > class t_tempMultiplex, class t_TyTpTuplePack, template < class ... > class t_tempVariadicHolder >
struct _MultiplexTuplePackHelper;
template< template<class... > class t_tempMultiplex, class ... t_TysExtractTuplePack, template < class ... > class t_tempVariadicHolder >
struct _MultiplexTuplePackHelper<t_tempMultiplex, tuple< t_TysExtractTuplePack ... >, t_tempVariadicHolder >
{
  using type = t_tempVariadicHolder< t_tempMultiplex< t_TysExtractTuplePack > ... >;
};
template< template<class... > class t_tempMultiplex, class t_TyTpTuplePack, template < class ... > class t_tempVariadicHolder = tuple >
struct MultiplexTuplePack
{
    using type = typename _MultiplexTuplePackHelper< t_tempMultiplex, t_TyTpTuplePack, t_tempVariadicHolder >::type;
};
template< template<class... > class t_tempMultiplex, class t_TyTpTuplePack, template < class ... > class t_tempVariadicHolder = tuple >
using MultiplexTuplePack_t = typename MultiplexTuplePack< t_tempMultiplex, t_TyTpTuplePack, t_tempVariadicHolder >::type;

// Multiplex templates with tuples and contain them in some type of variant container. This one also adds a monostate.
template< template<class... > class t_tempMultiplex, class t_TyTpTuplePack, template < class ... > class t_tempVariadicHolder >
struct _MultiplexMonostateTuplePackHelper;
template< template<class... > class t_tempMultiplex, class ... t_TysExtractTuplePack, template < class ... > class t_tempVariadicHolder >
struct _MultiplexMonostateTuplePackHelper<t_tempMultiplex, tuple< t_TysExtractTuplePack ... >, t_tempVariadicHolder >
{
  using type = t_tempVariadicHolder< monostate, t_tempMultiplex< t_TysExtractTuplePack > ... >;
};
template< template<class... > class t_tempMultiplex, class t_TyTpTuplePack, template < class ... > class t_tempVariadicHolder = tuple >
struct MultiplexMonostateTuplePack
{
    using type = typename _MultiplexMonostateTuplePackHelper< t_tempMultiplex, t_TyTpTuplePack, t_tempVariadicHolder >::type;
};
template< template<class... > class t_tempMultiplex, class t_TyTpTuplePack, template < class ... > class t_tempVariadicHolder = variant >
using MultiplexMonostateTuplePack_t = typename MultiplexMonostateTuplePack< t_tempMultiplex, t_TyTpTuplePack, t_tempVariadicHolder >::type;

// Multiplex templates with 2-dimensional tuple-types and contain them in some type of variant/tuple/etc container.
template < template<class... > class t_tempMultiplex, class t_TyTpTuplePack >
struct  _MultiplexTuplePack2DHelper_2;
template < template<class... > class t_tempMultiplex, class ... t_TysExtractTuplePack >
struct  _MultiplexTuplePack2DHelper_2< t_tempMultiplex, tuple< t_TysExtractTuplePack ... > >
{
  typedef t_tempMultiplex< t_TysExtractTuplePack ... > type;
};
template< template<class... > class t_tempMultiplex, class t_TyTp2DTuplePack, template < class ... > class t_tempVariadicHolder >
struct _MultiplexTuplePack2DHelper;
template< template<class... > class t_tempMultiplex, class ... t_TyTpsExtractTuplePack, template < class ... > class t_tempVariadicHolder >
struct _MultiplexTuplePack2DHelper<t_tempMultiplex, tuple< t_TyTpsExtractTuplePack ... >, t_tempVariadicHolder >
{
  using type = t_tempVariadicHolder< typename _MultiplexTuplePack2DHelper_2< t_tempMultiplex, t_TyTpsExtractTuplePack >::type ... >;
};
template< template<class... > class t_tempMultiplex, class t_TyTp2DTuplePack, template < class ... > class t_tempVariadicHolder = tuple >
struct MultiplexTuplePack2D
{
    using type = typename _MultiplexTuplePack2DHelper< t_tempMultiplex, t_TyTp2DTuplePack, t_tempVariadicHolder >::type;
};
template< template<class... > class t_tempMultiplex, class t_TyTp2DTuplePack, template < class ... > class t_tempVariadicHolder = tuple >
using MultiplexTuplePack2D_t = typename MultiplexTuplePack2D< t_tempMultiplex, t_TyTp2DTuplePack, t_tempVariadicHolder >::type;

// has_type: Test a tuple or a variant (or any other variadic type container) to see if it contains (not necessarily currently mind you) the type you care about.
// From: https://stackoverflow.com/questions/25958259/how-do-i-find-out-if-a-tuple-contains-a-type but I made it completely generic.
template <typename t_TyT, typename t_TyVariadicCont>
struct has_type;
template <typename t_TyT, template < class ... > class t_tempTest, typename... t_Tys >
struct has_type<t_TyT, t_tempTest<t_Tys...>> : std::disjunction<std::is_same<t_TyT, t_Tys>...> 
{
  typedef t_tempTest<t_Tys...> container; // This supports find_container<>.
};
template <typename t_TyT, typename t_TyVariadicCont>
inline constexpr bool has_type_v = has_type<t_TyT,t_TyVariadicCont>::value;

template <typename t_TyContainer>
struct _default_find_container : std::true_type 
{
  using container = t_TyContainer;
};
// find_container: Searches for the variadic container, inside the 2D variadic container t_Ty2DVariadicCont, that contains the type t_TyT.
template <typename t_TyT, typename t_Ty2DVariadicCont, typename t_TyDefaultType = void>
struct find_container;
template <typename t_TyT, template < class ... > class t_tempTest, typename... t_Tys, typename t_TyDefaultType >
struct find_container<t_TyT, t_tempTest<t_Tys...>, t_TyDefaultType>
{
  typedef typename std::disjunction<has_type<t_TyT, t_Tys>..., _default_find_container<t_TyDefaultType>>::container type;
};
template <typename t_TyT, typename t_Ty2DVariadicCont, typename t_TyDefaultType = void>
using find_container_t = typename find_container<t_TyT,t_Ty2DVariadicCont,t_TyDefaultType>::type;
template <typename t_TyT, typename t_Ty2DVariadicCont>
inline constexpr bool find_container_v = !is_same_v< find_container_t<t_TyT,t_Ty2DVariadicCont>, void >;

// DimensionOf: Length of a static array.
template < class t_Ty, size_t t_kN >
constexpr size_t DimensionOf( t_Ty (&)[ t_kN ] )
{
  return t_kN;
}
// StaticStringLen: Length of a static null terminated string.
template < class t_Ty, size_t t_kN >
constexpr size_t StaticStringLen( t_Ty (&)[ t_kN ] )
{
  static_assert( t_kN );
  return t_kN-1;
}

// https://stackoverflow.com/questions/21028299/is-this-behavior-of-vectorresizesize-type-n-under-c11-and-boost-container/21028912#21028912
// https://en.cppreference.com/w/cpp/container/vector/resize
// Allocator adaptor that interposes construct() calls to
// convert value initialization into default initialization.
template < typename T, typename t_TyBaseAllocator = std::allocator< T > >
class default_init_allocator : public t_TyBaseAllocator 
{
  typedef default_init_allocator _TyThis;
  typedef t_TyBaseAllocator _TyBase;
public:
  typedef std::allocator_traits<t_TyBaseAllocator> _TyAllocatorTraits;
  using _TyBase::value_type;

  template <typename U> struct rebind {
    using other =
      default_init_allocator<
        U, typename _TyAllocatorTraits::template rebind_alloc<U>
      >;
  };

  using t_TyBaseAllocator::t_TyBaseAllocator;

  template <typename U>
  void construct(U* ptr)
    noexcept(std::is_nothrow_default_constructible<U>::value) {
    ::new(static_cast<void*>(ptr)) U;
  }
  template <typename U, typename...Args>
  void construct(U* ptr, Args&&... args) {
    _TyAllocatorTraits::construct(static_cast<t_TyBaseAllocator&>(*this),
                   ptr, std::forward<Args>(args)...);
  }
};

template < class t_Ty1, class t_Ty2 >
struct TAreSameSizeTypes
{
  using type = typename std::conditional< sizeof(t_Ty1) == sizeof(t_Ty2), true_type, false_type >::type;
  static constexpr bool value = type::value;
};
template < class t_Ty1, class t_Ty2 >
inline constexpr bool TAreSameSizeTypes_v = TAreSameSizeTypes< t_Ty1, t_Ty2 >::value;

// Argument extraction:
#define VAARG_EXPAND_(arg) arg
#define VAARG_GET_FIRST_(first, ...) first
#define VAARG_GET_SECOND_(first, second, ...) second
// USAGE:
// VAARG_EXPAND_(VAARG_GET_FIRST_(__VA_ARGS__, DUMMY_PARAM_))

// forward_capture: Correct forwarding inside lambda capture:
// https://isocpp.org/blog/2016/12/capturing-perfectly-forwarded-objects-in-lambdas-vittorio-romeo

#define __FWD(...) std::forward<decltype(__VA_ARGS__)>(__VA_ARGS__)
namespace impl
{
  template <typename... Ts>
  auto fwd_capture(Ts&&... xs)
  {
      return std::tuple<Ts...>(__FWD(xs)...); 
  }
} // namespace impl
template <typename T>
decltype(auto) access_fwd(T&& x) { return std::get<0>(__FWD(x)); }
#define FWD_CAPTURE(...) impl::fwd_capture(__FWD(__VA_ARGS__))
// Usage:
#if 0
  struct A
  {
      int _value{0};
  };

  auto foo = [](auto&& a)
  {
      return [a = FWD_CAPTURE(a)]() mutable 
      { 
          ++access(a)._value;
          std::cout << access_fwd(a)._value << "\n";
      };
  };
#endif //0

__BIENUTIL_END_NAMESPACE
