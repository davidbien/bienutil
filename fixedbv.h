#pragma once
#include <stdint.h>
#include <array>
#include "bienutil.h"

__BIENUTIL_BEGIN_NAMESPACE

template < size_t t_kN, typename t_TyT = uint8_t > class FixedBV
{
  typedef FixedBV _TyThis;

private:
  typedef std::array< t_TyT, ( t_kN + CHAR_BIT * sizeof( t_TyT ) - 1 ) / ( CHAR_BIT * sizeof( t_TyT ) ) > _TyArray;
  _TyArray m_rgT;
  // Invariant: All bits beyond t_kN are always zero.
  void _Trim()
  {
    const size_t kexcessBits = ( m_rgT.size() * CHAR_BIT * sizeof( t_TyT ) ) - t_kN;
    if constexpr ( kexcessBits > 0 )
      m_rgT[ m_rgT.size() - 1 ] &= ( t_TyT( 1 ) << ( CHAR_BIT * sizeof( t_TyT ) - kexcessBits ) ) - 1;
  }
public:
  typedef t_TyT _TyT;
  static const size_t s_kN = t_kN;

  FixedBV()
    : m_rgT{}
  {
  }
  ~FixedBV() = default;
  FixedBV( _TyThis const & ) = default;
  FixedBV( _TyThis && ) = default;
  _TyThis & operator=( _TyThis const & ) = default;
  _TyThis & operator=( _TyThis && ) = default;

  FixedBV( std::initializer_list< size_t > init )
  {
    for ( size_t pos : init )
    {
      VerifyThrowSz( pos < t_kN, "Invalid bit index." );
      size_t kbyteIndex = pos / ( CHAR_BIT * sizeof( t_TyT ) );
      size_t kbitIndex = pos % ( CHAR_BIT * sizeof( t_TyT ) );
      m_rgT[ kbyteIndex ] |= t_TyT( 1 ) << kbitIndex;
    }
    AssertValid();
  }
  void AssertValid() const
  {
#if ASSERTSENABLED
    // check if any bits above the t_kNth bit are set - don't throw - just assert.
    if constexpr ( t_kN % ( CHAR_BIT * sizeof( t_TyT ) ) )
      Assert( !( m_tgT[ t_kN / ( CHAR_BIT * sizeof( t_TyT ) ) ] & ( ~((t_TyT(1) << (t_kN % ( CHAR_BIT * sizeof( t_TyT )))) - 1) ) ) );
#endif // ASSERTSENABLED
  }
  /// @brief compare two bitvectors
  /// @param _kother bitvector
  /// @return std::strong_ordering
  auto operator<=>( const FixedBV & _kother ) const
  {
    AssertValid();
    _kother.AssertValid();
    for ( size_t _kindex = 0; _kindex < m_rgT.size(); ++_kindex )
    {
      if ( std::strong_ordering cmp = m_rgT[ _kindex ] <=> _kother.m_rgT[ _kindex ]; cmp != 0 )
        return cmp;
    }
    return std::strong_ordering::equal;
  }
  /// @brief return true if bit is set
  /// @param _kindex bit index
  bool test( const size_t _kindex ) const noexcept( false )
  {
    AssertValid();
    VerifyThrowSz( _kindex < t_kN, "Invalid bit index." );
    const size_t kelIndex = _kindex / ( CHAR_BIT * sizeof( t_TyT ) );
    const size_t kbitIndex = _kindex % ( CHAR_BIT * sizeof( t_TyT ) );
    return ( m_rgT[ kelIndex ] & ( t_TyT( 1 ) << kbitIndex ) ) != 0;
  }
  /// @brief set bit to passed value
  /// @param _kindex bit index
  /// @param _kfvalue value
  void set( const size_t _kindex, const bool _kfvalue ) noexcept( false )
  {
    AssertValid();
    VerifyThrowSz( _kindex < t_kN, "Invalid bit index." );
    const size_t kelIndex = _kindex / ( CHAR_BIT * sizeof( t_TyT ) );
    const size_t kbitIndex = _kindex % ( CHAR_BIT * sizeof( t_TyT ) );
    if ( _kfvalue )
      m_rgT[ kelIndex ] |= ( t_TyT( 1 ) << kbitIndex );
    else
      m_rgT[ kelIndex ] &= ~( t_TyT( 1 ) << kbitIndex );
  }
  /// @brief set bit
  /// @param _kindex bit index
  void set( const size_t _kindex ) noexcept( false )
  {
    AssertValid();
    VerifyThrowSz( _kindex < t_kN, "Invalid bit index." );
    const size_t kelIndex = _kindex / ( CHAR_BIT * sizeof( t_TyT ) );
    const size_t kbitIndex = _kindex % ( CHAR_BIT * sizeof( t_TyT ) );
    m_rgT[ kelIndex ] |= ( t_TyT( 1 ) << kbitIndex );
  }
  /// @brief reset bit
  /// @param _kindex bit index
  void reset( const size_t _kindex ) noexcept( false )
  {
    AssertValid();
    VerifyThrowSz( _kindex < t_kN, "Invalid bit index." );
    const size_t kelIndex = _kindex / ( CHAR_BIT * sizeof( t_TyT ) );
    const size_t kbitIndex = _kindex % ( CHAR_BIT * sizeof( t_TyT ) );
    m_rgT[ kelIndex ] &= ~( t_TyT( 1 ) << kbitIndex );
  }
  /// @brief flip bit
  /// @param _kindex bit index
  void flip( const size_t _kindex ) noexcept( false )
  {
    AssertValid();
    VerifyThrowSz( _kindex < t_kN, "Invalid bit index." );
    const size_t kelIndex = _kindex / ( CHAR_BIT * sizeof( t_TyT ) );
    const size_t kbitIndex = _kindex % ( CHAR_BIT * sizeof( t_TyT ) );
    m_rgT[ kelIndex ] ^= ( t_TyT( 1 ) << kbitIndex );
  }
  /// @brief return true if all bits are set
  bool all() const
  {
    AssertValid();
    for ( const auto & element : m_rgT )
      if ( element != ~t_TyT( 0 ) )
        return false;
    return true;
  }
  /// @brief return true if any bits are set
  bool any() const
  {
    AssertValid();
    for ( const auto & element : m_rgT )
      if ( element != 0 )
        return true;
    return false;
  }
  /// @brief return true if no bits are set
  bool none() const { return !any(); }
  /// @brief return number of bits set
  size_t count() const
  {
    AssertValid();
    size_t count = 0;
    for ( size_t _kindex = 0; _kindex < m_rgT.size(); ++_kindex )
    {
#if defined( __GNUC__ ) || defined( __clang__ )
      if constexpr ( sizeof( t_TyT ) <= 4 )
        count += __builtin_popcount( m_rgT[ _kindex ] );
      else
        count += __builtin_popcountll( m_rgT[ _kindex ] );
#elif defined( _MSC_VER )
      if constexpr ( sizeof( t_TyT ) <= 4 )
        count += __popcnt( m_rgT[ _kindex ] );
      else
        count += __popcnt64( m_rgT[ _kindex ] );
#else
      // Fallback to manual bit counting for other compilers
      for ( t_TyT _kbit = m_rgT[ _kindex ]; _kbit; _kbit &= _kbit - 1 )
      {
        ++count;
      }
#endif
    }
    return count;
  }
  // Bitwise operators *do not* take the count of bits into account - all inspecting methods must ignore data beyond the end.
  /// @brief bitwise AND
  _TyThis & operator&=( const _TyThis & _kother )
  {
    AssertValid();
    _kother.AssertValid();
    for ( size_t _kindex = 0; _kindex < m_rgT.size(); ++_kindex )
      m_rgT[ _kindex ] &= _kother.m_rgT[ _kindex ];
    return *this;
  }
  /// @brief bitwise OR
  _TyThis & operator|=( const _TyThis & _kother )
  {
    AssertValid();
    _kother.AssertValid();
    for ( size_t _kindex = 0; _kindex < m_rgT.size(); ++_kindex )
      m_rgT[ _kindex ] |= _kother.m_rgT[ _kindex ];
    return *this;
  }
  /// @brief bitwise XOR
  _TyThis & operator^=( const _TyThis & _kother )
  {
    AssertValid();
    _kother.AssertValid();
    for ( size_t _kindex = 0; _kindex < m_rgT.size(); ++_kindex )
      m_rgT[ _kindex ] ^= _kother.m_rgT[ _kindex ];
    return *this;
  }
  /// @brief bitwise invert
  _TyThis & invert()
  {
    AssertValid();
    for ( size_t _kindex = 0; _kindex < m_rgT.size(); ++_kindex )
      m_rgT[ _kindex ] = ~m_rgT[ _kindex ];
    _Trim();
    return *this;
  }
  /// @brief bitwise NOT
  _TyThis operator~() const
  {
    _TyThis result( *this );
    return result.invert();
  }
  /// @brief shift left
  /// @param _kshift shift amount
  /// @return *this
  _TyThis & operator<<=( const size_t _kshift )
  {
    AssertValid();
    const size_t kwholeShifts = _kshift / ( CHAR_BIT * sizeof( t_TyT ) );
    if ( kwholeShifts != 0 )
    {
      for ( ptrdiff_t _kindex = m_rgT.size() - 1; 0 <= _kindex; --_kindex )
        m_rgT[ _kindex ] = kwholeShifts <= _kindex ? m_rgT[ _kindex - kwholeShifts ] : 0;
    }
    const size_t kbitShifts = _kshift % ( CHAR_BIT * sizeof( t_TyT ) );
    if ( kbitShifts )
    {
      for ( ptrdiff_t _kindex = m_rgT.size() - 1; 0 < _kindex; --_kindex )
        m_rgT[ _kindex ] = ( m_rgT[ _kindex ] << kbitShifts ) | ( m_rgT[ _kindex - 1 ] >> ( ( CHAR_BIT * sizeof( t_TyT ) ) - kbitShifts ) );
      m_rgT[ 0 ] <<= kbitShifts;
    }
    _Trim();
    AssertValid();
    return *this;
  }
  /// @brief shift right
  /// @param _kshift number of bits
  /// @return reference to *this
  _TyThis & operator>>=( const size_t _kshift )
  {
    AssertValid();
    if ( kwholeShifts != 0 )
    {
      for ( size_t _kindex = 0; _kindex <= m_rgT.size(); ++_kindex )
        m_rgT[ _kindex ] = kwholeShifts <= m_rgT.size() - _kindex ? m_rgT[ _kindex + kwholeShifts ] : 0;
    }
    const size_t kbitShifts = _kshift % ( CHAR_BIT * sizeof( t_TyT ) );
    if ( kbitShifts != 0 )
    {
      for ( size_t _kindex = 0; _kindex < m_rgT.size() - 1; ++_kindex )
        m_rgT[ _kindex ] = ( m_rgT[ _kindex ] >> kbitShifts ) | ( m_rgT[ _kindex + 1 ] << ( ( CHAR_BIT * sizeof( t_TyT ) ) - kbitShifts ) );
      m_rgT[ m_rgT.size() - 1 ] >>= kbitShifts;
    }
    AssertValid();
    return *this;
  }
  /// @brief shift left
  /// @param _kshift number of bits
  /// @return a copy of *this
  _TyThis operator<<( const size_t _kshift ) const
  {
    _TyThis result( *this );
    result <<= _kshift;
    return result;
  }
  /// @brief shift right
  /// @param _kshift number of bits
  /// @return a copy of *this
  _TyThis operator>>( const size_t _kshift ) const
  {
    _TyThis result( *this );
    result >>= _kshift;
    return result;
  }
  /// @brief Return the next set bit after the specified index.
  /// @param _kindex starting index or std::numeric_limits<size_t>::max() to start at the first set bit
  /// @return index of next set bit or std::numeric_limits<size_t>::max() if not found
  size_t get_next_bit( size_t _kindex = ( std::numeric_limits< size_t >::max )() ) const
  {
    AssertValid();
    size_t nBitStart = _kindex + 1;
    if ( nBitStart >= t_kN )
      return ( std::numeric_limits< size_t >::max )();
    size_t kelIndex = nBitStart / ( CHAR_BIT * sizeof( t_TyT ) );
    size_t kbitIndex = nBitStart % ( CHAR_BIT * sizeof( t_TyT ) );
    for ( t_TyT mask = m_rgT[ kelIndex ] & ( ~t_TyT( 0 ) << kbitIndex ); ; )
    {
      if ( mask != 0 )
      {
#ifdef _MSC_VER
        #pragma intrinsic(_BitScanForward64)
        unsigned long index;
        if constexpr ( sizeof( t_TyT ) == 8 )
          _BitScanForward64( &index, mask );
        else
          _BitScanForward( &index, mask );
        return kelIndex * ( CHAR_BIT * sizeof( t_TyT ) ) + index;
#elif defined( __GNUC__ ) || defined( __clang__ )
        if constexpr ( sizeof( t_TyT ) == 8 )
          return kelIndex * ( CHAR_BIT * sizeof( t_TyT ) ) + __builtin_ctzll( mask );
        else
          return kelIndex * ( CHAR_BIT * sizeof( t_TyT ) ) + __builtin_ctz( mask );
#else
        // Brute force method for other compilers
        for ( size_t i = 0; i < CHAR_BIT * sizeof( t_TyT ); ++i )
        {
          if ( ( mask >> i ) & 1 )
          {
            return kelIndex * ( CHAR_BIT * sizeof( t_TyT ) ) + i;
          }
        }
#endif
      }
      // No set bit found in this element, continue with the next one
      if ( ++kelIndex < m_rgT.size() )
        mask = m_rgT[ kelIndex ];
      else
        break;
    }
    // No set bit found in any element
    return ( std::numeric_limits< size_t >::max )();
  }
};
template< size_t t_kN, typename t_TyT >
inline FixedBV< t_kN, t_TyT > operator|( FixedBV< t_kN, t_TyT > const& _left, FixedBV< t_kN, t_TyT > const& _right )
{
  FixedBV< t_kN, t_TyT > result( _left );
  result |= _right;
  return result;
}
template< size_t t_kN, typename t_TyT >
inline FixedBV< t_kN, t_TyT > operator&( FixedBV< t_kN, t_TyT > const& _left, FixedBV< t_kN, t_TyT > const& _right )
{
  FixedBV< t_kN, t_TyT > result( _left );
  result &= _right;
  return result;
}

__BIENUTIL_END_NAMESPACE