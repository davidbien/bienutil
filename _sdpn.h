#ifndef __SDPN_H
#define __SDPN_H

//          Copyright David Lawrence Bien 1997 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _sdpn.h

// array deallocation object

#include "bien_assert.h"
#include "_allbase.h"

__BIENUTIL_BEGIN_NAMESPACE

template < class t_TyP, class t_TyAllocator >
class _sdpn
	: public _alloc_base< t_TyP, t_TyAllocator >
{
	typedef _sdpn< t_TyP, t_TyAllocator >				_TyThis;
	typedef _alloc_base< t_TyP, t_TyAllocator >	_TyBase;

	t_TyP *		m_pt;
	size_t	m_st;

public:

	_sdpn( t_TyAllocator const & _rAlloc ) _BIEN_NOTHROW
		: _TyBase( _rAlloc ),
			m_pt( 0 )
	{
	}

	_sdpn( t_TyP * _pt, size_t _st, t_TyAllocator const & _rAlloc ) _BIEN_NOTHROW
		: _TyBase( _rAlloc ),
			m_pt( _pt ),
			m_st( _st )
	{
	}

	_sdpn( _TyThis const & _r )
		: _TyBase( _r ),
			m_pt( 0 )
	{
		// If the other object has been allocated then allocate this one:
		if ( _r.m_pt )
		{
			allocate( _r.m_st );
		}
	}

	~_sdpn() _BIEN_NOTHROW
	{
		if ( m_pt )
		{
			_TyBase::deallocate_n( m_pt, m_st );
		}
	}

	size_t	size() const
	{
		return m_pt ? m_st : 0;
	}

	t_TyP *	begin()
	{
		return m_pt;
	}
	const t_TyP *	begin() const
	{
		return m_pt;
	}
	t_TyP *	end()
	{
		return m_pt ? m_pt + m_st : 0;
	}
	const t_TyP *	end() const
	{
		return m_pt ? m_pt + m_st : 0;
	}

	t_TyP &	operator []( size_t _st )
	{
		Assert( _st < m_st );
		return m_pt[ _st ];
	}

	t_TyP const &	operator []( size_t _st ) const
	{
		Assert( _st < m_st );
		return m_pt[ _st ];
	}

	t_TyP *	operator + ( size_t _st )
	{
		Assert( _st < m_st );
		return m_pt + _st;
	}

	void
	allocate( size_t _st )
	{
		Assert( !m_pt );
		_TyBase::allocate_n( m_pt, m_st = _st );
	}

	void
	destruct()
	{
		if ( m_pt )
		{
			t_TyP *	pt = m_pt;
			t_TyP *	ptEnd = m_pt + m_st;
			for ( ; pt != ptEnd; ++pt )
			{
				pt->~t_TyP();
			}
		}
	}

	template < class t_Ty1 >
	void
	construct1( t_Ty1 _r1 )
	{
		t_TyP *	pt = m_pt;
		t_TyP *	ptEnd = m_pt + m_st;
		for ( ; pt != ptEnd; ++pt )
		{
			new( pt ) t_TyP( _r1 );
		}
	}

	template < class t_Ty1, class t_Ty2 >
	void
	construct2( t_Ty1 _r1, t_Ty2 _r2 )
	{
		t_TyP *	pt = m_pt;
		t_TyP *	ptEnd = m_pt + m_st;
		for ( ; pt != ptEnd; ++pt )
		{
			new( pt ) t_TyP( _r1, _r2 );
		}
	}
	// etc.

	t_TyP *	transfer() _BIEN_NOTHROW
	{
		t_TyP * _pt = m_pt;
		m_pt = 0;
		return _pt;
	}

	operator t_TyP * () const _BIEN_NOTHROW
	{
		return m_pt;
	}

#if 0	// These make very little sense on this object - PtrRef() is dangerous - and unworkable
			//	since m_st is currently inaccessible
	t_TyP *& PtrRef() _BIEN_NOTHROW
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
#endif //0
};

__BIENUTIL_END_NAMESPACE

#endif //__SDPN_H
