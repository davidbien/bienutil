//          Copyright David Lawrence Bien 1997 - 2026.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// containertest.cpp
// Unit tests of the bit utilities ( _bitutil.h ), the bit vector ( _simpbv.h ) and the logarithmic array
//  ( _logarray.h ) - what lexang builds its DFAs with and xmlp keeps a tag's attributes in.
// Each container is compared with the standard library's under random operations, its elements counted so that every
//  one made is destroyed once; some elements throw when made, as an element of a lexer's may.
// dbien
// 27NOV2026

#include "bienutiltest.h"
#include "_simpbv.h"
#include "_logarray.h"
#include "upslist.h"
#include <random>
#include <vector>
#include <thread>

std::string g_strProgramName;

namespace ns_bienutiltest
{
__BIENUTIL_USING_NAMESPACE

// ------------------------------------------------------------------------------------------------------------------
// _bitutil.h

TEST( BitUtil, MostSignificantBit )
{
  for ( unsigned n = 0; n < 64; ++n )
  {
    const uint64_t u64 = uint64_t( 1 ) << n;
    EXPECT_EQ( MSBitSet( u64 ), n );
    EXPECT_EQ( KMSBitSet( u64 ), n );
    EXPECT_EQ( MSBitSet( u64 | ( u64 - 1 ) ), n ); // and every bit below.
    EXPECT_EQ( KMSBitSet( u64 | ( u64 >> 1 ) ), n );
    if ( n < 32 )
    {
      EXPECT_EQ( MSBitSet( uint32_t( u64 ) ), n );
      EXPECT_EQ( KMSBitSet( uint32_t( u64 | ( u64 - 1 ) ) ), n );
    }
    if ( n < 16 )
      EXPECT_EQ( KMSBitSet( uint16_t( u64 ) ), n );
    if ( n < 8 )
    {
      EXPECT_EQ( KMSBitSet( uint8_t( u64 ) ), n );
      EXPECT_EQ( MSBitSet( uint8_t( u64 ) ), n );
    }
  }
  EXPECT_EQ( MSBitSet( uint64_t( 0 ) ), ( std::numeric_limits< size_t >::max )() );
  static_assert( 63 == KMSBitSet( ~uint64_t( 0 ) ) );
}

TEST( BitUtil, CountBits )
{
  EXPECT_EQ( NCountBitsSet( uint64_t( 0 ) ), 0u );
  EXPECT_EQ( NCountBitsSet( ~uint64_t( 0 ) ), 64u );
  EXPECT_EQ( NCountBitsSet( uint32_t( 0xF0F0F0F0 ) ), 16u );
  EXPECT_EQ( NCountBitsSet( uint16_t( 0x8001 ) ), 2u );
  EXPECT_EQ( NCountBitsSet( uint8_t( 0xFF ) ), 8u );
}

TEST( BitUtil, MultiplyOverflow )
{
  std::mt19937_64 rng( 1127 );
  for ( int n = 0; n < 100000; ++n )
  {
    const uint64_t u64L = rng() >> ( rng() % 64 ), u64R = rng() >> ( rng() % 64 );
    // The reference: no builtin ( MSVC has none ) - overflowed if more than the most / one factor; the product wraps.
    const bool fExpect = !!u64R && ( u64L > ~0ull / u64R );
    const uint64_t u64Expect = u64L * u64R;
    uint64_t u64;
    ASSERT_EQ( KMultiplyTestOverflow( u64L, u64R, u64 ), fExpect ) << u64L << " * " << u64R;
    ASSERT_EQ( u64, u64Expect );
  }
  uint64_t u64;
  EXPECT_TRUE( KMultiplyTestOverflow( ( 1ull << 32 ) + 1, ( 1ull << 32 ) + 1, u64 ) ); // wraps to more than either.
  EXPECT_FALSE( KMultiplyTestOverflow( 0, ~0ull, u64 ) );
}

TEST( BitUtil, Power )
{
  for ( uint64_t u64Base : { 0ull, 1ull, 2ull, 3ull, 7ull, 10ull, 255ull, 65536ull, ( 1ull << 32 ), ( 1ull << 32 ) + 1, ~0ull } )
    for ( unsigned nExp = 0; nExp < 256; ++nExp )
    {
      // The reference: multiply nExp times, overflow found before each multiplication - no 128 bit type ( MSVC has none ).
      uint64_t u64 = 1;
      bool fOverflow = false;
      for ( unsigned n = 0; n < nExp && !fOverflow; ++n )
      {
        fOverflow = !!u64Base && ( u64 > ~0ull / u64Base );
        u64 *= u64Base;
      }
      if ( fOverflow )
        EXPECT_THROW( KUPow( u64Base, uint8_t( nExp ) ), std::exception ) << u64Base << "^" << nExp;
      else
        EXPECT_EQ( KUPow( u64Base, uint8_t( nExp ) ), u64 ) << u64Base << "^" << nExp;
    }
  static_assert( 1000000000000000000ull == KUPow( 10, 18 ) );
  static_assert( 9 == KUPow( 3, 2 ) );
}

// ------------------------------------------------------------------------------------------------------------------
// _simple_bitvec

template < class t_TyEl >
struct _BitVecTest
{
  typedef _simple_bitvec< t_TyEl > _TyBv;
  static _TyBv BvOf( const std::vector< bool > & _rg )
  {
    _TyBv bv( _rg.size() );
    for ( size_t n = 0; n < _rg.size(); ++n )
      if ( _rg[ n ] )
        bv.setbit( n );
    return bv;
  }
  static void Check( const _TyBv & _rbv, const std::vector< bool > & _rg )
  {
    ASSERT_EQ( _rbv.size(), _rg.size() );
    size_t nSet = 0;
    for ( size_t n = 0; n < _rg.size(); ++n )
    {
      ASSERT_EQ( !!_rbv.isbitset( n ), !!_rg[ n ] ) << n;
      nSet += _rg[ n ];
    }
    EXPECT_EQ( _rbv.countsetbits(), nSet );
    EXPECT_EQ( _rbv.empty(), !nSet );
    // The bits past the last are clear: the invariant.
    if ( _rbv.size() % _TyBv::ms_kstElSizeBits )
    {
      const t_TyEl kelLast = const_cast< _TyBv & >( _rbv ).begin()[ _rbv.size_bytes() / sizeof( t_TyEl ) - 1 ];
      EXPECT_FALSE( kelLast & t_TyEl( ~( ( t_TyEl( 1 ) << ( _rbv.size() % _TyBv::ms_kstElSizeBits ) ) - 1 ) ) ) << "bits set past the last";
    }
  }
  static size_t NFirst( const std::vector< bool > & _rg, size_t _nFrom, bool _fSet )
  {
    for ( size_t n = _nFrom; n < _rg.size(); ++n )
      if ( _rg[ n ] == _fSet )
        return n;
    return _rg.size();
  }
  static void Run()
  {
    std::mt19937 rng( 271127 + sizeof( t_TyEl ) );
    for ( size_t nBits : { 0, 1, 7, 8, 9, 31, 32, 33, 63, 64, 65, 127, 128, 129, 200 } )
      for ( int nRound = 0; nRound < 40; ++nRound )
      {
        std::vector< bool > rg( nBits );
        const unsigned nDensity = rng() % 4;
        for ( size_t n = 0; n < nBits; ++n )
          rg[ n ] = ( 0 == nDensity ) ? false : ( 3 == nDensity ) ? true : !( rng() % ( nDensity + 1 ) );
        _TyBv bv = BvOf( rg );
        Check( bv, rg );
        // Searches from each place.
        EXPECT_EQ( bv.getfirstset(), NFirst( rg, 0, true ) );
        for ( size_t n = 0; n < nBits; ++n )
        {
          ASSERT_EQ( bv.getnextset( n ), NFirst( rg, n + 1, true ) ) << nBits << " " << n;
          ASSERT_EQ( bv.getnextnotset( n ), NFirst( rg, n + 1, false ) ) << nBits << " " << n;
        }
        // Clearing each set bit in turn, from the first.
        {
          _TyBv bvClear( bv );
          std::vector< bool > rgClear( rg );
          size_t nFound = bvClear.getclearfirstset();
          for ( ; nFound != nBits; nFound = bvClear.getclearfirstset( nFound ) )
          {
            ASSERT_TRUE( rgClear[ nFound ] );
            rgClear[ nFound ] = false;
          }
          EXPECT_TRUE( bvClear.empty() );
        }
        // Clearing from a place: only bits after it - those before it stay.
        for ( size_t nFrom = 0; nFrom < nBits; nFrom += 1 + rng() % 9 )
        {
          _TyBv bvClear( bv );
          std::vector< bool > rgClear( rg );
          const size_t nExpect = NFirst( rg, nFrom + 1, true );
          ASSERT_EQ( bvClear.getclearfirstset( nFrom ), nExpect ) << nBits << " " << nFrom;
          if ( nExpect != nBits )
            rgClear[ nExpect ] = false;
          Check( bvClear, rgClear );
        }
        // Other operations against another vector of the same size.
        std::vector< bool > rgOther( nBits );
        for ( size_t n = 0; n < nBits; ++n )
          rgOther[ n ] = !( rng() % 3 );
        const _TyBv bvOther = BvOf( rgOther );
        std::vector< bool > rgOr( nBits ), rgAnd( nBits ), rgAndNot( nBits ), rgInv( nBits );
        size_t nFirstIntersection = nBits;
        for ( size_t n = 0; n < nBits; ++n )
        {
          rgOr[ n ] = rg[ n ] || rgOther[ n ];
          rgAnd[ n ] = rg[ n ] && rgOther[ n ];
          rgAndNot[ n ] = rg[ n ] && !rgOther[ n ];
          rgInv[ n ] = !rg[ n ];
          if ( rgAnd[ n ] && ( nBits == nFirstIntersection ) )
            nFirstIntersection = n;
        }
        Check( bv | bvOther, rgOr );
        Check( bv & bvOther, rgAnd );
        _TyBv bvAndNot( bv );
        bvAndNot.and_not_equals( bvOther );
        Check( bvAndNot, rgAndNot );
        _TyBv bvInv( bv );
        bvInv.invert();
        Check( bvInv, rgInv );
        EXPECT_EQ( bv.FIntersects( bvOther ), nBits != nFirstIntersection );
        EXPECT_EQ( bv.FirstIntersection( bvOther ), nFirstIntersection );
        for ( size_t n = 0; n < nBits; ++n )
        {
          size_t nExpect = nBits;
          for ( size_t m = n + 1; m < nBits; ++m )
            if ( rgAnd[ m ] )
            {
              nExpect = m;
              break;
            }
          ASSERT_EQ( bv.NextIntersection( bvOther, n ), nExpect );
        }
        _TyBv bvIntersection( nBits );
        EXPECT_EQ( bvIntersection.intersection( bv, bvOther ), nBits != nFirstIntersection );
        Check( bvIntersection, rgAnd );
        EXPECT_TRUE( bv == BvOf( rg ) );
        EXPECT_EQ( bv == bvOther, rg == rgOther );
        EXPECT_EQ( bv.hash(), BvOf( rg ).hash() );
      }
  }
  // The vector as a number: increment and decrement, wrapping at its size.
  static void RunArithmetic()
  {
    for ( size_t nBits : { 1, 3, 7, 8, 9, 16, 17, 40, 64 } )
    {
      const uint64_t u64Mask = ( 64 == nBits ) ? ~0ull : ( ( 1ull << nBits ) - 1 );
      for ( uint64_t u64Start : std::initializer_list< uint64_t >{ 0ull, 1ull, u64Mask, u64Mask - 1, u64Mask >> 1, 0x5555555555555555ull & u64Mask } )
      {
        std::vector< bool > rg( nBits );
        for ( size_t n = 0; n < nBits; ++n )
          rg[ n ] = !!( ( u64Start >> n ) & 1 );
        _TyBv bvInc = BvOf( rg ), bvDec = BvOf( rg );
        bvInc.increment();
        bvDec.decrement();
        const uint64_t u64Inc = ( u64Start + 1 ) & u64Mask, u64Dec = ( u64Start - 1 ) & u64Mask;
        std::vector< bool > rgInc( nBits ), rgDec( nBits );
        for ( size_t n = 0; n < nBits; ++n )
        {
          rgInc[ n ] = !!( ( u64Inc >> n ) & 1 );
          rgDec[ n ] = !!( ( u64Dec >> n ) & 1 );
        }
        Check( bvInc, rgInc );
        Check( bvDec, rgDec );
      }
    }
  }
};

TEST( SimpleBitVec, Bytes ) { _BitVecTest< uint8_t >::Run(); }
TEST( SimpleBitVec, Words ) { _BitVecTest< uint32_t >::Run(); }
TEST( SimpleBitVec, LongWords ) { _BitVecTest< uint64_t >::Run(); }
TEST( SimpleBitVec, IncrementDecrement )
{
  _BitVecTest< uint8_t >::RunArithmetic();
  _BitVecTest< uint32_t >::RunArithmetic();
  _BitVecTest< uint64_t >::RunArithmetic();
}

// Vectors of different sizes: assigned, compared. They are used with vectors of the same size only, and a debug build
//  asserts that: what they do all the same, for a build without asserts, is tested there alone.
#if !ASSERTSENABLED
TEST( SimpleBitVec, OfDifferentSizes )
{
  typedef _simple_bitvec< uint32_t > _TyBv;
  _TyBv bvSmall( 10 ), bvLarge( 100 ), bvEmpty;
  bvLarge.setbit( 99 );
  bvSmall.setbit( 3 );
  EXPECT_FALSE( bvSmall == bvLarge );
  EXPECT_TRUE( bvSmall != bvLarge );
  EXPECT_TRUE( bvSmall < bvLarge );
  EXPECT_FALSE( bvLarge < bvSmall );
  EXPECT_TRUE( bvEmpty == _TyBv() );
  bvSmall = bvLarge;
  EXPECT_EQ( bvSmall.size(), 100u );
  EXPECT_TRUE( bvSmall.isbitset( 99 ) );
  EXPECT_TRUE( bvSmall == bvLarge );
  bvEmpty = bvLarge;
  EXPECT_TRUE( bvEmpty == bvLarge );
  _TyBv bvAssigned( 100 );
  bvAssigned = bvLarge; // the same size.
  EXPECT_TRUE( bvAssigned.isbitset( 99 ) );
  _TyBv bvMoved( std::move( bvAssigned ) );
  EXPECT_TRUE( bvMoved.isbitset( 99 ) );
  EXPECT_EQ( bvAssigned.size(), 0u );
  bvMoved.clear();
  EXPECT_TRUE( bvMoved.empty() );
  _TyBv bvZero;
  bvZero.clear(); // of no elements.
  EXPECT_EQ( bvZero.getfirstset(), 0u );
  EXPECT_EQ( bvZero.getclearfirstset(), 0u );
  EXPECT_EQ( bvZero.getnextset( 0 ), 0u );
  std::swap( bvSmall, bvZero );
  EXPECT_EQ( bvZero.size(), 100u );
}
#endif //!ASSERTSENABLED

// ------------------------------------------------------------------------------------------------------------------
// LogArray

// An element which counts the elements alive, and throws when it is made once a countdown comes to 0.
struct _Counted
{
  static inline ssize_t s_nLive = 0;
  static inline int s_nThrowCountdown = -1; // < 0: never.
  int m_n;
  static void _MaybeThrow()
  {
    if ( s_nThrowCountdown >= 0 && !s_nThrowCountdown-- )
      throw std::runtime_error( "_Counted throws as asked." );
  }
  explicit _Counted( int _n = 0 ) : m_n( _n )
  {
    _MaybeThrow();
    ++s_nLive;
  }
  _Counted( const _Counted & _r ) : m_n( _r.m_n )
  {
    _MaybeThrow();
    ++s_nLive;
  }
  _Counted( _Counted && _rr ) noexcept : m_n( _rr.m_n ) { ++s_nLive; }
  _Counted & operator=( const _Counted & ) = default;
  _Counted & operator=( _Counted && ) noexcept = default;
  ~_Counted() { --s_nLive; }
};

template < class t_TyArray >
static std::vector< int > RgOf( const t_TyArray & _ra )
{
  std::vector< int > rg;
  _ra.ApplyContiguous( 0, _ra.NElements(),
                       [ & ]( const _Counted * _pBegin, const _Counted * _pEnd )
                       {
                         for ( ; _pBegin != _pEnd; ++_pBegin )
                           rg.push_back( _pBegin->m_n );
                       } );
  return rg;
}

template < size_t t_knMin, size_t t_knMax, bool t_fFreeList >
static void LogArrayRandom()
{
  typedef LogArray< _Counted, t_knMin, t_knMax, t_fFreeList > _TyArray;
  ASSERT_EQ( _Counted::s_nLive, 0 );
  std::mt19937 rng( unsigned( 2711 + t_knMin * 31 + t_knMax ) );
  {
    _TyArray la;
    std::vector< int > rg;
    for ( int nOp = 0; nOp < 6000; ++nOp )
    {
      const unsigned nWhich = rng() % 20;
      if ( nWhich < 8 )
      {
        const int n = int( rng() % 100000 );
        la.emplaceAtEnd( n );
        rg.push_back( n );
      }
      else if ( nWhich < 9 )
      {
        const size_t nSize = rng() % ( 3 * rg.size() + 70 );
        la.SetSize( nSize, -1 );
        rg.resize( nSize, -1 );
      }
      else if ( nWhich < 11 && !rg.empty() )
      {
        const size_t nPos = rng() % rg.size(), nEls = rng() % ( rg.size() - nPos + 1 );
        la.Remove( nPos, nEls );
        rg.erase( rg.begin() + nPos, rg.begin() + nPos + nEls );
      }
      else if ( nWhich < 12 && !rg.empty() )
      {
        _simple_bitvec< uint64_t > bv( rg.size() );
        std::vector< int > rgKept;
        for ( size_t n = 0; n < rg.size(); ++n )
          if ( !( rng() % 3 ) )
            bv.setbit( n );
          else
            rgKept.push_back( rg[ n ] );
        la.RemoveBvElements( bv );
        rg = rgKept;
      }
      else if ( nWhich < 13 )
      {
        // An element which throws as it is made: the array is as it was, and goes on.
        _Counted::s_nThrowCountdown = 0;
        EXPECT_THROW( la.emplaceAtEnd( 7 ), std::runtime_error );
        _Counted::s_nThrowCountdown = -1;
        ASSERT_EQ( la.NElements(), rg.size() );
      }
      else if ( nWhich < 14 )
      {
        _TyArray laCopy( la );
        ASSERT_EQ( RgOf( laCopy ), rg );
        _TyArray laMoved( std::move( laCopy ) );
        ASSERT_EQ( RgOf( laMoved ), rg );
        ASSERT_EQ( laCopy.NElements(), 0u );
        LogArray< _Counted, 1, 6, false > laOther( la ); // of other powers.
        ASSERT_EQ( RgOf( laOther ), rg );
        _TyArray laBack;
        laBack = laOther;
        ASSERT_EQ( RgOf( laBack ), rg );
      }
      else if ( nWhich < 15 && !rg.empty() )
      {
        // A copy which throws part way: nothing is left of it.
        const ssize_t nLiveBefore = _Counted::s_nLive;
        _Counted::s_nThrowCountdown = int( rng() % rg.size() );
        EXPECT_THROW( _TyArray laCopy( la ), std::runtime_error );
        _Counted::s_nThrowCountdown = -1;
        ASSERT_EQ( _Counted::s_nLive, nLiveBefore );
      }
      else if ( nWhich < 16 && !rg.empty() )
      {
        // Every element as ElGet() has it, and contiguous ranges of each kind.
        for ( size_t n = 0; n < rg.size(); ++n )
          ASSERT_EQ( la[ n ].m_n, rg[ n ] ) << n;
        ASSERT_EQ( la.RTail().m_n, rg.back() );
        const size_t nBegin = rng() % ( rg.size() + 1 ), nEnd = nBegin + rng() % ( rg.size() - nBegin + 1 );
        std::vector< int > rgRange;
        la.ApplyContiguous( nBegin, nEnd, [ & ]( _Counted * _pBegin, _Counted * _pEnd ) { for ( ; _pBegin != _pEnd; ++_pBegin ) rgRange.push_back( _pBegin->m_n ); } );
        ASSERT_EQ( rgRange, std::vector< int >( rg.begin() + nBegin, rg.begin() + nEnd ) );
        // Stopped at an element: the count up to it.
        const size_t nStop = nBegin + ( nEnd > nBegin ? rng() % ( nEnd - nBegin ) : 0 );
        size_t nPos = nBegin;
        const size_t nApplied = la.NApplyContiguous( nBegin, nEnd,
            [ & ]( _Counted * _pBegin, _Counted * _pEnd ) -> size_t
            {
              size_t n = 0;
              for ( ; _pBegin != _pEnd && nPos != nStop; ++_pBegin, ++nPos, ++n )
                ;
              return n;
            } );
        ASSERT_EQ( nApplied, ( nEnd > nBegin ) ? ( nStop - nBegin ) : 0u );
      }
      else if ( nWhich < 17 )
      {
        la.Clear();
        rg.clear();
      }
      ASSERT_EQ( la.NElements(), rg.size() );
      ASSERT_EQ( _Counted::s_nLive, ssize_t( rg.size() ) ) << "elements made and not destroyed, or destroyed twice";
      if ( !( nOp % 64 ) )
        ASSERT_EQ( RgOf( la ), rg );
    }
    ASSERT_EQ( RgOf( la ), rg );
  }
  EXPECT_EQ( _Counted::s_nLive, 0 );
}

TEST( LogArray, Random_0_1 ) { LogArrayRandom< 0, 1, false >(); }
TEST( LogArray, Random_0_3 ) { LogArrayRandom< 0, 3, false >(); }
TEST( LogArray, Random_1_3 ) { LogArrayRandom< 1, 3, false >(); }
TEST( LogArray, Random_2_5 ) { LogArrayRandom< 2, 5, false >(); }
TEST( LogArray, Random_3_8_FreeList ) { LogArrayRandom< 3, 8, true >(); }
TEST( LogArray, Random_4_5_FreeList ) { LogArrayRandom< 4, 5, true >(); }

// The blocks: of 2^min elements, doubling up to 2^max, then of 2^max.
TEST( LogArray, BlocksAsDocumented )
{
  LogArray< int, 2, 4 > la;
  for ( int n = 0; n < 100; ++n )
    la.emplaceAtEnd( n );
  std::vector< size_t > rgBlockSizes;
  la.ApplyContiguous( 0, la.NElements(), [ & ]( int * _pBegin, int * _pEnd ) { rgBlockSizes.push_back( size_t( _pEnd - _pBegin ) ); } );
  std::vector< size_t > rgExpect = { 4, 8 }; // 12 elements, then blocks of 16.
  for ( size_t n = 12; n < 100; n += 16 )
    rgExpect.push_back( ( std::min )( size_t( 16 ), size_t( 100 - n ) ) );
  EXPECT_EQ( rgBlockSizes, rgExpect );
  LogArray< int, 2, 4 > laSized( 30 );
  EXPECT_EQ( laSized.NElements(), 30u );
  EXPECT_THROW( laSized.ElGet( 30 ), std::exception );
  EXPECT_THROW( laSized.SetSizeSmaller( 31 ), std::exception );
  EXPECT_THROW( ( LogArray< int, 2, 4 >().RTail() ), std::exception );
  EXPECT_THROW( laSized.Remove( 25, 6 ), std::exception ); // throws - before, std::terminate() in a noexcept method.
  _simple_bitvec< uint64_t > bvWrongSize( 29 );
  bvWrongSize.setbit( 0 );
  EXPECT_THROW( laSized.RemoveBvElements( bvWrongSize ), std::exception );
}

// Freed blocks are kept per thread: blocks freed on one thread and reused on another.
TEST( LogArray, FreeListAcrossThreads )
{
  typedef LogArray< _Counted, 3, 6, true > _TyArray;
  std::vector< _TyArray > rgArrays( 50 );
  for ( auto & rla : rgArrays )
    for ( int n = 0; n < 5; ++n )
      rla.emplaceAtEnd( n );
  std::thread thr( [ & ] { rgArrays.clear(); } ); // freed - cached - on another thread, freed when it ends.
  thr.join();
  EXPECT_EQ( _Counted::s_nLive, 0 );
  for ( int nRound = 0; nRound < 3; ++nRound )
  {
    std::vector< _TyArray > rg( 300 ); // more than the 256 a thread keeps.
    for ( auto & rla : rg )
      rla.emplaceAtEnd( 1 );
  }
  EXPECT_EQ( _Counted::s_nLive, 0 );
}

// ------------------------------------------------------------------------------------------------------------------
// UniquePtrSList

TEST( UniquePtrSList, PushPopFind )
{
  UniquePtrSList< std::string > list;
  EXPECT_TRUE( list.empty() );
  EXPECT_EQ( list.count(), 0u );                          // of an empty list: before, * of a null unique_ptr.
  EXPECT_EQ( list.PListElFront(), nullptr );
  EXPECT_EQ( list.PListElFind( "x" ), nullptr );
  EXPECT_FALSE( list.FFind( nullptr ) );
  EXPECT_THROW( list.front(), std::exception );
  EXPECT_THROW( list.pop(), std::exception );             // before, null was dereferenced.
  list.push( std::string( "c" ) );
  const std::string strB( "b" );
  list.push( strB );
  UniquePtrSList< std::string >::_TyPtr upA = std::make_unique< UniquePtrSList< std::string >::_TyListEl >( std::string( "a" ) );
  list.push( upA );
  EXPECT_FALSE( !!upA );
  EXPECT_EQ( list.count(), 3u );
  EXPECT_EQ( list.front(), "a" );
  std::string strAll;
  list.Apply( [ & ]( std::string & _rstr ) { strAll += _rstr; } );
  EXPECT_EQ( strAll, "abc" );
  const UniquePtrSList< std::string > & rlistConst = list;
  std::string strConst;
  rlistConst.Apply( [ & ]( const std::string & _rstr ) { strConst += _rstr; } ); // didn't compile before.
  EXPECT_EQ( strConst, "abc" );
  auto * pelB = list.PListElFind( "b" );                   // nor this, of a non-const list.
  ASSERT_NE( pelB, nullptr );
  EXPECT_EQ( **pelB, "b" );
  EXPECT_TRUE( list.FFind( pelB ) );
  EXPECT_EQ( list.PListElFind( "z" ), nullptr );
  UniquePtrSList< std::string >::_TyPtr upTop;
  list.pop( upTop );
  EXPECT_EQ( **upTop, "a" );
  EXPECT_FALSE( list.FFind( upTop.get() ) );
  list.pop();
  EXPECT_EQ( list.front(), "c" );
  UniquePtrSList< std::string > listMoved( std::move( list ) );
  EXPECT_TRUE( list.empty() );
  EXPECT_EQ( listMoved.count(), 1u );
  UniquePtrSList< std::string > listOther;
  listOther.push( std::string( "o" ) );
  listOther = std::move( listMoved );
  EXPECT_EQ( listOther.front(), "c" );
  listOther = std::move( listOther ); // to itself.
  EXPECT_EQ( listOther.front(), "c" );
  listOther.clear();
  EXPECT_TRUE( listOther.empty() );
}

// A long list is destroyed one element at a time: as unique_ptr<>s alone it was a recursion as deep as the list.
TEST( UniquePtrSList, LongListDestroyed )
{
  {
    UniquePtrSList< _Counted > list;
    for ( int n = 0; n < 1000000; ++n )
      list.push( _Counted( n ) );
    EXPECT_EQ( _Counted::s_nLive, 1000000 );
  }
  EXPECT_EQ( _Counted::s_nLive, 0 );
}

} // namespace ns_bienutiltest
