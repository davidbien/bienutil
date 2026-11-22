#ifndef __DTORP_H
#define __DTORP_H

//          Copyright David Lawrence Bien 1997 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _dtorp.h

// Object lifetime holder. Does not maintain allocation.

__BIENUTIL_BEGIN_NAMESPACE

// How _dtorp ends the life of its object: the destructor. An overload for a type which must do more - _sdpv.h's
//  _sdp_vbase<> frees itself as well - is found by argument-dependent lookup.
template < class t_Ty >
void _DtorpDestroy( t_Ty * _pt ) _BIEN_NOTHROW
{
  _pt->~t_Ty();
}

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

  ~_dtorp() _BIEN_NOTHROW
  {
    if ( m_pt )
    {
      _DtorpDestroy( m_pt );
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
      _DtorpDestroy( m_pt );
      m_pt = 0;
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

  // acquire <_pt> - destruct any current object.
  void  operator = ( t_Ty * _pt ) _BIEN_NOTHROW
  {
    if ( m_pt )
    {
      _DtorpDestroy( m_pt );
    }
    m_pt = _pt;
  }

  // Transfers ownership from <_r>.
  void operator = ( _TyThis & _r )
  {
    if ( m_pt )
    {
      _DtorpDestroy( m_pt );
    }
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

#ifdef __GNUC__ // This gets rid of some warnings from gcc.
  operator bool () _BIEN_NOTHROW
  {
    return m_pt;
  }
#endif //__GNUC__
};

__BIENUTIL_END_NAMESPACE

#endif //__DTORP_H
