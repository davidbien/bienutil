//          Copyright David Lawrence Bien 1997 - 2026.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// jsontest.cpp
// Unit tests of the JSON reader and writer - jsonstrm.h - and of the values built on them - jsonobjs.h.
// What is tested: JSON read as RFC 8259 has it ( what it refuses as much as what it reads ), JsoValue's accessors,
//  the writer's output to the character, escaping, the round trip of random values, the read cursor used directly
//  ( reading some values and skipping others ), the filters, files, and the other character types.
// dbien
// 27NOV2026

#include "bienutiltest.h"
#include <random>
#include <sstream>
#include <cmath>
#include <filesystem>

std::string g_strProgramName;

namespace ns_bienutiltest
{
__BIENUTIL_USING_NAMESPACE

typedef JsoValue< char > _TyJv;
typedef JsonCharTraits< char > _TyTraits;
typedef JsonFixedMemInputStream< _TyTraits > _TyMemIn;
typedef JsonReadCursor< _TyMemIn > _TyCursor;
typedef JsonOutputMemStream< _TyTraits > _TyMemOut;
typedef JsonValueLife< _TyMemOut > _TyLife;
typedef JsonFormatSpec< _TyTraits > _TyFormat;

static _TyJv JvParse( const std::string & _rstr )
{
  _TyJv jv;
  jv.FromString( _rstr.c_str(), _rstr.length() );
  return jv;
}
static std::string StrWrite( const _TyJv & _rjv, const _TyFormat * _pjfs = nullptr )
{
  std::string str;
  _rjv.ToString( str, _pjfs );
  return str;
}
static bool FParses( const std::string & _rstr )
{
  try
  {
    (void)JvParse( _rstr );
    return true;
  }
  catch ( const std::exception & )
  {
    return false;
  }
}
static std::string StrOfMemOut( _TyMemOut & _rjos )
{
  std::string str( _rjos.GetLengthChars(), 0 );
  _rjos.GetMemStream().Seek( 0, SEEK_SET );
  _rjos.GetMemStream().Read( str.data(), str.length() );
  return str;
}
static _TyFormat JfsPretty()
{
  _TyFormat jfs;
  jfs.m_fUseCRLF = false;
  return jfs;
}

// ------------------------------------------------------------------------------------------------------------------
// Reading: what is JSON.

TEST( JsonRead, Literals )
{
  EXPECT_TRUE( JvParse( "null" ).FIsNull() );
  EXPECT_TRUE( JvParse( "true" ).FIsTrue() );
  EXPECT_TRUE( JvParse( "false" ).FIsFalse() );
  EXPECT_TRUE( JvParse( " \t\r\n true \t\r\n " ).FIsTrue() );
  EXPECT_TRUE( JvParse( "[null,true,false]" )[ 2 ].FIsFalse() );
}

TEST( JsonRead, LiteralsMisspelt )
{
  for ( const char * psz : { "nul", "nill", "nulll", "nope", "tru", "trUe", "truth", "fals", "falsy", "n", "t", "f", "[nul]", "[tru]", "{\"a\":falze}", "[truex]" } )
    EXPECT_FALSE( FParses( psz ) ) << psz;
}

TEST( JsonRead, Numbers )
{
  for ( const char * psz : { "0", "-0", "7", "12", "-12", "12.5", "-0.25", "1e10", "1E10", "1e+10", "1e-10", "1.5E-3", "123456789012345678901234567890", "0.0", "0e0" } )
  {
    _TyJv jv = JvParse( psz );
    ASSERT_TRUE( jv.FIsNumber() ) << psz;
    EXPECT_STREQ( jv.StrGet().c_str(), psz ); // A number is kept as its text.
    _TyJv jvInArray = JvParse( std::string( "[ " ) + psz + " ]" );
    EXPECT_STREQ( jvInArray[ 0 ].StrGet().c_str(), psz );
    _TyJv jvInObject = JvParse( std::string( "{\"n\":" ) + psz + "}" );
    EXPECT_STREQ( jvInObject[ "n" ].StrGet().c_str(), psz );
  }
}

TEST( JsonRead, NumbersRefused )
{
  for ( const char * psz : { "-", "+1", ".5", "1.", "1.e5", "01", "-01", "00", "1e", "1e+", "1E-", "--1", "0x10", "1.5.2", "1e5.5", "Infinity", "NaN", "-Infinity", "1 2", "[01]", "[1.]", "[-]", "{\"a\":01}" } )
    EXPECT_FALSE( FParses( psz ) ) << psz;
}

TEST( JsonRead, NumberTooLongIsRefused )
{
  EXPECT_TRUE( FParses( std::string( 500, '1' ) ) );
  EXPECT_FALSE( FParses( std::string( 600, '1' ) ) );
  EXPECT_FALSE( FParses( "[" + std::string( 600, '1' ) + "]" ) );
}

TEST( JsonRead, Strings )
{
  EXPECT_STREQ( JvParse( "\"\"" ).StrGet().c_str(), "" );
  EXPECT_STREQ( JvParse( "\"abc\"" ).StrGet().c_str(), "abc" );
  EXPECT_STREQ( JvParse( R"("q\"b\\s\/f\bF\fN\nR\rT\t")" ).StrGet().c_str(), "q\"b\\s/f\bF\fN\nR\rT\t" );
  EXPECT_STREQ( JvParse( R"("\u0041\u0062\u007E")" ).StrGet().c_str(), "Ab~" );
  EXPECT_STREQ( JvParse( "\"caf\xC3\xA9 \xE2\x82\xAC\"" ).StrGet().c_str(), "caf\xC3\xA9 \xE2\x82\xAC" ); // UTF-8 as it is.
}

// \u escapes are of UTF-16 code units: in a string of char they are UTF-8.
TEST( JsonRead, UnicodeEscapesAreUtf8 )
{
  EXPECT_STREQ( JvParse( R"("\u00e9")" ).StrGet().c_str(), "\xC3\xA9" );
  EXPECT_STREQ( JvParse( R"("\u00E9")" ).StrGet().c_str(), "\xC3\xA9" );
  EXPECT_STREQ( JvParse( R"("\u07ff")" ).StrGet().c_str(), "\xDF\xBF" );
  EXPECT_STREQ( JvParse( R"("\u0800")" ).StrGet().c_str(), "\xE0\xA0\x80" );
  EXPECT_STREQ( JvParse( R"("\u20ac")" ).StrGet().c_str(), "\xE2\x82\xAC" );
  EXPECT_STREQ( JvParse( R"("\uffff")" ).StrGet().c_str(), "\xEF\xBF\xBF" );
  EXPECT_STREQ( JvParse( R"("\ud83d\ude00")" ).StrGet().c_str(), "\xF0\x9F\x98\x80" ); // a surrogate pair.
  EXPECT_STREQ( JvParse( R"("f\u00fcr")" ).StrGet().c_str(), "f\xC3\xBCr" );          // as Python's json.dump writes German.
  EXPECT_STREQ( JvParse( R"({"\u00e9":1})" ).begin().GetKeyValue().first.c_str(), "\xC3\xA9" );
}

TEST( JsonRead, StringsRefused )
{
  for ( const char * psz : { "\"", "\"abc", "\"\\\"", "\"\\x\"", "\"\\u12\"", "\"\\u12g4\"", "\"\\U0041\"", "'abc'", "[\"a]", "\"\\ud83d\"", "\"\\ud83dx\"", "\"\\ud83d\\u0041\"", "\"\\ude00\"" } )
    EXPECT_FALSE( FParses( psz ) ) << psz;
}

// The reader's buffer is 1023 characters: strings about that length, and escapes across its edge.
TEST( JsonRead, LongStrings )
{
  for ( size_t n : { 47, 48, 1022, 1023, 1024, 1025, 2046, 2047, 5000 } )
  {
    std::string str;
    for ( size_t nCh = 0; nCh < n; ++nCh )
      str += char( 'a' + ( nCh % 26 ) );
    EXPECT_EQ( std::string( JvParse( "\"" + str + "\"" ).StrGet().c_str() ), str ) << n;
    // The same with multi-byte escapes at every place about the edge.
    std::string strEsc, strExpect;
    for ( size_t nCh = 0; nCh < n; ++nCh )
    {
      strEsc += "\\u20ac";
      strExpect += "\xE2\x82\xAC";
    }
    EXPECT_EQ( std::string( JvParse( "\"" + strEsc + "\"" ).StrGet().c_str() ), strExpect ) << n;
  }
}

TEST( JsonRead, Objects )
{
  _TyJv jv = JvParse( R"( { "b" : 2 , "a" : { "c" : [ 1 , { } , [ ] ] } , "" : "empty key" } )" );
  ASSERT_TRUE( jv.FIsObject() );
  EXPECT_EQ( jv.GetSize(), 3u );
  EXPECT_STREQ( jv[ "b" ].StrGet().c_str(), "2" );
  EXPECT_EQ( jv[ "a" ][ "c" ].GetSize(), 3u );
  EXPECT_TRUE( jv[ "a" ][ "c" ][ 1 ].FIsObject() );
  EXPECT_EQ( jv[ "a" ][ "c" ][ 1 ].GetSize(), 0u );
  EXPECT_TRUE( jv[ "a" ][ "c" ][ 2 ].FIsArray() );
  EXPECT_STREQ( jv[ "" ].StrGet().c_str(), "empty key" );
  EXPECT_EQ( JvParse( "{}" ).GetSize(), 0u );
  EXPECT_EQ( JvParse( "{ }" ).GetSize(), 0u );
}

TEST( JsonRead, ObjectsRefused )
{
  for ( const char * psz : { "{", "}", "{\"a\"}", "{\"a\":}", "{\"a\" 1}", "{\"a\":1,}", "{,}", "{\"a\":1 \"b\":2}", "{a:1}", "{1:1}", "{'a':1}", "{\"a\":1,\"a\":2}", "{\"a\":1]", "{\"a\":{}" } )
    EXPECT_FALSE( FParses( psz ) ) << psz;
}

TEST( JsonRead, Arrays )
{
  _TyJv jv = JvParse( "[ 1 , \"two\" , null , [ [ ] ] , { \"k\" : true } ]" );
  ASSERT_TRUE( jv.FIsArray() );
  EXPECT_EQ( jv.GetSize(), 5u );
  EXPECT_STREQ( jv[ 1 ].StrGet().c_str(), "two" );
  EXPECT_TRUE( jv[ 2 ].FIsNull() );
  EXPECT_EQ( jv[ 3 ][ 0 ].GetSize(), 0u );
  EXPECT_TRUE( jv[ 4 ][ "k" ].FIsTrue() );
  EXPECT_EQ( JvParse( "[]" ).GetSize(), 0u );
}

TEST( JsonRead, ArraysRefused )
{
  for ( const char * psz : { "[", "]", "[1,]", "[,1]", "[,]", "[1 2]", "[1,,2]", "[1}", "[[]", "[\"a\":1]" } )
    EXPECT_FALSE( FParses( psz ) ) << psz;
}

// What follows the value is whitespace and nothing else.
TEST( JsonRead, NothingAfterTheValue )
{
  for ( const char * psz : { "{} x", "{}}", "{}{}", "[] []", "[]]", "1 2", "12x", "\"a\"\"b\"", "null null", "true,", "nullx", "falsey", "{\"a\":1} ,", "0 1" } )
    EXPECT_FALSE( FParses( psz ) ) << psz;
  for ( const char * psz : { "{} ", "[]\n", "1\r\n", "\"a\"\t", "null " } )
    EXPECT_TRUE( FParses( psz ) ) << psz;
}

TEST( JsonRead, EmptyAndIllegal )
{
  for ( const char * psz : { "", " ", "\n\t", "x", "/", "<", "@" } )
    EXPECT_FALSE( FParses( psz ) ) << "[" << psz << "]";
  EXPECT_FALSE( FParses( std::string( "[1,\0 2]", 7 ) ) );   // NUL is no character of JSON.
  EXPECT_FALSE( FParses( std::string( "\"a\0b\"", 5 ) ) );
}

// Each proper prefix of a document which is an object is no JSON - and none crashes the reader.
TEST( JsonRead, EveryTruncationIsRefused )
{
  const std::string strDoc = R"({"format":1,"id":"x-1.0","list":[1,-2.5e3,"t\"\u00e9",true,false,null,{"a":[]},{}],"o":{"k":"v"}})";
  ASSERT_TRUE( FParses( strDoc ) );
  for ( size_t n = 0; n < strDoc.length(); ++n )
    EXPECT_FALSE( FParses( strDoc.substr( 0, n ) ) ) << strDoc.substr( 0, n );
}

// Every character of a document changed to each of a set of characters: the reader reads it or refuses it, and
//  never crashes ( this is for the sanitizers ). Where what is read is written, it reads again the same.
TEST( JsonRead, EveryCorruptionIsReadOrRefused )
{
  const std::string strDoc = R"({"a":[1,-2.5e3,"t\"\u00e9\n",true,false,null,{"b":[]},{}],"c":{"d":"v"}})";
  for ( size_t n = 0; n < strDoc.length(); ++n )
    for ( char ch : std::string( "{}[]\":,\\ 0-.eE1tfnu\x01" ) )
    {
      std::string str( strDoc );
      str[ n ] = ch;
      try
      {
        _TyJv jv = JvParse( str );
        _TyJv jvAgain = JvParse( StrWrite( jv ) );
        EXPECT_TRUE( jv == jvAgain ) << str;
      }
      catch ( const std::exception & )
      {
      }
    }
}

TEST( JsonRead, DeepNesting )
{
  const size_t knDepth = 500;
  std::string str = std::string( knDepth, '[' ) + "7" + std::string( knDepth, ']' );
  _TyJv jv = JvParse( str );
  const _TyJv * pjv = &jv;
  for ( size_t n = 0; n < knDepth; ++n )
  {
    ASSERT_TRUE( pjv->FIsArray() );
    ASSERT_EQ( pjv->GetSize(), 1u );
    pjv = &( *pjv )[ 0 ];
  }
  EXPECT_STREQ( pjv->StrGet().c_str(), "7" );
  EXPECT_EQ( StrWrite( jv ), str );
}

TEST( JsonRead, LargeArray )
{
  std::string str = "[";
  for ( int n = 0; n < 20000; ++n )
    str += ( n ? "," : "" ) + std::to_string( n );
  str += "]";
  _TyJv jv = JvParse( str );
  ASSERT_EQ( jv.GetSize(), 20000u );
  int32_t i = 0;
  jv[ 12345 ].GetValue( i );
  EXPECT_EQ( i, 12345 );
  EXPECT_EQ( StrWrite( jv ), str );
}

// ------------------------------------------------------------------------------------------------------------------
// JsoValue: what it gives and what it refuses.

TEST( JsoValue, TypePredicates )
{
  _TyJv jv = JvParse( R"({"n":null,"t":true,"f":false,"i":1,"s":"x","o":{},"a":[]})" );
  EXPECT_TRUE( jv[ "n" ].FIsNull() && jv[ "n" ].FIsNullOrEmpty() );
  EXPECT_TRUE( jv[ "t" ].FIsBoolean() && jv[ "t" ].FIsTrue() && !jv[ "t" ].FIsFalse() );
  EXPECT_TRUE( jv[ "f" ].FIsBoolean() && jv[ "f" ].FIsFalse() );
  EXPECT_TRUE( jv[ "i" ].FIsNumber() && !jv[ "i" ].FIsString() );
  EXPECT_TRUE( jv[ "s" ].FIsString() && !jv[ "s" ].FIsNumber() );
  EXPECT_TRUE( jv[ "o" ].FIsObject() && jv[ "o" ].FIsAggregate() );
  EXPECT_TRUE( jv[ "a" ].FIsArray() && jv[ "a" ].FIsAggregate() );
  EXPECT_TRUE( _TyJv().FEmpty() );
  bool f = false;
  jv[ "t" ].GetBoolValue( f );
  EXPECT_TRUE( f );
  jv[ "f" ].GetBoolValue( f );
  EXPECT_FALSE( f );
}

TEST( JsoValue, MisuseThrows )
{
  _TyJv jv = JvParse( R"({"s":"x","a":[1,2,3]})" );
  EXPECT_THROW( jv[ "missing" ], std::exception );
  EXPECT_THROW( jv[ (size_t)0 ], std::exception );      // an object is not indexed.
  EXPECT_THROW( jv[ "a" ][ "k" ], std::exception );     // an array has no keys.
  EXPECT_THROW( jv[ "s" ].GetSize(), std::exception );
  EXPECT_THROW( jv[ "a" ].StrGet(), std::exception );
  bool f;
  EXPECT_THROW( jv[ "s" ].GetBoolValue( f ), std::exception );
  int32_t i;
  EXPECT_THROW( jv[ "s" ].GetValue( i ), std::exception );
}

// The index of an array is below its size: at its size it is refused, as beyond it.
TEST( JsoValue, ArrayIndexAtTheSizeThrows )
{
  _TyJv jv = JvParse( "[10,20,30]" );
  EXPECT_STREQ( jv[ 2 ].StrGet().c_str(), "30" );
  EXPECT_THROW( jv[ 3 ], std::exception );
  EXPECT_THROW( jv[ 4 ], std::exception );
  const _TyJv & rjvConst = jv;
  EXPECT_THROW( rjvConst[ 3 ], std::exception );
  _TyJv jvEmpty = JvParse( "[]" );
  EXPECT_THROW( jvEmpty[ 0 ], std::exception );
}

TEST( JsoValue, IntegerValues )
{
  uint8_t by;
  int8_t sby;
  uint16_t us;
  int16_t ss;
  uint32_t ui;
  int32_t si;
  uint64_t ul;
  int64_t sl;
  JvParse( "255" ).GetValue( by );
  EXPECT_EQ( by, 255 );
  JvParse( "-128" ).GetValue( sby );
  EXPECT_EQ( sby, -128 );
  JvParse( "65535" ).GetValue( us );
  EXPECT_EQ( us, 65535 );
  JvParse( "-32768" ).GetValue( ss );
  EXPECT_EQ( ss, -32768 );
  JvParse( "4294967295" ).GetValue( ui );
  EXPECT_EQ( ui, 4294967295u );
  JvParse( "-2147483648" ).GetValue( si );
  EXPECT_EQ( si, INT32_MIN );
  JvParse( "18446744073709551615" ).GetValue( ul );
  EXPECT_EQ( ul, UINT64_MAX );
  JvParse( "-9223372036854775808" ).GetValue( sl );
  EXPECT_EQ( sl, INT64_MIN );
}

// A number which the type cannot hold is refused - not wrapped, not cut.
TEST( JsoValue, IntegerValuesOutOfRangeThrow )
{
  uint8_t by;
  int32_t si;
  uint32_t ui;
  uint64_t ul;
  EXPECT_THROW( JvParse( "256" ).GetValue( by ), std::exception );
  EXPECT_THROW( JvParse( "-1" ).GetValue( by ), std::exception );
  EXPECT_THROW( JvParse( "-1" ).GetValue( ui ), std::exception );
  EXPECT_THROW( JvParse( "2147483648" ).GetValue( si ), std::exception );
  EXPECT_THROW( JvParse( "18446744073709551616" ).GetValue( ul ), std::exception );
  EXPECT_THROW( JvParse( "1.5" ).GetValue( si ), std::exception );
  EXPECT_THROW( JvParse( "1e3" ).GetValue( si ), std::exception );
}

TEST( JsoValue, FloatingValues )
{
  double dbl = 0;
  JvParse( "1.5" ).GetValue( dbl );
  EXPECT_EQ( dbl, 1.5 );
  JvParse( "0.1" ).GetValue( dbl );
  EXPECT_EQ( dbl, 0.1 );
  JvParse( "-2.5e-3" ).GetValue( dbl );
  EXPECT_EQ( dbl, -2.5e-3 );
  JvParse( "1e300" ).GetValue( dbl );
  EXPECT_EQ( dbl, 1e300 );
  JvParse( "42" ).GetValue( dbl );
  EXPECT_EQ( dbl, 42.0 );
  float fl = 0;
  JvParse( "0.25" ).GetValue( fl );
  EXPECT_EQ( fl, 0.25f );
  long double ldbl = 0;
  JvParse( "0.5" ).GetValue( ldbl );
  EXPECT_EQ( ldbl, 0.5L );
}

// A double written is read again the same - the shortest text which is that double.
TEST( JsoValue, DoublesRoundTrip )
{
  for ( double dbl : { 0.0, 1.0, -1.0, 0.1, 1.0 / 3.0, 1e-10, 1e300, -1e-300, 123456789.123, 2.2250738585072014e-308, 1.7976931348623157e308, 5e-324 } )
  {
    _TyJv jv;
    jv.SetValue( dbl );
    double dblBack = 0;
    jv.GetValue( dblBack );
    EXPECT_EQ( dblBack, dbl ) << jv.StrGet().c_str();
    double dblRead = 0;
    JvParse( StrWrite( jv ) ).GetValue( dblRead );
    EXPECT_EQ( dblRead, dbl );
  }
  _TyJv jv;
  jv.SetValue( 0.1 );
  EXPECT_STREQ( jv.StrGet().c_str(), "0.1" );
  jv.SetValue( 1e-10 );
  EXPECT_STREQ( jv.StrGet().c_str(), "1e-10" );
}

// JSON has no NaN nor infinity: they are not written.
TEST( JsoValue, NonFiniteDoublesThrow )
{
  _TyJv jv;
  EXPECT_THROW( jv.SetValue( std::nan( "" ) ), std::exception );
  EXPECT_THROW( jv.SetValue( HUGE_VAL ), std::exception );
  EXPECT_THROW( jv.SetValue( -HUGE_VAL ), std::exception );
}

TEST( JsoValue, Build )
{
  _TyJv jv( ejvtObject );
  jv( "name" ).SetStringValue( std::string( "x" ) );
  jv( "count" ).SetValue( int32_t( 3 ) );
  jv( "flag" ) = true;
  jv( "list" )( (size_t)2 ).SetValue( uint64_t( 7 ) ); // null, null, 7
  jv( "list" ).AppendEl().SetStringValue( "end" );
  jv( "nothing" ).SetNullValue();
  EXPECT_EQ( StrWrite( jv ), R"({"count":3,"flag":true,"list":[null,null,7,"end"],"name":"x","nothing":null})" );
  _TyJv jvBool;
  _TyJv & rjv = ( jvBool = false ); // operator=(bool) gives back the value.
  EXPECT_EQ( &rjv, &jvBool );
  EXPECT_TRUE( jvBool.FIsFalse() );
}

TEST( JsoValue, CopyMoveCompare )
{
  _TyJv jvA = JvParse( R"({"a":[1,"two",{"three":3}],"long":")" + std::string( 100, 'L' ) + R"("})" );
  _TyJv jvCopy( jvA );
  EXPECT_TRUE( jvCopy == jvA );
  _TyJv jvAssigned;
  jvAssigned = jvA;
  EXPECT_TRUE( jvAssigned == jvA );
  jvCopy[ "a" ][ 0 ].SetValue( int32_t( 2 ) );
  EXPECT_TRUE( jvCopy != jvA );
  EXPECT_TRUE( ( jvA <=> jvCopy ) < 0 );
  _TyJv jvMoved( std::move( jvCopy ) );
  EXPECT_STREQ( jvMoved[ "a" ][ 0 ].StrGet().c_str(), "2" );
  _TyJv jvMoveAssigned;
  jvMoveAssigned = std::move( jvMoved );
  EXPECT_STREQ( jvMoveAssigned[ "a" ][ 0 ].StrGet().c_str(), "2" );
  EXPECT_EQ( std::string( jvMoveAssigned[ "long" ].StrGet().c_str() ), std::string( 100, 'L' ) );
  EXPECT_TRUE( JvParse( "1" ) != JvParse( "\"1\"" ) );
}

// Every pair of kinds of value swapped.
TEST( JsoValue, Swap )
{
  const std::vector< std::string > rgDocs = { "null", "true", "17", "\"short\"", "\"" + std::string( 80, 'x' ) + "\"", R"({"k":"v","o":{"p":[1]}})", "{}", "[1,[2],{\"a\":3}]", "[]" };
  for ( const std::string & rstrA : rgDocs )
    for ( const std::string & rstrB : rgDocs )
    {
      _TyJv jvA = JvParse( rstrA ), jvB = JvParse( rstrB );
      jvA.swap( jvB );
      EXPECT_EQ( StrWrite( jvA ), rstrB );
      EXPECT_EQ( StrWrite( jvB ), rstrA );
      _TyJv jvC( jvA ); // each still a whole value after the swap.
      EXPECT_TRUE( jvC == jvA );
    }
  _TyJv jvSelf = JvParse( "[1]" );
  jvSelf.swap( jvSelf );
  EXPECT_EQ( StrWrite( jvSelf ), "[1]" );
}

// An object's members are iterated in the order of their keys; an array's in its own.
TEST( JsoValue, Iteration )
{
  _TyJv jv = JvParse( R"({"c":3,"a":1,"b":2})" );
  std::string strKeys, strValues;
  for ( auto it = jv.begin(); it != jv.end(); ++it )
  {
    strKeys += it.GetKeyValue().first.c_str();
    strValues += it->StrGet().c_str();
  }
  EXPECT_EQ( strKeys, "abc" );
  EXPECT_EQ( strValues, "123" );
  const _TyJv jvArray = JvParse( "[3,1,2]" );
  std::string strArray;
  for ( auto it = jvArray.begin(); it != jvArray.end(); ++it )
    strArray += ( *it ).StrGet().c_str();
  EXPECT_EQ( strArray, "312" );
  auto it = jvArray.begin();
  auto itCopy = it;
  ++it;
  EXPECT_TRUE( itCopy != it );
  EXPECT_EQ( it - itCopy, 1 );
  EXPECT_THROW( JvParse( "1" ).begin(), std::exception );
}

TEST( JsoValue, OfOtherStrings )
{
  _TyJv jv;
  jv.SetStringValue( std::string_view( "view" ) );
  EXPECT_STREQ( jv.StrGet().c_str(), "view" );
  jv = std::string( "assigned" );
  EXPECT_STREQ( jv.StrGet().c_str(), "assigned" );
  std::string strLong( 70, 'q' );
  jv.SetStringValue( std::move( strLong ) );
  EXPECT_EQ( std::string( jv.StrGet().c_str() ), std::string( 70, 'q' ) );
  jv.FromString( std::string( "[1]" ) );
  EXPECT_EQ( jv.GetSize(), 1u );
}

// ------------------------------------------------------------------------------------------------------------------
// Writing.

TEST( JsonWrite, Compact )
{
  _TyJv jv = JvParse( R"( { "b" : [ true , null , 1.5 ] , "a" : "x" , "e" : { } , "f" : [ ] } )" );
  EXPECT_EQ( StrWrite( jv ), R"({"a":"x","b":[true,null,1.5],"e":{},"f":[]})" );
  EXPECT_EQ( StrWrite( JvParse( "null" ) ), "null" );
  EXPECT_EQ( StrWrite( JvParse( "\"s\"" ) ), "\"s\"" );
  EXPECT_EQ( StrWrite( JvParse( "-0.5e+7" ) ), "-0.5e+7" );
}

TEST( JsonWrite, Pretty )
{
  _TyJv jv = JvParse( R"({"a":1,"b":[true,null],"c":{},"d":[]})" );
  const _TyFormat jfs = JfsPretty();
  EXPECT_EQ( StrWrite( jv, &jfs ), "{\n    \"a\": 1,\n    \"b\": [\n        true,\n        null\n    ],\n    \"c\": {},\n    \"d\": []\n}" );
  _TyFormat jfsTabs = JfsPretty();
  jfsTabs.m_fUseTabs = true;
  jfsTabs.m_nWhitespacePerIndent = 1;
  jfsTabs.m_fUseCRLF = true;
  EXPECT_EQ( StrWrite( JvParse( "[1,[2]]" ), &jfsTabs ), "[\r\n\t1,\r\n\t[\r\n\t\t2\r\n\t]\r\n]" );
  EXPECT_TRUE( JvParse( StrWrite( jv, &jfs ) ) == jv );
}

TEST( JsonWrite, StreamOperator )
{
  std::ostringstream oss;
  oss << JvParse( R"({"a":[1]})" );
#ifdef WIN32
  EXPECT_EQ( oss.str(), "{\r\n    \"a\": [\r\n        1\r\n    ]\r\n}" );
#else
  EXPECT_EQ( oss.str(), "{\n    \"a\": [\n        1\n    ]\n}" );
#endif
}

TEST( JsonWrite, Escaping )
{
  _TyJv jv;
  jv.SetStringValue( std::string( "q\"b\\n\n\t\r\b\f\x01\x1f/\x7f caf\xC3\xA9" ) );
  EXPECT_EQ( StrWrite( jv ), R"("q\"b\\n\n\t\r\b\f\u0001\u001F/)" "\x7f" " caf\xC3\xA9\"" );
  // Keys are escaped as values are.
  _TyJv jvObject( ejvtObject );
  jvObject( "k\"\n" ).SetNullValue();
  EXPECT_EQ( StrWrite( jvObject ), R"({"k\"\n":null})" );
}

// Every character of 1 to 127, and UTF-8, written and read again.
TEST( JsonWrite, EveryCharacterRoundTrips )
{
  std::string str;
  for ( int ch = 1; ch < 128; ++ch )
    str += char( ch );
  str += "\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80";
  _TyJv jv( ejvtObject );
  jv( str.c_str() ).SetStringValue( str );
  const std::string strJson = StrWrite( jv );
  for ( char ch : strJson )
    EXPECT_FALSE( ( (unsigned char)ch < 0x20 ) ) << "a control character written as it is";
  _TyJv jvRead = JvParse( strJson );
  EXPECT_EQ( std::string( jvRead.begin().GetKeyValue().first.c_str() ), str );
  EXPECT_EQ( std::string( jvRead.begin()->StrGet().c_str() ), str );
}

// m_fEscapePrintableWhitespace false: newlines, returns and tabs as they are - but quotes, backslashes and the other
//  control characters escaped still, and the whitespace at the end of a value too when asked.
TEST( JsonWrite, PrintableWhitespaceNotEscaped )
{
  _TyJv jv( ejvtArray );
  jv.AppendEl().SetStringValue( std::string( "a\nb\t\"c\\\x01\b\n" ) );
  _TyFormat jfs = JfsPretty();
  jfs.m_fEscapePrintableWhitespace = false;
  jfs.m_fEscapePrintableWhitespaceAtEndOfLine = true;
  EXPECT_EQ( StrWrite( jv, &jfs ), "[\n    \"a\nb\t\\\"c\\\\\\u0001\\b\\n\"\n]" );
  jfs.m_fEscapePrintableWhitespaceAtEndOfLine = false;
  EXPECT_EQ( StrWrite( jv, &jfs ), "[\n    \"a\nb\t\\\"c\\\\\\u0001\\b\n\"\n]" );
  // Whitespace only: all of it at the end.
  _TyJv jvWs;
  jvWs.SetStringValue( std::string( "\n\t" ) );
  jfs.m_fEscapePrintableWhitespaceAtEndOfLine = true;
  EXPECT_EQ( StrWrite( jvWs, &jfs ), R"("\n\t")" );
}

// JsonValueLife used directly, as lexang and others write.
TEST( JsonWrite, ValueLife )
{
  _TyMemOut jos;
  {
    _TyLife jvl( jos, ejvtObject );
    jvl.WriteValue( "i", int32_t( -5 ) );
    jvl.WriteValue( "u", uint64_t( UINT64_MAX ) );
    jvl.WriteValue( "l", int64_t( INT64_MIN ) );
    jvl.WriteValue( "b", uint8_t( 200 ) );
    jvl.WriteStringValue( "s", std::string( "x\"y" ) );
    jvl.WriteBoolValue( "t", true );
    jvl.WriteNullValue( "n" );
    jvl.WriteValueType( "n2", ejvtNull );
    jvl.WriteValueType( "f2", ejvtFalse );
    jvl.WriteValue( "d", 0.1 );
    {
      _TyLife jvlArray( jvl, "a", ejvtArray );
      jvlArray.WriteValue( int32_t( 1 ) );
      jvlArray.WriteStringValue( std::string( "two" ) );
      jvlArray.WriteNullValue();
      jvlArray.WriteValueType( ejvtTrue );
      jvlArray.WriteBoolValue( false );
      jvlArray.WriteValue( 2.5 );
      _TyLife jvlInner( jvlArray, ejvtObject );
    }
  }
  EXPECT_EQ( StrOfMemOut( jos ), R"({"i":-5,"u":18446744073709551615,"l":-9223372036854775808,"b":200,"s":"x\"y","t":true,"n":null,"n2":null,"f2":false,"d":0.1,"a":[1,"two",null,true,false,2.5,{}]})" );
}

TEST( JsonWrite, ValueLifeMisuseThrows )
{
  _TyMemOut jos;
  _TyLife jvl( jos, ejvtArray );
  EXPECT_THROW( jvl.WriteNullValue( "key" ), std::exception ); // a key in an array.
  EXPECT_THROW( jvl.WriteValueType( ejvtNumber ), std::exception );
  _TyMemOut jos2;
  _TyLife jvl2( jos2, ejvtObject );
  EXPECT_THROW( jvl2.WriteNullValue(), std::exception ); // no key in an object.
  EXPECT_THROW( jvl2.WriteValue( "d", std::nan( "" ) ), std::exception );
  jvl.SetDontWritePostAmble();
  jvl2.SetDontWritePostAmble();
}

// A key given with its length is that many characters - not the rest of the text it is the start of.
TEST( JsonWrite, KeyOfALength )
{
  _TyMemOut jos;
  {
    _TyLife jvl( jos, ejvtObject );
    const std::string strValue( "v" );
    jvl.WriteStringValue( "keyXYZ", 3, strValue );
    jvl.WriteStringValue( "k\"XYZ", 2, "value-of-which-two", 2 );
  }
  EXPECT_EQ( StrOfMemOut( jos ), R"({"key":"v","k\"":"va"})" );
}

// ------------------------------------------------------------------------------------------------------------------
// Round trips of random values.

struct _RandomJson
{
  std::mt19937 m_rng{ 20261127 };
  std::string StrRandom( size_t _nMax )
  {
    static const char * const s_rgpsz[] = { "a", "Z", "0", " ", "\"", "\\", "/", "\n", "\r", "\t", "\b", "\f", "\x01", "\x1f", "\x7f", "\xC3\xA9", "\xE2\x82\xAC", "\xF0\x9F\x98\x80", "u", "{", "]" };
    std::string str;
    const size_t n = m_rng() % ( _nMax + 1 );
    for ( size_t i = 0; i < n; ++i )
      str += s_rgpsz[ m_rng() % std::size( s_rgpsz ) ];
    return str;
  }
  void Fill( _TyJv & _rjv, unsigned _nDepth )
  {
    switch ( m_rng() % ( _nDepth ? 9 : 7 ) )
    {
    case 0: _rjv.SetNullValue(); break;
    case 1: _rjv.SetBoolValue( true ); break;
    case 2: _rjv.SetBoolValue( false ); break;
    case 3: _rjv.SetValue( int64_t( ( uint64_t( m_rng() ) << 32 ) | m_rng() ) ); break;
    case 4: _rjv.SetValue( std::ldexp( double( m_rng() ) / 7.0, int( m_rng() % 200 ) - 100 ) ); break;
    case 5:
    case 6: _rjv.SetStringValue( StrRandom( ( m_rng() & 3 ) ? 10 : 200 ) ); break;
    case 7:
    {
      _rjv.SetValueType( ejvtObject );
      const size_t n = m_rng() % 6;
      for ( size_t i = 0; i < n; ++i )
        Fill( _rjv( StrRandom( 8 ).c_str() ), _nDepth - 1 );
    }
    break;
    case 8:
    {
      _rjv.SetValueType( ejvtArray );
      const size_t n = m_rng() % 6;
      for ( size_t i = 0; i < n; ++i )
        Fill( _rjv.AppendEl(), _nDepth - 1 );
    }
    break;
    }
  }
};

TEST( JsonRoundTrip, RandomValues )
{
  _RandomJson rj;
  _TyFormat jfsPretty = JfsPretty();
  _TyFormat jfsRaw = JfsPretty();
  jfsRaw.m_fEscapePrintableWhitespace = false;
  for ( int n = 0; n < 2000; ++n )
  {
    _TyJv jv;
    rj.Fill( jv, 5 );
    const std::string strCompact = StrWrite( jv );
    _TyJv jvCompact = JvParse( strCompact );
    ASSERT_TRUE( jvCompact == jv ) << strCompact;
    EXPECT_EQ( StrWrite( jvCompact ), strCompact );
    _TyJv jvPretty = JvParse( StrWrite( jv, &jfsPretty ) );
    ASSERT_TRUE( jvPretty == jv ) << StrWrite( jv, &jfsPretty );
    // Newlines and tabs as they are: the reader takes them.
    _TyJv jvRaw = JvParse( StrWrite( jv, &jfsRaw ) );
    ASSERT_TRUE( jvRaw == jv ) << StrWrite( jv, &jfsRaw );
  }
}

// ------------------------------------------------------------------------------------------------------------------
// The read cursor used directly.

// Some members read, the others skipped: whatever a skipped value holds - escapes, \u, nested aggregates.
TEST( JsonCursor, SkipValues )
{
  const std::string str = R"({"a":"\u0041\"x\\","b":[1,"\u00e9",{"c":"\u0041"}],"c":{"d":"\u0041","e":[[]]},"d":-1.5e3,"e":true,"f":null,"g":"last"})";
  _TyMemIn jis( str.c_str(), str.length() );
  _TyCursor jrc;
  jis.AttachReadCursor( jrc );
  ASSERT_TRUE( jrc.FMoveDown() );
  std::string strKeys;
  std::string strLast;
  for ( ; !jrc.FAtEndOfAggregate(); (void)jrc.FNextElement() )
  {
    _TyCursor::_tyStdStr strKey;
    EJsonValueType jvt;
    ASSERT_TRUE( jrc.FGetKeyCurrent( strKey, jvt ) );
    strKeys += strKey.c_str();
    if ( std::string( "g" ) == strKey.c_str() )
    {
      _TyCursor::_tyStdStr strValue;
      jrc.GetValue( strValue );
      strLast = strValue.c_str();
    }
  }
  EXPECT_EQ( strKeys, "abcdefg" );
  EXPECT_EQ( strLast, "last" );
}

// Skipping half-way through an aggregate the cursor went into.
TEST( JsonCursor, SkipFromWithin )
{
  const std::string str = R"([{"a":"\u0041","b":[1,2,3],"c":"z"},"after"])";
  _TyMemIn jis( str.c_str(), str.length() );
  _TyCursor jrc;
  jis.AttachReadCursor( jrc );
  ASSERT_TRUE( jrc.FMoveDown() );     // into the array: at the object.
  ASSERT_TRUE( jrc.FMoveDown() );     // into the object: at "a".
  ASSERT_TRUE( jrc.FNextElement() );  // at "b".
  ASSERT_TRUE( jrc.FMoveDown() );     // into [1,2,3]: at 1.
  int32_t i = 0;
  jrc.GetValue( i );
  EXPECT_EQ( i, 1 );
  ASSERT_TRUE( jrc.FMoveUp() );       // at "b".
  ASSERT_TRUE( jrc.FMoveUp() );       // at the object.
  ASSERT_TRUE( jrc.FNextElement() );  // the rest of the object skipped: at "after".
  _TyCursor::_tyStdStr strValue;
  jrc.GetValue( strValue );
  EXPECT_STREQ( strValue.c_str(), "after" );
  EXPECT_FALSE( jrc.FNextElement() );
}

// A value read twice is the same each time: reading does not take it from the cursor.
TEST( JsonCursor, GetValueTwice )
{
  const std::string str = R"(["a value long enough to be kept in a string object, not the buffer", "short"])";
  _TyMemIn jis( str.c_str(), str.length() );
  _TyCursor jrc;
  jis.AttachReadCursor( jrc );
  ASSERT_TRUE( jrc.FMoveDown() );
  for ( int n = 0; n < 2; ++n )
  {
    _TyCursor::_tyStdStr strValue;
    jrc.GetValue( strValue );
    EXPECT_STREQ( strValue.c_str(), "a value long enough to be kept in a string object, not the buffer" ) << n;
  }
  ASSERT_TRUE( jrc.FNextElement() );
  for ( int n = 0; n < 2; ++n )
  {
    _TyCursor::_tyStdStr strValue;
    jrc.GetValue( strValue );
    EXPECT_STREQ( strValue.c_str(), "short" ) << n;
  }
}

TEST( JsonCursor, Values )
{
  const std::string str = R"([true, 3000000000, -7, 2.5, "s"])";
  _TyMemIn jis( str.c_str(), str.length() );
  _TyCursor jrc;
  jis.AttachReadCursor( jrc );
  ASSERT_TRUE( jrc.FMoveDown() );
  bool f = false;
  jrc.GetValue( f );
  EXPECT_TRUE( f );
  ASSERT_TRUE( jrc.FNextElement() );
  uint32_t ui = 0;
  jrc.GetValue( ui );
  EXPECT_EQ( ui, 3000000000u );
  int16_t ss = 0;
  EXPECT_THROW( jrc.GetValue( ss ), std::exception );
  ASSERT_TRUE( jrc.FNextElement() );
  int64_t sl = 0;
  jrc.GetValue( sl );
  EXPECT_EQ( sl, -7 );
  ASSERT_TRUE( jrc.FNextElement() );
  double dbl = 0;
  jrc.GetValue( dbl );
  EXPECT_EQ( dbl, 2.5 );
  ASSERT_TRUE( jrc.FNextElement() );
  EXPECT_THROW( jrc.GetValue( dbl ), std::exception );
  EXPECT_FALSE( jrc.FNextElement() );
}

// ------------------------------------------------------------------------------------------------------------------
// Filters.

TEST( JsonFilter, Read )
{
  const std::string str = R"({"keep":1,"_skip":"\u0041","o":{"_x":[1,{"y":"\u00e9"}],"k":2},"a":[{"_z":0,"w":3}]})";
  auto lambdaFilter = []( _TyCursor & _rjrc, const _TyJv & _rjvContainer ) -> bool
  {
    if ( !_rjvContainer.FIsObject() )
      return true;
    _TyCursor::_tyStdStr strKey;
    EJsonValueType jvt;
    _rjrc.FGetKeyCurrent( strKey, jvt );
    return '_' != strKey.c_str()[ 0 ];
  };
  _TyJv jv;
  jv.FromString( str.c_str(), str.length(), lambdaFilter );
  EXPECT_EQ( StrWrite( jv ), R"({"a":[{"w":3}],"keep":1,"o":{"k":2}})" );
}

TEST( JsonFilter, Write )
{
  _TyJv jv = JvParse( R"({"a":1,"_b":2,"c":[1,2,3,4],"d":{"_e":1,"f":2}})" );
  auto lambdaFilter = []( const _TyJv & _rjvContainer, const JsoIterator< char, true > & _rit ) -> bool
  {
    if ( _rjvContainer.FIsObject() )
      return '_' != _rit.GetKeyValue().first.c_str()[ 0 ];
    return 0 != ( ( _rit->StrGet().c_str()[ 0 ] - '0' ) % 2 ); // the odd numbers.
  };
  std::string strOut;
  jv.ToString( strOut, lambdaFilter );
  EXPECT_EQ( strOut, R"({"a":1,"c":[1,3],"d":{"f":2}})" );
}

// ------------------------------------------------------------------------------------------------------------------
// Files.

TEST( JsonFile, WriteAndRead )
{
  const std::string strPath = ( std::filesystem::path( testing::TempDir() ) / "bienutil_jsontest.json" ).string();
  const _TyJv jv = JvParse( R"({"a":[1,2.5,"\u00e9\n"],"b":{"c":null},"long":")" + std::string( 3000, 'x' ) + R"("})" );
  {
    JsonFileOutputStream< _TyTraits > jos;
    jos.Open( strPath.c_str() );
    const _TyFormat jfs = JfsPretty();
    JsonValueLife< JsonFileOutputStream< _TyTraits > > jvl( jos, jv.JvtGetValueType(), &jfs );
    jv.ToJSONStream( jvl );
  }
  {
    JsonFileInputStream< _TyTraits > jis;
    jis.Open( strPath.c_str() );
    JsonReadCursor< JsonFileInputStream< _TyTraits > > jrc;
    jis.AttachReadCursor( jrc );
    _TyJv jvRead;
    jvRead.FromJSONStream( jrc );
    EXPECT_TRUE( jvRead == jv );
  }
  std::filesystem::remove( strPath );
}

// ------------------------------------------------------------------------------------------------------------------
// The other character types.

TEST( JsonChars, Char8 )
{
  typedef JsoValue< char8_t > _TyJv8;
  const std::u8string str = u8R"({"k":"caf\u00e9 \ud83d\ude00","n":[1,true]})";
  _TyJv8 jv;
  jv.FromString( str.c_str(), str.length() );
  EXPECT_EQ( std::u8string( jv[ u8"k" ].StrGet().c_str() ), std::u8string( u8"caf\u00e9 \U0001F600" ) );
  std::u8string strOut;
  jv.ToString( strOut );
  EXPECT_EQ( strOut, std::u8string( u8"{\"k\":\"caf\u00e9 \U0001F600\",\"n\":[1,true]}" ) );
}

TEST( JsonChars, Char16 )
{
  typedef JsoValue< char16_t > _TyJv16;
  const std::u16string str = uR"({"k":"caf\u00e9 \ud83d\ude00","n":[1,null]})";
  _TyJv16 jv;
  jv.FromString( str.c_str(), str.length() );
  EXPECT_EQ( std::u16string( jv[ u"k" ].StrGet().c_str() ), std::u16string( u"caf\u00e9 \U0001F600" ) );
  std::u16string strOut;
  jv.ToString( strOut );
  EXPECT_EQ( strOut, std::u16string( u"{\"k\":\"caf\u00e9 \U0001F600\",\"n\":[1,null]}" ) );
}

TEST( JsonChars, Char32 )
{
  typedef JsoValue< char32_t > _TyJv32;
  const std::u32string str = UR"({"k":"\ud83d\ude00","n":-2})";
  _TyJv32 jv;
  jv.FromString( str.c_str(), str.length() );
  EXPECT_EQ( std::u32string( jv[ U"k" ].StrGet().c_str() ), std::u32string( U"\U0001F600" ) );
  std::u32string strOut;
  jv.ToString( strOut );
  EXPECT_EQ( strOut, std::u32string( U"{\"k\":\"\U0001F600\",\"n\":-2}" ) );
}

} // namespace ns_bienutiltest
