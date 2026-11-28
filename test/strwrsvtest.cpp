//          Copyright David Lawrence Bien 1997 - 2026.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// strwrsvtest.cpp
// Unit tests of StrWRsv - strwrsv.h: the string with a reserve buffer which every JSON string and key is kept in.
// A string shorter than the reserve is in the buffer; a longer one is a std::basic_string built in the buffer.
// dbien
// 27NOV2026

#include "bienutiltest.h"

std::string g_strProgramName;

namespace ns_bienutiltest
{
__BIENUTIL_USING_NAMESPACE

typedef StrWRsv< std::string > _TyStr;
typedef StrWRsv< std::u16string > _TyStr16;

// The texts of each length about the reserve: within the buffer, at its edge, and in a string object.
static std::vector< std::string > RgTexts()
{
  std::vector< std::string > rg;
  for ( size_t n : { 0, 1, 15, 16, 22, 23, 46, 47, 48, 49, 100, 1000 } )
  {
    std::string str;
    for ( size_t nCh = 0; nCh < n; ++nCh )
      str += char( 'a' + ( nCh % 26 ) );
    rg.push_back( str );
  }
  return rg;
}

TEST( StrWRsv, AssignAndLength )
{
  for ( const std::string & rstr : RgTexts() )
  {
    _TyStr str;
    str.assign( rstr.c_str(), rstr.length() );
    EXPECT_EQ( str.length(), rstr.length() );
    EXPECT_STREQ( str.c_str(), rstr.c_str() );
    _TyStr strOfPsz( rstr.c_str() );
    EXPECT_STREQ( strOfPsz.c_str(), rstr.c_str() );
    _TyStr strOfStd( rstr );
    EXPECT_STREQ( strOfStd.c_str(), rstr.c_str() );
  }
}

TEST( StrWRsv, AssignOfARange )
{
  const char * psz = "abcdefgh";
  _TyStr str;
  str.assign( psz + 2, psz + 5 );
  EXPECT_STREQ( str.c_str(), "cde" );
}

// Copying leaves the source as it was - whether the source is const or not.
TEST( StrWRsv, CopyLeavesTheSource )
{
  for ( const std::string & rstr : RgTexts() )
  {
    _TyStr strSource( rstr );
    _TyStr strCopy( strSource );
    EXPECT_STREQ( strCopy.c_str(), rstr.c_str() );
    EXPECT_STREQ( strSource.c_str(), rstr.c_str() );
    _TyStr strAssigned;
    strAssigned = strSource; // strSource is not const here.
    EXPECT_STREQ( strAssigned.c_str(), rstr.c_str() );
    EXPECT_STREQ( strSource.c_str(), rstr.c_str() ) << "assignment from a non-const string took its value";
  }
}

TEST( StrWRsv, MoveTakesTheValue )
{
  for ( const std::string & rstr : RgTexts() )
  {
    _TyStr strSource( rstr );
    _TyStr strMoved( std::move( strSource ) );
    EXPECT_STREQ( strMoved.c_str(), rstr.c_str() );
    _TyStr strAssigned( "something before" );
    strAssigned = std::move( strMoved );
    EXPECT_STREQ( strAssigned.c_str(), rstr.c_str() );
  }
}

// Every pair of lengths swapped: buffer with buffer, buffer with string object, string object with string object.
TEST( StrWRsv, SwapEveryPair )
{
  for ( const std::string & rstrA : RgTexts() )
    for ( const std::string & rstrB : RgTexts() )
    {
      _TyStr strA( rstrA ), strB( rstrB );
      strA.swap( strB );
      EXPECT_STREQ( strA.c_str(), rstrB.c_str() );
      EXPECT_STREQ( strB.c_str(), rstrA.c_str() );
      EXPECT_EQ( strA.length(), rstrB.length() );
      EXPECT_EQ( strB.length(), rstrA.length() );
      std::swap( strA, strB );
      EXPECT_STREQ( strA.c_str(), rstrA.c_str() );
      EXPECT_STREQ( strB.c_str(), rstrB.c_str() );
      // Both still work after the swaps: they grow and are destroyed cleanly.
      strA += "!";
      strB += "?";
      EXPECT_EQ( std::string( strA.c_str() ), rstrA + "!" );
      EXPECT_EQ( std::string( strB.c_str() ), rstrB + "?" );
    }
  _TyStr strSelf( "self" );
  strSelf.swap( strSelf );
  EXPECT_STREQ( strSelf.c_str(), "self" );
}

// A std::string moved in - short ones too, which a std::string keeps within itself.
TEST( StrWRsv, OfAMovedStdString )
{
  for ( const std::string & rstr : RgTexts() )
  {
    std::string strStd( rstr );
    _TyStr str( std::move( strStd ) );
    EXPECT_STREQ( str.c_str(), rstr.c_str() );
    _TyStr strOther( std::move( str ) ); // relocated.
    EXPECT_STREQ( strOther.c_str(), rstr.c_str() );
    std::string strStd2( rstr );
    _TyStr strAssigned( "x" );
    strAssigned = std::move( strStd2 );
    _TyStr strSwapped( "y" );
    strSwapped.swap( strAssigned );
    EXPECT_STREQ( strSwapped.c_str(), rstr.c_str() );
    EXPECT_STREQ( strAssigned.c_str(), "y" );
  }
}

TEST( StrWRsv, Append )
{
  for ( const std::string & rstrA : RgTexts() )
    for ( const std::string & rstrB : RgTexts() )
    {
      _TyStr str( rstrA );
      str += rstrB.c_str();
      EXPECT_EQ( std::string( str.c_str() ), rstrA + rstrB );
      EXPECT_EQ( str.length(), rstrA.length() + rstrB.length() );
      _TyStr strN( rstrA );
      strN.append( rstrB.c_str(), rstrB.length() );
      EXPECT_EQ( std::string( strN.c_str() ), rstrA + rstrB );
    }
}

TEST( StrWRsv, AppendOfWideCharacters )
{
  const std::u16string str16A( u"abcé" ), str16B( u"€" );
  _TyStr16 str( str16A );
  str += str16B.c_str();
  EXPECT_EQ( std::u16string( str.c_str() ), str16A + str16B );
  std::u16string str16Long( 60, u'Ж' );
  _TyStr16 strLong( str16A );
  strLong += str16Long.c_str();
  EXPECT_EQ( std::u16string( strLong.c_str() ), str16A + str16Long );
}

// resize() as std::string's: what is there is kept, what is added is the character given.
TEST( StrWRsv, ResizeKeepsThePrefix )
{
  for ( const std::string & rstr : RgTexts() )
    for ( size_t nLen : { 0, 3, 40, 47, 48, 60, 2000 } )
    {
      _TyStr str( rstr );
      str.resize( nLen, 'z' );
      std::string strExpect( rstr );
      strExpect.resize( nLen, 'z' );
      EXPECT_EQ( str.length(), nLen );
      EXPECT_EQ( std::string( str.c_str() ), strExpect ) << rstr.length() << " -> " << nLen;
    }
}

TEST( StrWRsv, Compare )
{
  _TyStr strA( "abc" ), strB( "abd" ), strLong( std::string( 60, 'a' ) );
  EXPECT_TRUE( strA < strB );
  EXPECT_TRUE( strB > strA );
  EXPECT_TRUE( strA == _TyStr( "abc" ) );
  EXPECT_TRUE( strA != strB );
  EXPECT_TRUE( strLong < strA );
  EXPECT_TRUE( ( strA <=> strA ) == 0 );
}

TEST( StrWRsv, Clear )
{
  for ( const std::string & rstr : RgTexts() )
  {
    _TyStr str( rstr );
    str.clear();
    EXPECT_EQ( str.length(), 0u );
    EXPECT_STREQ( str.c_str(), "" );
    str = "again";
    EXPECT_STREQ( str.c_str(), "again" );
  }
}

} // namespace ns_bienutiltest
