#ifndef __SDPD_H
#define __SDPD_H

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
	}
	_sdpd & operator = ( _TyThis && _rr )
	{
		m_alloc = _rr.m_alloc; // Don't move the allocator - at least for now.
		m_fConstructed = _rr.m_fConstructed;
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
			_tyBase::clear();
		
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
// Construction.
	template < class ... t_vtyArgs >
	_tyT & emplace( t_vtyArgs && ... _args )
	{
		clear( false );
		if ( !m_pt )
			allocate();
		new ( m_pt ) t_TyP( std::forward< t_vtyArgs >( _args )... );
		m_fConstructed = true;
	}
	void	destruct()
	{
		if ( m_fConstructed )
		{
			m_fConstructed = false;
			_TyBase::destruct();
		}
	}
#if 0
	// construction templates:
	void	construct()
	{
		if ( !m_pt )
			allocate();
		assert( !m_fConstructed );
		new ( m_pt ) t_TyP();
		m_fConstructed = true;
	}
	template < class t_Ty1 >
	void
	construct1( t_Ty1 _r1 )
	{
		if ( !m_pt )
			allocate();
		assert( !m_fConstructed );
		new( m_pt ) t_TyP( _r1 );
		m_fConstructed = true;
	}
	template < class t_Ty1, class t_Ty2 >
	void
	construct2( t_Ty1 _r1, t_Ty2 _r2 )
	{
		if ( !m_pt )
			allocate();
		assert( !m_fConstructed );
		new( m_pt ) t_TyP( _r1, _r2 );
		m_fConstructed = true;
	}
	template < class t_Ty1, class t_Ty2, class t_Ty3 >
	void
	construct3( t_Ty1 _r1, t_Ty2 _r2, t_Ty3 _r3 )
	{
		if ( !m_pt )
			allocate();
		assert( !m_fConstructed );
		new( m_pt ) t_TyP( _r1, _r2, _r3 );
		m_fConstructed = true;
	}
	template < class t_Ty1, class t_Ty2, class t_Ty3, class t_Ty4 >
	void
	construct4( t_Ty1 _r1, t_Ty2 _r2, t_Ty3 _r3, t_Ty4 _r4 )
	{
		if ( !m_pt )
			allocate();
		assert( !m_fConstructed );
		new( m_pt ) t_TyP( _r1, _r2, _r3, _r4 );
		m_fConstructed = true;
	}
	// etc...
#endif //0
};

__BIENUTIL_END_NAMESPACE

#endif //__SDPD_H
