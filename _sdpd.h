#ifndef __SDPD_H
#define __SDPD_H

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _sdpd.h

// Smart deallocation pointer with auto-destruction capabilities.

#include "bienutil.h"
#include "_sdp.h"

__BIENUTIL_BEGIN_NAMESPACE

// Use protected inheritance to avoid leaking a destruction.
template < class t_TyP, class t_TyAllocator >
class _sdpd : protected _sdp< t_TyP, t_TyAllocator >
{
	typedef _sdp< t_TyP, t_TyAllocator > _TyBase;
	typedef _sdpd _TyThis;

	bool m_fConstructed{false};	// Set to true if this object has been successfully constructed.
	using _TyBase::m_pt;

public:
	using _TyBase::get_allocator;
	using _TyBase::allocate;
	using _TyBase::Ptr;
	using _TyBase::PtrRef;
	using _TyBase::operator t_TyP *;
	using _TyBase::operator ->;
	using _TyBase::operator *;
	using _TyBase::operator !;

  _sdpd() _BIEN_NOTHROW
    : _TyBase( t_TyAllocator() )
  {
  }

	explicit _sdpd( t_TyAllocator const & _rAlloc ) _BIEN_NOTHROW
		: _TyBase( _rAlloc )
	{
	}

	// Disallow copy construction and assignment - especially since we have been using it as an r-value-ref.
	_sdpd( _TyThis const & _r ) = delete;
	_sdpd & operator=( _TyThis const & _r ) = delete;

	// Allow move construction and assignment.
	_sdpd( _TyThis && _rr )
		: _TyBase( _rr.get_allocator() ),
			m_fConstructed( _rr.m_fConstructed )
	{
		m_pt = _rr.transfer();
		_rr.m_fConstructed = false;
	}
	_sdpd & operator = ( _TyThis && _rr )
	{
		//m_alloc = _rr.m_alloc; // Don't muck with the allocator.
		if (m_fConstructed)
		{
			m_fConstructed = false;
			_TyBase::destruct();
			_TyBase::clear();
		}
		m_fConstructed = _rr.m_fConstructed;
		_rr.m_fConstructed = false;
		m_pt = _rr.transfer();
	}

  // Aquire a potentially constructed pointer:
  _sdpd(  t_TyP * _pt, bool _fConstructed, 
          t_TyAllocator const & _rAlloc )
    : _TyBase( _rAlloc ),
      m_fConstructed( _fConstructed )
  {
    m_pt = _pt;
  }    

	~_sdpd()
	{
		if ( m_fConstructed )
			_TyBase::destruct();
	}
	void clear( bool _fDeallocate = true )
	{
		if ( m_fConstructed )
		{
			m_fConstructed = false;
			_TyBase::destruct();
		}
		if ( _fDeallocate )
			_TyBase::clear();		
	}

  // Indicate that the contained object is constructed.
  void  SetConstructed() _BIEN_NOTHROW
  {
    m_fConstructed = true;
  }
	t_TyP *	transfer() _BIEN_NOTHROW
	{
		m_fConstructed = false;
		return _TyBase::transfer();
	}
// Construction/destruction.
	template < class ... t_vtyArgs >
	t_TyP & emplace( t_vtyArgs && ... _args )
	{
		clear( false );
		if ( !m_pt )
			allocate();
		new ( m_pt ) t_TyP( std::forward< t_vtyArgs >( _args )... );
		m_fConstructed = true;
		return *m_pt;
	}
	void destruct()
	{
		if ( m_fConstructed )
		{
			m_fConstructed = false;
			_TyBase::destruct();
		}
	}
};

__BIENUTIL_END_NAMESPACE

#endif //__SDPD_H
