#pragma once

//          Copyright David Lawrence Bien 1997 - 2021.
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
#include <string>
#include <string_view>
#include <compare>
#include <utility>
#include <cstddef>
#include "bientypes.h"
#include "_namdexc.h"
#include "_smartp.h"
#include "_fdobjs.h"
#include "_assert.h"
#if __APPLE__
#include <libproc.h>
#include <mach-o/dyld.h>
#elif __linux__
#include <sys/auxv.h>
#include <sys/stat.h>
#else
#endif
#include "utfconvert.h"

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

// TIsString, TIsString_v: Is this a std::basic_string type?
template < class t_ty >
struct TIsString
{
	static constexpr bool value = false;
};
template < class t_tyChar, class t_tyCharTraits >
struct TIsString< std::basic_string< t_tyChar, t_tyCharTraits > >
{
	static constexpr bool value = true;
};
template < class t_ty >
inline constexpr bool TIsString_v = TIsString< t_ty >::value;

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

// Concept for a class that is either a string or string view:
template < class t_TyStrViewOrString >
concept CIsStringViewOrString = TIsStringView_v< t_TyStrViewOrString > || TIsString_v< t_TyStrViewOrString >;

// StringTransparentHash:
// Allows lookup within an unordered_set by a string_view or char*. I like.
template < class t_tyChar, class t_tyCharTraits = std::char_traits< t_tyChar >, class t_tyAllocator = std::allocator< t_tyChar > >
struct StringTransparentHash
{
	typedef t_tyChar _TyChar;
	typedef t_tyCharTraits _TyCharTraits;
	typedef t_tyAllocator _TyAllocator;
	using _TyStringView = basic_string_view< _TyChar, _TyCharTraits >;
	using _TyString = basic_string< _TyChar, _TyCharTraits >;
  using hash_type = std::hash<_TyStringView>;
  using is_transparent = void;

  size_t operator()(const _TyChar * _psz) const { return hash_type{}(_psz); }
  size_t operator()(_TyStringView _sv) const { return hash_type{}(_sv); }
  size_t operator()(_TyString const& _str) const { return hash_type{}(_str); }
};

template < class t_tyChar >
void MemSet( t_tyChar * _rgcBuf, t_tyChar _cFill, size_t _nValues )
{
	t_tyChar * pcEndBuf = _rgcBuf + _nValues;
	for ( ; pcEndBuf != _rgcBuf; ++_rgcBuf )
		*_rgcBuf = _cFill;
}

// StrSpn: Return count of characters matching a given character set.
template <class t_tyChar>
size_t StrSpn( const t_tyChar * _psz, size_t _stNChars, const t_tyChar * _pszCharSet )
{
	typedef const t_tyChar * _tyLPCSTR;
	_tyLPCSTR pszCur = _psz;
	_tyLPCSTR const pszEnd = _psz + _stNChars;
	for ( ; ( pszEnd != pszCur ); ++pszCur )
	{
		Assert( !!*pszCur );
		_tyLPCSTR pszCharSetCur = _pszCharSet;
		for (; !!*pszCharSetCur && (*pszCharSetCur != *pszCur); ++pszCharSetCur)
			;
		if (!*pszCharSetCur)
			break;
	}
	return pszCur - _psz;
}

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
static size_t StrCSpn(const t_tyChar * _psz, const t_tyChar * _pszCharSet)
{
	typedef const t_tyChar * _tyLPCSTR;
	_tyLPCSTR pszCur = _psz;
	for (; !!*pszCur; ++pszCur)
	{
		_tyLPCSTR pszCharSetCur = _pszCharSet;
		for (; !!*pszCharSetCur && (*pszCharSetCur != *pszCur); ++pszCharSetCur)
			;
		if (!!*pszCharSetCur)
			break;
	}
	return pszCur - _psz;
}

template <class t_tyChar>
static size_t StrCSpn(const t_tyChar * _psz, t_tyChar _tcBegin, t_tyChar _tcEnd, const t_tyChar * _pszCharSet)
{
	typedef const t_tyChar * _tyLPCSTR;
	_tyLPCSTR pszCur = _psz;
	for (; !!*pszCur && ((*pszCur < _tcBegin) || (*pszCur >= _tcEnd)); ++pszCur)
	{
		// Passed the first test, now ensure it isn't one of the _pszCharSet chars:
		_tyLPCSTR pszCharSetCur = _pszCharSet;
		for (; !!*pszCharSetCur && (*pszCharSetCur != *pszCur); ++pszCharSetCur)
			;
		if (!!*pszCharSetCur)
			break;
	}
	return pszCur - _psz;
}

template <class t_tyChar>
size_t StrNLen( const t_tyChar *_psz, size_t _stMaxLen )
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

template < class t_tyString >
void VPrintfStdStr(t_tyString& _rstr, const typename t_tyString::value_type* _pcFmt, va_list _ap) noexcept(false);

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

// char8_t and char:
template < class t_tyString >
void VPrintfStdStr( t_tyString &_rstr, const typename t_tyString::value_type *_pcFmt, va_list _ap ) noexcept(false) 
			requires ( sizeof( typename t_tyString::value_type ) == sizeof( char ) )
{
	typedef char _tyChar;
  	int nRequired;
	{//B
		va_list ap2;
		va_copy(ap2, _ap); // Do this correctly
		// Initially just pass a single character bufffer since we only care about the return value:
		PrepareErrNo();
		nRequired = VNSPrintf((_tyChar*)nullptr, 0, (const _tyChar *)_pcFmt, ap2 );
		va_end(ap2);
		if (nRequired < 0)
			THROWNAMEDEXCEPTIONERRNO(GetLastErrNo(), "vsnprintf() returned nRequired[%d].", nRequired);
		_rstr.resize(nRequired); // this will reserve nRequired+1.
	}//EB
	PrepareErrNo();
	int nRet = VNSPrintf((_tyChar *)&_rstr[0], nRequired + 1, (const _tyChar *)_pcFmt, _ap);
	if (nRet < 0)
		THROWNAMEDEXCEPTIONERRNO(GetLastErrNo(), "vsnprintf() returned nRet[%d].", nRet);
}

// wchar_t and whatever sized standardized char<n>_t that corresponds to: char16_t on Windows and char32_t on Linux.
template < class t_tyString >
void VPrintfStdStr( t_tyString &_rstr, const typename t_tyString::value_type *_pcFmt, va_list _ap ) noexcept(false) 
			requires ( sizeof( typename t_tyString::value_type ) == sizeof( wchar_t )  )
{
#ifdef WIN32
	// Of course MS did this correctly - they have all the str..._s() methods as well - Linux is lagging.
	typedef wchar_t _tyChar;
  	int nRequired;
	{//B
		va_list ap2;
		va_copy(ap2, _ap); // Do this correctly
		PrepareErrNo();
		nRequired = _vscwprintf( (const _tyChar*)_pcFmt, ap2 );
		va_end(ap2);
		if (nRequired < 0)
			THROWNAMEDEXCEPTIONERRNO(GetLastErrNo(), "_vscwprintf() returned nRequired[%d].", nRequired);
		_rstr.resize(nRequired); // this will reserve nRequired+1.
	}//EB
	PrepareErrNo();
	int nRet = VNSPrintf((_tyChar*)&_rstr[0], nRequired + 1, (const _tyChar*)_pcFmt, _ap);
	if (nRet < 0)
		THROWNAMEDEXCEPTIONERRNO(GetLastErrNo(), "vsnprintf() returned nRet[%d].", nRet);
#else //!WIN32
	// Due to C99 people sucking butt we have to do this in an awkward way...
	typedef wchar_t _tyChar;
	_tyChar rgcFirstTry[ 256 ];
	size_t stBufSize = 256;
	static constexpr size_t kst2ndTryBufSize = 8192;
	static constexpr size_t kstMaxBufSize = ( 1ull << 24 ); // arbitrary but large.
	_tyChar * pcBuf = rgcFirstTry;
	for (;;)
	{
		va_list ap2;
		va_copy(ap2, _ap);
		PrepareErrNo();
		int nWritten = VNSPrintf( pcBuf, stBufSize, (const _tyChar*)_pcFmt, ap2 );
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
				pcBuf = (_tyChar*)&_rstr[0];
			}
		}
		else
		{
			// Success!!!
			if ( stBufSize <= kst2ndTryBufSize )
				_rstr.assign( (typename t_tyString::value_type*)pcBuf, nWritten );
			else
				_rstr.resize(nWritten); // truncate appropriately.
			return;
		}
	}
#endif //!WIN32
}

// Versions of the above that take a buffer size for the format string.
// (numeric_limits<size_t>::max)() may be passed if the string is null terminated.
template < class t_tyString >
void VPrintfStdStr( t_tyString &_rstr, size_t _nLenFmt, const typename t_tyString::value_type *_pcFmt, va_list _ap ) noexcept(false) 
			requires( TAreSameSizeTypes_v< typename t_tyString::value_type, char > ||
								TAreSameSizeTypes_v< typename t_tyString::value_type, wchar_t > )
{
	typedef conditional_t< TAreSameSizeTypes_v< typename t_tyString::value_type, char >, char, wchar_t > _TyChar;
	if ( (numeric_limits<size_t>::max)() == _nLenFmt )
		VPrintfStdStr( _rstr,  _pcFmt, _ap ); // null terminated.
	else
	{
		basic_string< _TyChar > strTempBuf;
		_TyChar * pcBuf;
		if ( ( _nLenFmt * sizeof( _TyChar ) ) < vknbyMaxAllocaSize )
		{
			pcBuf = (_TyChar*)alloca( ( _nLenFmt + 1 ) * sizeof( _TyChar ) );
			pcBuf[_nLenFmt] = 0;
		}
		else
		{
      strTempBuf.resize( _nLenFmt );
      pcBuf = &strTempBuf[0];
		}
		memcpy( pcBuf, _pcFmt, _nLenFmt * sizeof( _TyChar ) );
		VPrintfStdStr( _rstr,  pcBuf, _ap ); // null terminated.
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
		SetLastErrNo( vkerrInvalidArgument );
		if (_fThrowOnError)
			THROWNAMEDEXCEPTION("Null or empty string passed.");
		return -1;
	}
	if (_sstLen <= 0)
		_sstLen = StrNLen(_psz);
	const _tyChar *pszCur = _psz;
	const _tyChar *const pszEnd = pszCur + _sstLen;
	bool fGotDigit = false; // If we got a digit we will ignore non-numeric characters beyond it.
	for (; pszEnd != pszCur; ++pszCur)
	{
		int iCur = int(*pszCur) - int('0'); // using the fact that '0' is same in all character types.
		if ((iCur < 0) || (iCur > 9))
		{
			if ( !fGotDigit )
			{
				SetLastErrNo( vkerrInvalidArgument );
				if (_fThrowOnError)
					THROWNAMEDEXCEPTION("Non-digit passed.");
				return -2;
			}
			else
				return 0; // We got our number.
		}
		fGotDigit = true;
		t_tyNum numBefore = _rNum;
		_rNum *= 10;
		_rNum += iCur;
		if (_rNum < numBefore) // overflow.
		{
			SetLastErrNo( vkerrOverflow );
			if (_fThrowOnError)
				THROWNAMEDEXCEPTION("Overflow.");
			return -1;
		}
	}
	return 0;
}

// This one accepts a radix and a maximum.
template <class t_tyChar, class t_tyNum>
int IReadPositiveNum(size_t _stRadix, const t_tyChar *_psz, ssize_t _sstLen, t_tyNum &_rNum, const t_tyNum _kNumMax, bool _fThrowOnError)
{
	Assert( _stRadix <= 36 ); // We employ 0->9,A->Z(a->z).
	typedef t_tyChar _tyChar;
	_rNum = 0;
	if (!_psz || !*_psz || ( _stRadix > 36 ) )
	{
		SetLastErrNo( vkerrInvalidArgument );
		if (_fThrowOnError)
			THROWNAMEDEXCEPTION( ( _stRadix > 36 ) ? "Radix > 36." : "Null or empty string passed." );
		return -1;
	}
	if (_sstLen <= 0)
		_sstLen = StrNLen(_psz);
	const t_tyNum knumRadix = (t_tyNum)_stRadix;
	const _tyChar *pszCur = _psz;
	const _tyChar *const pszEnd = pszCur + _sstLen;
	for (; pszEnd != pszCur; ++pszCur)
	{
    typedef typename std::make_signed<t_tyNum>::type _tyNumSigned;
		_tyNumSigned numCur;
		if ( _tyNumSigned(*pszCur) > _tyNumSigned('a') )
			numCur = _tyNumSigned(*pszCur) - _tyNumSigned('a') + 10;
		else
		if ( _tyNumSigned(*pszCur) > _tyNumSigned('A') )
			numCur = _tyNumSigned(*pszCur) - _tyNumSigned('A') + 10;
		else
			numCur = _tyNumSigned(*pszCur) - _tyNumSigned('0');
		if ((numCur < 0) || ((t_tyNum)numCur >= knumRadix))
		{
			SetLastErrNo( vkerrInvalidArgument );
			if (_fThrowOnError)
				THROWNAMEDEXCEPTION("Invalid character passed.");			
			return -1;
		}
		t_tyNum numBefore = _rNum;
		_rNum *= knumRadix;
		_rNum += (t_tyNum)numCur;
		if ( ( _rNum < numBefore ) || ( _rNum > _kNumMax ) ) // overflow.
		{
			SetLastErrNo( vkerrOverflow );
			if (_fThrowOnError)
				THROWNAMEDEXCEPTION("Overflow.");
			return -1;
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
		if (!cpMallocedPath) // not throwing here because we don't want this to fail.
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
		_rstrPath.assign(rgcBuf, dwWritten);
#else
#error What to do in this sichiation.
#endif
	std::string::size_type stLastSlash = _rstrPath.find_last_of( TChGetFileSeparator<char>() );
	if (std::string::npos != stLastSlash)
		_rstrPath.resize(stLastSlash + 1);
}

// Use the new namespace globally.
using namespace ns_CONVBIEN;

// wrappers for the above methods.
// This one should work for both strings and string_views.
template < class t_tyStringDest, class t_tyStringSrc >
void ConvertString( t_tyStringDest & _rstrDest, t_tyStringSrc const & _rstrSrc ) 
	requires TAreSameSizeTypes_v< typename t_tyStringSrc::value_type, typename t_tyStringDest::value_type >
{
	_rstrDest.assign( (const typename t_tyStringDest::value_type *)&_rstrSrc[0], _rstrSrc.length() );
}
// The below should work for all kinds of strings and also when the source is a string view.
template < class t_tyStringDest, class t_tyStringSrc >
void ConvertString( t_tyStringDest & _rstrDest, t_tyStringSrc && _rrstrSrc ) 
	requires is_same_v< typename t_tyStringSrc::value_type, typename t_tyStringDest::value_type >
{
	_rstrDest = std::move( _rrstrSrc );
}
// The below will work when the source is a string view as well.
template < class t_tyStringDest, class t_tyStringSrc >
void ConvertString( t_tyStringDest & _rstrDest, t_tyStringSrc const & _rstrSrc ) 
	requires ( !TAreSameSizeTypes_v< typename t_tyStringSrc::value_type, typename t_tyStringDest::value_type > )
{
	ConvertString( _rstrDest, &_rstrSrc[0], _rstrSrc.length() );
}

// ASCII string "conversions": As long as the characters are less than 128 they can be "converted" in place:
// Throws on encountering a character 128 or greater.
template < class t_tyCharDest, class t_tyCharSrc >
void ConvertAsciiString( t_tyCharDest * _rgcBufDest, size_t _nBufDest, const t_tyCharSrc * _pcSrc, size_t _stLenSrc = (numeric_limits< size_t >::max)() )
{
	if ( (numeric_limits< size_t >::max)() == _stLenSrc )
		_stLenSrc = StrNLen( _pcSrc );
	else
		Assert( _stLenSrc == StrNLen( _pcSrc, _stLenSrc ) );
	size_t nCopy = (min)( _stLenSrc, _nBufDest-1 );
	const t_tyCharSrc * const pcsrcEnd = _pcSrc + nCopy;
	t_tyCharDest * pcdestCur = _rgcBufDest;
	for ( const t_tyCharSrc * pcsrcCur = _pcSrc; ( pcsrcEnd != pcsrcCur ); )
	{
		VerifyThrowSz( *pcsrcCur < t_tyCharDest(128), "This is a size conversion only - can't convert characters over 128." );
		*pcdestCur++ = (t_tyCharDest)*pcsrcCur++;
	}
	*pcdestCur = 0;
}

// StrConvertString:
template < class t_tyCharConvertTo, class t_tyCharConvertFrom >
basic_string< t_tyCharConvertTo > StrConvertString( const t_tyCharConvertFrom * _pc, size_t _len )
	requires TAreSameSizeTypes_v< t_tyCharConvertTo, t_tyCharConvertFrom >
{
	return basic_string< t_tyCharConvertTo >( (const t_tyCharConvertTo*)_pc, _len );
}
template < class t_tyCharConvertTo, class t_tyCharConvertFrom >
basic_string< t_tyCharConvertTo > StrConvertString( const t_tyCharConvertFrom * _pc, size_t _len )
	requires ( !TAreSameSizeTypes_v< t_tyCharConvertTo, t_tyCharConvertFrom > )
{
	basic_string< t_tyCharConvertTo > strConverted;
	ConvertString( strConverted, _pc, _len );
	return strConverted;
}
template < class t_tyCharConvertTo, class t_tyStringOrStringView >
basic_string< t_tyCharConvertTo > StrConvertString( t_tyStringOrStringView const & _rsvorstr )
{
	return StrConvertString< t_tyCharConvertTo >( &_rsvorstr[0], _rsvorstr.length() );
}

// trivial type for conversion buffer when no conversion necessary.
template < class t_tyCharConvertTo >
struct FakeConversionBuffer
{
};
template < class t_tyCharConvertFrom, class t_tyCharConvertTo >
struct TGetConversionBuffer
{
	typedef FakeConversionBuffer< t_tyCharConvertTo > _TyFakeBuffer;
	typedef basic_string< t_tyCharConvertTo > _TyRealBuffer;
	typedef conditional_t< TAreSameSizeTypes_v< t_tyCharConvertTo, t_tyCharConvertFrom >, _TyFakeBuffer, _TyRealBuffer > _TyBufferType;
};
template < CIsStringViewOrString t_TyStrViewOrStringConvertFrom, CIsStringViewOrString t_TyStrViewOrStringConvertTo >
struct TGetConversionBuffer< t_TyStrViewOrStringConvertFrom, t_TyStrViewOrStringConvertTo >
{
	typedef FakeConversionBuffer< typename t_TyStrViewOrStringConvertTo::value_type > _TyFakeBuffer;
	typedef basic_string< typename t_TyStrViewOrStringConvertTo::value_type > _TyRealBuffer;
	typedef conditional_t< TAreSameSizeTypes_v< typename t_TyStrViewOrStringConvertTo::value_type, typename t_TyStrViewOrStringConvertFrom::value_type >, _TyFakeBuffer, _TyRealBuffer > _TyBufferType;
};
template < class t_tyCharConvertFrom, class t_tyCharConvertTo >
using TGetConversionBuffer_t = typename TGetConversionBuffer< t_tyCharConvertFrom, t_tyCharConvertTo >::_TyBufferType;

// StrViewConvertString: Return a string view on a perhaps converted string (or not). Must pass a string buffer that may not be used.
template < class t_tyCharConvertFrom, class t_tyCharConvertTo >
basic_string_view< t_tyCharConvertTo > StrViewConvertString( const t_tyCharConvertFrom * _pc, size_t _len, FakeConversionBuffer< t_tyCharConvertTo > &  )
	requires TAreSameSizeTypes_v< t_tyCharConvertTo, t_tyCharConvertFrom >
{
	return basic_string_view< t_tyCharConvertTo >( (const t_tyCharConvertTo*)_pc, _len );
}
template < class t_tyCharConvertFrom, class t_tyCharConvertTo >
basic_string_view< t_tyCharConvertTo > StrViewConvertString( const t_tyCharConvertFrom * _pc, size_t _len, basic_string< t_tyCharConvertTo > & _strConvertBuf )
	requires ( !TAreSameSizeTypes_v< t_tyCharConvertTo, t_tyCharConvertFrom > )
{
	Assert( !!_pc || !_len );
	if ( !_pc || !_len )
		return basic_string_view< t_tyCharConvertTo >();
	ConvertString( _strConvertBuf, _pc, _len );
	return basic_string_view< t_tyCharConvertTo >( _strConvertBuf );
}
template < class t_tyStringOrStringViewFrom, class t_tyCharConvertTo >
basic_string_view< t_tyCharConvertTo > StrViewConvertString( t_tyStringOrStringViewFrom const & _rsvorstr, FakeConversionBuffer< t_tyCharConvertTo > & _strConvertBuf )
	requires ( TAreSameSizeTypes_v< t_tyCharConvertTo, typename t_tyStringOrStringViewFrom::value_type > )
{
	return StrViewConvertString( &_rsvorstr[0], _rsvorstr.length(), _strConvertBuf );
}
template < class t_tyStringOrStringViewFrom, class t_tyCharConvertTo >
basic_string_view< t_tyCharConvertTo > StrViewConvertString( t_tyStringOrStringViewFrom const & _rsvorstr, basic_string< t_tyCharConvertTo > & _strConvertBuf )
	requires ( !TAreSameSizeTypes_v< t_tyCharConvertTo, typename t_tyStringOrStringViewFrom::value_type > )
{
	return StrViewConvertString( &_rsvorstr[0], _rsvorstr.length(), _strConvertBuf );
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

// BOM detection stuff:
enum EFileCharacterEncoding
{
	efceUTF8,
	efceUTF16BE,
	efceUTF16LE,
	efceUTF32BE,
	efceUTF32LE,
	efceFileCharacterEncodingCount // This should be last.
};

// Return the encoding implemented by a set of types:
template < class t_tyChar, class t_tyFSwitchEndian >
EFileCharacterEncoding GetCharacterEncoding();
template <>
inline constexpr EFileCharacterEncoding GetCharacterEncoding< char8_t, false_type >()
{
	return efceUTF8;
}
template <>
inline constexpr EFileCharacterEncoding GetCharacterEncoding< char8_t, true_type >()
{
	return efceUTF8;
}
template <>
inline constexpr EFileCharacterEncoding GetCharacterEncoding< char, false_type >()
{
	return efceUTF8;
}
template <>
inline constexpr EFileCharacterEncoding GetCharacterEncoding< char, true_type >()
{
	return efceUTF8;
}
template <>
inline constexpr EFileCharacterEncoding GetCharacterEncoding< char16_t, vTyFIsLittleEndian >()
{
	return efceUTF16BE;
}
template <>
inline constexpr EFileCharacterEncoding GetCharacterEncoding< char16_t, vTyFIsBigEndian >()
{
	return efceUTF16LE;
}
template <>
inline constexpr EFileCharacterEncoding GetCharacterEncoding< char32_t, vTyFIsLittleEndian >()
{
	return efceUTF32BE;
}
template <>
inline constexpr EFileCharacterEncoding GetCharacterEncoding< char32_t, vTyFIsBigEndian >()
{
	return efceUTF32LE;
}
template <>
inline constexpr EFileCharacterEncoding GetCharacterEncoding< wchar_t, vTyFIsLittleEndian >()
{
#ifdef BIEN_WCHAR_16BIT
	return efceUTF16BE;
#else //!BIEN_WCHAR_16BIT
	return efceUTF32BE;
#endif //!BIEN_WCHAR_16BIT
}
template <>
inline constexpr EFileCharacterEncoding GetCharacterEncoding< wchar_t, vTyFIsBigEndian >()
{
#ifdef BIEN_WCHAR_16BIT
	return efceUTF16LE;
#else //!BIEN_WCHAR_16BIT
	return efceUTF32LE;
#endif //!BIEN_WCHAR_16BIT
}

// Get the corresponding encoding for this machine:
inline constexpr EFileCharacterEncoding GetEncodingThisMachine( EFileCharacterEncoding _efce )
{
	switch( _efce )
	{
		case efceUTF8:
			return efceUTF8;
		break;
		case efceUTF16BE:
			return vkfIsBigEndian ? efceUTF16BE : efceUTF16LE;
		break;
		case efceUTF16LE:
			return vkfIsLittleEndian ? efceUTF16LE : efceUTF16BE;
		break;
		case efceUTF32BE:
		{
			return vkfIsBigEndian ? efceUTF32BE : efceUTF32LE;
		}
		break;
		case efceUTF32LE:
			return vkfIsLittleEndian ? efceUTF32LE : efceUTF32BE;
		break;
		default:
			VerifyThrowSz( false, "Invalid EFileCharacterEncoding[%d]", (int)_efce );
		break;
	}
	return efceFileCharacterEncodingCount;
}

static const uint64_t vknBytesBOM = 4;
// GetCharacterEncodingFromBOM:
// Detect if there is a BOM present and if so return it. _ru64Len should be at least 4 bytes.
// Upon return _ru64Len is set to the length of the BOM if a valid BOM is found or zero is no BOM is found.
// If no valid BOM is found then efceFileCharacterEncodingCount is returned.
inline EFileCharacterEncoding GetCharacterEncodingFromBOM( uint8_t * _pbyBufFileBegin, uint64_t & _ru64Len )
{
	Assert( _ru64Len >= vknBytesBOM );
	VerifyThrowSz( _ru64Len >= vknBytesBOM, "Requires vknBytesBOM(%lu) of file to determine BOM.", vknBytesBOM );

	if ( ( 0xEF == _pbyBufFileBegin[0] ) && ( 0xBB == _pbyBufFileBegin[1] ) && ( 0xBF == _pbyBufFileBegin[2] ) )
	{
		_ru64Len = 3;
		return efceUTF8;
	}
	if ( ( 0xFF == _pbyBufFileBegin[0] ) && ( 0xFE == _pbyBufFileBegin[1] ) )
	{
		if ( ( 0x00 == _pbyBufFileBegin[2] ) && ( 0x00 == _pbyBufFileBegin[3] ) )
		{
			_ru64Len = 4;
			return efceUTF32LE;
		}
		else
		{
			_ru64Len = 2;
			return efceUTF16LE;
		}
	}
	if ( ( 0xFE == _pbyBufFileBegin[0] ) && ( 0xFF == _pbyBufFileBegin[1] ) )
	{
		_ru64Len = 2;
		return efceUTF16BE;
	}
	if ( ( 0x00 == _pbyBufFileBegin[0] ) && ( 0x00 == _pbyBufFileBegin[1] ) && ( 0xFE == _pbyBufFileBegin[2] ) && ( 0xFF == _pbyBufFileBegin[3] ) )
	{
		_ru64Len = 4;
		return efceUTF32BE;
	}
	_ru64Len = 0;
	return efceFileCharacterEncodingCount;
}

// DetectEncodingXmlFile:
// If the above GetCharacterEncodingFromBOM() fails then we can try to detect the encoding using the fact that the first character in an XML file is an '<'.
inline EFileCharacterEncoding DetectEncodingXmlFile( uint8_t * _pbyBufFileBegin, uint64_t _u64Len )
{
	Assert( _u64Len >= vknBytesBOM );
	if ( _u64Len < vknBytesBOM )
			return efceFileCharacterEncodingCount;
	if ( ( '<' == _pbyBufFileBegin[0] ) && ( 0x00 != _pbyBufFileBegin[1] ) && ( 0x00 != _pbyBufFileBegin[2] ) )
			return efceUTF8;
	if ( ( 0x00 == _pbyBufFileBegin[0] ) && ( '<' == _pbyBufFileBegin[1] ) && ( 0x00 == _pbyBufFileBegin[2] ) )
			return efceUTF16BE;
	if ( ( '<' == _pbyBufFileBegin[0] ) && ( 0x00 == _pbyBufFileBegin[1] ) && ( 0x00 != _pbyBufFileBegin[2] ) )
			return efceUTF16LE;
	if ( ( 0x00 == _pbyBufFileBegin[0] ) && ( 0x00 == _pbyBufFileBegin[1] ) && ( 0x00 == _pbyBufFileBegin[2] ) && ( '<' == _pbyBufFileBegin[3] ) )
			return efceUTF32BE;
	if ( ( '<' == _pbyBufFileBegin[0] ) && ( 0x00 == _pbyBufFileBegin[1] ) && ( 0x00 == _pbyBufFileBegin[2] ) && ( 0x00 == _pbyBufFileBegin[3] ) )
			return efceUTF32LE;
	return efceFileCharacterEncodingCount;
}
inline string StrGetBOMForEncoding( EFileCharacterEncoding _efce )
{
	switch( _efce )
	{
		case efceUTF8:
			return "\xEF\xBB\xBF";
		break;
		case efceUTF16BE:
			return "\xFE\xFF";
		break;
		case efceUTF16LE:
			return "\xFF\xFE";
		break;
		case efceUTF32BE:
		{
			return string( "\x00\x00\xFE\xFF", 4 );
		}
		break;
		case efceUTF32LE:
			return string( "\xFF\xFE\x00\x00", 4 );
		break;
		default:
			VerifyThrowSz( false, "Invalid EFileCharacterEncoding[%d]", (int)_efce );
		break;
	}
	return string();
}

template < class t_tyChar, class t_tyFSwitchEndian >
void WriteBOM( vtyFileHandle _hFile )
{
	EFileCharacterEncoding efce = GetCharacterEncoding< t_tyChar, t_tyFSwitchEndian >();
	Assert( efceFileCharacterEncodingCount != efce );
	VerifyThrowSz( efceFileCharacterEncodingCount != efce, "Unknown char/switch endian encoding." );
	string strBOM = StrGetBOMForEncoding( efce );
	FileWriteOrThrow( _hFile, strBOM.c_str(), strBOM.length() );
}

inline const char *
PszCharacterEncodingShort( EFileCharacterEncoding _efce )
{
	switch( _efce )
	{
		case efceUTF8:
			return "UTF8";
		break;
		case efceUTF16BE:
			return "UTF16BE";
		break;
		case efceUTF16LE:
			return "UTF16LE";
		break;
		case efceUTF32BE:
			return "UTF32BE";
		break;
		case efceUTF32LE:
			return "UTF32LE";
		break;
		default:
			VerifyThrowSz( false, "Invalid EFileCharacterEncoding[%d]", (int)_efce );
		break;
	}
	return "wont get here"; // lol - compiler thinks so.
}

// PszCharacterEncodingName() can't distiguish between small and big endian.
// The byte order mark of a file manages that for encodings in XML.
template < class t_tyChar >
auto SvCharacterEncodingName( EFileCharacterEncoding _efce ) -> basic_string_view< t_tyChar >
{
	typedef t_tyChar _TyChar;
	switch( _efce )
	{
		case efceUTF8:
		{
			static basic_string< t_tyChar > strEN( str_array_cast< _TyChar >( "UTF-8" ).c_str() );
			return strEN;
		}
		break;
		case efceUTF16BE:
		case efceUTF16LE:
		{
			static basic_string< t_tyChar > strEN( str_array_cast< _TyChar >( "UTF-16" ).c_str() );
			return strEN;
		}
		break;
		case efceUTF32BE:
		case efceUTF32LE:
		{
			static basic_string< t_tyChar > strEN( str_array_cast< _TyChar >( "UTF-32" ).c_str() );
			return strEN;
		}
		break;
		default:
			VerifyThrowSz( false, "Invalid EFileCharacterEncoding[%d]", (int)_efce );
		break;
	}
	return basic_string_view< t_tyChar >();
}

// ConvertFileMapped:
// Take a file handle, map it at its current position, then convert to the character type given and write it to _pszFileOutput, potentially switching endian and adding a BOM.
// There shouldn't be a BOM in the file handle passed - i.e. something else should have read that BOM if it was present and then called this method.
inline void ConvertFileMapped( vtyFileHandle _hFileSrc, EFileCharacterEncoding _efceSrc, const char * _pszFileNameDest, EFileCharacterEncoding _efceDst, bool _fAddBOM )
{
	// Map the source at its current seek point - this might be just past the BOM or somewhere in the middle of the file.
	uint64_t nbySizeSrc;
	uint64_t u64MapAtPostion = (size_t)NFileSeekAndThrow(_hFileSrc, 0, vkSeekCur);
	FileMappingObj fmoSrc( MapReadOnlyHandle( _hFileSrc, &nbySizeSrc, &u64MapAtPostion ) );
	VerifyThrowSz( fmoSrc.FIsOpen(), "Couldn't map source file." );
	void * pvMapped = fmoSrc.Pby( (size_t)u64MapAtPostion ); // u64MapAtPostion is updated by MapReadOnlyHandle to be less than a page size.
	nbySizeSrc -= u64MapAtPostion;
	VerifyThrowSz( nbySizeSrc < (size_t)(std::numeric_limits<ssize_t>::max)(), "Source file is too large to be converted."); // Limit appropriately under 32bit.

	FileObj foDst( CreateWriteOnlyFile( _pszFileNameDest ) );
	VerifyThrowSz( foDst.FIsOpen(), "Couldn't create file[%s].", _pszFileNameDest );
	// If we are to write the BOM then write it now:
	if ( _fAddBOM )
	{
		string strBOM = StrGetBOMForEncoding( _efceDst );
		FileWriteOrThrow( foDst.HFileGet(), &strBOM[0], strBOM.length() );
	}
	// If there is no change in encoding:
	if ( _efceSrc == _efceDst )
	{
		// Then just write the source to the dest:
		return FileWriteOrThrow( foDst.HFileGet(), pvMapped, nbySizeSrc );
	}

	if ( ( efceUTF16BE == _efceSrc ) || ( efceUTF16LE == _efceSrc ) )
	{
		VerifyThrowSz( !( nbySizeSrc % sizeof(char16_t) ), "Source file is not a integral number of char16_t characters - something is fishy." );
		// We're just gonna use a string to convert the file. It could be huge and we might run out of memory but I ain't solving that problem in this method.
		basic_string< char16_t > strSrcData( (char16_t*)pvMapped, (size_t)nbySizeSrc / sizeof(char16_t) );
		if ( _efceSrc != GetEncodingThisMachine( _efceSrc ) )
		{
			// Then we must switch the endian before doing anything with it.
			SwitchEndian( &strSrcData[0], &strSrcData[0] + strSrcData.length() );
			_efceSrc = GetEncodingThisMachine( _efceSrc );
		}
		if ( _efceSrc == _efceDst )
		{
			// Then just write the source to the dest:
			return FileWriteOrThrow( foDst.HFileGet(), &strSrcData[0], strSrcData.length() * sizeof( char16_t ) );
		}
		// Conversion must occur:
		if ( efceUTF8 == _efceDst )
		{
			basic_string< char8_t > strConverted;
			ConvertString( strConverted, strSrcData );
			return FileWriteOrThrow( foDst.HFileGet(), &strConverted[0], strConverted.length() * sizeof( char8_t ) );
		}
		else
		{
			// Then UTF32BE or LE. First must convert to UTF32 anyway.
			basic_string< char32_t > strConverted;
			ConvertString( strConverted, strSrcData );
			if ( _efceDst != GetEncodingThisMachine( _efceDst ) )
				SwitchEndian( &strConverted[0], &strConverted[0] + strConverted.length() );
			return FileWriteOrThrow( foDst.HFileGet(), &strConverted[0], strConverted.length() * sizeof( char32_t ) );
		}
	}
	if ( ( efceUTF32BE == _efceSrc ) || ( efceUTF32LE == _efceSrc ) )
	{
		VerifyThrowSz( !( nbySizeSrc % sizeof(char32_t) ), "Source file is not a integral number of char32_t characters - something is fishy." );
		// We're just gonna use a string to convert the file. It could be huge and we might run out of memory but I ain't solving that problem in this method.
		basic_string< char32_t > strSrcData( (char32_t*)pvMapped, (size_t)nbySizeSrc / sizeof(char32_t) );
		if ( _efceSrc != GetEncodingThisMachine( _efceSrc ) )
		{
			// Then we must switch the endian before doing anything with it.
			SwitchEndian( &strSrcData[0], &strSrcData[0] + strSrcData.length() );
			_efceSrc = GetEncodingThisMachine( _efceSrc );
		}
		if ( _efceSrc == _efceDst )
		{
			// Then just write the source to the dest:
			return FileWriteOrThrow( foDst.HFileGet(), &strSrcData[0], strSrcData.length() * sizeof( char32_t ) );
		}
		// Conversion must occur:
		if ( efceUTF8 == _efceDst )
		{
			basic_string< char8_t > strConverted;
			ConvertString( strConverted, strSrcData );
			return FileWriteOrThrow( foDst.HFileGet(), &strConverted[0], strConverted.length() * sizeof( char8_t ) );
		}
		else
		{
			// Then UTF16BE or LE. First must convert to UTF16 anyway.
			basic_string< char16_t > strConverted;
			ConvertString( strConverted, strSrcData );
			if ( _efceDst != GetEncodingThisMachine( _efceDst ) )
				SwitchEndian( &strConverted[0], &strConverted[0] + strConverted.length() );
			return FileWriteOrThrow( foDst.HFileGet(), &strConverted[0], strConverted.length() * sizeof( char16_t ) );
		}
	}
	Assert( efceUTF8 == _efceSrc );
	VerifyThrowSz( efceUTF8 == _efceSrc, "Unknown encoding _efceSrc[%u].", _efceSrc );
	basic_string_view< char8_t > svSrcData( (char8_t*)pvMapped, (size_t)nbySizeSrc );
	if ( ( efceUTF16BE == _efceDst ) || ( efceUTF16LE == _efceDst ) )
	{
		basic_string< char16_t > strConverted;
		ConvertString( strConverted, svSrcData );
		if ( _efceDst != GetEncodingThisMachine( _efceDst ) )
			SwitchEndian( &strConverted[0], &strConverted[0] + strConverted.length() );
		return FileWriteOrThrow( foDst.HFileGet(), &strConverted[0], strConverted.length() * sizeof( char16_t ) );
	}
	else
	{
		basic_string< char32_t > strConverted;
		ConvertString( strConverted, svSrcData );
		if ( _efceDst != GetEncodingThisMachine( _efceDst ) )
			SwitchEndian( &strConverted[0], &strConverted[0] + strConverted.length() );
		return FileWriteOrThrow( foDst.HFileGet(), &strConverted[0], strConverted.length() * sizeof( char32_t ) );
	}
	// listo.
}

__BIENUTIL_END_NAMESPACE

#if defined( __linux__ ) || defined( __APPLE__ )
// Support for this is spotty and ever-changing but growing...
namespace std
{
	// neither VC or clang define basic_string_view::operator <=>():
	template <class _Elem, class _Traits>
	constexpr strong_ordering operator<=>( const basic_string_view<_Elem, _Traits> _Lhs, const basic_string_view<_Elem, _Traits> _Rhs ) noexcept 
	{
		int iComp = _Lhs.compare(_Rhs);
		return ( iComp < 0 ) ? strong_ordering::less : ( ( iComp > 0 ) ? strong_ordering::greater : strong_ordering::equal );
	}
} // namespace std
#endif // _MSC_VER || __APPLE__