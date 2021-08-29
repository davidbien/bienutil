#pragma once

//          Copyright David Lawrence Bien 1997 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// updlist.h
// Unique ptr doubly linked list type.
// dbien
// 11JAN2021
// A simple doubly linked list  that use unique_ptr<> to maintain a reference to the next element.

#include "bienutil.h"

__BIENUTIL_BEGIN_NAMESPACE

#error I stopped coding this because I decided to just use std::list for what I was doing.

template < class t_TyT >
class _UniquePtrDListEl
{
  typedef _UniquePtrDListEl _TyThis;
public:
  typedef t_TyT _TyT;
  typedef unique_ptr< _TyThis > _TyPtr;

  ~_UniquePtrDListEl() = default;
  _UniquePtrDListEl() = delete;
  _UniquePtrDListEl( const _UniquePtrDListEl & ) = delete;
  _UniquePtrDListEl & operator=( const _UniquePtrDListEl & ) = delete;
  _UniquePtrDListEl( _UniquePtrDListEl && ) = default;
  _UniquePtrDListEl & operator=( _UniquePtrDListEl && ) = default;

  _UniquePtrDListEl( t_TyT const & _rt )
    : m_t( _rt )
  {
  }
  _UniquePtrDListEl( t_TyT && _rrt )
    : m_t( std::move( _rrt ) )
  {
  }
  void swap( _TyThis & _r )
    requires( is_member_function_pointer_v<decltype(&t_TyT::swap)> )
  {
    m_upThis.swap( _r.m_upThis );
    m_t.swap( _r.m_t );
  }
  void swap( _TyThis & _r )
    requires( !is_member_function_pointer_v<decltype(&t_TyT::swap)> )
  {
    m_upThis.swap( _r.m_upThis );
    std::swap( m_t, _r.m_t );
  }
  const t_TyT & operator *() const
  {
    return m_t;
  }
  t_TyT & operator *()
  {
    return m_t;
  }
  const t_TyT * operator ->() const
  {
    return &m_t;
  }
  t_TyT * operator ->()
  {
    return &m_t;
  }
  _TyPtr & PtrNext()
  {
    return m_upNext;
  }
  const _TyPtr & PtrNext() const
  {
    return m_upNext;
  }
protected:
  _TyPtr m_upNext;
  _TyThis * m_pPrev;
  t_TyT m_t;
};

// UniquePtrDList: Yer doubly linked list container object.
template < class t_TyT >
class UniquePtrDList
{
  typedef UniquePtrDList _TyThis;
public:
  typedef t_TyT _TyT;
  typedef t_TyT value_type;
  typedef _UniquePtrDListEl< _TyT > _TyListEl;
  typedef unique_ptr< _TyListEl > _TyPtr;

  const t_TyT & front() const
  {
    VerifyThrow( !!m_upHead );
    return **m_upHead;
  }
  t_TyT & front()
  {
    VerifyThrow( !!m_upHead );
    return **m_upHead;
  }
  const _TyListEl * PListElFront() const
  {
    return &*m_upHead;
  }
  _TyListEl * PListElFront()
  {
    return &*m_upHead;
  }
  void push( _TyPtr & _rpt )
  {
    Assert( !_rpt.PtrNext() );
    _rpt.PtrNext().swap( _rpt );
    m_upHead.swap( _rpt );
  }
  void push( t_TyT && _rrt )
  {
    _TyPtr upPush = make_unique<_TyListEl>( std::move( _rrt ) );
    push( upPush );
  }
  void push( const t_TyT & _rt )
  {
    _TyPtr upPush = make_unique<_TyListEl>( _rt );
    push( upPush );
  }
  void pop()
  {
    _TyPtr upClear;
    upClear.swap( m_upHead->PtrNext() );
    m_upHead.swap( upClear );
  }
  // Pop and return the top in _TyPtr;
  void pop( _TyPtr & _upTop )
  {
    _upTop.reset();
    _upTop.swap( m_upHead->PtrNext() );
    m_upHead.swap( _upTop );
  }
  bool FFind( const _TyListEl * _pel )
  {
    const _TyListEl * pelCur = &*m_upHead;
    for ( ; !!pelCur && ( pelCur != _pel ); pelCur = *&pelCur->PtrNext() )
      ;
    return !!pelCur;
  }
  const _TyListEl * PListElFind( const t_TyT & _rt ) const
  {
    const _TyListEl * pelCur = &*m_upHead;
    for ( ; !!pelCur && ( **pelCur != _rt ); pelCur = *&pelCur->PtrNext() )
      ;
    return pelCur;
  }
  _TyListEl * PListElFind( const t_TyT & _rt )
  {
    const _TyListEl * pelCur = &*m_upHead;
    for ( ; !!pelCur && ( **pelCur != _rt ); pelCur = *&pelCur->PtrNext() )
      ;
    return pelCur;
  }
protected:
  _TyPtr m_upHead; // The head of the list.
};


__BIENUTIL_END_NAMESPACE
