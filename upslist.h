#pragma once

//          Copyright David Lawrence Bien 1997 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

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
    requires( c_has_swap<t_TyT> )
  {
    m_upNext.swap( _r.m_upNext );
    m_t.swap( _r.m_t );
  }
  void swap( _TyThis & _r )
    requires( !c_has_swap<t_TyT> )
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
  UniquePtrSList & operator=( UniquePtrSList && _rr )
  {
    if ( this != &_rr )
    {
      clear();
      swap( _rr );
    }
    return *this;
  }
  // The elements are freed one at a time: left to the unique_ptr<>s, each frees the next within itself - as deep a
  //  recursion as the list is long.
  ~UniquePtrSList()
  {
    clear();
  }
  void clear()
  {
    while ( !!m_upHead )
    {
      _TyPtr upNext;
      upNext.swap( m_upHead->PtrNext() );
      m_upHead.swap( upNext ); // upNext is now the old head, alone: freed here.
    }
  }
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
    // get(), not &*: * of an empty unique_ptr<> is undefined.
    const _TyListEl * pelCur = m_upHead.get();
    for ( ; !!pelCur; pelCur = pelCur->PtrNext().get() )
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
    return m_upHead.get();
  }
  _TyListEl * PListElFront()
  {
    return m_upHead.get();
  }
  void push( _TyPtr & _rpt )
  {
    VerifyThrow( !!_rpt );
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
    VerifyThrow( !!m_upHead ); // Of an empty list: before, null was dereferenced.
    _TyPtr upClear;
    upClear.swap( m_upHead->PtrNext() );
    m_upHead.swap( upClear );
  }
  // Pop and return the top in _TyPtr;
  void pop( _TyPtr & _upTop )
  {
    VerifyThrow( !!m_upHead );
    _upTop.reset();
    _upTop.swap( m_upHead->PtrNext() );
    m_upHead.swap( _upTop );
  }
  bool FFind( const _TyListEl * _pel ) const
  {
    const _TyListEl * pelCur = m_upHead.get();
    for ( ; !!pelCur && ( pelCur != _pel ); pelCur = pelCur->PtrNext().get() )
      ;
    return !!pelCur;
  }
  const _TyListEl * PListElFind( const t_TyT & _rt ) const
  {
    const _TyListEl * pelCur = m_upHead.get();
    for ( ; !!pelCur && ( **pelCur != _rt ); pelCur = pelCur->PtrNext().get() )
      ;
    return pelCur;
  }
  _TyListEl * PListElFind( const t_TyT & _rt )
  {
    return const_cast< _TyListEl * >( const_cast< const _TyThis * >( this )->PListElFind( _rt ) );
  }
  // The functor is given each value, from the front: const of a const list. ( Before, the const version did not
  //  compile, and would have given the element rather than the value. )
  template < class t_TyFunctor >
  void Apply( t_TyFunctor&& _rrFtor ) const
  {
    const _TyListEl * pelCur = m_upHead.get();
    for ( ; !!pelCur; pelCur = pelCur->PtrNext().get() )
      std::forward< t_TyFunctor >( _rrFtor )( **pelCur );
  }
  template < class t_TyFunctor >
  void Apply( t_TyFunctor&& _rrFtor )
  {
    _TyListEl * pelCur = m_upHead.get();
    for ( ; !!pelCur; pelCur = pelCur->PtrNext().get() )
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
