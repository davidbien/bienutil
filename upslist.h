#pragma once

// upslist.h
// Unique ptr singly linked list type.
// dbien
// 11JAN2021
// A simple list that use unique_ptr<> to maintain a reference to the next element.

#include "bienutil.h"
#include "_util.h"

__BIENUTIL_BEGIN_NAMESPACE

template < class t_TyT >
class _UniquePtrSListEl
{
  typedef _UniquePtrSListEl _TyThis;
public:
  typedef t_TyT _TyT;
  typedef unique_ptr< _TyThis > _TyPtr;

  ~_UniquePtrSListEl() = default;
  _UniquePtrSListEl() = delete;
  _UniquePtrSListEl( const _UniquePtrSListEl & ) = delete;
  _UniquePtrSListEl & operator=( const _UniquePtrSListEl & ) = delete;
  _UniquePtrSListEl( _UniquePtrSListEl && ) = default;
  _UniquePtrSListEl & operator=( _UniquePtrSListEl && ) = default;

  _UniquePtrSListEl( t_TyT const & _rt )
    : m_t( _rt )
  {
  }
  _UniquePtrSListEl( t_TyT && _rrt )
    : m_t( std::move( _rrt ) )
  {
  }
  void swap( _TyThis & _r )
    requires( has_swap_c<t_TyT> )
  {
    m_upNext.swap( _r.m_upNext );
    m_t.swap( _r.m_t );
  }
  void swap( _TyThis & _r )
    requires( !has_swap_c<t_TyT> )
  {
    m_upNext.swap( _r.m_upNext );
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
  t_TyT m_t;
};

template < class t_TyT >
class UniquePtrSList
{
  typedef UniquePtrSList _TyThis;
public:
  typedef t_TyT _TyT;
  typedef t_TyT value_type;
  typedef _UniquePtrSListEl< _TyT > _TyListEl;
  typedef unique_ptr< _TyListEl > _TyPtr;

  UniquePtrSList() = default;
  UniquePtrSList( const UniquePtrSList & ) = delete;
  UniquePtrSList & operator=( const UniquePtrSList & ) = delete;
  UniquePtrSList( UniquePtrSList && ) = default;
  UniquePtrSList & operator=( UniquePtrSList && ) = default;
  void swap( _TyThis & _r )
  {
    m_upHead.swap( _r.m_upHead );
  }
  bool empty() const
  {
    return !m_upHead;
  }
  size_t count() const
  {
    size_t count = 0;
    const _TyListEl * pelCur = &*m_upHead;
    for ( ; !!pelCur; pelCur = &*pelCur->PtrNext() )
      ++count;
    return count;
  }
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
    Assert( !_rpt->PtrNext() );
    _rpt->PtrNext().swap( m_upHead );
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
    for ( ; !!pelCur && ( pelCur != _pel ); pelCur = &*pelCur->PtrNext() )
      ;
    return !!pelCur;
  }
  const _TyListEl * PListElFind( const t_TyT & _rt ) const
  {
    const _TyListEl * pelCur = &*m_upHead;
    for ( ; !!pelCur && ( **pelCur != _rt ); pelCur = &*pelCur->PtrNext() )
      ;
    return pelCur;
  }
  _TyListEl * PListElFind( const t_TyT & _rt )
  {
    const _TyListEl * pelCur = &*m_upHead;
    for ( ; !!pelCur && ( **pelCur != _rt ); pelCur = &*pelCur->PtrNext() )
      ;
    return pelCur;
  }
  template < class t_TyFunctor >
  void Apply( t_TyFunctor&& _rrFtor ) const
  {
    _TyListEl * pelCur = &*m_upHead;
    for ( ; !!pelCur; pelCur = &*pelCur->PtrNext() )
      std::forward< t_TyFunctor >( _rrFtor )( *pelCur );
  }
  template < class t_TyFunctor >
  void Apply( t_TyFunctor&& _rrFtor )
  {
    _TyListEl * pelCur = &*m_upHead;
    for ( ; !!pelCur; pelCur = &*pelCur->PtrNext() )
      std::forward< t_TyFunctor >( _rrFtor )( **pelCur );
  }
protected:
  _TyPtr m_upHead; // The head of the list.
};

__BIENUTIL_END_NAMESPACE

namespace std
{
__BIENUTIL_USING_NAMESPACE
  template < class t_TyT >
  void swap(UniquePtrSList< t_TyT >& _rl, UniquePtrSList< t_TyT >& _rr)
  {
    _rl.swap(_rr);
  }
}
