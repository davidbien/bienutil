#ifndef __SIMPBV_H
#define __SIMPBV_H

#include "_bitutil.h"

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
  assert( _rt );  // We assume that there is such a bit to get.
  t_Ty tBit = _bv_clear_first_set( _rt, _rt );
  return UMSBitSet( tBit );
}

template < class t_Ty >
__INLINE size_t
_bv_get_first_set( t_Ty const & _rt )
{
  assert( _rt );  // We assume that there is such a bit to get.
  t_Ty tBit = _bv_first_set( _rt );
  return UMSBitSet( tBit );
}

#ifdef IVEC_H_INCLUDED  // If we have the intel ivec stuff included.

// Define some specializations:
_STLP_TEMPLATE_NULL
__INLINE size_t
_bv_get_clear_first_set( Iu64vec1 & _rt )
{
  assert( !!_rt );  // We assume that there is such a bit to get.

  // We don't 64 bit subtraction in the mmx stuff - we need to simulate it
  //  using 32 bit - only need to subtract 1 from the high dword if we
  //  have zero in the low dword.

  #error incomplete

}

#endif //IVEC_H_INCLUDED


// Note: Should be able to use intel integer vectors as elements. Will have
//  to specialize _bv_clear_first_set() and UMSBSet().

template < class t_TyEl, class t_TyAllocator >
class _simple_bitvec
  : public _alloc_base< t_TyEl, t_TyAllocator >
{
private:
  typedef _simple_bitvec< t_TyEl, t_TyAllocator > _TyThis;
  typedef _alloc_base< t_TyEl, t_TyAllocator >      _TyAllocBase;
public:
  
  typedef t_TyEl size_type;
  typedef t_TyAllocator _TyAllocator;
  typedef t_TyEl        _TyEl;

  static const int  ms_kiElSizeBits = CHAR_BIT * sizeof( t_TyEl );

  const size_type m_kstBits;
  const size_type m_kstSize;
  t_TyEl *        m_rgEls;

  explicit _simple_bitvec(  size_type _stBits,
                            const t_TyAllocator & _rA = t_TyAllocator() )
    : m_kstBits( _stBits ),
      m_kstSize( (_stBits-1)/ms_kiElSizeBits + 1 ),
      _TyAllocBase( _rA ),
      m_rgEls( 0 )
  {
    if ( m_kstBits )
    {
      _TyAllocBase::allocate_n( m_rgEls, m_kstSize );
    }
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
  }

  // Only copy the allocator.
  _simple_bitvec( _TyThis const & _r, std::false_type )
    : m_kstBits( 0 ),
      m_kstSize( 0 ),
      _TyAllocBase( (_TyAllocBase const &)_r ),
      m_rgEls( 0 )
  {
  }

  ~_simple_bitvec() _BIEN_NOTHROW
  {
    if ( m_rgEls )
    {
      _TyAllocBase::deallocate_n( m_rgEls, m_kstSize );
    }
  }

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

  _TyThis & 
  operator = ( _TyThis const & _r ) _BIEN_NOTHROW
  {
    if ( this != &_r )
    {
      assert( _r.m_kstBits == m_kstBits );
      memcpy( m_rgEls, _r.m_rgEls, m_kstSize * sizeof( t_TyEl ) );
    }
    return *this;
  }

  bool
  operator < ( _TyThis const & _r ) const _BIEN_NOTHROW
  {
    assert( _r.m_kstBits == m_kstBits );
    return 0 > memcmp( m_rgEls, _r.m_rgEls, m_kstSize * sizeof( t_TyEl ) );
  }

  void clear() _BIEN_NOTHROW
  {
    memset( m_rgEls, 0, m_kstSize * sizeof( t_TyEl ) );
  }

  bool  empty() const _BIEN_NOTHROW
  {
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
    assert( _rstBit < size() );
    m_rgEls[ _rstBit / ms_kiElSizeBits ] |= 
      ( static_cast< t_TyEl >( 1 ) << ( _rstBit % ms_kiElSizeBits ) );
  }

  void  clearbit( size_type _rstBit ) _BIEN_NOTHROW
  {
    assert( _rstBit < size() );
    m_rgEls[ _rstBit / ms_kiElSizeBits ] &= 
      ~( static_cast< t_TyEl >( 1 ) << ( _rstBit % ms_kiElSizeBits ) );
  }

  bool  isbitset( size_type _rstBit ) const _BIEN_NOTHROW
  {
    assert( _rstBit < size() );
    return m_rgEls[ _rstBit / ms_kiElSizeBits ] &
      ( static_cast< t_TyEl >( 1 ) << ( _rstBit % ms_kiElSizeBits ) );
  }

  // Clear the first set least significant bit - if none found then return
  //  with ( _rstFound == m_kstBits ) - otherwise return index of bit.
  size_type getclearfirstset( ) _BIEN_NOTHROW
  {
    return _getclearfirstset( m_rgEls );
  }

  size_type getclearfirstset( size_type _stLast )
  {
    return _getclearfirstset( m_rgEls + ( (_stLast+1) / ms_kiElSizeBits ) );
  }

  // Obtain the index first least significant bit set.
  size_type getfirstset( ) const _BIEN_NOTHROW
  {
    return _getset( m_rgEls );
  }

  size_type getnextset( size_type _stLast ) const _BIEN_NOTHROW
  {
    assert( _stLast < size() );
    // First process any partial elements:
    t_TyEl * pElNext = m_rgEls + ( ++_stLast / ms_kiElSizeBits );
    if ( _stLast %= ms_kiElSizeBits )
    {
      // We know that beyond the end is empty ( invariant ):
      t_TyEl  el;
      if ( !!( el = ( *pElNext & ~( ( 1 << _stLast ) - 1 ) ) ) )
      {
        _stLast = size_type(_bv_get_first_set( el )) + size_type( pElNext - m_rgEls ) * ms_kiElSizeBits;
        assert( _stLast < size() );
        return _stLast;
      }
      ++pElNext;
    }

    return _getset( pElNext );
  }

  size_type countsetbits() const _BIEN_NOTHROW
  {
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

  bool  operator == ( _TyThis const & _r ) const _BIEN_NOTHROW
  {
    return !memcmp( _r.m_rgEls, m_rgEls, m_kstSize * sizeof( t_TyEl ) );
  }

  _TyThis & operator |= ( _TyThis const & _r ) _BIEN_NOTHROW
  {
    // The invariant is that beyond the last bit is empty.
    // Since both bit vectors should follow that invariant the result should follow it.
    assert( _r.m_kstBits == m_kstBits );
    _or_equals( _r.m_rgEls );
    return *this;
  }
  void  _or_equals( t_TyEl * pcurThat )
  {
    t_TyEl *  pendThis = m_rgEls + m_kstSize;
    t_TyEl *  pcurThis = m_rgEls;
    for ( ; pcurThis != pendThis; ++pcurThat, ++pcurThis )
    {
      *pcurThis |= *pcurThat;
    }
  }

  void and_not( _TyThis const & _r )
  {
    // The invariant is that beyond the last bit is empty.
    // Since both bit vectors should follow that invariant the result should follow it.
    assert( _r.m_kstBits == m_kstBits );
    _and_not( _r.m_rgEls );
    return *this;
  }
  void _and_not( t_TyEl * pcurThat )
  {
    t_TyEl *  pendThis = m_rgEls + m_kstSize;
    t_TyEl *  pcurThis = m_rgEls;
    for ( ; pcurThis != pendThis; ++pcurThat, ++pcurThis )
    {
      *pcurThis &= ~*pcurThat;
    }
  }

  bool  FIntersects( _TyThis const & _r ) const _BIEN_NOTHROW
  {
    assert( _r.m_kstBits == m_kstBits );
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
    assert( _rl.m_kstBits == m_kstBits );
    assert( _rr.m_kstBits == m_kstBits );
    t_TyEl *  pendThis = m_rgEls + m_kstSize;
    t_TyEl *  pcurLeft = _rl.m_rgEls;
    t_TyEl *  pcurRight = _rr.m_rgEls;
    t_TyEl * pcurThis = m_rgEls;
    for ( ; pcurThis != pendThis; ++pcurLeft, ++pcurRight, ++pcurThis )
    {
      if ( *pcurThis = *pcurLeft & *pcurRight )
        return true;
    }
    return false;
  }

  size_type FirstIntersection( _TyThis const & _r ) const _BIEN_NOTHROW
  {
    assert( _r.m_kstBits == m_kstBits );
    return _NIntersection( m_rgEls, _r.m_rgEls );
  }

  size_type NextIntersection( _TyThis const & _r, size_type _stLast ) const _BIEN_NOTHROW
  {
    assert( _stLast < size() );
    // First process any partial elements:
    t_TyEl * pElNextThis = m_rgEls + ( ++_stLast / ms_kiElSizeBits );
    t_TyEl * pElNextThat = _r.m_rgEls + ( pElNextThis - m_rgEls );
    if ( _stLast %= ms_kiElSizeBits )
    {
      // We know that beyond the end is empty ( invariant ):
      t_TyEl  el;
      if ( !!( el = ( *pElNextThis & *pElNextThat & ~( ( 1 << _stLast ) - 1 ) ) ) )
      {
        _stLast = size_type(_bv_get_first_set( el )) + size_type( pElNextThis - m_rgEls ) * ms_kiElSizeBits;
        assert( _stLast < size() );
        return _stLast;
      }
      ++pElNextThis;
      ++pElNextThat;
    }

    return _NIntersection( pElNextThis, pElNextThat );
  }

  void  invert() _BIEN_NOTHROW
  {
    // We can invert all but the last element wholly - need to
    //  maintain null bits above bit limit on last element:
    if ( m_kstSize )
    {
      t_TyEl * pElBeforeEnd = m_rgEls + m_kstSize - 1;
			t_TyEl * pEl;
      for ( pEl = m_rgEls; pEl != pElBeforeEnd; ++pEl )
      {
        *pEl = ~*pEl;
      }
      t_TyEl  eMask = ~(0x0) << ( ms_kiElSizeBits - ( m_kstSize * ms_kiElSizeBits - m_kstBits ) );
      *pEl = ~( *pEl | eMask );
    }
  }

  size_t  hash() const _BIEN_NOTHROW
  {
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
  }

  // A dangerous method - swaps in any memory.
  // Make sure to worry about throw-safety when using this method.
  void  swap_vector( t_TyEl *& _rrgEls )
  {
    __STD::swap( m_rgEls, _rrgEls );
  }

protected:

  size_type _getclearfirstset( t_TyEl * pEl )
  {
    t_TyEl * pElEnd = m_rgEls + m_kstSize;
    for ( ; pEl != pElEnd; ++pEl )
    {
      if ( *pEl )
      {
        size_type stFound = size_type(pEl - m_rgEls) * ms_kiElSizeBits + (size_type)_bv_get_clear_first_set( *pEl );
        assert( stFound < m_kstBits );
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
        size_type stFound = size_type(pEl - m_rgEls) * ms_kiElSizeBits + size_type(_bv_get_first_set( *pEl ));
        assert( stFound < m_kstBits );
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
        return size_type(pcurThis - m_rgEls) * ms_kiElSizeBits + size_type(_bv_get_first_set( el ));
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
} // end namespace std

#endif //__SIMPBV_H
