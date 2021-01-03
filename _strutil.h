#pragma once

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _strutil.h
// String utilities. A *very* loose and unorganized collection at this point - on a need basis - not designed.
// dbien: 12MAR2020

#include <stdlib.h>
#include <wchar.h>
#include <limits.h>
#ifndef WIN32
#include <unistd.h>
#endif //!WIN32
#include <unicode/ustring.h>
#include <string>
#include <string_view>
#include <compare>
#include <utility>
#include <cstddef>
#include "_namdexc.h"
#include "_smartp.h"
#include "_assert.h"
#if __APPLE__
#include <libproc.h>
#include <mach-o/dyld.h>
#elif __linux__
#include <sys/auxv.h>
#include <sys/stat.h>
#else
#endif

__BIENUTIL_BEGIN_NAMESPACE

// TIsCharType. TIsCharType_v: Is this a character type?
template < class t_ty >
struct TIsCharType
{
	static constexpr bool value = false;
};
template <>
struct TIsCharType< char >
{
	static constexpr bool value = true;
};
template <>
struct TIsCharType< wchar_t >
{
	static constexpr bool value = true;
};
template <>
struct TIsCharType< char8_t >
{
	static constexpr bool value = true;
};
template <>
struct TIsCharType< char16_t >
{
	static constexpr bool value = true;
};
template <>
struct TIsCharType< char32_t >
{
	static constexpr bool value = true;
};
template < class t_ty >
inline constexpr bool TIsCharType_v = TIsCharType< t_ty >::value;

// TIsStringView, TIsStringView_v: Is this a std::basic_string_view type?
template < class t_ty >
struct TIsStringView
{
	static constexpr bool value = false;
};
template < class t_tyChar, class t_tyCharTraits >
struct TIsStringView< std::basic_string_view< t_tyChar, t_tyCharTraits > >
{
	static constexpr bool value = true;
};
template < class t_ty >
inline constexpr bool TIsStringView_v = TIsStringView< t_ty >::value;

// StrRSpn:
// Find the count of _pszSet chars that occur at the end of [_pszBegin,_pszEnd).
template <class t_tyChar>
size_t StrRSpn(const t_tyChar *_pszBegin, const t_tyChar *_pszEnd, const t_tyChar *_pszSet)
{
	const t_tyChar *pszCur = _pszEnd;
	for (; pszCur-- != _pszBegin;)
	{
		const t_tyChar *pszCurSet = _pszSet;
		for (; !!*pszCurSet && (*pszCurSet != *pszCur); ++pszCurSet)
			;
		if (!*pszCurSet)
			break;
	}
	++pszCur;
	return _pszEnd - pszCur;
}

template <class t_tyChar>
size_t StrNLen(const t_tyChar *_psz, size_t _stMaxLen = (std::numeric_limits<size_t>::max)())
{
	if (!_psz || !_stMaxLen)
		return 0;
	const t_tyChar *pszMax = ((std::numeric_limits<size_t>::max)() == _stMaxLen) ? (_psz - 1) : (_psz + _stMaxLen);
	const t_tyChar *pszCur = _psz;
	for (; (pszMax != pszCur) && !!*pszCur; ++pszCur)
		;
	return pszCur - _psz;
}

template <class t_tyChar>
std::strong_ordering ICompareStr(const t_tyChar *_pszLeft, const t_tyChar *_pszRight)
{
	for (; !!*_pszLeft && (*_pszLeft == *_pszRight); ++_pszLeft, ++_pszRight)
		;
	return (*_pszLeft < *_pszRight) ? std::strong_ordering::less : ((*_pszLeft > *_pszRight) ? std::strong_ordering::greater : std::strong_ordering::equal);
}

template < class t_tyChar >
int VNSPrintf( t_tyChar * _rgcOut, size_t _n, const t_tyChar * _szFormat, va_list _ap );
template < > inline
int VNSPrintf( char * _rgcOut, size_t _n, const char * _szFormat, va_list _ap )
{
	return vsnprintf( _rgcOut, _n, _szFormat, _ap );
}
template < > inline
int VNSPrintf( wchar_t * _rgcOut, size_t _n, const wchar_t * _szFormat, va_list _ap )
{
	return vswprintf( _rgcOut, _n, _szFormat, _ap );
}

// Return a string formatted like printf. Throws.
template < class t_tyString >
void PrintfStdStr( t_tyString &_rstr, const typename t_tyString::value_type *_pcFmt, ...) noexcept(false) 
{
	va_list ap;
	va_start(ap, _pcFmt);
	try
	{
		VPrintfStdStr(_rstr, _pcFmt, ap);
	}
	catch( ... )
	{
		va_end(ap);
		throw;
	}
	va_end(ap);
}

template < class t_tyString >
void VPrintfStdStr( t_tyString &_rstr, const typename t_tyString::value_type *_pcFmt, va_list _ap ) noexcept(false) 
			requires ( std::is_same_v<typename t_tyString::value_type, char> )
{
	typedef typename t_tyString::value_type _tyChar;
  int nRequired;
	{ //B
		va_list ap2;
		va_copy(ap2, _ap); // Do this correctly
		// Initially just pass a single character bufffer since we only care about the return value:
		_tyChar tc;
		PrepareErrNo();
		nRequired = VNSPrintf( &tc, 1, _pcFmt, ap2 );
		va_end(ap2);
		if (nRequired < 0)
			THROWNAMEDEXCEPTIONERRNO(GetLastErrNo(), "vsnprintf() returned nRequired[%d].", nRequired);
		_rstr.resize(nRequired); // this will reserve nRequired+1.
	}//EB
	PrepareErrNo();
	int nRet = VNSPrintf(&_rstr[0], nRequired + 1, _pcFmt, _ap);
	if (nRet < 0)
		THROWNAMEDEXCEPTIONERRNO(GetLastErrNo(), "vsnprintf() returned nRet[%d].", nRet);
}

template < class t_tyString >
void VPrintfStdStr( t_tyString &_rstr, const typename t_tyString::value_type *_pcFmt, va_list _ap ) noexcept(false) 
			requires ( std::is_same_v<typename t_tyString::value_type, wchar_t> )
{
	// Due to C99 people sucking butt we have to do this in an awkward way...
	typedef typename t_tyString::value_type _tyChar;
	_tyChar rgcFirstTry[ 256 ];
	size_t stBufSize = 256;
	static constexpr size_t kst2ndTryBufSize = 8192;
	static constexpr size_t kstMaxBufSize = ( 1 << 24 ); // arbitrary but large.
	_tyChar * pcBuf = rgcFirstTry;
	
	for (;;)
	{
		va_list ap2;
		va_copy(ap2, _ap);
		PrepareErrNo();
		int nWritten = VNSPrintf( pcBuf, stBufSize, _pcFmt, ap2 );
		va_end(ap2);
		if ( -1 == nWritten )
		{
			if ( 0 != GetLastErrNo() )
			{
				// Then some error besides not having a big enough buffer.
				THROWNAMEDEXCEPTIONERRNO(GetLastErrNo(), "vsnprintf() returned nRequired[%d].", nWritten);
			}
			if ( pcBuf == rgcFirstTry )
				pcBuf = (_tyChar*)alloca( stBufSize = kst2ndTryBufSize );
			else
			{
				stBufSize *= 2;
				if ( stBufSize > kstMaxBufSize )
					THROWNAMEDEXCEPTION("Overflowed maximum buffer size since vswprintf is a crappy implementation kstMaxBufSize[%ld].", kstMaxBufSize);
				_rstr.resize( stBufSize-1 );
			}
		}
		else
		{
			// Success!!!
			if ( stBufSize <= kst2ndTryBufSize )
				_rstr.assign( pcBuf, nWritten );
			else
				_rstr.resize(nWritten); // truncate appropriately.
			return;
		}
	}
}

// Return a string formatted like printf. Doesn't throw.
template < class t_tyString >
bool FPrintfStdStrNoThrow( t_tyString &_rstr, const typename t_tyString::value_type *_pcFmt, ...)  
{
	va_list ap;
	va_start(ap, _pcFmt);
	try
	{
		VPrintfStdStr(_rstr, _pcFmt, ap);
	}
	catch( ... )
	{
		va_end(ap);
		return false;
	}
	va_end(ap);
	return true;
}

// In this overload the caller has already made the first call to obtain the size of the buffer requires.
template < class t_tyString >
int NPrintfStdStr( t_tyString &_rstr, int _nRequired, const typename t_tyString::value_type *_pcFmt, va_list _ap)
{
	_rstr.resize(_nRequired); // this will reserver nRequired+1.
	int nRet = VNSPrintf(&_rstr[0], _nRequired + 1, _pcFmt, _ap);
	return nRet;
}

// Return an error message string to the caller in a standard manner.
template < class t_tyAllocator >
inline void GetErrnoStdStr(vtyErrNo _errno, std::basic_string<char, std::char_traits<char>, t_tyAllocator> &_rstr)
{
	const int knErrorMesg = 256;
	char rgcErrorMesg[knErrorMesg];
	if ( !GetErrorString( _errno, rgcErrorMesg, knErrorMesg ) )
	{
		rgcErrorMesg[knErrorMesg - 1] = 0;
		PrintfStdStr(_rstr, "errno:[%d]: %s", _errno, rgcErrorMesg);
	}
	else
		PrintfStdStr(_rstr, "errno:[%d]", _errno);
}

// Just return the error description if found.
template <class t_tyAllocator >
inline void GetErrnoDescStdStr(vtyErrNo _errno, std::basic_string<char, std::char_traits<char>, t_tyAllocator> &_rstr)
{
	const int knErrorMesg = 256;
	char rgcErrorMesg[knErrorMesg];
	if (!GetErrorString(_errno, rgcErrorMesg, knErrorMesg))
		_rstr = rgcErrorMesg;
	else
		_rstr.clear();
}

// This method will not look for negative numbers but it can read into a signed number.
template <class t_tyChar, class t_tyNum>
int IReadPositiveNum(const t_tyChar *_psz, ssize_t _sstLen, t_tyNum &_rNum, bool _fThrowOnError)
{
	typedef t_tyChar _tyChar;
	_rNum = 0;
	if (!_psz || !*_psz)
	{
		if (_fThrowOnError)
			THROWNAMEDEXCEPTION("Null or empty string passed.");
		return -1;
	}
	if (_sstLen <= 0)
		_sstLen = StrNLen(_psz);
	const _tyChar *pszCur = _psz;
	const _tyChar *const pszEnd = pszCur + _sstLen;
	for (; pszEnd != pszCur; ++pszCur)
	{
		int iCur = int(*pszCur) - int('0'); // using the fact that '0' is same in all character types.
		if ((iCur < 0) || (iCur > 9))
		{
			if (_fThrowOnError)
				THROWNAMEDEXCEPTION("Non-digit passed.");
			return -2;
		}
		t_tyNum numBefore = _rNum;
		_rNum *= 10;
		_rNum += iCur;
		if (_rNum < numBefore) // overflow.
		{
			if (_fThrowOnError)
				THROWNAMEDEXCEPTION("Overflow.");
			return -3;
		}
	}
	return 0;
}

// Get the full executable path - not including the executable but yes including a final '/'.
inline void
GetCurrentExecutablePath(std::string &_rstrPath)
{
	_rstrPath.clear();
#if __APPLE__
	char rgBuf[PROC_PIDPATHINFO_MAXSIZE];
	pid_t pid = getpid();
	int iRet = proc_pidpath(pid, rgBuf, sizeof(rgBuf));
	Assert(iRet > 0);
	if (iRet > 0)
		_rstrPath = rgBuf;
	else
	{ // Then try a different way.
		__BIENUTIL_USING_NAMESPACE
		char c;
		uint32_t len = 1;
		std::string strInitPath;
		(void)_NSGetExecutablePath(&c, &len);
		strInitPath.resize(len - 1); // will reserver len.
		int iRet = _NSGetExecutablePath(&strInitPath[0], &len);
		Assert(!iRet);
		char *cpMallocedPath = realpath(strInitPath.c_str(), 0);
		FreeVoid fv(cpMallocedPath); // Ensure we free.
		Assert(!!cpMallocedPath);
		if (!cpMallocedPath)
			_rstrPath = strInitPath;
		else
			_rstrPath = cpMallocedPath;
	}
#elif __linux__
	char *cpPath = (char *)getauxval(AT_EXECFN);
	if (!!cpPath)
		_rstrPath = cpPath;
	else
	{
		const int knBuf = PATH_MAX * 4;
		char rgcBuf[knBuf];
		ssize_t len = readlink("/proc/self/exe", rgcBuf, knBuf);
		if (len > knBuf)
			_rstrPath.clear();
		else
			_rstrPath = rgcBuf;
	}
#elif defined( WIN32 )
	const DWORD kdwBuf = 4096;
	char rgcBuf[kdwBuf];
	DWORD dwWritten = ::GetModuleFileNameA(NULL, rgcBuf, kdwBuf);
	Assert(!!dwWritten);
	if (!dwWritten)
		_rstrPath.clear();
	else
		_rstrPath.assign(rgcBuf, kdwBuf);
#else
#error What to do in this sichiation.
#endif
	std::string::size_type stLastSlash = _rstrPath.find_last_of('/');
	if (std::string::npos != stLastSlash)
		_rstrPath.resize(stLastSlash + 1);
}

static const UChar32 vkc32RelacementChar = 0xFFFD;

// String conversion:
// The non-converting copier.
template < class t_tyString >
void ConvertString( t_tyString & _rstrDest, const typename t_tyString::value_type * _pcSource, size_t _stLenSource = (std::numeric_limits<size_t>::max)())
{
	if ( (std::numeric_limits<size_t>::max)() == _stLenSource )
		_stLenSource = StrNLen( _pcSource );
	else
		Assert( _stLenSource == StrNLen( _pcSource ) ); // Not sure how these u_*() methods react to embedded nulls.
	_rstrDest.assign( _pcSource, _stLenSource );
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
	else
		Assert( _stLenSource == StrNLen( _pc32Source, _stLenSource ) );
	int32_t nLenReq = 0;
	(void)u_strFromUTF32WithSub( nullptr, 0, &nLenReq, (const UChar32 *)_pc32Source, (int32_t)_stLenSource, vkc32RelacementChar, 0, &ec );
	if ( U_FAILURE( ec ) && ( U_BUFFER_OVERFLOW_ERROR != ec ) ) // It seems to return U_BUFFER_OVERFLOW_ERROR when preflighting the buffer size.
	{
		const char * cpErrorCode = u_errorName( ec );
		THROWNAMEDEXCEPTION( "u_strFromUTF32WithSub() returned UErrorCode[%ld][%s].", ptrdiff_t(ec), cpErrorCode ? cpErrorCode : "u_errorName() returned null" );
	}
	_rstrDest.resize( nLenReq );
	ec = U_ZERO_ERROR;
	(void)u_strFromUTF32WithSub( (UChar*)&_rstrDest[0], nLenReq, nullptr, (const UChar32 *)_pc32Source, (int32_t)_stLenSource, vkc32RelacementChar, 0, &ec );
	if ( U_FAILURE( ec ) )
	{
		const char * cpErrorCode = u_errorName( ec );
		THROWNAMEDEXCEPTION( "u_strFromUTF32WithSub() returned UErrorCode[%ld][%s].", ptrdiff_t(ec), cpErrorCode ? cpErrorCode : "u_errorName() returned null" );
	}
	Assert( StrNLen( &_rstrDest[0], nLenReq ) == nLenReq );// get what we paid for.
}
#if 0
template < class t_tyString16 >
void ConvertString( t_tyString16 & _rstrDest, const wchar_t * _pc32Source, size_t _stLenSource = (std::numeric_limits<size_t>::max)() )
	requires ( ( sizeof( typename t_tyString16::value_type ) == 2 ) && ( sizeof( wchar_t ) == 4 ) )
{
	ConvertString( _rstrDest, (char32_t*)_pc32Source, _stLenSource );
}
#endif //0

// UTF16->UTF32
template < class t_tyString32, class t_tyCharSource >
void ConvertString( t_tyString32 & _rstrDest, const t_tyCharSource * _pc16Source, size_t _stLenSource = (std::numeric_limits<size_t>::max)() )
	requires( ( sizeof( typename t_tyString32::value_type ) == 4 ) && ( sizeof( t_tyCharSource ) == 2 ) )
{
	UErrorCode ec = U_ZERO_ERROR;
	if ( _stLenSource == (std::numeric_limits<size_t>::max)() )
		_stLenSource = StrNLen( _pc16Source );
	else
		Assert( _stLenSource == StrNLen( _pc16Source, _stLenSource ) );
	int32_t nLenReq = 0;
	(void)u_strToUTF32WithSub( nullptr, 0, &nLenReq, _pc16Source, (int32_t)_stLenSource, vkc32RelacementChar, nullptr, &ec );
	if ( U_FAILURE( ec ) && ( U_BUFFER_OVERFLOW_ERROR != ec ) ) // It seems to return U_BUFFER_OVERFLOW_ERROR when preflighting the buffer size.
	{
		const char * cpErrorCode = u_errorName( ec );
		THROWNAMEDEXCEPTION( "u_strToUTF32WithSub() returned UErrorCode[%ld][%s].", ptrdiff_t(ec), cpErrorCode ? cpErrorCode : "u_errorName() returned null" );
	}
	_rstrDest.resize( nLenReq );
	ec = U_ZERO_ERROR;
	(void)u_strToUTF32WithSub( (UChar32*)&_rstrDest[0], nLenReq, nullptr, _pc16Source, (int32_t)_stLenSource, vkc32RelacementChar, nullptr, &ec );
	if ( U_FAILURE( ec ) )
	{
		const char * cpErrorCode = u_errorName( ec );
		THROWNAMEDEXCEPTION( "u_strToUTF32WithSub() returned UErrorCode[%ld][%s].", ptrdiff_t(ec), cpErrorCode ? cpErrorCode : "u_errorName() returned null" );
	}
	Assert( StrNLen( &_rstrDest[0], nLenReq ) == nLenReq );
}

// UTF16->UTF8
template < class t_tyString8, class t_tyCharSource >
void ConvertString( t_tyString8 & _rstrDest, const t_tyCharSource * _pc16Source, size_t _stLenSource = (std::numeric_limits<size_t>::max)() )
	requires( ( sizeof( typename t_tyString8::value_type ) == 1 ) && ( sizeof( t_tyCharSource ) == 2 ) )
{
	UErrorCode ec = U_ZERO_ERROR;
	if ( _stLenSource == (std::numeric_limits<size_t>::max)() )
		_stLenSource = StrNLen( _pc16Source );
	else
		Assert( _stLenSource == StrNLen( _pc16Source, _stLenSource ) );
	int32_t nLenReq = 0;
	(void)u_strToUTF8WithSub( nullptr, 0, &nLenReq, _pc16Source, (int32_t)_stLenSource, vkc32RelacementChar, nullptr, &ec );
	if ( U_FAILURE( ec ) && ( U_BUFFER_OVERFLOW_ERROR != ec ) ) // It seems to return U_BUFFER_OVERFLOW_ERROR when preflighting the buffer size.
	{
		const char * cpErrorCode = u_errorName( ec );
		THROWNAMEDEXCEPTION( "u_strToUTF8WithSub() returned UErrorCode[%ld][%s].", ptrdiff_t(ec), cpErrorCode ? cpErrorCode : "u_errorName() returned null" );
	}
	_rstrDest.resize( nLenReq );
	ec = U_ZERO_ERROR;
	(void)u_strToUTF8WithSub( (char*)&_rstrDest[0], nLenReq, nullptr, _pc16Source, (int32_t)_stLenSource, vkc32RelacementChar, nullptr, &ec );
	if ( U_FAILURE( ec ) )
	{
		const char * cpErrorCode = u_errorName( ec );
		THROWNAMEDEXCEPTION( "u_strToUTF8WithSub() returned UErrorCode[%ld][%s].", ptrdiff_t(ec), cpErrorCode ? cpErrorCode : "u_errorName() returned null" );
	}
	Assert( StrNLen( &_rstrDest[0], nLenReq ) == nLenReq );
}
#if 0
// This handles casting for the Windows' case of wchar_t being 2 bytes long for the above two functions.
template < class t_tyString8 >
void ConvertString( t_tyString8 & _rstrDest, const wchar_t * _pwc16Source, size_t _stLenSource = (std::numeric_limits<size_t>::max)() )
	requires( ( ( sizeof( typename t_tyString8::value_type ) == 4 ) || ( sizeof( typename t_tyString8::value_type ) == 1 ) ) && ( sizeof( wchar_t ) == 2 ) )
{
	ConvertString( _rstrDest, (char16_t*)_pwc16Source, _stLenSource );
}
#endif //0

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
	else
		Assert( _stLenSource == StrNLen( _pc8Source, _stLenSource ) );
	int8_t nLenReq = 0;
	(void)u_strFromUTF8WithSub( nullptr, 0, &nLenReq, _pc8Source, (int8_t)_stLenSource, vkc32RelacementChar, 0, &ec );
	if ( U_FAILURE( ec ) && ( U_BUFFER_OVERFLOW_ERROR != ec ) ) // It seems to return U_BUFFER_OVERFLOW_ERROR when preflighting the buffer size.
	{
		const char * cpErrorCode = u_errorName( ec );
		THROWNAMEDEXCEPTION( "u_strFromUTF8WithSub() returned UErrorCode[%ld][%s].", ptrdiff_t(ec), cpErrorCode ? cpErrorCode : "u_errorName() returned null" );
	}
	_rstrDest.resize( nLenReq );
	ec = U_ZERO_ERROR;
	(void)u_strFromUTF8WithSub( (UChar*)&_rstrDest[0], nLenReq, nullptr, _pc8Source, (int8_t)_stLenSource, vkc32RelacementChar, 0, &ec );
	if ( U_FAILURE( ec ) )
	{
		const char * cpErrorCode = u_errorName( ec );
		THROWNAMEDEXCEPTION( "u_strFromUTF8WithSub() returned UErrorCode[%ld][%s].", ptrdiff_t(ec), cpErrorCode ? cpErrorCode : "u_errorName() returned null" );
	}
	Assert( StrNLen( &_rstrDest[0], nLenReq ) == nLenReq );// get what we paid for.
}
#if 0
// casting char->char8_t.
template < class t_tyString16 >
void ConvertString( t_tyString16 & _rstrDest, const char * _pcSource, size_t _stLenSource = (std::numeric_limits<size_t>::max)() )
	requires ( ( sizeof( typename t_tyString16::value_type ) == 2 ) )
{
	ConvertString( _rstrDest, (char8_t*)_pcSource, _stLenSource );
}
#endif //0

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

// wrappers for the above methods.
// This one should work for both strings and string_views.
template < class t_tyStringDest, class t_tyStringSrc >
void ConvertString( t_tyStringDest & _rstrDest, t_tyStringSrc const & _rstrSrc ) 
	requires(	std::is_same_v< typename t_tyStringSrc::value_type, typename t_tyStringDest::value_type > )
{
	_rstrDest = _rstrSrc;
}
// The below should work for all kinds of strings and also when the source is a string view.
template < class t_tyStringDest, class t_tyStringSrc >
void ConvertString( t_tyStringDest & _rstrDest, t_tyStringSrc && _rrstrSrc ) 
	requires ( std::is_same_v< typename t_tyStringSrc::value_type, typename t_tyStringDest::value_type > && !TIsStringView_v< t_tyStringDest > )
{
	_rstrDest = std::move( _rrstrSrc );
}
// The below will work when the source is a string view as well.
template < class t_tyStringDest, class t_tyStringSrc >
void ConvertString( t_tyStringDest & _rstrDest, t_tyStringSrc const & _rstrSrc ) 
	requires( !std::is_same_v< typename t_tyStringSrc::value_type, typename t_tyStringDest::value_type > && !TIsStringView_v< t_tyStringDest > )
{
	ConvertString( _rstrDest, &_rstrSrc[0], _rstrSrc.length() );
}

namespace n_StrArrayStaticCast
{
	template <typename t_tyChar, std::size_t t_knLength>
	struct str_array
	{
		constexpr t_tyChar const *c_str() const { return data_; }
		constexpr t_tyChar const *data() const { return data_; }
		constexpr t_tyChar operator[](std::size_t i) const { return data_[i]; }
		constexpr t_tyChar const *begin() const { return data_; }
		constexpr t_tyChar const *end() const { return data_ + t_knLength; }
		constexpr std::size_t size() const { return t_knLength; }
		constexpr operator const t_tyChar *() const { return data_; }
		// TODO: add more members of std::basic_string

		t_tyChar data_[t_knLength + 1]; // +1 for null-terminator
	};

	template <typename t_tyCharResult, typename t_tyCharSource>
	constexpr t_tyCharResult static_cast_ascii(t_tyCharSource x)
	{
		if (!(x >= 0 && x <= 127))
			THROWNAMEDEXCEPTION("Character value must be in basic ASCII range (0..127)");
		return static_cast<t_tyCharResult>(x);
	}

	template <typename t_tyCharResult, typename t_tyCharSource, std::size_t N, std::size_t... I>
	constexpr str_array<t_tyCharResult, N - 1> do_str_array_cast(const t_tyCharSource (&a)[N], std::index_sequence<I...>)
	{
		return {static_cast_ascii<t_tyCharResult>(a[I])..., 0};
	}
} //namespace n_StrArrayStaticCast

template <typename t_tyCharResult, typename t_tyCharSource, std::size_t N, typename Indices = std::make_index_sequence<N - 1>>
constexpr n_StrArrayStaticCast::str_array<t_tyCharResult, N - 1> str_array_cast(const t_tyCharSource (&a)[N])
{
	return n_StrArrayStaticCast::do_str_array_cast<t_tyCharResult>(a, Indices{});
}

__BIENUTIL_END_NAMESPACE
