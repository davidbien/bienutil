/*
 * Copyright (c) 1996-1997
 * Silicon Graphics Computer Systems, Inc.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Silicon Graphics makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 */

/* NOTE: This is an internal header file, included by other STL headers.
 *   You should not attempt to use it directly.
 */

// dbien: Need this for various things - abstracts SGI concept of instanceless allocators.


#ifndef __ALOCTRT_H
#define __ALOCTRT_H

// Another allocator adaptor: _Alloc_traits.  This serves two
// purposes.  First, make it possible to write containers that can use
// either SGI-style allocators or standard-conforming allocator.
// Second, provide a mechanism so that containers can query whether or
// not the allocator has distinct instances.  If not, the container
// can avoid wasting a word of memory to store an empty object.

// This adaptor uses partial specialization.  The general case of
// _Alloc_traits<_Tp, _Alloc> assumes that _Alloc is a
// standard-conforming allocator, possibly with non-equal instances
// and non-static members.  (It still behaves correctly even if _Alloc
// has static member and if all instances are equal.  Refinements
// affect performance, not correctness.)

// There are always two members: allocator_type, which is a standard-
// conforming allocator type for allocating objects of type _Tp, and
// _S_instanceless, a static const member of type bool.  If
// _S_instanceless is true, this means that there is no difference
// between any two instances of type allocator_type.  Furthermore, if
// _S_instanceless is true, then _Alloc_traits has one additional
// member: _Alloc_type.  This type encapsulates allocation and
// deallocation of objects of type _Tp through a static interface; it
// has two member functions, whose signatures are
//    static _Tp* allocate(size_t)
//    static void deallocate(_Tp*, size_t)

// The fully general version.

#ifndef _USE_STLPORT

#include <memory>

template <class _Tp, class _Allocator>
struct _Alloc_traits
{
	static const bool _S_instanceless = false;
  template< typename t_TyT >
  using RebindAlloc = typename std::allocator_traits<_Allocator>::template rebind_alloc<t_TyT>;
  typedef RebindAlloc< _Tp > allocator_type;
  using size_type = typename std::allocator_traits< allocator_type >::size_type;
	// deprecated typedef typename _Allocator::template rebind<_Tp>::other allocator_type;
};

#else //!_USE_STLPORT
_STLP_BEGIN_NAMESPACE

template <class _Tp, class _Allocator>
struct _Alloc_traits
{
  static const bool _S_instanceless = false;
  typedef typename _Allocator::_STLP_TEMPLATE rebind<_Tp>::other 
          allocator_type;
};

template <class _Tp, class _Allocator>
const bool _Alloc_traits<_Tp, _Allocator>::_S_instanceless;

// The version for the default allocator.

template <class _Tp, class _Tp1>
struct _Alloc_traits<_Tp, allocator<_Tp1> >
{
  static const bool _S_instanceless = true;
  typedef simple_alloc<_Tp, alloc> _Alloc_type;
  typedef allocator<_Tp> allocator_type;
};

_STLP_END_NAMESPACE
#endif _USE_STLPORT

#endif //__ALOCTRT_H
