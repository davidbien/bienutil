#pragma once

//          Copyright David Lawrence Bien 1998 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _allbase.h
// dbien: Sometime in 1999/2000 I think.
// Generic base class for abstracting the difference between an instanced and static allocator.
// REVIEW: We only support instanced allocators at this point (post STLport).

#include <memory>
#include "bienutil.h"
#include "_util.h"
#include "_dbgthrw.h"

__BIENUTIL_BEGIN_NAMESPACE

template <	class t_TyAllocate, class t_TyAllocator >
class _alloc_base
{
	typedef _alloc_base< t_TyAllocate, t_TyAllocator >	_TyThis;
	typedef allocator_traits< t_TyAllocator > _TyAllocTraitsAsPassed;
public:

	// We always rebind the allocator type, whatever it is passed in as, to t_TyAllocate.
	using _TyAllocatorTraits = typename _TyAllocTraitsAsPassed::template rebind_traits< t_TyAllocate >;
	typedef typename _TyAllocatorTraits::allocator_type allocator_type;
	typedef allocator_type _TyAllocatorType;
	typedef typename _TyAllocatorTraits::size_type size_type;

	_TyAllocatorType m_alloc;

	_alloc_base( _alloc_base const & ) = default;

  // Allow copy construction from any alloc base - this could fail if the allocators were incompatible.
  template < class t__TyAllocate, class t__TyAllocator >
  _alloc_base( _alloc_base< t__TyAllocate, t__TyAllocator > const & _rOther ) _BIEN_NOTHROW
    : m_alloc( _rOther.get_allocator() )
  {
  }

	// Allow initialization with any allocator:
	template < class t__TyAllocator >
	explicit _alloc_base( t__TyAllocator const & _rOther ) _BIEN_NOTHROW 
		: m_alloc( _rOther )
	{ 
	}
	
	#if 0
  template < class t__TyAllocate, class t__TyAllocator >
	_alloc_base( _alloc_base< t__TyAllocate, t__TyAllocator > && _rrOther ) _BIEN_NOTHROW 
		: m_alloc( std::move( _rrOther.m_alloc ) )
	{ 
	}
	template < class t__TyAllocator >
	_alloc_base( t__TyAllocator && _rrAllocOther ) _BIEN_NOTHROW 
		: m_alloc( std::move( _rrAllocOther ) )
	{ 
	}
	_TyThis & operator = ( _TyThis && _rr )
	{
		m_alloc = std::move( _rr.m_alloc );
		return *this;
	}
#endif //0

	__INLINE _TyAllocatorType & get_allocator_ref() _BIEN_NOTHROW
	{
		return m_alloc;
	}
	__INLINE _TyAllocatorType & get_allocator() _BIEN_NOTHROW
	{
		return m_alloc;
	}

  __INLINE const _TyAllocatorType & get_allocator_ref() const _BIEN_NOTHROW
  {
    return m_alloc;
  }
  __INLINE const _TyAllocatorType & get_allocator() const _BIEN_NOTHROW
  {
    return m_alloc;
  }
  __INLINE t_TyAllocate *	allocate_type()
	{
		__THROWPT(e_ttMemory);
		return m_alloc.allocate( 1 );
	}
	__INLINE void deallocate_type( t_TyAllocate * _pNode ) _BIEN_NOTHROW
	{
		m_alloc.deallocate( _pNode, 1 );
	}
	__INLINE void allocate_n( t_TyAllocate *& _rpNode, size_type _n )
	{
		__THROWPT(e_ttMemory);
		_rpNode = m_alloc.allocate( _n );
	}
	__INLINE void deallocate_n( t_TyAllocate * _pNode, size_type _n ) _BIEN_NOTHROW
	{
    m_alloc.deallocate( _pNode, _n );
	}

	// PCreate() and DestroyP() perform both allocation/deallocation and construction/destruction.
	template < class ... t_vtyArgs >
	t_TyAllocate * PCreate( t_vtyArgs && ... _args )
	{
		t_TyAllocate * pNode = allocate_type();
		_BIEN_TRY
		{
			new ( pNode ) t_TyAllocate( std::forward< t_vtyArgs >( _args )... );
		}
		_BIEN_UNWIND( deallocate_type( pNode ) );
		return pNode;
	}
	void DestroyP( t_TyAllocate * _pNode )
	{
		if ( _pNode )
		{
			_BIEN_TRY
			{
				_pNode->~t_TyAllocate();
			}
			_BIEN_UNWIND( deallocate_type( _pNode ) ); // Still deallocate the memory then rethrow.
			deallocate_type( _pNode );
		}
	}
};

__BIENUTIL_END_NAMESPACE
