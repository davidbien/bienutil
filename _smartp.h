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

  ~_sptr() __STLP_NOTHROW
  {
    if ( m_pt )
    {
      delete m_pt;
    }
  }

  t_Ty *  Ptr() const __STLP_NOTHROW
  {
    return m_pt;
  }
  t_Ty *& PtrRef() __STLP_NOTHROW
  {
    return m_pt;
  }

  void  Release() __STLP_NOTHROW
  {
    if ( m_pt )
    {
      t_Ty * _pt = m_pt;
      m_pt = 0;
      delete _pt;
    }
  }

  void  Reset() __STLP_NOTHROW
  {
    m_pt = 0;
  }
  void  Reset( t_Ty * _pt ) __STLP_NOTHROW
  {
    m_pt = _pt;
  }

	t_Ty *	transfer() __STLP_NOTHROW
	{
		t_Ty * _pt = m_pt;
    Reset();
		return _pt;
	}


  // acquire <_pt> - destruct any current object.
  void  operator = ( t_Ty * _pt ) __STLP_NOTHROW
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

  t_Ty *  operator ->() const __STLP_NOTHROW
  {
    return m_pt;
  }
  t_Ty &  operator *() const __STLP_NOTHROW
  {
    return *m_pt;
  }

  // Make convert to object pointer a non-const member,
  //  prevents accidental construction or assignment to
  //  a const-_TyThis - since then conversion to pointer is
  //  inaccessible.
  operator t_Ty * () __STLP_NOTHROW
  {
    return m_pt;
  }

  operator const t_Ty * () const __STLP_NOTHROW
  {
    return m_pt;
  }

};

__BIENUTIL_END_NAMESPACE

#endif //__TICONT_H__

