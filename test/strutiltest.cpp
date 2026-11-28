//          Copyright David Lawrence Bien 1997 - 2026.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// strutiltest.cpp
// Unit tests of _strutil.h and utfconvert.h: the conversions between UTF-8, UTF-16 and UTF-32 which xmlp converts
//  every document with, what is made of what is no UTF, the detection of an encoding by its byte order mark or by an
//  XML document's first character, the C string functions, printf to a string, the reading of numbers.
// What is right is computed here by an encoder written for the test - each code point of Unicode is checked.
// dbien
// 27NOV2026

#include "bienutiltest.h"
#include <random>

std::string g_strProgramName;

namespace ns_bienutiltest
{
__BIENUTIL_USING_NAMESPACE

// The test's own encoders: the standard's tables written out, not bienutil's code.
static std::u8string StrU8Of( char32_t _uc )
{
  std::u8string str;
  if ( _uc < 0x80 )
    str += char8_t( _uc );
  else if ( _uc < 0x800 )
  {
    str += char8_t( 0xC0 | ( _uc >> 6 ) );
    str += char8_t( 0x80 | ( _uc & 0x3F ) );
  }
  else if ( _uc < 0x10000 )
  {
    str += char8_t( 0xE0 | ( _uc >> 12 ) );
    str += char8_t( 0x80 | ( ( _uc >> 6 ) & 0x3F ) );
    str += char8_t( 0x80 | ( _uc & 0x3F ) );
  }
  else
  {
    str += char8_t( 0xF0 | ( _uc >> 18 ) );
    str += char8_t( 0x80 | ( ( _uc >> 12 ) & 0x3F ) );
    str += char8_t( 0x80 | ( ( _uc >> 6 ) & 0x3F ) );
    str += char8_t( 0x80 | ( _uc & 0x3F ) );
  }
  return str;
}
static std::u16string StrU16Of( char32_t _uc )
{
  std::u16string str;
  if ( _uc < 0x10000 )
    str += char16_t( _uc );
  else
  {
    str += char16_t( 0xD800 + ( ( _uc - 0x10000 ) >> 10 ) );
    str += char16_t( 0xDC00 + ( ( _uc - 0x10000 ) & 0x3FF ) );
  }
  return str;
}
static std::u8string U8( const char * _psz )
{
  return std::u8string( (const char8_t *)_psz );
}
template < class t_TyStrDest, class t_TyStrSrc >
static t_TyStrDest StrConv( const t_TyStrSrc & _rstr, char16_t _chReplace = vkutf16ReplacementCharDefault )
{
  t_TyStrDest str;
  ConvertString( str, _rstr.data(), _rstr.length(), _chReplace );
  return str;
}

// ------------------------------------------------------------------------------------------------------------------
// UTF conversions.

// Every code point of Unicode - every one which is no surrogate - each way between the three encodings, one at a time
//  and all in one string.
TEST( UtfConvert, EveryCodePoint )
{
  std::u8string str8All;
  std::u16string str16All;
  std::u32string str32All;
  for ( char32_t uc = 1; uc <= 0x10FFFF; ++uc )
  {
    if ( ( uc >= 0xD800 ) && ( uc <= 0xDFFF ) )
      continue;
    const std::u32string str32( 1, uc );
    const std::u8string str8 = StrU8Of( uc );
    const std::u16string str16 = StrU16Of( uc );
    // Every 97th one alone, so this takes a moment - and all of them together below.
    if ( !( uc % 97 ) || ( uc < 0x900 ) || ( ( uc >= 0xFFF0 ) && ( uc <= 0x10010 ) ) || ( uc > 0x10FFF0 ) )
    {
      ASSERT_EQ( StrConv< std::u8string >( str32 ), str8 ) << std::hex << (uint32_t)uc;
      ASSERT_EQ( StrConv< std::u16string >( str32 ), str16 ) << std::hex << (uint32_t)uc;
      ASSERT_EQ( StrConv< std::u32string >( str8 ), str32 ) << std::hex << (uint32_t)uc;
      ASSERT_EQ( StrConv< std::u16string >( str8 ), str16 ) << std::hex << (uint32_t)uc;
      ASSERT_EQ( StrConv< std::u8string >( str16 ), str8 ) << std::hex << (uint32_t)uc;
      ASSERT_EQ( StrConv< std::u32string >( str16 ), str32 ) << std::hex << (uint32_t)uc;
    }
    str8All += str8;
    str16All += str16;
    str32All += str32;
  }
  // Strings of megabytes: converted in the string itself, not on the stack.
  EXPECT_TRUE( StrConv< std::u8string >( str32All ) == str8All );
  EXPECT_TRUE( StrConv< std::u16string >( str32All ) == str16All );
  EXPECT_TRUE( StrConv< std::u32string >( str8All ) == str32All );
  EXPECT_TRUE( StrConv< std::u16string >( str8All ) == str16All );
  EXPECT_TRUE( StrConv< std::u8string >( str16All ) == str8All );
  EXPECT_TRUE( StrConv< std::u32string >( str16All ) == str32All );
}

TEST( UtfConvert, OfCharAndWchar )
{
  std::string str( "caf\xC3\xA9 \xE2\x82\xAC \xF0\x9F\x98\x80" );
  std::wstring wstr;
  ConvertString( wstr, str.data(), str.length() );
  EXPECT_EQ( wstr, std::wstring( L"café € \U0001F600" ) );
  std::string strBack;
  ConvertString( strBack, wstr.data(), wstr.length() );
  EXPECT_EQ( strBack, str );
  EXPECT_EQ( StrConvertString< char16_t >( str ), std::u16string( u"café € \U0001F600" ) );
  EXPECT_EQ( StrConvertString< char >( u"xé" ), std::string( "x\xC3\xA9" ) );
  EXPECT_EQ( StrConvertString< char8_t >( str.c_str() ), U8( str.c_str() ) ); // the same size: copied.
  std::u32string str32;
  ConvertString( str32, std::string_view( str ) ); // of a view.
  EXPECT_EQ( str32, std::u32string( U"café € \U0001F600" ) );
}

TEST( UtfConvert, Empty )
{
  EXPECT_EQ( StrConv< std::u16string >( std::u8string() ), std::u16string() );
  EXPECT_EQ( StrConv< std::u8string >( std::u32string() ), std::u8string() );
  EXPECT_EQ( StrConvertString< char32_t >( std::string_view() ), std::u32string() );
  std::u16string str16Buf;
  EXPECT_TRUE( StrViewConvertString( (const char8_t *)nullptr, 0, str16Buf ).empty() );
  char8_t rgch[ 4 ];
  char8_t * pchDest = rgch;
  const char8_t * pchSrc = u8"";
  EXPECT_EQ( PCConvertString( pchSrc, 0, pchDest, 4 ), pchSrc ); // same size, nothing to copy.
}

// What is no UTF-8 is replaced as Unicode recommends ( "U+FFFD Substitution of Maximal Subparts", chapter 3 ): one
//  replacement character for each maximal subpart of a sequence - the longest start of a sequence which could be
//  completed - else for each byte. And a sequence cut short by the end is the lead byte and the trail bytes which
//  follow it, no more.
TEST( UtfConvert, InvalidUtf8IsReplaced )
{
  const std::u16string kstrR( 1, u'�' );
  struct { const char * m_psz; std::u16string m_str; } rg[] = {
    { "\x80", kstrR },                                // a trail byte alone.
    { "a\xBF" "b", u"a" + kstrR + u"b" },
    { "\xC0\x80", kstrR + kstrR },                    // an overlong NUL: C0 is never a lead byte.
    { "\xC1\xBF", kstrR + kstrR },
    { "\xE0\x80\x80", kstrR + kstrR + kstrR },        // overlong: E0 is followed by A0 to BF.
    { "\xED\xA0\x80", kstrR + kstrR + kstrR },        // a surrogate: ED is followed by 80 to 9F.
    { "\xF4\x90\x80\x80", kstrR + kstrR + kstrR + kstrR }, // above U+10FFFF.
    { "\xF5\x80\x80\x80", kstrR + kstrR + kstrR + kstrR }, // F5 is never a lead byte.
    { "\xE2\x82" "\xE2\x82\xAC", kstrR + u"\u20AC" },   // a maximal subpart, then a whole one.
    { "\xFF", kstrR },
    { "\xE2\x82", kstrR },                            // cut short by the end.
    { "\xE2" "A", kstrR + u"A" },                     // cut short by the end, then a character.
    { "x\xF0\x9F\x98", u"x" + kstrR },
    { "\xF0\x9F" "AB", kstrR + u"AB" },
    { "\xE2\x82" "A\xE2\x82\xAC", kstrR + u"A€" },
  };
  for ( auto & r : rg )
  {
    const std::u8string str8 = U8( r.m_psz );
    EXPECT_EQ( StrConv< std::u16string >( str8 ), r.m_str ) << r.m_psz;
    std::u32string str32Expect;
    ConvertString( str32Expect, r.m_str.data(), r.m_str.length() );
    EXPECT_EQ( StrConv< std::u32string >( str8 ), str32Expect ) << r.m_psz;
  }
}

TEST( UtfConvert, InvalidUtf16IsReplaced )
{
  const char32_t kchR = 0xFFFD;
  struct { std::u16string m_str; std::u32string m_str32; } rg[] = {
    { std::u16string( 1, char16_t( 0xD800 ) ), std::u32string( 1, kchR ) },                     // a high surrogate at the end.
    { std::u16string( { char16_t( 0xD800 ), u'A' } ), std::u32string( { kchR, U'A' } ) },       // not followed by a low one.
    { std::u16string( 1, char16_t( 0xDC00 ) ), std::u32string( 1, kchR ) },                     // a low surrogate alone.
    { std::u16string( { u'A', char16_t( 0xDFFF ), u'B' } ), std::u32string( { U'A', kchR, U'B' } ) },
    { std::u16string( { char16_t( 0xDC00 ), char16_t( 0xD800 ) } ), std::u32string( { kchR, kchR } ) }, // the wrong way round.
  };
  for ( auto & r : rg )
  {
    EXPECT_EQ( StrConv< std::u32string >( r.m_str ), r.m_str32 );
    EXPECT_EQ( StrConv< std::u8string >( r.m_str ), StrConv< std::u8string >( r.m_str32 ) );
  }
}

TEST( UtfConvert, InvalidUtf32IsReplaced )
{
  const std::u32string str32( { U'A', char32_t( 0xD800 ), char32_t( 0x110000 ), char32_t( 0xFFFFFFFF ), U'B' } );
  EXPECT_EQ( StrConv< std::u16string >( str32 ), std::u16string( u"A���B" ) );
  EXPECT_EQ( StrConv< std::u8string >( str32 ), U8( "A\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD" "B" ) );
}

// Given no replacement character, what is no UTF throws.
TEST( UtfConvert, InvalidThrowsWithoutAReplacement )
{
  for ( const char * psz : { "\x80", "\xC0\x80", "\xED\xA0\x80", "\xE2\x82", "a\xFF" } )
    EXPECT_THROW( ( StrConv< std::u16string >( U8( psz ), vkutf16ReplacementCharError ) ), std::exception ) << psz;
  EXPECT_THROW( ( StrConv< std::u32string >( std::u16string( 1, char16_t( 0xDC00 ) ), vkutf16ReplacementCharError ) ), std::exception );
  EXPECT_THROW( ( StrConv< std::u8string >( std::u16string( 1, char16_t( 0xD800 ) ), vkutf16ReplacementCharError ) ), std::exception );
  EXPECT_THROW( ( StrConv< std::u16string >( std::u32string( 1, char32_t( 0x110000 ) ), vkutf16ReplacementCharError ) ), std::exception );
  EXPECT_EQ( ( StrConv< std::u16string >( U8( "ok\xE2\x82\xAC" ), vkutf16ReplacementCharError ) ), std::u16string( u"ok€" ) );
}

// Random bytes: whatever they are, converting them neither crashes nor reads outside them ( for the sanitizers ),
//  and what comes of them is UTF.
TEST( UtfConvert, RandomBytes )
{
  std::mt19937 rng( 271127 );
  for ( int n = 0; n < 20000; ++n )
  {
    std::u8string str8( rng() % 12, u8'\0' );
    for ( char8_t & rch : str8 )
      rch = char8_t( ( rng() & 1 ) ? ( 0x80 | ( rng() & 0x7F ) ) : ( rng() & 0xFF ) );
    const std::u32string str32 = StrConv< std::u32string >( str8 );
    for ( char32_t uc : str32 )
      ASSERT_TRUE( FIsValidUTF32( uc ) );
    // What is UTF-8 already comes back as it was.
    const std::u8string str8Again = StrConv< std::u8string >( str32 );
    EXPECT_EQ( StrConv< std::u32string >( str8Again ), str32 );
    std::u16string str16( rng() % 6, u'\0' );
    for ( char16_t & rch : str16 )
      rch = char16_t( ( rng() & 1 ) ? ( 0xD800 + ( rng() & 0x7FF ) ) : rng() );
    for ( char32_t uc : StrConv< std::u32string >( str16 ) )
      ASSERT_TRUE( FIsValidUTF32( uc ) ) << std::hex << (uint32_t)uc;
  }
}

// Converted into a buffer of a size: as much as fits, whole characters only.
TEST( UtfConvert, IntoABufferOfASize )
{
  const std::u8string str8 = U8( "ab\xE2\x82\xAC" "c" );
  char16_t rgch[ 3 ];
  char16_t * pchDest = rgch;
  const char8_t * pchEnd = PCConvertString( str8.data(), str8.length(), pchDest, 3 );
  ASSERT_TRUE( !!pchEnd );
  EXPECT_EQ( pchDest - rgch, 2 ); // room for 2 more units only while 2 remain: "ab".
  EXPECT_EQ( pchEnd - str8.data(), 2 );
  char16_t rgchSmall[ 1 ];
  pchDest = rgchSmall;
  EXPECT_EQ( PCConvertString( str8.data(), str8.length(), pchDest, 1 ), nullptr ); // never room for a pair.
}

TEST( UtfConvert, Functor )
{
  const std::u8string str8 = U8( "x\xF0\x9F\x98\x80" );
  std::u16string str16;
  EXPECT_TRUE( FWriteUTFFunctor< char16_t >( str8.data(), str8.length(), [ & ]( const char16_t * _pch, size_t _n ) { str16.append( _pch, _n ); return true; } ) );
  EXPECT_EQ( str16, std::u16string( u"x\U0001F600" ) );
  EXPECT_FALSE( FWriteUTFFunctor< char16_t >( str8.data(), str8.length(), []( const char16_t *, size_t ) { return false; } ) );
}

// ------------------------------------------------------------------------------------------------------------------
// Encodings.

TEST( Encoding, ByteOrderMark )
{
  struct { const char * m_pby; size_t m_n; EFileCharacterEncoding m_efce; uint64_t m_nLen; } rg[] = {
    { "\xEF\xBB\xBF<", 4, efceUTF8, 3 },
    { "\xFF\xFE<\0", 4, efceUTF16LE, 2 },
    { "\xFE\xFF\0<", 4, efceUTF16BE, 2 },
    { "\xFF\xFE\0\0", 4, efceUTF32LE, 4 },
    { "\0\0\xFE\xFF", 4, efceUTF32BE, 4 },
    { "<?xm", 4, efceFileCharacterEncodingCount, 0 },
  };
  for ( auto & r : rg )
  {
    uint64_t nLen = r.m_n;
    EXPECT_EQ( GetCharacterEncodingFromBOM( (uint8_t *)r.m_pby, nLen ), r.m_efce );
    EXPECT_EQ( nLen, r.m_nLen );
  }
  for ( EFileCharacterEncoding efce : { efceUTF8, efceUTF16BE, efceUTF16LE, efceUTF32BE, efceUTF32LE } )
  {
    std::string strBOM = StrGetBOMForEncoding( efce );
    strBOM.resize( 4, '<' );
    if ( efceUTF16LE == efce )
      strBOM[ 3 ] = 0; // "<\0" - else FF FE 3C 3C.
    uint64_t nLen = 4;
    EXPECT_EQ( GetCharacterEncodingFromBOM( (uint8_t *)strBOM.data(), nLen ), efce ) << PszCharacterEncodingShort( efce );
  }
  uint64_t nShort = 2;
  EXPECT_THROW( GetCharacterEncodingFromBOM( (uint8_t *)"ab", nShort ), std::exception );
}

TEST( Encoding, XmlFirstCharacter )
{
  struct { const char * m_pby; EFileCharacterEncoding m_efce; } rg[] = {
    { "<?xm", efceUTF8 },
    { " <a>", efceUTF8 },
    { "\0<\0?", efceUTF16BE },
    { "<\0?\0", efceUTF16LE },
    { "\n\0<\0", efceUTF16LE },
    { "\0\0\0<", efceUTF32BE },
    { "<\0\0\0", efceUTF32LE },
    { "abcd", efceFileCharacterEncodingCount },
  };
  for ( auto & r : rg )
    EXPECT_EQ( DetectEncodingXmlFile( (uint8_t *)r.m_pby, 4 ), r.m_efce ) << int( r.m_efce );
}

TEST( Encoding, OfTypesAndOfThisMachine )
{
  EXPECT_EQ( ( GetCharacterEncoding< char, std::false_type >() ), efceUTF8 );
  EXPECT_EQ( ( GetCharacterEncoding< char16_t, std::false_type >() ), vkfIsLittleEndian ? efceUTF16LE : efceUTF16BE );
  EXPECT_EQ( ( GetCharacterEncoding< char16_t, std::true_type >() ), vkfIsLittleEndian ? efceUTF16BE : efceUTF16LE );
  EXPECT_EQ( ( GetCharacterEncoding< char32_t, std::false_type >() ), vkfIsLittleEndian ? efceUTF32LE : efceUTF32BE );
  EXPECT_EQ( ( GetCharacterEncoding< char32_t, std::true_type >() ), vkfIsLittleEndian ? efceUTF32BE : efceUTF32LE );
  EXPECT_EQ( GetEncodingThisMachine( efceUTF16BE ), vkfIsLittleEndian ? efceUTF16LE : efceUTF16BE );
  EXPECT_EQ( GetEncodingThisMachine( efceUTF32LE ), vkfIsLittleEndian ? efceUTF32LE : efceUTF32BE );
  EXPECT_STREQ( PszCharacterEncodingShort( efceUTF16LE ), "UTF16LE" );
  EXPECT_EQ( SvCharacterEncodingName< char16_t >( efceUTF16LE ), std::u16string_view( u"UTF-16" ) );
  EXPECT_EQ( SvCharacterEncodingName< char >( efceUTF8 ), std::string_view( "UTF-8" ) );
}

TEST( Encoding, SwitchEndian )
{
  char16_t ch16 = 0x1234;
  SwitchEndian( ch16 );
  EXPECT_EQ( ch16, 0x3412 );
  char32_t ch32 = 0x12345678;
  SwitchEndian( ch32 );
  EXPECT_EQ( ch32, char32_t( 0x78563412 ) );
  uint64_t u64 = 0x0102030405060708ull;
  SwitchEndian( u64 );
  EXPECT_EQ( u64, 0x0807060504030201ull );
  std::u16string str( u"AB" );
  SwitchEndian( str.data(), str.data() + str.length() );
  EXPECT_EQ( str[ 0 ], char16_t( 0x4100 ) );
}

// ------------------------------------------------------------------------------------------------------------------
// C strings.

TEST( CStrings, Lengths )
{
  EXPECT_EQ( StrNLen( "abc" ), 3u );
  EXPECT_EQ( StrNLen( "abc", 2 ), 2u );
  EXPECT_EQ( StrNLen( "abc", 10 ), 3u );
  EXPECT_EQ( StrNLen( "", 10 ), 0u );
  EXPECT_EQ( StrNLen( (const char *)nullptr, 10 ), 0u );
  EXPECT_EQ( StrNLen( u"éé" ), 2u );
  EXPECT_EQ( StrNLen( U"x", 0 ), 0u );
}

TEST( CStrings, Spans )
{
  EXPECT_EQ( StrSpn( "aabxa", 5, "ab" ), 3u );
  EXPECT_EQ( StrSpn( "xa", 2, "ab" ), 0u );
  EXPECT_EQ( StrSpn( "ab", 2, "ab" ), 2u );
  EXPECT_EQ( StrCSpn( "abc,d", "," ), 3u );
  EXPECT_EQ( StrCSpn( "abc", "," ), 3u );
  EXPECT_EQ( StrCSpn( "ab\x01" "c", '\x01', '\x20', "\"" ), 2u );
  EXPECT_EQ( StrCSpn( "ab\"c", '\x01', '\x20', "\"" ), 2u );
  const char * psz = "abc \t\n";
  EXPECT_EQ( StrRSpn( psz, psz + 6, " \t\n" ), 3u );
  EXPECT_EQ( StrRSpn( psz, psz + 3, " \t\n" ), 0u );
  EXPECT_EQ( StrRSpn( psz + 3, psz + 6, " \t\n" ), 3u );
  EXPECT_EQ( StrRSpn( psz, psz, " " ), 0u );
}

TEST( CStrings, Compare )
{
  EXPECT_TRUE( ICompareStr( "abc", "abd" ) < 0 );
  EXPECT_TRUE( ICompareStr( "abc", "abc" ) == 0 );
  EXPECT_TRUE( ICompareStr( "abcd", "abc" ) > 0 );
  EXPECT_TRUE( ICompareStr( "", "a" ) < 0 );
}

TEST( CStrings, AsciiConversion )
{
  char rgc[ 8 ];
  ConvertAsciiString( rgc, 8, u"abc" ); // into char: each character below 128.
  EXPECT_STREQ( rgc, "abc" );
  char16_t rgch[ 8 ];
  ConvertAsciiString( rgch, 8, "xyz" );
  EXPECT_EQ( std::u16string( rgch ), std::u16string( u"xyz" ) );
  ConvertAsciiString( rgc, 3, u"abcdef" ); // cut to the buffer.
  EXPECT_STREQ( rgc, "ab" );
  EXPECT_THROW( ConvertAsciiString( rgch, 8, "caf\xC3\xA9" ), std::exception ); // a char of 128 or more, negative.
  EXPECT_THROW( ConvertAsciiString( rgc, 8, u"é" ), std::exception );
  constexpr auto kstr = str_array_cast< char16_t >( "true" );
  EXPECT_EQ( std::u16string( kstr.c_str() ), std::u16string( u"true" ) );
  EXPECT_EQ( kstr.size(), 4u );
}

TEST( CStrings, Unescape )
{
  EXPECT_EQ( StrUnescapeStr( std::string( "a\\nb\\t\\\"\\\\\\q" ) ), std::string( "a\nb\t\"\\q" ) );
  EXPECT_EQ( StrUnescapeStr( std::string() ), std::string() );
  EXPECT_EQ( StrUnescapeStr( std::string( "end\\" ) ), std::string( "end\\" ) );
  EXPECT_EQ( StrProcessJsonTarget( "\"a\\nb\"" ), std::string( "a\nb" ) );
  EXPECT_EQ( StrProcessJsonTarget( "" ), std::string() );
  EXPECT_EQ( StrProcessJsonTarget( "\"" ), std::string() );
  EXPECT_EQ( StrPercentDecode( "a%20b%2Fc" ), std::string( "a b/c" ) );
  EXPECT_EQ( StrPercentDecode( "%41" ), std::string( "A" ) );
}

// ------------------------------------------------------------------------------------------------------------------
// printf to a string.

TEST( Printf, Char )
{
  std::string str;
  PrintfStdStr( str, "%s-%d-%.2f", "x", 42, 1.5 );
  EXPECT_EQ( str, "x-42-1.50" );
  PrintfStdStr( str, "%s", "" );
  EXPECT_EQ( str, "" );
  const std::string strLong( 100000, 'q' );
  PrintfStdStr( str, "[%s]", strLong.c_str() );
  EXPECT_EQ( str, "[" + strLong + "]" );
  std::u8string str8;
  PrintfStdStr( str8, u8"%d", 7 );
  EXPECT_EQ( str8, std::u8string( u8"7" ) );
  EXPECT_TRUE( FPrintfStdStrNoThrow( str, "%d", 3 ) );
  EXPECT_EQ( str, "3" );
}

TEST( Printf, Wide )
{
  std::wstring wstr;
  PrintfStdStr( wstr, L"%ls-%d", L"x", 42 );
  EXPECT_EQ( wstr, std::wstring( L"x-42" ) );
  // Longer than each buffer vswprintf() is tried with: 256, 8192, then the string itself.
  for ( size_t n : { 255, 256, 300, 8191, 8192, 9000, 100000 } )
  {
    const std::wstring wstrLong( n, L'w' );
    PrintfStdStr( wstr, L"<%ls>", wstrLong.c_str() );
    EXPECT_EQ( wstr, L"<" + wstrLong + L">" ) << n;
  }
}

// ------------------------------------------------------------------------------------------------------------------
// Reading numbers.

TEST( ReadNumber, Decimal )
{
  uint32_t u = 7;
  EXPECT_EQ( IReadPositiveNum( "12345", 5, u, false ), 0 );
  EXPECT_EQ( u, 12345u );
  EXPECT_EQ( IReadPositiveNum( "12ab", 0, u, false ), 0 ); // what follows the digits is not read.
  EXPECT_EQ( u, 12u );
  EXPECT_EQ( IReadPositiveNum( "4294967295", 10, u, false ), 0 );
  EXPECT_EQ( u, 4294967295u );
  EXPECT_EQ( IReadPositiveNum( "x1", 2, u, false ), -2 );
  EXPECT_EQ( IReadPositiveNum( "", 0, u, false ), -1 );
  EXPECT_THROW( IReadPositiveNum( "x", 1, u, true ), std::exception );
}

// Overflow is refused - every way it happens.
TEST( ReadNumber, DecimalOverflow )
{
  uint32_t u;
  EXPECT_EQ( IReadPositiveNum( "4294967296", 10, u, false ), -1 );
  EXPECT_EQ( IReadPositiveNum( "42949672950", 11, u, false ), -1 );
  uint8_t by;
  EXPECT_EQ( IReadPositiveNum( "255", 3, by, false ), 0 );
  EXPECT_EQ( by, 255 );
  EXPECT_EQ( IReadPositiveNum( "256", 3, by, false ), -1 );
  EXPECT_EQ( IReadPositiveNum( "510", 3, by, false ), -1 ); // 51 * 10 wraps to 254 - more than 51.
  int8_t sby;
  EXPECT_EQ( IReadPositiveNum( "127", 3, sby, false ), 0 );
  EXPECT_EQ( IReadPositiveNum( "128", 3, sby, false ), -1 );
  int64_t i64;
  EXPECT_EQ( IReadPositiveNum( "9223372036854775807", 19, i64, false ), 0 );
  EXPECT_EQ( i64, INT64_MAX );
  EXPECT_EQ( IReadPositiveNum( "9223372036854775808", 19, i64, false ), -1 );
}

// With a radix: as xmlp reads a character reference.
TEST( ReadNumber, Radix )
{
  uint32_t u;
  EXPECT_EQ( IReadPositiveNum( 16, "1F600", 5, u, uint32_t( 0x10FFFF ), false ), 0 );
  EXPECT_EQ( u, 0x1F600u );
  EXPECT_EQ( IReadPositiveNum( 16, "aBcD", 4, u, uint32_t( 0x10FFFF ), false ), 0 );
  EXPECT_EQ( u, 0xABCDu );
  EXPECT_EQ( IReadPositiveNum( 10, "65", 2, u, uint32_t( 0x10FFFF ), false ), 0 );
  EXPECT_EQ( u, 65u );
  EXPECT_EQ( IReadPositiveNum( 36, "zZ", 2, u, uint32_t( 0xFFFF ), false ), 0 );
  EXPECT_EQ( u, 35u * 36 + 35 );
  EXPECT_EQ( IReadPositiveNum( 16, "110000", 6, u, uint32_t( 0x10FFFF ), false ), -1 ); // above the maximum.
  EXPECT_EQ( IReadPositiveNum( 10, "1A", 2, u, uint32_t( 0x10FFFF ), false ), -1 );
  EXPECT_EQ( IReadPositiveNum( 16, "g", 1, u, uint32_t( 0x10FFFF ), false ), -1 );
  // The characters between the digits and the letters are no digits.
  for ( const char * psz : { "1:", "1@", "1[", "1`", "1{", "1/", "1 " } )
    EXPECT_EQ( IReadPositiveNum( 16, psz, 2, u, uint32_t( 0xFFFFFFFF ), false ), -1 ) << psz;
  EXPECT_EQ( IReadPositiveNum( 16, "FFFFFFFF", 8, u, uint32_t( 0xFFFFFFFF ), false ), 0 );
  EXPECT_EQ( IReadPositiveNum( 16, "100000000", 9, u, uint32_t( 0xFFFFFFFF ), false ), -1 ); // more than 32 bits.
  EXPECT_EQ( IReadPositiveNum( 16, u"2603", 4, u, uint32_t( 0x10FFFF ), false ), 0 );
  EXPECT_EQ( u, 0x2603u );
}

} // namespace ns_bienutiltest
