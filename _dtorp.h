#ifndef __DTORP_H
#define __DTORP_H

// _dtorp.h

// Object lifetime holder. Does not maintain allocation.

__BIENUTIL_BEGIN_NAMESPACE

template < class t_Ty >
class _dtorp
{
  typedef _dtorp< t_Ty >  _TyThis;
  
  t_Ty * m_pt;

public:

  _dtorp() : m_pt( 0 )
  {
  }

  explicit _dtorp( t_Ty * _pt ) : m_pt( _pt )
  {
  }

  explicit _dtorp( _TyThis & _r ) : m_pt( _r.m_pt )
  {
    _r.Reset();
  }

  ~_dtorp() __STL_NOTHROW
  {
    if ( m_pt )
    {
      m_pt->~t_Ty();
    }
  }

  t_Ty *  Ptr() const __STL_NOTHROW
  {
    return m_pt;
  }
  t_Ty *& PtrRef() __STL_NOTHROW
  {
    return m_pt;
  }

  void  Release() __STL_NOTHROW
  {
    if ( m_pt )
    {
      m_pt->~t_Ty();
      m_pt = 0;
    }
  }

  void  Reset() __STL_NOTHROW
  {
    m_pt = 0;
  }
  void  Reset( t_Ty * _pt ) __STL_NOTHROW
  {
    m_pt = _pt;
  }

  // acquire <_pt> - destruct any current object.
  void  operator = ( t_Ty * _pt ) __STL_NOTHROW
  {
    if ( m_pt )
    {
      m_pt->~t_Ty();
    }
    m_pt = _pt;
  }

  // Transfers ownership from <_r>.
  void operator = ( _TyThis & _r )
  {
    if ( m_pt )
    {
      m_pt->~t_Ty();
    }
    m_pt = _r;
    _r.Reset();
  }

  t_Ty *  operator ->() const __STL_NOTHROW
  {
    return m_pt;
  }
  t_Ty &  operator *() const __STL_NOTHROW
  {
    return *m_pt;
  }

  // Make convert to object pointer a non-const member,
  //  prevents accidental construction or assignment to
  //  a const-_TyThis - since then conversion to pointer is
  //  inaccessible.
  operator t_Ty * () __STL_NOTHROW
  {
    return m_pt;
  }

  operator const t_Ty * () const __STL_NOTHROW
  {
    return m_pt;
  }

#ifdef __GNUC__ // This gets rid of some warnings from gcc.
  operator bool () __STL_NOTHROW
  {
    return m_pt;
  }
#endif __GNUC__
};

__BIENUTIL_END_NAMESPACE

#endif __DTORP_H
