#pragma once

//          Copyright David Lawrence Bien 1997 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// icu_strutil.h
// String utilities based on the ICU library.
// dbien: split off of _strutil.h on 20JAN2022

#include <unicode/urename.h>
#include <unicode/ustring.h>

__BIENUTIL_BEGIN_NAMESPACE

// namespace for conversion using ICU's library.
namespace ns_CONVICU
{

static const UChar32 vkc32RelacementChar = 0xFFFD;

// String conversion:
// The non-converting copier.
template < class t_tyStringDest, class t_tyCharSource >
void ConvertString( t_tyStringDest & _rstrDest, const t_tyCharSource * _pcSource, size_t _stLenSource = (std::numeric_limits<size_t>::max)())
	requires TAreSameSizeTypes_v< typename t_tyStringDest::value_type, t_tyCharSource >
{
	if ( (std::numeric_limits<size_t>::max)() == _stLenSource )
		_stLenSource = StrNLen( _pcSource );
	else
		Assert( _stLenSource == StrNLen( _pcSource, _stLenSource ) ); // Not sure how these u_*() methods react to embedded nulls.
	_rstrDest.assign( reinterpret_cast< const typename t_tyStringDest::value_type * >( _pcSource ), _stLenSource );
}

// UTF32->UTF16:
template < class t_tyString16, class t_tyCharSource >
void ConvertString( t_tyString16 & _rstrDest, const t_tyCharSource * _pc32Source, size_t _stLenSource = (std::numeric_limits<size_t>::max)() )
	// This should allow Linux's std::basic_string< char16_t > as well as Windows' std::basic_string< wchar_t >.
	requires ( ( sizeof( typename t_tyString16::value_type ) == 2 ) && ( sizeof( t_tyCharSource ) == 4 ) ) 
{
	static_assert( sizeof(UChar) == 2 );
	UErrorCode ec = U_ZERO_ERROR;
	if ( _stLenSource == (std::numeric_limits<size_t>::max)() )
		_stLenSource = StrNLen( _pc32Source );
	int32_t nLenReq = 0;
	(void)u_strFromUTF32WithSub( nullptr, 0, &nLenReq, (const UChar32 *)_pc32Source, (int32_t)_stLenSource, vkc32RelacementChar, 0, &ec );
	if ( U_FAILURE( ec ) && ( U_BUFFER_OVERFLOW_ERROR != ec ) ) // It seems to return U_BUFFER_OVERFLOW_ERROR when preflighting the buffer size.
	{
		const char * cpErrorCode = u_errorName( ec );
		THROWNAMEDEXCEPTION( "u_strFromUTF32WithSub() returned UErrorCode[%zd][%s].", ptrdiff_t(ec), cpErrorCode ? cpErrorCode : "u_errorName() returned null" );
	}
	_rstrDest.resize( nLenReq );
	ec = U_ZERO_ERROR;
	(void)u_strFromUTF32WithSub( (UChar*)&_rstrDest[0], nLenReq, nullptr, (const UChar32 *)_pc32Source, (int32_t)_stLenSource, vkc32RelacementChar, 0, &ec );
	if ( U_FAILURE( ec ) )
	{
		const char * cpErrorCode = u_errorName( ec );
		THROWNAMEDEXCEPTION( "u_strFromUTF32WithSub() returned UErrorCode[%zd][%s].", ptrdiff_t(ec), cpErrorCode ? cpErrorCode : "u_errorName() returned null" );
	}
}

// UTF16->UTF32
template < class t_tyString32, class t_tyCharSource >
void ConvertString( t_tyString32 & _rstrDest, const t_tyCharSource * _pc16Source, size_t _stLenSource = (std::numeric_limits<size_t>::max)() )
	requires( ( sizeof( typename t_tyString32::value_type ) == 4 ) && ( sizeof( t_tyCharSource ) == 2 ) )
{
	UErrorCode ec = U_ZERO_ERROR;
	if ( _stLenSource == (std::numeric_limits<size_t>::max)() )
		_stLenSource = StrNLen( _pc16Source );
	int32_t nLenReq = 0;
	(void)u_strToUTF32WithSub( nullptr, 0, &nLenReq, (const UChar*)_pc16Source, (int32_t)_stLenSource, vkc32RelacementChar, nullptr, &ec );
	if ( U_FAILURE( ec ) && ( U_BUFFER_OVERFLOW_ERROR != ec ) ) // It seems to return U_BUFFER_OVERFLOW_ERROR when preflighting the buffer size.
	{
		const char * cpErrorCode = u_errorName( ec );
		THROWNAMEDEXCEPTION( "u_strToUTF32WithSub() returned UErrorCode[%zd][%s].", ptrdiff_t(ec), cpErrorCode ? cpErrorCode : "u_errorName() returned null" );
	}
	_rstrDest.resize( nLenReq );
	ec = U_ZERO_ERROR;
	(void)u_strToUTF32WithSub( (UChar32*)&_rstrDest[0], nLenReq, nullptr, (const UChar*)_pc16Source, (int32_t)_stLenSource, vkc32RelacementChar, nullptr, &ec );
	if ( U_FAILURE( ec ) )
	{
		const char * cpErrorCode = u_errorName( ec );
		THROWNAMEDEXCEPTION( "u_strToUTF32WithSub() returned UErrorCode[%zd][%s].", ptrdiff_t(ec), cpErrorCode ? cpErrorCode : "u_errorName() returned null" );
	}
}

// UTF16->UTF8
template < class t_tyString8, class t_tyCharSource >
void ConvertString( t_tyString8 & _rstrDest, const t_tyCharSource * _pc16Source, size_t _stLenSource = (std::numeric_limits<size_t>::max)() )
	requires( ( sizeof( typename t_tyString8::value_type ) == 1 ) && ( sizeof( t_tyCharSource ) == 2 ) )
{
	UErrorCode ec = U_ZERO_ERROR;
	if ( _stLenSource == (std::numeric_limits<size_t>::max)() )
		_stLenSource = StrNLen( _pc16Source );
	int32_t nLenReq = 0;
	(void)u_strToUTF8WithSub( nullptr, 0, &nLenReq, (const UChar*)_pc16Source, (int32_t)_stLenSource, vkc32RelacementChar, nullptr, &ec );
	if ( U_FAILURE( ec ) && ( U_BUFFER_OVERFLOW_ERROR != ec ) ) // It seems to return U_BUFFER_OVERFLOW_ERROR when preflighting the buffer size.
	{
		const char * cpErrorCode = u_errorName( ec );
		THROWNAMEDEXCEPTION( "u_strToUTF8WithSub() returned UErrorCode[%zd][%s].", ptrdiff_t(ec), cpErrorCode ? cpErrorCode : "u_errorName() returned null" );
	}
	_rstrDest.resize( nLenReq );
	ec = U_ZERO_ERROR;
	(void)u_strToUTF8WithSub( (char*)&_rstrDest[0], nLenReq, nullptr, (const UChar*)_pc16Source, (int32_t)_stLenSource, vkc32RelacementChar, nullptr, &ec );
	if ( U_FAILURE( ec ) )
	{
		const char * cpErrorCode = u_errorName( ec );
		THROWNAMEDEXCEPTION( "u_strToUTF8WithSub() returned UErrorCode[%zd][%s].", ptrdiff_t(ec), cpErrorCode ? cpErrorCode : "u_errorName() returned null" );
	}
}

// UTF8->UTF16:
template < class t_tyString16, class t_tyCharSource >
void ConvertString( t_tyString16 & _rstrDest, const t_tyCharSource * _pc8Source, size_t _stLenSource = (std::numeric_limits<size_t>::max)() )
	// This should allow Linux's std::basic_string< char16_t > as well as Windows' std::basic_string< wchar_t >.
	requires ( ( sizeof( typename t_tyString16::value_type ) == 2 ) && ( sizeof( t_tyCharSource ) == 1 ) )
{
	static_assert( sizeof(UChar) == 2 );
	UErrorCode ec = U_ZERO_ERROR;
	if ( _stLenSource == (std::numeric_limits<size_t>::max)() )
		_stLenSource = StrNLen( _pc8Source );
	int32_t nLenReq = 0;
	(void)u_strFromUTF8WithSub( nullptr, 0, &nLenReq, (const char*)_pc8Source, (int32_t)_stLenSource, vkc32RelacementChar, 0, &ec );
	if ( U_FAILURE( ec ) && ( U_BUFFER_OVERFLOW_ERROR != ec ) ) // It seems to return U_BUFFER_OVERFLOW_ERROR when preflighting the buffer size.
	{
		const char * cpErrorCode = u_errorName( ec );
		THROWNAMEDEXCEPTION( "u_strFromUTF8WithSub() returned UErrorCode[%zd][%s].", ptrdiff_t(ec), cpErrorCode ? cpErrorCode : "u_errorName() returned null" );
	}
	_rstrDest.resize( nLenReq );
	ec = U_ZERO_ERROR;
	(void)u_strFromUTF8WithSub( (UChar*)&_rstrDest[0], nLenReq, nullptr, (const char*)_pc8Source, (int32_t)_stLenSource, vkc32RelacementChar, 0, &ec );
	if ( U_FAILURE( ec ) )
	{
		const char * cpErrorCode = u_errorName( ec );
		THROWNAMEDEXCEPTION( "u_strFromUTF8WithSub() returned UErrorCode[%zd][%s].", ptrdiff_t(ec), cpErrorCode ? cpErrorCode : "u_errorName() returned null" );
	}
}

// UTF8 <-> UTF32 require double conversion:
// UTF8->UTF32:
template < class t_tyString32, class t_tyCharSource >
void ConvertString( t_tyString32 & _rstrDest, const t_tyCharSource * _pc8Source, size_t _stLenSource = (std::numeric_limits<size_t>::max)() )
	requires ( ( sizeof( typename t_tyString32::value_type ) == 4 ) && ( sizeof( t_tyCharSource ) == 1 ) )
{
	typedef std::basic_string< char16_t > _tyString16;
	_tyString16 str16;
	ConvertString( str16, _pc8Source, _stLenSource );
	ConvertString( _rstrDest, &str16[0], str16.length() );
}
// UTF32->UTF8:
template < class t_tyString8, class t_tyCharSource >
void ConvertString( t_tyString8 & _rstrDest, const t_tyCharSource * _pc32Source, size_t _stLenSource = (std::numeric_limits<size_t>::max)() )
	requires ( ( sizeof( typename t_tyString8::value_type ) == 1 ) && ( sizeof( t_tyCharSource ) == 4 ) )
{
	typedef std::basic_string< char16_t > _tyString16;
	_tyString16 str16;
	ConvertString( str16, _pc32Source, _stLenSource );
	ConvertString( _rstrDest, &str16[0], str16.length() );
}

} //namespace ns_CONVICU

__BIENUTIL_END_NAMESPACE
