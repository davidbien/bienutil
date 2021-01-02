#ifndef __LISTEL_H__
#define __LISTEL_H__

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _listel.h

// List element, templatized by most derived class.

#include "bienutil.h"

__BIENUTIL_BEGIN_NAMESPACE

template < class t_TyMD >
class _list_el
{
  typedef _list_el< t_TyMD >  _TyThis;

public:

  t_TyMD *  m_pmdNext;
  t_TyMD ** m_ppmdPrevNext;

  void  insert( t_TyMD *& _pmdBefore )
  {
    m_ppmdPrevNext = &_pmdBefore;
    m_pmdNext = _pmdBefore;
    if ( _pmdBefore )
    {
      _pmdBefore->m_ppmdPrevNext = &m_pmdNext;
    }
    _pmdBefore = static_cast< t_TyMD *>( this );
  }
  void  remove()
  {
    if ( m_pmdNext )
    {
      m_pmdNext->m_ppmdPrevNext = m_ppmdPrevNext;
    }
    *m_ppmdPrevNext = m_pmdNext;
  }

  // methods for a list using a tail.
  void  insert_tail( t_TyMD *& _pmdBefore, t_TyMD **& _rppmdTail ) noexcept(true)
  {
    m_ppmdPrevNext = &_pmdBefore;
    m_pmdNext = _pmdBefore;
    if ( _pmdBefore )
    {
      _pmdBefore->m_ppmdPrevNext = &m_pmdNext;
    }
    else
    {
      Assert( m_ppmdPrevNext == _rppmdTail );
      _rppmdTail = &m_pmdNext;
    }
    _pmdBefore = static_cast< t_TyMD * >( this );
  }
  void  push_back_tail( t_TyMD **& _rppmdTail ) noexcept(true)
  {
    m_ppmdPrevNext = _rppmdTail;
    *_rppmdTail = this;
    m_pmdNext = 0;
    _rppmdTail = &m_pmdNext;
  }
  void  remove_tail( t_TyMD **& _rppmdTail ) noexcept(true)
  {
    if ( m_pmdNext )
    {
      m_pmdNext->m_ppmdPrevNext = m_ppmdPrevNext;
    }
    *m_ppmdPrevNext = m_pmdNext;
    if ( _rppmdTail == &m_pmdNext )
    {
      _rppmdTail = m_ppmdPrevNext;
    }
  }

  static t_TyMD * PMDFromPPNext( t_TyMD ** _ppcsNext )
  {
    return (t_TyMD*)( (char*)_ppcsNext - offsetof( t_TyMD, _TyThis::m_pmdNext ) );
  }


};

__BIENUTIL_END_NAMESPACE

#endif //__LISTEL_H__
