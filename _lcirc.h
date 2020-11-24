#pragma once 

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _lcirc.h
// Circular list.
// dbien 24JUM2020

// This impl will feature a "handle" (more or less) that will point to one member of the circular list.
// That handle might be null - i.e. there is no list.
// The object containing the handle is not part of the list itself.

#include <memory>
#include "bienutil.h"
#include "_allbase.h"

__BIENUTIL_BEGIN_NAMESPACE

// CircularElTraits: Allow implementors to override functionality.
template < class t_tyEl >
struct CircularElTraits;
template < class t_tyEl >
class CircularList;
template < class t_tyEl, class t_tyCircularListContainer >
class CircularListEl;

// Declare the non-specialized version:
template < class t_tyEl >
struct CircularElTraits
{
  typedef allocator< char > _tyAllocator;
  typedef CircularList< t_tyEl > _tyCircularListContainer;
  typedef CircularListEl< t_tyEl, _tyCircularListContainer > _tyCircularListEl;
};

// Example of how to override the container for CircularList< Foo >:
// struct Foo;
// template <>
// struct CircularElTraits< Foo >
// {
//   typedef BiensAwesomeAllocator< char > _tyAlloc;
//   typedef BiensOverriddenCircularList< BiensListEl, _tyAlloc > _tyCircularListContainer;
//   typedef CircularListEl< BiensListEl, _tyCircularListContainer > _tyCircularListEl;
// };

// CircularListEl:
// We have an allocator because you can create one by merely having one element.
template < class t_tyEl, class t_tyCircularListContainer >
class CircularListEl
{
  typedef CircularListEl _tyThis;
public:
  typedef t_tyEl _tyEl;
  typedef t_tyCircularListContainer _tyCircularListContainer;

  CircularListEl() = default;
  CircularListEl( _tyCircularListContainer & _rl
  CircularListEl( CircularListEl const & _r )
   : m_el( _r.m_el )
  {   
  }
  CircularListEl( _tyEl const & _rEl, _tyThis & _rInsertBefore )
    : m_el( _rEl ),
      m_pNext( &_rInsertBefore ),
      m_pPrev( _rInsertBefore.m_pPrev )
  {
    _rInsertBefore.m_pPrev->m_pNext = this;
    _rInsertBefore.m_pPrev = this;
  }
  CircularListEl( _tyThis & _rInsertAfter, _tyEl const & _rEl )
    : m_el( _rEl )
      m_pNext( _rInsertAfter.m_pNext ),
      m_pPrev( &_rInsertAfter )
  {
    _rInsertAfter.m_pNext->m_pPrev = this;
    _rInsertAfter.m_pNext = this;
  }
  CircularListEl & operator = ( CircularListEl const & _r )
  {
    m_el = _r.m_el;
  }
  // The default move operators don't copy the m_pNext and m_pPrev pointers as that is rarely desireable.
  CircularListEl( CircularListEl && _rr )
    : m_el( std::move( _rr.m_el ) )
  {
  }
  CircularListEl & operator = ( CircularListEl && _rr )
  {
    m_el = std::move( _rr.m_el );
  }
  // Use MoveAll if you want to move all the members and leave _rr in an initialized state.
  void MoveAll( CircularListEl && _rr )
  {
    m_el = std::move( _rr.m_el );
    m_pNext = _rr.m_pNext;
    _rr.m_pNext = &_rr;
    m_pPrev = _rr.m_pPrev;
    _rr.m_pPrev = &_rr;
  }
  CircularListEl( _tyEl && _rrEl )
    : m_el( std::move( _rrEl ) )
  {
  }
  CircularListEl( _tyEl && _rrEl, _tyThis & _rInsertBefore )
    : m_el( std::move( _rrEl ) ),
      m_pNext( &_rInsertBefore ),
      m_pPrev( _rInsertBefore.m_pPrev )
  {
    _rInsertBefore.m_pPrev->m_pNext = this;
    _rInsertBefore.m_pPrev = this;
  }
  CircularListEl( _tyThis & _rInsertAfter, _tyEl && _rrEl )
    : m_el( std::move( _rrEl ) )
      m_pNext( _rInsertAfter.m_pNext ),
      m_pPrev( &_rInsertAfter )
  {
    _rInsertAfter.m_pNext->m_pPrev = this;
    _rInsertAfter.m_pNext = this;
  }
  _tyEl & REl()
  {
    return m_el;
  }
  const _tyEl & REl() const
  {
    return m_el;
  }

  const CircularListEl * PNext() const
  {
    return m_pNext;
  }
  CircularListEl * PNext()
  {
    return m_pNext;
  }
  const CircularListEl * PPrev() const
  {
    return m_pPrev;
  }
  CircularListEl * PPrev()
  {
    return m_pPrev;
  }

  // Remove this element from the list _rcl. The list is left pointing at the next element or nullptr if no elements left.
  void Remove()
  {
    if ( m_plContainer.IsHead( this ) )
      m_plContainer->MoveFromHead(); // We can let the container decide how it likes to move away from the head of the list.
    if ( this != m_pNext )
    {
      // Unlink from next and previous. Leave our pointers intact, however.
      m_pNext->m_pPrev = m_pPrev;
      m_pPrev->m_pNext = m_pNext;
    }
  }
  void Remove( _tyEl & _relMoveInto )
  {
    Remove();
    _relMoveInto = std::move( m_el );
  }
  void InsertBefore( _tyThis & _rInsertBefore )
  {
    m_pNext = &_rInsertBefore;
    m_pPrev = _rInsertBefore.m_pPrev;
    _rInsertBefore.m_pPrev->m_pNext = this;
    _rInsertBefore.m_pPrev = this;
  }
  void InsertAfter( _tyThis & _rInsertAfter )
  {
    m_pNext = _rInsertAfter.m_pNext;
    m_pPrev = &_rInsertAfter;
    _rInsertAfter.m_pNext->m_pPrev = this;
    _rInsertAfter.m_pNext = this;
  }

protected:
  _tyCircularListContainer * m_plContainer{nullptr}; // Pointer to our container - we don't maintain a hard reference to it.
  _tyThis * m_pNext{this};
  _tyThis * m_pPrev{this};
  t_tyEl m_el;
};

// CircularList:
template < class t_tyEl >
class CircularList : public _alloc_base<  typename CircularElTraits< t_tyEl >::_tyCircularListEl,
                                          typename CircularElTraits< t_tyEl >::_tyAllocator >
{
  typedef CircularList _tyThis;
  typedef _alloc_base<  typename CircularElTraits< t_tyEl >::_tyCircularListEl,
                        typename CircularElTraits< t_tyEl >::_tyAllocator > _tyBase;
public:
  typedef CircularElTraits< t_tyEl >::_tyCircularListEl _tyCircularListEl;
  typedef CircularElTraits< t_tyEl >::_tyAllocator _tyAllocator;

  CircularList() = default;
  CircularList( CircularList const & _r )
    : _tyBase( _r )
  {
    Copy( _r );
  }
  ~CircularList()
  {
    if ( m_pcleHead )
      _Clear( m_pcleHead );
  }
  void Clear()
  {
    if ( m_pcleHead )
    {
      _tyCircularListEl * p = m_pcleHead;
      m_pcleHead = 0;
      _Clear( p );
    }
  }

  const _tyCircularListEl * PGetHead() const
  {
    return m_pcleHead;
  }
  _tyCircularListEl * PGetHead()
  {
    return m_pcleHead;
  }

  void Copy( _tyThis const & _r )
  {
    Clear();
    const _tyCircularListEl * pHeadThat = _r.PHead();
    if ( !pHeadThat )
      return;
    // We will clean up the copied list on throw.
    _tyCircularListEl * pHeadCopy = _tyBase::template PCreate< _tyCircularListEl const & >( *pHeadThat );
    _BIEN_TRY
    {
      _tyCircularListEl * pCurCopy = pHeadCopy;
      for ( const _tyCircularListEl * pCurThat = pHeadThat->PNext(); pCurThat; pCurThat = pCurThat->PNext() )
      {
        _tyCircularListEl * pNew = _tyBase::template PCreate< _tyCircularListEl &, t_tyEl const & >( *pCurCopy, pCurThat->GetEl() );
        pCurCopy = pNew; // yes, could have assigne above.
      }
    }
    _BIEN_UNWIND( _Clear( pCopyHead ) );
    // Now the try...catch doesn't own pHeadCopy anymore - we do:
    m_pcleHead = pCopyHead;
  }

protected:
  void _Clear( _tyCircularListEl * _p )
  {
    Assert( !!_p );
    _tyCircularListEl * pCur = _p;
    _tyCircularListEl * pNext;
    do
    {
      // We don't bother unlinking the elements as we are destroying the entire list:
      pNext = pCur->PNext();
      _tyBase::DestroyP( pCur );
    }
    while( _p != ( pCur = pNext ) );
  }
  
  _tyCircularListEl * m_pcleHead{nullptr};
  _tyAllocEl m_allocEl; // allocator for elements of the list.
};

__BIENUTIL_END_NAMESPACE
