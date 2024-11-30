#pragma once

//          Copyright David Lawrence Bien 1997 - 2024.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// ios_util.h
// Smart pointer wrapper for Core Foundation reference types.

#include "bienutil.h"
#include "_strutil.h"
#include <CoreFoundation/CoreFoundation.h>

__BIENUTIL_BEGIN_NAMESPACE

// CCFPtr:
// Smart pointer wrapper for Core Foundation reference types.
template < typename t_TyCFRef > struct _CCFPtr_CCFDeleter
{
  void operator()( t_TyCFRef _ref )
  {
    if ( _ref )
      CFRelease( _ref );
  }
};
template < typename t_TyCFRef > using CCFPtr = std::unique_ptr< typename std::remove_pointer< t_TyCFRef >::type, _CCFPtr_CCFDeleter< t_TyCFRef > >;

// Get a string of various flavors from a CFStringRef.
inline std::optional< std::string >
GetStringFromCFStringUTF8( CFStringRef _cfstr )
{
  if ( !_cfstr )
    return std::nullopt;
  const char * pszPtr = CFStringGetCStringPtr( _cfstr, kCFStringEncodingUTF8 );
  if ( pszPtr )
    return std::string( pszPtr );
  CFIndex nLength = CFStringGetLength( _cfstr );
  CFIndex nMaxSize = CFStringGetMaximumSizeForEncoding( nLength, kCFStringEncodingUTF8 ) + 1;
  std::vector< char > rgch( nMaxSize );
  if ( CFStringGetCString( _cfstr, rgch.data(), nMaxSize, kCFStringEncodingUTF8 ) )
    return std::string( rgch.data() );
  return std::nullopt;
}
inline std::optional< std::u16string >
GetStringFromCFStringUTF16( CFStringRef _cfstr )
{
  if ( !_cfstr )
    return std::nullopt;
  const UniChar * pwszPtr = CFStringGetCharactersPtr( _cfstr );
  static_assert( sizeof( UniChar ) == sizeof( char16_t ), "UniChar is not char16_t." );
  if ( pwszPtr )
    return std::u16string( reinterpret_cast< const char16_t* >( pwszPtr ), CFStringGetLength( _cfstr ) );
  CFIndex nLength = CFStringGetLength( _cfstr );
  std::vector< UniChar > rgwch( nLength );
  CFStringGetCharacters( _cfstr, CFRangeMake( 0, nLength ), rgwch.data() );
  return std::u16string( reinterpret_cast< const char16_t* >( rgwch.data() ), nLength );
}
inline std::optional< std::u32string >
GetStringFromCFStringUTF32( CFStringRef _cfstr )
{
  if ( auto optStr16 = GetStringFromCFStringUTF16( _cfstr ) )
    return StrConvertString< char32_t >( optStr16->data(), optStr16->length() );
  return std::nullopt;
}

__BIENUTIL_END_NAMESPACE