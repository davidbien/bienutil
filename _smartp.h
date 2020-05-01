#ifndef __SMARTP_H_
#define __SMARTP_H_


__BIENUTIL_BEGIN_NAMESPACE

// Simple list "delete" on destruct object.

template < class t_Ty >
class _sptr
{
  typedef _sptr _TyThis;
  t_Ty * m_pt;
public:
  _sptr() : m_pt( 0 )
  {
  }
  explicit _sptr( t_Ty * _pt ) : m_pt( _pt )
  {
  }
  explicit _sptr( _sptr & _r ) : m_pt( _r.m_pt )
  {
    _r.Reset();
  }
  _sptr( _sptr && _rr )
  {
    _rr.swap( *this );
  }
  ~_sptr() _BIEN_NOTHROW
  {
    if ( m_pt )
    {
      delete m_pt;
    }
  }
  void swap( _sptr & _r )
  {
    std:swap( _r.m_pt, m_pt );
  }

  t_Ty *  Ptr() const _BIEN_NOTHROW
  {
    return m_pt;
  }
  t_Ty *& PtrRef() _BIEN_NOTHROW
  {
    return m_pt;
  }

  void  Release() _BIEN_NOTHROW
  {
    if ( m_pt )
    {
      t_Ty * _pt = m_pt;
      m_pt = 0;
      delete _pt;
    }
  }
  void  Reset() _BIEN_NOTHROW
  {
    m_pt = 0;
  }
  void  Reset( t_Ty * _pt ) _BIEN_NOTHROW
  {
    m_pt = _pt;
  }
	t_Ty * transfer() _BIEN_NOTHROW
	{
		t_Ty * _pt = m_pt;
    Reset();
		return _pt;
	}


  // acquire <_pt> - destruct any current object.
  _TyThis & operator = ( t_Ty * _pt ) _BIEN_NOTHROW
  {
    if ( _pt != m_pt )
    {
      Release();
      m_pt = _pt;
    }
    return *this;
  }

  // Transfers ownership from <_r>.
  _TyThis & operator = ( _TyThis && _rr )
  {
    if ( this != &_rr )
    {
      Release();
      this->swap( _rr );
    }
    return *this;
  }

  t_Ty * operator ->() const _BIEN_NOTHROW
  {
    return m_pt;
  }
  t_Ty & operator *() const _BIEN_NOTHROW
  {
    return *m_pt;
  }

#if 0 // Don't see why these are desireable.
  // Make convert to object pointer a non-const member,
  //  prevents accidental construction or assignment to
  //  a const-_TyThis - since then conversion to pointer is
  //  inaccessible.
  operator t_Ty * () _BIEN_NOTHROW
  {
    return m_pt;
  }

  operator const t_Ty * () const _BIEN_NOTHROW
  {
    return m_pt;
  }
#endif //0
};

// Implement a very simple object to call free() on a void* on destruct.

class FreeVoid
{
  typedef FreeVoid _TyThis;
public:
  FreeVoid( void* _pv )
    : m_pv(_pv)
  { }
  ~FreeVoid()
  {
    if ( m_pv )
      free( m_pv );
  }
  void Clear()
  {
    if ( m_pv )
    {
      void * pv = m_pv;
      m_pv = 0;
      free( pv );
    }
  }
  void * PvTransfer()
  {
      void * pv = m_pv;
      m_pv = 0;
      return pv;
  }
protected:
  void * m_pv;
};

__BIENUTIL_END_NAMESPACE

#endif //__SMARTP_H_

