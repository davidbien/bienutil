#ifndef __SIMPBV_H
#define __SIMPBV_H

//          Copyright David Lawrence Bien 1997 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

#include "_bitutil.h"
#include "_allbase.h"

#ifdef setbit // WTF? Why would someone make a PP macro called setbit()?
#undef setbit
#endif 

__BIENUTIL_BEGIN_NAMESPACE

// _simpbv.h

// Simple bit-vector implementation ( currently defining only the functionality I need ).

template < class t_Ty >
__INLINE t_Ty
_bv_clear_first_set( t_Ty _t, t_Ty & _rt )
{
  return _t & ~(_rt &= _rt-1);
}

template < class t_Ty >
__INLINE t_Ty
_bv_first_set( t_Ty const & _rt )
{
  return _rt & ~(_rt & _rt-1);
}

// Return the number of the first set bit in this type and clear the bit.
template < class t_Ty >
__INLINE size_t
_bv_get_clear_first_set( t_Ty & _rt )
{
  Assert( _rt );  // We assume that there is such a bit to get.
  t_Ty tBit = _bv_clear_first_set( _rt, _rt );
  return MSBitSet( tBit );
}

template < class t_Ty >
__INLINE size_t
_bv_get_first_set( t_Ty const & _rt )
{
  Assert( _rt );  // We assume that there is such a bit to get.
  t_Ty tBit = _bv_first_set( _rt );
  return MSBitSet( tBit );
}

// Note: Should be able to use intel integer vectors as elements. Will have
//  to specialize _bv_clear_first_set() and UMSBSet().

template < class t_TyEl, class t_TyAllocator = std::allocator< t_TyEl > >
class _simple_bitvec
  : public _alloc_base< t_TyEl, t_TyAllocator >
{
private:
  typedef _simple_bitvec< t_TyEl, t_TyAllocator > _TyThis;
  typedef _alloc_base< t_TyEl, t_TyAllocator >      _TyAllocBase;
public:
  
  typedef size_t size_type;
  typedef t_TyAllocator _TyAllocator;
  typedef t_TyEl        _TyEl;

  static const size_type  ms_kstElSizeBits = CHAR_BIT * sizeof( t_TyEl );

  const size_type m_kstBits{0};
  const size_type m_kstSize{0};
  t_TyEl *        m_rgEls{nullptr};

  ~_simple_bitvec() _BIEN_NOTHROW
  {
    if ( m_rgEls )
      _TyAllocBase::deallocate_n( m_rgEls, m_kstSize );
  }
  _simple_bitvec( const t_TyAllocator & _rA = t_TyAllocator() )
    : _TyAllocBase( _rA )
  {
    AssertValid();
  }
  explicit _simple_bitvec(  size_type _stBits,
                            const t_TyAllocator & _rA = t_TyAllocator() )
    : m_kstBits( _stBits ),
      m_kstSize( !m_kstBits ? 0 : ( (_stBits-1)/ms_kstElSizeBits + 1 ) ),
      _TyAllocBase( _rA ),
      m_rgEls( nullptr )
  {
    if ( m_kstBits )
    {
      _TyAllocBase::allocate_n( m_rgEls, m_kstSize );
      memset( m_rgEls, 0, m_kstSize * sizeof( t_TyEl ) );
    }
    AssertValid();
  }
  _simple_bitvec( _TyThis const & _r )
    : m_kstBits( _r.m_kstBits ),
      m_kstSize( _r.m_kstSize ),
      _TyAllocBase( _r.get_allocator() ),
      m_rgEls( 0 )
  {
    if ( _r.m_rgEls )
    {
      _TyAllocBase::allocate_n( m_rgEls, m_kstSize );
      memcpy( m_rgEls, _r.m_rgEls, m_kstSize * sizeof( t_TyEl ) );
    }
    AssertValid();
  }
  // Only copy the allocator.
  _simple_bitvec( _TyThis const & _r, std::false_type )
    : _TyAllocBase( (_TyAllocBase const &)_r )
  {
    AssertValid();
  }
  _simple_bitvec( _TyThis && _rr )
    : _TyAllocBase( (_TyAllocBase const &)_rr ) // copy the allocator, don't move it - we want to leave _rr empty but usable.
  {
    swap( _rr );
    AssertValid();
  }
  _simple_bitvec & operator = ( _TyThis && _rr )
  {
    _TyThis acquire( std::move( _rr ) );
    swap( acquire );
    AssertValid();
    return *this;
  }

  void AssertValid() const
  {
#if ASSERTSENABLED
    // The main assertion is that this object is clear after m_kstBits.
    Assert( !m_rgEls == !m_kstBits );
    Assert( !m_rgEls == !m_kstSize );
    if ( m_kstBits )
    {
      Assert( m_kstSize == ( (m_kstBits-1ull)/ms_kstElSizeBits + 1ull ) );
      const size_type kstLastFill = m_kstBits % ms_kstElSizeBits;
      Assert( !kstLastFill || !( m_rgEls[m_kstSize-1] & ~( ( 1ull << kstLastFill ) - 1ull ) ) );
      const size_type kstLastFillBP = m_kstBits % ms_kstElSizeBits;
    }
#endif //ASSERTSENABLED
  }

  using _TyAllocBase::get_allocator;

  size_type size() const _BIEN_NOTHROW
  {
    return m_kstBits;
  }

  size_type size_bytes() const _BIEN_NOTHROW
  {
    return m_kstSize * sizeof( t_TyEl );
  }

  t_TyEl *  begin()
  {
    return m_rgEls;
  }

  // Bit vectors are used with those of the same size only: asserted. Should one of another size be assigned all the
  //  same, this becomes of that size ( before, only this one's size was copied - too little or too much ).
  _TyThis & 
  operator = ( _TyThis const & _r )
  {
    if ( this != &_r )
    {
      Assert( _r.m_kstBits == m_kstBits );
      if ( _r.m_kstBits == m_kstBits )
      {
        if ( m_kstSize )
          memcpy( m_rgEls, _r.m_rgEls, m_kstSize * sizeof( t_TyEl ) );
      }
      else
      {
        _TyThis copy( _r );
        swap( copy );
      }
    }
    AssertValid();
    return *this;
  }

  // Of the same size only - asserted; should the sizes differ all the same, the shorter is less rather than memory past
  //  the shorter read.
  bool
  operator < ( _TyThis const & _r ) const _BIEN_NOTHROW
  {
    Assert( _r.m_kstBits == m_kstBits );
    if ( _r.m_kstBits != m_kstBits )
      return m_kstBits < _r.m_kstBits;
    return !!m_kstSize && ( 0 > memcmp( m_rgEls, _r.m_rgEls, m_kstSize * sizeof( t_TyEl ) ) );
  }

  void clear() _BIEN_NOTHROW
  {
    if ( m_kstSize )
      memset( m_rgEls, 0, m_kstSize * sizeof( t_TyEl ) );
  }

  bool  empty() const _BIEN_NOTHROW
  {
    AssertValid();
    t_TyEl * pElEnd = m_rgEls + m_kstSize;
    for ( t_TyEl * pEl = m_rgEls; pEl != pElEnd; ++pEl )
    {
      if ( *pEl )
        return false;     
    }
    return true;
  }

  void  setbit( size_type _rstBit ) _BIEN_NOTHROW
  {
    Assert( _rstBit < size() );
    m_rgEls[ _rstBit / ms_kstElSizeBits ] |= 
      ( static_cast< t_TyEl >( 1 ) << ( _rstBit % ms_kstElSizeBits ) );
    AssertValid();
  }
  // Version that takes a boolean allowing setting or clearing.
  void  setbit( size_type _rstBit, bool _fSet ) _BIEN_NOTHROW
  {
    if ( _fSet )
      setbit( _rstBit );
    else
      clearbit( _rstBit );
  }

  void  clearbit( size_type _rstBit ) _BIEN_NOTHROW
  {
    Assert( _rstBit < size() );
    m_rgEls[ _rstBit / ms_kstElSizeBits ] &= 
      ~( static_cast< t_TyEl >( 1 ) << ( _rstBit % ms_kstElSizeBits ) );
    AssertValid();
  }

  bool  isbitset( size_type _rstBit ) const _BIEN_NOTHROW
  {
    AssertValid();
    Assert( _rstBit < size() );
    return m_rgEls[ _rstBit / ms_kstElSizeBits ] &
      ( static_cast< t_TyEl >( 1 ) << ( _rstBit % ms_kstElSizeBits ) );
  }

  // Clear the first set least significant bit - if none found then return
  //  with ( _rstFound == m_kstBits ) - otherwise return index of bit.
  size_type getclearfirstset( ) _BIEN_NOTHROW
  {
    AssertValid();
    return _getclearfirstset( m_rgEls );
  }

  // Clear the first set bit after _stLast and return its index - m_kstBits if there is none. ( Before, the bits of
  //  _stLast's element at and before it were looked at too. )
  size_type getclearfirstset( size_type _stLast )
  {
    AssertValid();
    if ( ++_stLast >= m_kstBits )
      return m_kstBits;
    t_TyEl * pElNext = m_rgEls + ( _stLast / ms_kstElSizeBits );
    if ( const size_type kstBit = _stLast % ms_kstElSizeBits )
    {
      const t_TyEl kelMask = t_TyEl( ~( ( t_TyEl( 1 ) << kstBit ) - 1 ) );
      if ( t_TyEl el = t_TyEl( *pElNext & kelMask ) )
      {
        const size_type kstFound = size_type( _bv_get_first_set( el ) );
        *pElNext &= t_TyEl( ~( t_TyEl( 1 ) << kstFound ) );
        return size_type( pElNext - m_rgEls ) * ms_kstElSizeBits + kstFound;
      }
      ++pElNext;
    }
    return _getclearfirstset( pElNext );
  }

  // Obtain the index first least significant bit set.
  size_type getfirstset( ) const _BIEN_NOTHROW
  {
    AssertValid();
    return _getset( m_rgEls );
  }

  size_type getnextset( size_type _stLast ) const _BIEN_NOTHROW
  {
    AssertValid();
    Assert( _stLast <= size() );
    if ( _stLast >= size() )
      return size();
    // First process any partial elements:
    t_TyEl * pElNext = m_rgEls + ( ++_stLast / ms_kstElSizeBits );
    if ( _stLast %= ms_kstElSizeBits )
    {
      // We know that beyond the end is empty ( invariant ):
      t_TyEl  el;
      if ( !!( el = ( *pElNext & ~( ( size_type(1) << _stLast ) - 1 ) ) ) )
      {
        _stLast = size_type(_bv_get_first_set( el )) + size_type( pElNext - m_rgEls ) * ms_kstElSizeBits;
        Assert( _stLast < size() );
        return _stLast;
      }
      ++pElNext;
    }

    return _getset( pElNext );
  }
  // Return the next unset bit after the given bit.
  size_type getnextnotset( size_type _stLast ) const _BIEN_NOTHROW
  {
    AssertValid();
    Assert( _stLast <= size() );
    if ( _stLast >= size() )
      return size();
    // First process any partial elements:
    t_TyEl * pElNext = m_rgEls + ( ++_stLast / ms_kstElSizeBits );
    if ( _stLast %= ms_kstElSizeBits )
    {
      // We know that beyond the end is empty ( invariant ):
      t_TyEl  el;
      if ( !!( el = t_TyEl( t_TyEl( ~*pElNext ) & t_TyEl( ~( ( size_type(1) << _stLast ) - 1 ) ) ) ) )
      {
        _stLast = size_type(_bv_get_first_set( el )) + size_type( pElNext - m_rgEls ) * ms_kstElSizeBits;
        Assert( _stLast <= size() ); // may be size() and that is by design.
        return _stLast;
      }
      ++pElNext;
    }
    return _getnotset( pElNext );
  }

  size_type countsetbits() const _BIEN_NOTHROW
  {
    AssertValid();
    size_type stSet = 0;
    t_TyEl * pElEnd = m_rgEls + m_kstSize;
    for ( t_TyEl * pEl = m_rgEls; pEl != pElEnd; ++pEl )
    {
      if ( *pEl )
      {
        t_TyEl el = *pEl;
        do
        {
          ++stSet;
        }
        while( el &= (el-1) );
      }
    }
    return stSet;
  }

  // Of the same size only - asserted; should the sizes differ all the same, they are unequal rather than memory past
  //  the shorter read.
  bool  operator == ( _TyThis const & _r ) const _BIEN_NOTHROW
  {
    Assert( _r.m_kstBits == m_kstBits );
    return ( _r.m_kstBits == m_kstBits ) && ( !m_kstSize || !memcmp( _r.m_rgEls, m_rgEls, m_kstSize * sizeof( t_TyEl ) ) );
  }
  bool  operator != ( _TyThis const & _r ) const _BIEN_NOTHROW
  {
    return !( *this == _r );
  }

  _TyThis & operator |= ( _TyThis const & _r ) _BIEN_NOTHROW
  {
    // The invariant is that beyond the last bit is empty.
    // Since both bit vectors should follow that invariant the result should follow it.
    Assert( _r.m_kstBits == m_kstBits );
    or_equals( _r.m_rgEls );
    AssertValid();
    return *this;
  }
  _TyThis operator | ( _TyThis const & _r ) const
  {
    _TyThis ret( *this );
    ret |= _r;
    return ret;
  }
  void  or_equals( t_TyEl * pcurThat )
  {
    t_TyEl *  pendThis = m_rgEls + m_kstSize;
    t_TyEl *  pcurThis = m_rgEls;
    for ( ; pcurThis != pendThis; ++pcurThat, ++pcurThis )
    {
      *pcurThis |= *pcurThat;
    }
    AssertValid();
  }
  _TyThis & operator &= ( _TyThis const & _r ) _BIEN_NOTHROW
  {
    Assert( _r.m_kstBits == m_kstBits );
    and_equals( _r.m_rgEls );
    return *this;
  }
  _TyThis operator & ( _TyThis const & _r ) const
  {
    _TyThis ret( *this );
    ret &= _r;
    return ret;
  }
  void  and_equals( t_TyEl * pcurThat )
  {
    t_TyEl *  pendThis = m_rgEls + m_kstSize;
    t_TyEl *  pcurThis = m_rgEls;
    for ( ; pcurThis != pendThis; ++pcurThat, ++pcurThis )
    {
      *pcurThis &= *pcurThat;
    }
    AssertValid();
  }
  _TyThis & and_not_equals( _TyThis const & _r )
  {
    AssertValid();
    // The invariant is that beyond the last bit is empty.
    // Since both bit vectors should follow that invariant the result should follow it.
    Assert( _r.m_kstBits == m_kstBits );
    _and_not_equals( _r.m_rgEls );
    return *this;
  }
  void _and_not_equals( t_TyEl * pcurThat )
  {
    t_TyEl *  pendThis = m_rgEls + m_kstSize;
    t_TyEl *  pcurThis = m_rgEls;
    for ( ; pcurThis != pendThis; ++pcurThat, ++pcurThis )
    {
      *pcurThis &= ~*pcurThat;
    }
    AssertValid();
  }

  bool  FIntersects( _TyThis const & _r ) const _BIEN_NOTHROW
  {
    AssertValid();
    Assert( _r.m_kstBits == m_kstBits );
    t_TyEl *  pendThis = m_rgEls + m_kstSize;
    t_TyEl *  pcurThat = _r.m_rgEls;
    t_TyEl * pcurThis = m_rgEls;
    for ( ; pcurThis != pendThis; ++pcurThat, ++pcurThis )
    {
      if ( *pcurThis & *pcurThat )
      {
        return true;
      }
    }
    return false;
  }

  // Set this to the intersection of the two passed sets - 
  //  return if intersection is non-NULL.
  bool  intersection( _TyThis const & _rl, _TyThis const & _rr )
  {
    Assert( _rl.m_kstBits == m_kstBits );
    Assert( _rr.m_kstBits == m_kstBits );
    t_TyEl *  pendThis = m_rgEls + m_kstSize;
    t_TyEl *  pcurLeft = _rl.m_rgEls;
    t_TyEl *  pcurRight = _rr.m_rgEls;
    t_TyEl * pcurThis = m_rgEls;
    bool fAnyIntersection = false;
    for ( ; pcurThis != pendThis; ++pcurLeft, ++pcurRight, ++pcurThis )
    {
      if ( !!( *pcurThis = ( *pcurLeft & *pcurRight ) ) )
        fAnyIntersection = true;
    }
    AssertValid();
    return fAnyIntersection;
  }

  size_type FirstIntersection( _TyThis const & _r ) const _BIEN_NOTHROW
  {
    AssertValid();
    Assert( _r.m_kstBits == m_kstBits );
    return _NIntersection( m_rgEls, _r.m_rgEls );
  }

  size_type NextIntersection( _TyThis const & _r, size_type _stLast ) const _BIEN_NOTHROW
  {
    AssertValid();
    Assert( _stLast < size() );
    if ( _stLast >= size() )
      return size();
    // First process any partial elements:
    t_TyEl * pElNextThis = m_rgEls + ( ++_stLast / ms_kstElSizeBits );
    t_TyEl * pElNextThat = _r.m_rgEls + ( pElNextThis - m_rgEls );
    if ( _stLast %= ms_kstElSizeBits )
    {
      // We know that beyond the end is empty ( invariant ):
      t_TyEl  el;
      if ( !!( el = ( *pElNextThis & *pElNextThat & ~( ( size_type(1) << _stLast ) - 1 ) ) ) )
      {
        _stLast = size_type(_bv_get_first_set( el )) + size_type( pElNextThis - m_rgEls ) * ms_kstElSizeBits;
        Assert( _stLast < size() );
        return _stLast;
      }
      ++pElNextThis;
      ++pElNextThat;
    }

    return _NIntersection( pElNextThis, pElNextThat );
  }

  void  invert() _BIEN_NOTHROW
  {
    AssertValid();
    // We can invert all but the last element wholly - need to
    //  maintain null bits above bit limit on last element:
    if ( m_kstSize )
    {
      t_TyEl * pElEnd = m_rgEls + m_kstSize;
			t_TyEl * pEl;
      for ( pEl = m_rgEls; pEl != pElEnd; ++pEl )
        *pEl = ~*pEl;
      const size_t kstLastFill = m_kstBits % ms_kstElSizeBits;
      if ( !!kstLastFill ) // update the last element?
        pEl[-1] &= ( ( 1ull << kstLastFill ) - 1ull );
    }
    AssertValid();
  }

  // This will increment the entire bitvector as if it were a number.
  void increment()
  {
    AssertValid();
    if ( m_kstSize )
    {
      t_TyEl * pElEnd = m_rgEls + m_kstSize;
			t_TyEl * pEl;
      for ( pEl = m_rgEls; pEl != pElEnd; ++pEl )
      {
        if ( !!++*pEl )
          break;
      }
      const size_t kstLastFill = m_kstBits % ms_kstElSizeBits;
      if ( ( pEl == pElEnd-1 ) && !!kstLastFill ) // update the last element?
        *pEl &= ( ( 1ull << kstLastFill ) - 1ull );
    }    
    AssertValid();
  }
  // This will decrement the entire bitvector as if it were a number.
  void decrement()
  {
    AssertValid();
    if ( m_kstSize )
    {
      t_TyEl * pElEnd = m_rgEls + m_kstSize;
			t_TyEl * pEl;
      for ( pEl = m_rgEls; pEl != pElEnd; ++pEl )
      {
        if ( !!(*pEl)-- )
          break;
      }
      // A borrow through every element - of zero - sets every bit, past the last too: those are kept clear.
      const size_t kstLastFill = m_kstBits % ms_kstElSizeBits;
      if ( !!kstLastFill )
        pElEnd[-1] &= ( ( 1ull << kstLastFill ) - 1ull );
    }    
    AssertValid();
  }

  size_t  hash() const _BIEN_NOTHROW
  {
    AssertValid();
    // We add all the elements together ( not worrying about overflow ):
    size_t  st = 0;
    t_TyEl * pElEnd = m_rgEls + m_kstSize;
    for ( t_TyEl * pEl = m_rgEls; pEl != pElEnd; ++pEl )
    {
      st += *pEl;
    }
    return st;
  }

  void  swap( _TyThis & _r ) _BIEN_NOTHROW
  {
    __STD::swap(  const_cast< size_type& >( m_kstBits ),
                  const_cast< size_type& >( _r.m_kstBits ) );
    __STD::swap(  const_cast< size_type& >( m_kstSize ),
                  const_cast< size_type& >( _r.m_kstSize ) );
    __STD::swap( m_rgEls, _r.m_rgEls );
    AssertValid();
  }

  // A dangerous method - swaps in any memory.
  // Make sure to worry about throw-safety when using this method.
  void  swap_vector( t_TyEl *& _rrgEls )
  {
    AssertValid();
    __STD::swap( m_rgEls, _rrgEls );
    AssertValid();
  }

protected:

  size_type _getclearfirstset( t_TyEl * pEl )
  {
    t_TyEl * pElEnd = m_rgEls + m_kstSize;
    for ( ; pEl != pElEnd; ++pEl )
    {
      if ( *pEl )
      {
        AssertStatement( t_TyEl dbg_elBefore = *pEl );
        size_type nBitEl = (size_type)_bv_get_clear_first_set( *pEl );
        Assert( dbg_elBefore & ( t_TyEl( 1 ) << nBitEl ) );
        Assert( !( *pEl & ( t_TyEl( 1 ) << nBitEl ) ) );
        size_type stFound = size_type(pEl - m_rgEls) * ms_kstElSizeBits + nBitEl;
        Assert( stFound < m_kstBits );
        return stFound;
      }
    }
    return m_kstBits;
  }

  size_type _getset( t_TyEl * pEl ) const _BIEN_NOTHROW
  {
    t_TyEl * pElEnd = m_rgEls + m_kstSize;
    for ( ; pEl != pElEnd; ++pEl )
    {
      if ( *pEl )
      {
        size_type stFound = size_type(pEl - m_rgEls) * ms_kstElSizeBits + size_type(_bv_get_first_set( *pEl ));
        Assert( stFound < m_kstBits );
        return stFound;
      }
    }
    return m_kstBits;
  }
  // Return first unset bit starting at this element's container.
  size_type _getnotset( t_TyEl * pEl ) const _BIEN_NOTHROW
  {
    t_TyEl * pElEnd = m_rgEls + m_kstSize;
    for ( ; pEl != pElEnd; ++pEl )
    {
      t_TyEl inverted = ~*pEl;
      if ( inverted )
      {
        size_type stFound = size_type(pEl - m_rgEls) * ms_kstElSizeBits + size_type(_bv_get_first_set( inverted ));
        Assert( stFound <= m_kstBits );
        return stFound;
      }
    }
    return m_kstBits;
  }

  size_type _NIntersection( t_TyEl * pcurThis, t_TyEl * pcurThat ) const _BIEN_NOTHROW
  {
    t_TyEl *  pendThis = m_rgEls + m_kstSize;
    for ( ; pcurThis != pendThis; ++pcurThat, ++pcurThis )
    {
      t_TyEl  el;
      if ( !!( el = ( *pcurThis & *pcurThat ) ) )
      {
        return size_type(pcurThis - m_rgEls) * ms_kstElSizeBits + size_type(_bv_get_first_set( el ));
      }
    }
    return m_kstBits;
  }
};

__BIENUTIL_END_NAMESPACE

namespace std {
__BIENUTIL_USING_NAMESPACE
  template < class t_TyEl, class t_TyAllocator >
  struct hash< _simple_bitvec< t_TyEl, t_TyAllocator > >
  {
    typedef _simple_bitvec< t_TyEl, t_TyAllocator > _TyBitVec;
    size_t operator()(const _TyBitVec & _r) const
    {
      return _r.hash();
    }
  };
  template < class t_TyEl, class t_TyAllocator >
  void  swap( _simple_bitvec< t_TyEl, t_TyAllocator > & _rl, _simple_bitvec< t_TyEl, t_TyAllocator > & _rr ) _BIEN_NOTHROW
  {
    _rl.swap( _rr );
  }
} // end namespace std

#endif //__SIMPBV_H
