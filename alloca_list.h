#pragma once

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// alloca_list.h
// 22FEB2020 David Bien
// Implement a generic singly-linked list using _alloca().

#include "bienutil.h"

__BIENUTIL_BEGIN_NAMESPACE

template < class t_tyT, bool t_fOwnObjectLifetime >
class AllocaList;

template <class t_TyT>
class _AllocaListEl
{
  typedef _AllocaListEl _TyThis;
  friend AllocaList< t_TyT, true >;
  friend AllocaList< t_TyT, false >;
public:
  typedef t_TyT _TyT;

  ~_AllocaListEl() = default;
  _AllocaListEl() = default;
  _AllocaListEl(_AllocaListEl const &) = default;
  _AllocaListEl &operator=(_AllocaListEl const &) = default;
  _AllocaListEl(_AllocaListEl &&) = default;
  _AllocaListEl &operator=(_AllocaListEl &&) = default;

  template < class ... t_TysArgs >
  _AllocaListEl( t_TysArgs&& ... _args )
    : m_t( std::forward<t_TysArgs>(_args) ... )
  {
  }
#if 0
  _AllocaListEl(_TyT const & _rt, _TyThis *_pNext)
    : m_t( _rt ),
      m_pNext( _pNext )
  {
  }
  _AllocaListEl(_TyT && _rrt, _TyThis *_pNext)
    : m_t( std::move( _rrt ) ),
      m_pNext( _pNext )
  {
  }
#endif //0
  _TyThis * PNext() const
  {
    return m_pNext;
  }
	const _TyT * operator ->() const
	{
		return &m_t;
	}
	_TyT * operator ->()
	{
		return &m_t;
	}
	const _TyT & operator *() const
	{
		return m_t;
	}
	_TyT & operator *()
	{
		return m_t;
	}
protected : 
  _TyThis *m_pNext{nullptr};
  _TyT m_t;
};

template < class t_tyT, bool t_fOwnObjectLifetime >
class AllocaList
{
  typedef AllocaList _TyThis;
public:
  typedef t_tyT _TyT;
  static constexpr bool s_kfOwnObjectLifetime = t_fOwnObjectLifetime;
  typedef _AllocaListEl< _TyT > _TyAllocaListEl;

  ~AllocaList()
  {
    if ( s_kfOwnObjectLifetime )
      _Clear();
  }
  AllocaList() = default;
  AllocaList( const AllocaList & ) = delete;
  AllocaList & operator=( const AllocaList & ) = delete;
  AllocaList( AllocaList && _rr )
  {
    std::swap( m_pHead, _rr.m_pHead );
  }
  AllocaList & operator=( AllocaList && _rr )
  {
    AllocaList acquire( std::move( _rr ) );
    swap( acquire );
  }
  void swap( _TyThis & _r )
  {
    std::swap( m_pHead, _r.m_pHead );
  }
  template < class ... t_TysArgs >
  void push_emplace( void * _pvPushAllocated, t_TysArgs&& ... _args )
  {
    _TyAllocaListEl * pale = new( _pvPushAllocated ) _TyAllocaListEl( std::forward< t_TysArgs >( _args ) ...  ); // This may throw and that's ok.
    pale->m_pNext = m_pHead; // this shouldn't throw.
    m_pHead = pale;
  }
  bool FFind( _TyT const & _rt )
  {
    _TyAllocaListEl * pCur = m_pHead;
    for( ; pCur && !( **pCur == _rt ); pCur = pCur->m_pNext )
      ;
    return !!pCur;
  }
  void Clear()
  {
    if ( s_kfOwnObjectLifetime )
      _Clear();
    else
      m_pHead = nullptr;
  }
protected:
  void _Clear()
  {
    _TyAllocaListEl * pCur = m_pHead;
    m_pHead = nullptr; // In case one of these throws.
    for( ; pCur; pCur = pCur->m_pNext )
      pCur->~_TyAllocaListEl();
  }
  _TyAllocaListEl * m_pHead{nullptr};
};

__BIENUTIL_END_NAMESPACE

namespace std
{
__BIENUTIL_USING_NAMESPACE
  template < class t_tyT, bool t_fOwnObjectLifetime >
  void swap( AllocaList< t_tyT, t_fOwnObjectLifetime > & _rl, AllocaList< t_tyT, t_fOwnObjectLifetime > & _rr )
  {
    _rl.swap( _rr );
  }
}

#define ALLOCA_LIST_PUSH( LIST, ... ) LIST.push_emplace( alloca( sizeof( decltype(LIST)::_TyAllocaListEl ) ), ##__VA_ARGS__ )

