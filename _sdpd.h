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
	typedef _sdp< t_TyP, t_TyAllocator >			_TyBase;
	typedef _sdpd< t_TyP, t_TyAllocator >			_TyThis;

	bool	m_fConstructed;	// Set to true if this object has been successfully constructed.
	using _TyBase::m_pt;

public:
	using _TyBase::allocate;
	using _TyBase::Ptr;
	using _TyBase::PtrRef;
	using _TyBase::operator t_TyP *;
	using _TyBase::operator ->;
	using _TyBase::operator *;
	using _TyBase::operator !;

  _sdpd() _BIEN_NOTHROW
    : _TyBase( t_TyAllocator() ),
			m_fConstructed( false )
  {
  }

	explicit _sdpd( t_TyAllocator const & _rAlloc ) _BIEN_NOTHROW
		: _TyBase( _rAlloc ),
			m_fConstructed( false )
	{
	}

	// We transfer ownership on copy-construct:
	explicit _sdpd( _TyThis const & _r )
		: _TyBase( _r.get_allocator() ),
			m_fConstructed( _r.m_fConstructed )
	{
		m_pt = const_cast< _TyThis & >( _r ).transfer();
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

  // Indicate that the contained object is constructed.
  void  SetConstructed() _BIEN_NOTHROW
  {
    m_fConstructed = true;
  }

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

	void	destruct()
	{
		if ( m_fConstructed )
		{
			m_fConstructed = false;
			_TyBase::destruct();
		}
	}

	t_TyP *	transfer() _BIEN_NOTHROW
	{
		m_fConstructed = false;
		return _TyBase::transfer();
	}

};

__BIENUTIL_END_NAMESPACE

#endif //__SDPD_H
