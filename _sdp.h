#ifndef __SDP_H
#define __SDP_H

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _sdp.h

// Smart deallocation pointers - these deallocate memory allocated with an allocator.
// A good way ( though reasonably espensive ) to ensure deletion in throwing situations.

#include <assert.h>
#include <_allbase.h>

__BIENUTIL_BEGIN_NAMESPACE

template <	class t_TyP, class t_TyAllocator, class t_TyBase >
class _sdp;

template <	class t_TyP, class t_TyAllocator,
						class t_TyBase = _alloc_base< t_TyP, t_TyAllocator > >
class _sdp
	: public t_TyBase
{
	typedef _sdp _TyThis;
	typedef t_TyBase _TyBase;
protected:
	t_TyP *	m_pt{nullptr};
public:

	explicit _sdp( t_TyAllocator const & _rAlloc = t_TyAllocator() ) _BIEN_NOTHROW
		: _TyBase( _rAlloc )
	{
	}

	_sdp( t_TyP * _pt, t_TyAllocator const & _rAlloc ) _BIEN_NOTHROW
		: _TyBase( _rAlloc )
	{
	}

	_sdp( _TyThis const & _r ) = delete;
	_TyThis & operator=( _TyThis const & ) = delete;

	// Allow move construction and assignment.
	_sdp( _TyThis && _rr )
		: _TyBase( std::move( _rr ) )
	{
		m_pt = _rr.transfer();	
	}
	_TyThis & operator = ( _TyThis && _rr )
	{
		clear();
		((_tyBase&)*this) = std::move( (_tyBase&)_rr );
		m_pt = _rr.transfer();
		return *this;
	}
	~_sdp() _BIEN_NOTHROW
	{
		if ( m_pt )
			_TyBase::deallocate_type( m_pt );
	}

	void allocate()
	{
		assert( !m_pt );
		m_pt = _TyBase::allocate_type();
	}
	void clear()
	{
		if ( !!m_pt )
		{
			t_TyP * pt = m_pt;
			m_pt = 0;
			_TyBase::deallocate_type( pt );
		}
	}
	t_TyP *	transfer() _BIEN_NOTHROW
	{
		t_TyP * _pt = m_pt;
		m_pt = 0;
		return _pt;
	}
	t_TyP *	Ptr() const _BIEN_NOTHROW
	{
		return m_pt;
	}
	t_TyP *& PtrRef() _BIEN_NOTHROW
	{
		return m_pt;
	}
	operator t_TyP * () const _BIEN_NOTHROW
	{
		return m_pt;
	}
	t_TyP * operator ->() const _BIEN_NOTHROW
	{
		return m_pt;
	}
	t_TyP & operator *() const _BIEN_NOTHROW
	{
		return *m_pt;
	}
	bool operator !() const _BIEN_NOTHROW
	{
		return !m_pt;
	}
protected: // Are these accessed publicly - if not then use _sdpd
	void destruct()
	{
		if ( m_pt )
			m_pt->~t_TyP();
	}
};

__BIENUTIL_END_NAMESPACE

#define __SDP( _TyP, _TyAllocator, _alloc, p )	_sdp< _TyP, _TyAllocator >	p( _alloc )
#define __SDP_TRANSFER( p )											((p).transfer())
// This macro activates when we are not using SDP:
#define __SDP_CHECK_VOID( p )
#define __SDP_PTRREF( p ) ((p).PtrRef())

#if 0 // #else !__NUSESDP

// No need to zero - assumption is we don't care.
#define __SDP( _TyP, _TyAllocator, _alloc, p )	_TyP * p
#define __SDP_TRANSFER( p )											(p)
#define __SDP_CHECK_VOID( p )										if ( !(p) )	return
#define __SDP_PTRREF( p ) (p)

#endif //0 !__NUSESDP

#endif //__SDP_H
