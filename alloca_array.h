#pragma once

//          Copyright David Lawrence Bien 1997 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// alloca_array.h
// Simple typed stack allocated array with optional element lifetime ownership.
// dbien
// 20JAN2021

#include "bienutil.h"

__BIENUTIL_BEGIN_NAMESPACE

template < class t_TyEl, bool t_fOwnObjectLifetime >
class AllocaArray
{
  typedef AllocaArray _TyThis;
public:
  typedef t_TyEl _TyEl;
  static constexpr bool s_kfOwnObjectLifetime = t_fOwnObjectLifetime;

  ~AllocaArray()
  {
    AssertValid();
    if ( s_kfOwnObjectLifetime )
      _Clear();
  }
  AllocaArray() = delete;
  AllocaArray(size_t _nEls, _TyEl * _pEls)
    : m_nElsPool( _nEls ),
      m_rgEls( _pEls ),
      m_pElEnd( _pEls )
  {
  }
  AllocaArray( AllocaArray const & ) = delete;
  AllocaArray & operator=( AllocaArray const & ) = delete;
  AllocaArray( AllocaArray && _rr )
  {
    _rr.AssertValid();
    swap( _rr );
  }
  AllocaArray & operator=( AllocaArray && _rr )
  {
    _rr.AssertValid();
    AllocaArray acquire( std::move( _rr ) );
    swap( acquire );
  }
  void swap( _TyThis & _r )
  {
    AssertValid();
    _r.AssertValid();
    std::swap( m_rgEls, _r.m_rgEls );
    std::swap( m_nElsPool, _r.m_nElsPool );
  }
  template < class ... t_TysArgs >
  _TyEl & emplaceAtEnd( t_TysArgs ... _args )
  {
    AssertValid();
    Assert( m_pElEnd < m_rgEls + m_nElsPool );
    new( m_pElEnd ) _TyEl( std::forward< t_TysArgs >( _args ) ... );
    ++m_pElEnd;
  }
  size_t GetSize() const
  {
    return m_pElEnd - m_rgEls;
  }
  size_t GetAllocSize() const
  {
    return m_nElsPool;
  }
  _TyEl * begin()
  {
    return m_rgEls;
  }
  const _TyEl * begin() const
  {
    return m_rgEls;
  }
  _TyEl * end()
  {
    return m_pElEnd;
  }
  const _TyEl * end() const
  {
    return m_pElEnd;
  }
  _TyEl const & ElGet( size_t _n ) const
  {
    Assert( _n < GetSize() );
    AssertValid();
    return m_rgEls[ _n ];
  }
  _TyEl & ElGet( size_t _n )
  {
    Assert( _n < GetSize() );
    AssertValid();
    return m_rgEls[ _n ];
  }
  _TyEl const & operator[]( size_t _n ) const
  {
    return ElGet( _n );
  }
  _TyEl & operator[]( size_t _n )
  {
    return ElGet( _n );
  }
  template < class t_TyFunctor >
  void Apply( size_t _nFrom, size_t _nTo, t_TyFunctor && _rrf ) const
  {
    AssertValid();
    Assert( _nFrom <= _nTo );
    if ( _nTo == _nFrom )
      return;
    const _TyEl * pelCur = &ElGet( _nFrom );
    const _TyEl * const pelEnd = pelCur + ( _nTo - _nFrom );
    for ( ; pelEnd > pelCur; ++pelCur )
      std::forward< t_TyFunctor >( _rrf )( *pelCur );
  }
  template < class t_TyFunctor >
  void Apply( size_t _nFrom, size_t _nTo, t_TyFunctor && _rrf )
  {
    AssertValid();
    Assert( _nFrom <= _nTo );
    if ( _nTo == _nFrom )
      return;
    _TyEl * pelCur = &ElGet( _nFrom );
    _TyEl * const pelEnd = pelCur + ( _nTo - _nFrom );
    for ( ; pelEnd > pelCur; ++pelCur )
      std::forward< t_TyFunctor >( _rrf )( *pelCur );
  }
void AssertValid()
{
#if ASSERTSENABLED
  Assert( m_pElEnd >= m_rgEls );
  Assert( m_pElEnd <= m_rgEls + m_nElsPool );
#endif //ASSERTSENABLED
}
protected:
  void _Clear()
  {
    if ( !s_kfOwnObjectLifetime )
      return;
    _TyEl * pelBegin = begin();
    _TyEl * pelCur = end();
    m_pElEnd = m_rgEls;
    for ( ; pelBegin != pelCur; )
      (--pelCur)->~_TyEl();
  }
  _TyEl * m_rgEls{nullptr};
  _TyEl * m_pElEnd{nullptr};
  size_t m_nElsPool{0};
};

#define ALLOCA_ARRAY_ALLOC( TYPE, N ) (TYPE*)alloca( (N) * sizeof(TYPE) )

__BIENUTIL_END_NAMESPACE
