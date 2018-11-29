#ifndef __ALLBASE_H
#define __ALLBASE_H

// _allbase.h

// Generic base class for abstracting the difference between an instanced and static allocator.

#include "bienutil.h"
#include "_util.h"
#include "_dbgthrw.h"

__BIENUTIL_BEGIN_NAMESPACE

#ifdef __ICL
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
	_alloc_base( t__TyAllocator const & _rAlloc ) _STLP_NOTHROW
		: m_alloc( _rAlloc )	// An error here indicates initialization with incompatible iterators.
	{ }

	__INLINE _TyAllocatorType const & get_allocator_ref() const _STLP_NOTHROW
	{
		return m_alloc;
	}
	__INLINE _TyAllocatorType & get_allocator_ref() _STLP_NOTHROW
	{
		return m_alloc;
	}

	__INLINE t_TyAllocator get_allocator() const _STLP_NOTHROW
	{
		return m_alloc;
	}
	
	__INLINE t_TyAllocate *	allocate_type( )
	{
		__THROWPT( e_ttMemory );
		return m_alloc.allocate( 1 );
	}

	template < class t_TyOther >
	__INLINE void allocate_other( t_TyOther *& _rpOther )
	{
		__THROWPT( e_ttMemory );
		typename _Alloc_traits< t_TyOther, t_TyAllocator >::allocator_type allocOther( m_alloc );
		_rpOther = allocOther.allocate( 1 );
	}
	
	__INLINE void deallocate_type( t_TyAllocate * _pNode )
	{
		m_alloc.deallocate( _pNode, 1 );
	}

	template < class t_TyOther >
	__INLINE void deallocate_other( t_TyOther * _pOther )
	{
		typename _Alloc_traits< t_TyOther, t_TyAllocator >::allocator_type deallocOther( m_alloc );
		deallocOther.deallocate( _pOther );
	}
	
	__INLINE void	allocate_n( t_TyAllocate *& _rpNode, size_type __n )
	{
		__THROWPT( e_ttMemory );
		_rpNode = m_alloc.allocate( __n );
	}
	__INLINE void	deallocate_n( t_TyAllocate * _pNode, size_type __n )
	{
		m_alloc.deallocate( _pNode, __n );
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
	_alloc_base( t__TyAllocator const & ) _STLP_NOTHROW { }

	static __INLINE _TyAllocatorType & get_allocator_ref() _STLP_NOTHROW
	{
		return ms_alloc;
	}
	
	static __INLINE t_TyAllocator get_allocator() _STLP_NOTHROW
	{
		return t_TyAllocator();
	}
	
	static __INLINE t_TyAllocate *	allocate_type( )
	{
		__THROWPT( e_ttMemory );
		return _TyStaticAllocType::allocate( 1 );
	}

	template < class t_TyOther >
	static __INLINE void	allocate_other( t_TyOther *& _rpOther )
	{
		__THROWPT( e_ttMemory );
		typedef typename _Alloc_traits< t_TyOther, t_TyAllocator >::_Alloc_type		_TyStaticAllocOther;
		_rpOther = _TyStaticAllocOther::allocate( 1 );
	}
	
	static __INLINE void deallocate_type( t_TyAllocate * _pNode )
	{
		_TyStaticAllocType::deallocate( _pNode, 1 );
	}

	template < class t_TyOther >
	static __INLINE void deallocate_other( t_TyOther * _pOther )
	{
		typedef typename _Alloc_traits< t_TyOther, t_TyAllocator >::_Alloc_type		_TyStaticAllocOther;
		_TyStaticAllocOther::deallocate( _pOther );
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
typename _Alloc_traits< t_TyAllocate, t_TyAllocator >::allocator_type
_alloc_base< t_TyAllocate, t_TyAllocator, true >::ms_alloc;

#else __ICL

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
	_alloc_base( t__TyAllocator const & ) _STLP_NOTHROW { }

	static __INLINE _TyAllocatorType & get_allocator_ref() _STLP_NOTHROW
	{
		return ms_alloc;
	}
	
	static __INLINE t_TyAllocator get_allocator() _STLP_NOTHROW
	{
		return t_TyAllocator();
	}
	
	static __INLINE t_TyAllocate *	allocate_type( )
	{
		__THROWPT( e_ttMemory );
		return _TyStaticAllocType::allocate( 1 );
	}

	template < class t_TyOther >
	static __INLINE void	allocate_other( t_TyOther *& _rpOther )
	{
		__THROWPT( e_ttMemory );
		typedef typename _Alloc_traits< t_TyOther, t_TyAllocator >::_Alloc_type		_TyStaticAllocOther;
		_rpOther = _TyStaticAllocOther::allocate( 1 );
	}
	
	static __INLINE void deallocate_type( t_TyAllocate * _pNode )
	{
		_TyStaticAllocType::deallocate( _pNode, 1 );
	}

	template < class t_TyOther >
	static __INLINE void deallocate_other( t_TyOther * _pOther )
	{
		typedef typename _Alloc_traits< t_TyOther, t_TyAllocator >::_Alloc_type		_TyStaticAllocOther;
		_TyStaticAllocOther::deallocate( _pOther );
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

#endif __ICL

__BIENUTIL_END_NAMESPACE

#endif __ALLBASE_H
