//          Copyright David Lawrence Bien 1997 - 2026.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// segarraytest.cpp
// Unit tests of segarray.h: SegArray - the segmented array xmlp keeps its text in -, SegArrayRotatingBuffer and
//  SegArrayView. Each is compared with a std::string or std::vector under random operations, with segments of a few
//  elements so that every operation crosses their edges; the elements of an array which owns them are counted.
// dbien
// 27NOV2026

#include "bienutiltest.h"
#include "segarray.h"
#include "_simpbv.h"
#include <random>
#include <vector>
#include <filesystem>
#include <fstream>

std::string g_strProgramName;

namespace ns_bienutiltest
{
__BIENUTIL_USING_NAMESPACE

typedef SegArray< char, std::false_type > _TySaChar;

static std::string StrOf( const _TySaChar & _rsa )
{
  std::string str;
  _rsa.GetString( str, 0, _rsa.NElements() );
  return str;
}
static void Append( _TySaChar & _rsa, const std::string & _rstr )
{
  for ( char ch : _rstr )
    _rsa.push_back( ch );
}

// A stream as OverwriteFromStream() reads one: Read( position in bytes, buffer, bytes ), throwing past its end.
struct _StrStream
{
  std::string m_str;
  void Read( size_t _nbyPos, void * _pv, size_t _nby ) const
  {
    if ( _nbyPos + _nby > m_str.length() )
      throw std::runtime_error( "EOF" );
    memcpy( _pv, m_str.data() + _nbyPos, _nby );
  }
};

TEST( SegArray, CharsRandom )
{
  std::mt19937 rng( 1127 );
  for ( size_t nbySegment : { 1, 2, 3, 8, 64 } )
  {
    _TySaChar sa( nbySegment );
    std::string str;
    auto lambdaRandomText = [ & ]( size_t _nMax )
    {
      std::string strText( rng() % ( _nMax + 1 ), ' ' );
      for ( char & rch : strText )
        rch = char( 'a' + rng() % 26 );
      return strText;
    };
    for ( int nOp = 0; nOp < 3000; ++nOp )
    {
      const unsigned nCase = rng() % 14;
      SCOPED_TRACE( "segment " + std::to_string( nbySegment ) + ", operation " + std::to_string( nOp ) + ", case " + std::to_string( nCase ) );
      switch ( nCase )
      {
      case 0:
      case 1:
      {
        const std::string strAdd = lambdaRandomText( 40 );
        Append( sa, strAdd );
        str += strAdd;
      }
      break;
      case 2:
      {
        const size_t nSize = rng() % ( str.length() + 50 );
        const bool fCompact = !!( rng() & 1 );
        if ( nSize <= str.length() )
        {
          sa.SetSize( nSize, fCompact );
          str.resize( nSize );
        }
        else
        {
          const size_t nOld = str.length();
          sa.SetSize( nSize, fCompact );
          // What grows is not initialized: write it.
          const std::string strFill = lambdaRandomText( 0 ) + std::string( nSize - nOld, 'z' );
          sa.Overwrite( nOld, strFill.data(), strFill.length() );
          str += strFill;
        }
      }
      break;
      case 3:
      {
        const std::string strIns = lambdaRandomText( 30 );
        const size_t nPos = rng() % ( str.length() + 1 );
        sa.Insert( nPos, strIns.data(), strIns.length() );
        str.insert( nPos, strIns );
      }
      break;
      case 4:
      {
        const std::string strOver = lambdaRandomText( 30 );
        const size_t nPos = rng() % ( str.length() + 1 );
        sa.Overwrite( nPos, strOver.data(), strOver.length() );
        if ( nPos + strOver.length() > str.length() )
          str.resize( nPos + strOver.length() );
        str.replace( nPos, strOver.length(), strOver );
      }
      break;
      case 5:
        if ( !str.empty() )
        {
          const size_t nPos = rng() % str.length(), nEls = rng() % ( str.length() - nPos + 1 );
          sa.Remove( nPos, nEls );
          str.erase( nPos, nEls );
        }
      break;
      case 6:
        if ( !str.empty() )
        {
          _simple_bitvec< uint32_t > bv( str.size() );
          std::string strKept;
          for ( size_t n = 0; n < str.size(); ++n )
            if ( rng() % 2 )
              bv.setbit( n );
            else
              strKept += str[ n ];
          sa.RemoveBvElements( bv );
          str = strKept;
        }
      break;
      case 7:
        if ( !str.empty() )
        {
          ASSERT_EQ( sa.pop_back( !!( rng() & 1 ) ), str.back() );
          str.pop_back();
        }
      break;
      case 8:
      {
        // Read, GetString and the views of each range.
        const size_t nBegin = rng() % ( str.length() + 1 ), nEnd = nBegin + rng() % ( str.length() - nBegin + 1 );
        std::string strGot;
        sa.GetString( strGot, nBegin, nEnd );
        ASSERT_EQ( strGot, str.substr( nBegin, nEnd - nBegin ) );
        std::string strRead( nEnd - nBegin + 5, '#' );
        ASSERT_EQ( sa.Read( nBegin, strRead.data(), strRead.length() ), ( std::min )( strRead.length(), str.length() - nBegin ) );
        ASSERT_EQ( strRead.substr( 0, nEnd - nBegin ), str.substr( nBegin, nEnd - nBegin ) );
        std::string_view sv;
        if ( sa.FGetStringView( sv, nBegin, nEnd ) )
          ASSERT_EQ( std::string( sv ), str.substr( nBegin, nEnd - nBegin ) );
        else
          ASSERT_NE( nBegin / sa.NElsPerSegment(), ( nEnd - 1 ) / sa.NElsPerSegment() ); // across segments only.
        std::u16string str16;
        sa.GetString( str16, nBegin, nEnd ); // converting.
        ASSERT_EQ( str16, StrConvertString< char16_t >( str.substr( nBegin, nEnd - nBegin ) ) );
        // Matching and spanning.
        ASSERT_TRUE( sa.FMatchString( nBegin, nEnd, str.substr( nBegin, nEnd - nBegin ).c_str() ) );
        if ( nEnd > nBegin )
        {
          std::string strOther = str.substr( nBegin, nEnd - nBegin );
          strOther.back() = ( 'A' == strOther.back() ) ? 'B' : 'A';
          ASSERT_FALSE( sa.FMatchString( nBegin, nEnd, strOther.c_str() ) );
        }
        const std::string strSet = str.substr( nBegin, nEnd - nBegin );
        ASSERT_TRUE( sa.FSpanChars( nBegin, nEnd, ( strSet + "!" ).c_str() ) );
        ASSERT_EQ( sa.FSpanChars( nBegin, nEnd, "abc" ), strSet.find_first_not_of( "abc" ) == std::string::npos );
      }
      break;
      case 9:
      {
        _TySaChar saCopy( sa );
        ASSERT_EQ( StrOf( saCopy ), str );
        _TySaChar saMoved( std::move( saCopy ) );
        ASSERT_EQ( StrOf( saMoved ), str );
        _TySaChar saAssigned;
        saAssigned = saMoved;
        ASSERT_EQ( StrOf( saAssigned ), str );
        saAssigned = std::move( saAssigned ); // to itself.
        ASSERT_EQ( StrOf( saAssigned ), str );
      }
      break;
      case 10:
      {
        _StrStream ss{ lambdaRandomText( 60 ) };
        if ( !ss.m_str.empty() )
        {
          const size_t nPosRead = rng() % ss.m_str.length(), nRead = rng() % ( ss.m_str.length() - nPosRead + 1 );
          const size_t nPosWrite = rng() % ( str.length() + 1 );
          sa.OverwriteFromStream( nPosWrite, ss, nPosRead, nRead );
          if ( nPosWrite + nRead > str.length() )
            str.resize( nPosWrite + nRead );
          str.replace( nPosWrite, nRead, ss.m_str.substr( nPosRead, nRead ) );
        }
      }
      break;
      case 11:
        sa.Compact();
      break;
      case 12:
        if ( !( rng() % 8 ) )
        {
          sa.Clear();
          str.clear();
        }
      break;
      case 13:
      {
        const size_t nBegin = rng() % ( str.length() + 1 ), nEnd = nBegin + rng() % ( str.length() - nBegin + 1 );
        std::string strApplied;
        sa.ApplyContiguous( nBegin, nEnd, [ & ]( const char * _pBegin, const char * _pEnd ) { strApplied.append( _pBegin, _pEnd ); } );
        ASSERT_EQ( strApplied, str.substr( nBegin, nEnd - nBegin ) );
      }
      break;
      }
      ASSERT_EQ( sa.NElements(), str.length() );
      if ( !( nOp % 32 ) )
        ASSERT_EQ( StrOf( sa ), str ) << nbySegment << " " << nOp;
    }
    ASSERT_EQ( StrOf( sa ), str );
  }
}

TEST( SegArray, Misuse )
{
  _TySaChar sa( 4 );
  Append( sa, "abcdefghij" );
  EXPECT_THROW( sa.Remove( 8, 3 ), std::exception ); // a throw - before, noexcept: std::terminate().
  _simple_bitvec< uint32_t > bvWrong( 3 );
  bvWrong.setbit( 0 );
  EXPECT_THROW( sa.RemoveBvElements( bvWrong ), std::exception );
  std::string str;
  EXPECT_THROW( sa.GetString( str, 5, 11 ), std::exception );
  EXPECT_THROW( sa.WriteToFile( vkhInvalidFileHandle, 5, 6 ), std::exception );
  EXPECT_EQ( sa.Read( 20, str.data(), 3 ), 0u );
  // A string of NULs is data as well.
  _TySaChar saNul( 4 );
  saNul.Overwrite( 0, "a\0b\0c", 5 );
  std::string strNul;
  saNul.GetString( strNul, 0, 5 );
  EXPECT_EQ( strNul, std::string( "a\0b\0c", 5 ) );
  // Compacted to nothing.
  _TySaChar saEmpty( 4 );
  Append( saEmpty, "abcdefgh" );
  saEmpty.SetSizeSmaller( 0, true );
  EXPECT_FALSE( saEmpty.FHasAnyCapacity() );
  Append( saEmpty, "xy" );
  EXPECT_EQ( StrOf( saEmpty ), "xy" );
}

TEST( SegArray, WriteToFile )
{
  _TySaChar sa( 3 );
  Append( sa, "the quick brown fox" );
  const std::string strPath = ( std::filesystem::path( testing::TempDir() ) / "bienutil_segarraytest.bin" ).string();
  {
    FileObj fo( CreateWriteOnlyFile( strPath.c_str() ) );
    ASSERT_TRUE( fo.FIsOpen() );
    sa.WriteToFile( fo.HFileGet(), 4, 11 );
  }
  std::ifstream ifs( strPath, std::ios::binary );
  std::string strRead( ( std::istreambuf_iterator< char >( ifs ) ), std::istreambuf_iterator< char >() );
  EXPECT_EQ( strRead, "quick brown" );
  ifs.close();
  std::filesystem::remove( strPath );
}

// An array which owns its elements: each made is destroyed once.
struct _Counted
{
  static inline ssize_t s_nLive = 0;
  static inline int s_nThrowCountdown = -1;
  int m_n;
  static void _MaybeThrow()
  {
    if ( s_nThrowCountdown >= 0 && !s_nThrowCountdown-- )
      throw std::runtime_error( "_Counted throws as asked." );
  }
  _Counted( int _n = 0 ) : m_n( _n ) { _MaybeThrow(); ++s_nLive; }
  _Counted( const _Counted & _r ) : m_n( _r.m_n ) { _MaybeThrow(); ++s_nLive; }
  _Counted( _Counted && _rr ) noexcept : m_n( _rr.m_n ) { ++s_nLive; }
  _Counted & operator=( const _Counted & ) = default;
  _Counted & operator=( _Counted && ) noexcept = default;
  ~_Counted() { --s_nLive; }
};

TEST( SegArray, OwnedLifetimesRandom )
{
  typedef SegArray< _Counted, std::true_type > _TySa;
  std::mt19937 rng( 2711 );
  {
    _TySa sa( 3 * sizeof( _Counted ) );
    std::vector< int > rg;
    for ( int nOp = 0; nOp < 4000; ++nOp )
    {
      const unsigned nCase = rng() % 9;
      SCOPED_TRACE( "operation " + std::to_string( nOp ) + ", case " + std::to_string( nCase ) );
      switch ( nCase )
      {
      case 0:
      case 1:
      {
        const int n = int( rng() );
        sa.emplaceAtEnd( n );
        rg.push_back( n );
      }
      break;
      case 2:
      {
        const size_t nSize = rng() % ( rg.size() + 20 );
        sa.SetSize( nSize, !!( rng() & 1 ) );
        rg.resize( nSize, 0 );
      }
      break;
      case 3:
        if ( !rg.empty() )
        {
          const size_t nPos = rng() % rg.size(), nEls = rng() % ( rg.size() - nPos + 1 );
          sa.Remove( nPos, nEls );
          rg.erase( rg.begin() + nPos, rg.begin() + nPos + nEls );
        }
      break;
      case 4:
        if ( !rg.empty() )
        {
          _simple_bitvec< uint64_t > bv( rg.size() );
          std::vector< int > rgKept;
          for ( size_t n = 0; n < rg.size(); ++n )
            if ( rng() % 3 )
              rgKept.push_back( rg[ n ] );
            else
              bv.setbit( n );
          sa.RemoveBvElements( bv );
          rg = rgKept;
        }
      break;
      case 5:
      {
        _TySa saCopy( sa );
        ASSERT_EQ( saCopy.NElements(), rg.size() );
        for ( size_t n = 0; n < rg.size(); ++n )
          ASSERT_EQ( saCopy[ n ].m_n, rg[ n ] );
      }
      break;
      case 6:
        if ( !rg.empty() )
        {
          // A copy which throws part way leaves nothing behind - the segment it threw in too.
          const ssize_t nLiveBefore = _Counted::s_nLive;
          _Counted::s_nThrowCountdown = int( rng() % rg.size() );
          EXPECT_THROW( _TySa saCopy( sa ), std::exception );
          _Counted::s_nThrowCountdown = -1;
          ASSERT_EQ( _Counted::s_nLive, nLiveBefore );
        }
      break;
      case 7:
        if ( !rg.empty() )
        {
          ASSERT_EQ( sa.pop_back().m_n, rg.back() );
          rg.pop_back();
        }
      break;
      case 8:
        if ( !( rng() % 10 ) )
        {
          sa.Clear();
          rg.clear();
        }
      break;
      }
      ASSERT_EQ( sa.NElements(), rg.size() );
      ASSERT_EQ( _Counted::s_nLive, ssize_t( rg.size() ) ) << nOp;
    }
    for ( size_t n = 0; n < rg.size(); ++n )
      ASSERT_EQ( sa[ n ].m_n, rg[ n ] );
  }
  EXPECT_EQ( _Counted::s_nLive, 0 );
}

// The rotating buffer: text is added at the end and consumed from the base; the segments before the base are reused.
TEST( SegArrayRotatingBuffer, RandomStream )
{
  typedef SegArrayRotatingBuffer< char > _TyRb;
  std::mt19937 rng( 99 );
  for ( size_t nbySegment : { 1, 4, 16 } )
  {
    _TyRb rb( nbySegment );
    std::string strAll; // Everything ever written - element n of the buffer is strAll[ n ].
    size_t nBase = 0;
    for ( int nOp = 0; nOp < 3000; ++nOp )
    {
      switch ( rng() % 5 )
      {
      case 0:
      case 1:
      {
        const size_t nAdd = rng() % 30;
        std::string strAdd( nAdd, ' ' );
        for ( char & rch : strAdd )
          rch = char( 'a' + rng() % 26 );
        rb.Overwrite( strAll.length(), strAdd.data(), strAdd.length() );
        strAll += strAdd;
      }
      break;
      case 2:
      {
        // Consume some: the base moves forward.
        const size_t nNewBase = nBase + rng() % ( strAll.length() - nBase + 1 );
        rb.SetIBaseEl( ssize_t( nNewBase ) );
        nBase = nNewBase;
      }
      break;
      case 3:
        if ( strAll.length() > nBase )
        {
          const size_t nLen = rng() % ( strAll.length() - nBase + 1 );
          std::string strCopied( nLen, '#' );
          rb.CopyDataAndAdvanceBuffer( nBase, strCopied.data(), nLen );
          ASSERT_EQ( strCopied, strAll.substr( nBase, nLen ) );
          nBase += nLen;
        }
      break;
      case 4:
      {
        const size_t nBegin = nBase + rng() % ( strAll.length() - nBase + 1 ), nEnd = nBegin + rng() % ( strAll.length() - nBegin + 1 );
        std::string strGot;
        rb.GetString( strGot, nBegin, nEnd );
        ASSERT_EQ( strGot, strAll.substr( nBegin, nEnd - nBegin ) );
        std::string_view sv;
        if ( rb.FGetStringView( sv, nBegin, nEnd ) )
          ASSERT_EQ( std::string( sv ), strAll.substr( nBegin, nEnd - nBegin ) );
        ASSERT_TRUE( rb.FMatchString( nBegin, nEnd, strAll.substr( nBegin, nEnd - nBegin ).c_str() ) );
        if ( nBase )
        {
          std::string strBefore;
          EXPECT_THROW( rb.GetString( strBefore, nBase - 1, nBase ), std::exception ); // before the base.
        }
      }
      break;
      }
      ASSERT_EQ( rb.NElements(), strAll.length() ) << nbySegment << " " << nOp;
      for ( size_t n = nBase; n < strAll.length(); n += 1 + rng() % 7 )
        ASSERT_EQ( rb[ n ], strAll[ n ] ) << n;
    }
  }
}

TEST( SegArrayRotatingBuffer, NegativeBase )
{
  SegArrayRotatingBuffer< char > rb( 4 );
  rb.Overwrite( 0, "abcdefghij", 10 );
  rb.SetIBaseEl( -100 ); // element 100 is the first.
  EXPECT_EQ( rb.NElements(), 110u );
  EXPECT_EQ( rb[ 100 ], 'a' );
  EXPECT_EQ( rb[ 109 ], 'j' );
  EXPECT_THROW( rb[ 99 ], std::exception );
  EXPECT_THROW( rb.SetIBaseEl( 5 ), std::exception ); // the sign does not change.
  SegArrayRotatingBuffer< char > rbMoved( std::move( rb ) );
  EXPECT_EQ( rbMoved[ 105 ], 'f' );
  rbMoved = std::move( rbMoved );
  EXPECT_EQ( rbMoved[ 105 ], 'f' );
}

TEST( SegArrayView, StringsOfViews )
{
  typedef SegArrayView< char, std::false_type > _TyView;
  _TySaChar sa( 4 );
  Append( sa, "hello segmented world" );
  // Over the array - across segments.
  _TyView viewSa( &sa, 3, 12 );
  std::string_view sv;
  std::string str;
  EXPECT_FALSE( viewSa.FGetStringViewOrString( sv, str ) );
  EXPECT_EQ( sv, "lo segmente" "d" );
  std::u16string_view sv16;
  std::u16string str16;
  EXPECT_FALSE( viewSa.FGetStringViewOrString( sv16, str16 ) ); // converting.
  EXPECT_EQ( std::u16string( sv16 ), std::u16string( u"lo segmented" ) );
  std::string strGot;
  viewSa.GetString( strGot );
  EXPECT_EQ( strGot, "lo segmented" );
  std::u32string str32;
  viewSa.GetString( str32 );
  EXPECT_EQ( str32, std::u32string( U"lo segmented" ) );
  // Of a pointer: the view is the memory itself.
  char rgch[] = "pointed";
  _TyView viewPtr( &sa, rgch, 7 );
  std::string_view svPtr;
  std::string strPtr;
  EXPECT_TRUE( viewPtr.FGetStringViewOrString( svPtr, strPtr ) );
  EXPECT_EQ( svPtr.data(), rgch );
  std::u16string_view svPtr16;
  std::u16string strPtr16;
  EXPECT_FALSE( viewPtr.FGetStringViewOrString( svPtr16, strPtr16 ) );
  EXPECT_EQ( std::u16string( svPtr16 ), std::u16string( u"pointed" ) );
  // Null and of no length.
  _TyView viewNull;
  EXPECT_TRUE( viewNull.FIsNull() );
  _TyView viewEmptyPtr( &sa, rgch, 0 );
  EXPECT_TRUE( viewEmptyPtr.FIsNull() );
  std::string_view svEmpty;
  std::string strEmpty;
  EXPECT_TRUE( viewEmptyPtr.FGetStringViewOrString( svEmpty, strEmpty ) );
  EXPECT_TRUE( svEmpty.empty() );
  // Long: more than is put on the stack.
  _TySaChar saLong( 64 );
  Append( saLong, std::string( 20000, 'L' ) );
  _TyView viewLong( &saLong, size_t( 0 ), 20000 );
  std::u32string str32Long;
  viewLong.GetString( str32Long );
  EXPECT_EQ( str32Long, std::u32string( 20000, U'L' ) );
}

} // namespace ns_bienutiltest
