#ifndef __TICONT_H__
#define __TICONT_H__


__BIENUTIL_BEGIN_NAMESPACE

// Simple list "delete" on destruct object.

template < class t_Ty >
class _sptr
{
  typedef _sptr< t_Ty >  _TyThis;
  
  t_Ty * m_pt;

public:

  _sptr() : m_pt( 0 )
  {
  }

  explicit _sptr( t_Ty * _pt ) : m_pt( _pt )
  {
  }

  explicit _sptr( _TyThis & _r ) : m_pt( _r.m_pt )
  {
    _r.Reset();
  }

  ~_sptr() _BIEN_NOTHROW
  {
    if ( m_pt )
    {
      delete m_pt;
    }
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

	t_Ty *	transfer() _BIEN_NOTHROW
	{
		t_Ty * _pt = m_pt;
    Reset();
		return _pt;
	}


  // acquire <_pt> - destruct any current object.
  void  operator = ( t_Ty * _pt ) _BIEN_NOTHROW
  {
    Release();
    m_pt = _pt;
  }

  // Transfers ownership from <_r>.
  void operator = ( _TyThis & _r )
  {
    Release();
    m_pt = _r;
    _r.Reset();
  }

  t_Ty *  operator ->() const _BIEN_NOTHROW
  {
    return m_pt;
  }
  t_Ty &  operator *() const _BIEN_NOTHROW
  {
    return *m_pt;
  }

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

};

// Implement a very simple object to call free() on a void& on destruct.

class FreeVoid
{
  typedef FreeVoid _tyThis;
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

#endif //__TICONT_H__

