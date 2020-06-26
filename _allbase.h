#pragma once

// _allbase.h
// dbien: Sometime in 1999/2000 I think.
// Generic base class for abstracting the difference between an instanced and static allocator.
// REVIEW: We only support instanced allocators at this point (post STLport).

#include <memory>
#include "bienutil.h"
#include "_util.h"
#include "_dbgthrw.h"

__BIENUTIL_BEGIN_NAMESPACE

#ifndef _USE_STLPORT

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
};

#else //!_USE_STLPORT

#ifndef __ICL

// msc++: doesn't support concept of instanceless allocator

template <	class t_TyAllocate, class t_TyAllocator >
class _alloc_base
{
	typedef _alloc_base< t_TyAllocate, t_TyAllocator >	_TyThis;

public:

	typedef simple_alloc< t_TyAllocate, __malloc_alloc >  _TyStaticAllocType;
	typedef _TyStaticAllocType 	_TyAllocatorType;
	typedef size_t	size_type;

	// Static allocator object - allows methods to return types desired in above 
	// This is likely not according to the operational characteristics of allocators.
	// But shouldn't be a problem since this template was chosen because this is
	// an instance-less allocator.
	static _TyAllocatorType ms_alloc;

	// Allow initialization with any allocator:
	template < class t__TyAllocator >
	_alloc_base( t__TyAllocator const & ) _BIEN_NOTHROW { }

	static __INLINE _TyAllocatorType & get_allocator_ref() _BIEN_NOTHROW
	{
		return ms_alloc;
	}
	
	static __INLINE t_TyAllocator get_allocator() _BIEN_NOTHROW
	{
		return t_TyAllocator();
	}
	
	static __INLINE t_TyAllocate *	allocate_type( )
	{
		__THROWPT( e_ttMemory );
		return _TyStaticAllocType::allocate( 1 );
	}

	static __INLINE void deallocate_type( t_TyAllocate * _pNode )
	{
		_TyStaticAllocType::deallocate( _pNode, 1 );
	}

	static __INLINE void	allocate_n( t_TyAllocate *& _rpNode, size_type __n )
	{
		__THROWPT( e_ttMemory );
		_rpNode = _TyStaticAllocType::allocate( __n );
	}
	static __INLINE void	deallocate_n( t_TyAllocate * _pNode, size_type __n )
	{
		_TyStaticAllocType::deallocate( _pNode, __n );
	}
};
  
template < class t_TyAllocate, class t_TyAllocator >
simple_alloc< t_TyAllocate, __malloc_alloc >
_alloc_base< t_TyAllocate, t_TyAllocator >::ms_alloc;

#else //!__ICL

// For an instanced allocator:
template <	class t_TyAllocate, class t_TyAllocator,
	bool t_fAllocStatic = _Alloc_traits< t_TyAllocate, t_TyAllocator >::_S_instanceless >
	class _alloc_base
{
	typedef _alloc_base< t_TyAllocate, t_TyAllocator, t_fAllocStatic >	_TyThis;

public:

	typedef typename _Alloc_traits< t_TyAllocate, t_TyAllocator >::_Alloc_type		_TyStaticAllocType;
	typedef typename _Alloc_traits< t_TyAllocate, t_TyAllocator >::allocator_type	_TyAllocatorType;
	typedef typename _TyAllocatorType::size_type	size_type;

protected:

	_TyAllocatorType	m_alloc;

public:

	// Allow initialization with an allocator.
	template < class t__TyAllocator >
	_alloc_base(t__TyAllocator const & _rAlloc) _BIEN_NOTHROW
		: m_alloc(_rAlloc)	// An error here indicates initialization with incompatible iterators.
	{ }

	__INLINE _TyAllocatorType const & get_allocator_ref() const _BIEN_NOTHROW
	{
		return m_alloc;
	}
	__INLINE _TyAllocatorType & get_allocator_ref() _BIEN_NOTHROW
	{
		return m_alloc;
	}

	__INLINE t_TyAllocator const & get_allocator() const _BIEN_NOTHROW
	{
		return m_alloc;
	}

	__INLINE t_TyAllocate *	allocate_type()
	{
		__THROWPT(e_ttMemory);
		return m_alloc.allocate(1);
	}

	__INLINE void deallocate_type(t_TyAllocate * _pNode)
	{
		m_alloc.deallocate(_pNode, 1);
	}

	template < class t_TyOther >
	__INLINE void deallocate_other(t_TyOther * _pOther)
	{
		typename _Alloc_traits< t_TyOther, t_TyAllocator >::allocator_type deallocOther(m_alloc);
		deallocOther.deallocate(_pOther);
	}

	__INLINE void	allocate_n(t_TyAllocate *& _rpNode, size_type __n)
	{
		__THROWPT(e_ttMemory);
		_rpNode = m_alloc.allocate(__n);
	}
	__INLINE void	deallocate_n(t_TyAllocate * _pNode, size_type __n)
	{
		m_alloc.deallocate(_pNode, __n);
	}
};

// For a static allocator.
template < class t_TyAllocate, class t_TyAllocator >
class _alloc_base< t_TyAllocate, t_TyAllocator, true >
{
	typedef _alloc_base< t_TyAllocate, t_TyAllocator, true >	_TyThis;

public:

	typedef typename _Alloc_traits< t_TyAllocate, t_TyAllocator >::_Alloc_type		_TyStaticAllocType;
	typedef typename _Alloc_traits< t_TyAllocate, t_TyAllocator >::allocator_type	_TyAllocatorType;
	typedef typename _TyAllocatorType::size_type	size_type;

	// Static allocator object - allows methods to return types desired in above 
	// This is likely not according to the operational characteristics of allocators.
	// But shouldn't be a problem since this template was chosen because this is
	// an instance-less allocator.
	static _TyAllocatorType ms_alloc;

	// Allow initialization with any allocator:
	template < class t__TyAllocator >
	_alloc_base(t__TyAllocator const &) _BIEN_NOTHROW { }

	static __INLINE _TyAllocatorType & get_allocator_ref() _BIEN_NOTHROW
	{
		return ms_alloc;
	}

	static __INLINE t_TyAllocator get_allocator() _BIEN_NOTHROW
	{
		return t_TyAllocator();
	}

	static __INLINE t_TyAllocate *	allocate_type()
	{
		__THROWPT(e_ttMemory);
		return _TyStaticAllocType::allocate(1);
	}

	static __INLINE void deallocate_type(t_TyAllocate * _pNode)
	{
		_TyStaticAllocType::deallocate(_pNode, 1);
	}

	template < class t_TyOther >
	static __INLINE void deallocate_other(t_TyOther * _pOther)
	{
		typedef typename _Alloc_traits< t_TyOther, t_TyAllocator >::_Alloc_type		_TyStaticAllocOther;
		_TyStaticAllocOther::deallocate(_pOther);
	}

	static __INLINE void	allocate_n(t_TyAllocate *& _rpNode, size_type __n)
	{
		__THROWPT(e_ttMemory);
		_rpNode = _TyStaticAllocType::allocate(__n);
	}
	static __INLINE void	deallocate_n(t_TyAllocate * _pNode, size_type __n)
	{
		_TyStaticAllocType::deallocate(_pNode, __n);
	}
};

template < class t_TyAllocate, class t_TyAllocator >
typename _Alloc_traits< t_TyAllocate, t_TyAllocator >::allocator_type
_alloc_base< t_TyAllocate, t_TyAllocator, true >::ms_alloc;

#endif //!__ICL

#endif //!_USE_STLPORT

__BIENUTIL_END_NAMESPACE
