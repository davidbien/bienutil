#pragma once

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// jsonstrm.h
// Templates to implement JSON input/output streaming. STAX design allows streaming of very large JSON files.
// dbien: 17FEB2020

#include <string>
#include <ostream>
#include <memory>
#include <compare>
#include <utility>
#ifndef WIN32
#include <unistd.h>
#include <sys/mman.h>
#include <uuid/uuid.h>
#endif //!WIN32
#include <stdexcept>
#include <fcntl.h>
#include <limits.h>

#include "bienutil.h"
#include "_compat.h"
#include "bientypes.h"
#include "_namdexc.h"
#include "_debug.h"
#include "_util.h"
#include "_fdobjs.h"
#include "_timeutl.h"
#include "_dbgthrw.h"
#include "memfile.h"
#include "syslogmgr.h"
#include "strwrsv.h"

// jsonstrm.h
// This implements JSON streaming in/out.
// dbien: 16FEB2020

// Goals:
//  1) Allow multiple files types but templatize rather than using virtual methods to allow more inlining.
//  2) Stax parser for the input.
//      a) Should allow very large files to be perused without much internal memory usage.
//      b) Read straight into internal data structures via stax streaming rather than using intermediate format.
//      c) Even array values should be streamed in.
//      d) Lazily read values upon demand - but read once and store. This allows skipping around and ignoring data - still have to read it but don't have to store it.
//  3) Output goes to any streaming output object.
//  4) Since output and input rarely if ever mixed no reason to provide both at the same time.

__BIENUTIL_BEGIN_NAMESPACE

// struct JsonCharTraits: Only specializations of this.
// This describes the type of file that the JSON files is persisted within.
template <class t_tyChar>
struct JsonCharTraits;
template <class t_tyCharTraits>
class JsonValue;
template <class t_tyCharTraits>
class JsonObject;
template <class t_tyCharTraits>
class JsonArray;
template <class t_tyJsonInputStream>
class JsonReadCursor;
template <class t_tyCharTraits>
class JsonFormatSpec;

#ifndef __JSONSTRM_DEFAULT_ALLOCATOR
#define __JSONSTRM_DEFAULT_ALLOCATOR __STD::allocator<char>
#define __JSONSTRM_GET_ALLOCATOR(t) __STD::allocator<t>
#endif //__JSONSTRM_DEFAULT_ALLOCATOR

// This exception will get thrown when there is an issue with the JSON stream.
template <class t_tyCharTraits>
class bad_json_stream_exception : public _t__Named_exception_errno<__JSONSTRM_DEFAULT_ALLOCATOR>
{
  typedef _t__Named_exception_errno<__JSONSTRM_DEFAULT_ALLOCATOR> _tyBase;

public:
  typedef t_tyCharTraits _tyCharTraits;

  bad_json_stream_exception(const string_type &__s, vtyErrNo _nErrno = 0)
      : _tyBase(__s, _nErrno)
  {
  }
  bad_json_stream_exception(const char *_pcFmt, va_list args)
  {
    RenderVA(_pcFmt, args);
  }
  bad_json_stream_exception(int _errno, const char *_pcFmt, va_list args)
      : _tyBase(_errno)
  {
    RenderVA(_pcFmt, args);
  }
  void RenderVA(const char *_pcFmt, va_list args)
  {
    // When we see %TC we should substitute the formating for the type of characters in t_tyCharTraits.
    const int knBuf = NAMEDEXC_BUFSIZE;
    char rgcBuf[knBuf + 1];
    char *pcBufCur = rgcBuf;
    char *const pcBufTail = rgcBuf + NAMEDEXC_BUFSIZE;
    for (const char *pcFmtCur = _pcFmt; !!*pcFmtCur && (pcBufCur != pcBufTail); ++pcFmtCur)
    {
      if ((pcFmtCur[0] == '%') && (pcFmtCur[1] == 'T') && (pcFmtCur[2] == 'C'))
      {
        pcFmtCur += 2;
        const char *pcCharFmtCur = _tyCharTraits::s_szFormatChar;
        for (; !!*pcCharFmtCur && (pcBufCur != pcBufTail); ++pcCharFmtCur)
          *pcBufCur++ = *pcCharFmtCur;
      }
      else
        *pcBufCur++ = *pcFmtCur;
    }
    *pcBufCur = 0;

    _tyBase::RenderVA(rgcBuf, args); // Render into the exception description buffer.
  }
};
// By default we will always add the __FILE__, __LINE__ even in retail for debugging purposes.
#define THROWBADJSONSTREAM(MESG, ...) ExceptionUsage<bad_json_stream_exception<_tyCharTraits>>::ThrowFileLineFunc(__FILE__, __LINE__, FUNCTION_PRETTY_NAME, MESG, ##__VA_ARGS__)
#define THROWBADJSONSTREAMERRNO(ERRNO, MESG, ...) ExceptionUsage<bad_json_stream_exception<_tyCharTraits>>::ThrowFileLineFuncErrno(__FILE__, __LINE__, FUNCTION_PRETTY_NAME, ERRNO, MESG,##__VA_ARGS__)

// This exception will get thrown if the user of the read cursor does something inappropriate given the current context.
template <class t_tyCharTraits>
class json_stream_bad_semantic_usage_exception : public _t__Named_exception<__JSONSTRM_DEFAULT_ALLOCATOR>
{
  typedef json_stream_bad_semantic_usage_exception _tyThis;
  typedef _t__Named_exception<__JSONSTRM_DEFAULT_ALLOCATOR> _tyBase;

public:
  typedef t_tyCharTraits _tyCharTraits;

  json_stream_bad_semantic_usage_exception(const char *_pc)
      : _tyBase(_pc)
  {
  }
  json_stream_bad_semantic_usage_exception(const string_type &__s)
      : _tyBase(__s)
  {
  }
  json_stream_bad_semantic_usage_exception(const char *_pcFmt, va_list args)
  {
    // When we see %TC we should substitute the formating for the type of characters in t_tyCharTraits.
    const int knBuf = NAMEDEXC_BUFSIZE;
    char rgcBuf[knBuf + 1];
    char *pcBufCur = rgcBuf;
    char *const pcBufTail = rgcBuf + NAMEDEXC_BUFSIZE;
    for (const char *pcFmtCur = _pcFmt; !!*pcFmtCur && (pcBufCur != pcBufTail); ++pcFmtCur)
    {
      if ((pcFmtCur[0] == '%') && (pcFmtCur[1] == 'T') && (pcFmtCur[2] == 'C'))
      {
        pcFmtCur += 2;
        const char *pcCharFmtCur = _tyCharTraits::s_szFormatChar;
        for (; !!*pcCharFmtCur && (pcBufCur != pcBufTail); ++pcCharFmtCur)
          *pcBufCur++ = *pcCharFmtCur;
      }
      else
        *pcBufCur++ = *pcFmtCur;
    }
    *pcBufCur = 0;

    RenderVA(rgcBuf, args); // Render into the exception description buffer.
  }
};
// By default we will always add the __FILE__, __LINE__ even in retail for debugging purposes.
#define THROWBADJSONSEMANTICUSE(MESG, ...) ExceptionUsage<json_stream_bad_semantic_usage_exception<_tyCharTraits>>::ThrowFileLineFunc(__FILE__, __LINE__, FUNCTION_PRETTY_NAME, MESG, ##__VA_ARGS__)

// JsonCharTraits< char > : Traits for 8bit char representation.
template <>
struct JsonCharTraits<char>
{
  typedef char _tyChar;
  typedef char _tyPersistAsChar;
  typedef _tyChar *_tyLPSTR;
  typedef const _tyChar *_tyLPCSTR;
  typedef StrWRsv<std::basic_string<_tyChar>> _tyStdStr;

  static const bool s_fThrowOnUnicodeOverflow = true; // By default we throw when a unicode character value overflows the representation.

  static const _tyChar s_tcLeftSquareBr = '[';
  static const _tyChar s_tcRightSquareBr = ']';
  static const _tyChar s_tcLeftCurlyBr = '{';
  static const _tyChar s_tcRightCurlyBr = '}';
  static const _tyChar s_tcColon = ':';
  static const _tyChar s_tcComma = ',';
  static const _tyChar s_tcPeriod = '.';
  static const _tyChar s_tcDoubleQuote = '"';
  static const _tyChar s_tcBackSlash = '\\';
  static const _tyChar s_tcForwardSlash = '/';
  static const _tyChar s_tcMinus = '-';
  static const _tyChar s_tcPlus = '+';
  static const _tyChar s_tc0 = '0';
  static const _tyChar s_tc1 = '1';
  static const _tyChar s_tc2 = '2';
  static const _tyChar s_tc3 = '3';
  static const _tyChar s_tc4 = '4';
  static const _tyChar s_tc5 = '5';
  static const _tyChar s_tc6 = '6';
  static const _tyChar s_tc7 = '7';
  static const _tyChar s_tc8 = '8';
  static const _tyChar s_tc9 = '9';
  static const _tyChar s_tca = 'a';
  static const _tyChar s_tcb = 'b';
  static const _tyChar s_tcc = 'c';
  static const _tyChar s_tcd = 'd';
  static const _tyChar s_tce = 'e';
  static const _tyChar s_tcf = 'f';
  static const _tyChar s_tcA = 'A';
  static const _tyChar s_tcB = 'B';
  static const _tyChar s_tcC = 'C';
  static const _tyChar s_tcD = 'D';
  static const _tyChar s_tcE = 'E';
  static const _tyChar s_tcF = 'F';
  static const _tyChar s_tct = 't';
  static const _tyChar s_tcr = 'r';
  static const _tyChar s_tcu = 'u';
  static const _tyChar s_tcl = 'l';
  static const _tyChar s_tcs = 's';
  static const _tyChar s_tcn = 'n';
  static const _tyChar s_tcBackSpace = '\b';
  static const _tyChar s_tcFormFeed = '\f';
  static const _tyChar s_tcNewline = '\n';
  static const _tyChar s_tcCarriageReturn = '\r';
  static const _tyChar s_tcTab = '\t';
  static const _tyChar s_tcSpace = ' ';
  static const _tyChar s_tcDollarSign = '$';
  static constexpr _tyLPCSTR s_szCRLF = "\r\n";
  static const _tyChar s_tcFirstUnprintableChar = '\01';
  static const _tyChar s_tcLastUnprintableChar = '\37';
  static constexpr _tyLPCSTR s_szPrintableWhitespace = "\t\n\r";
  static constexpr _tyLPCSTR s_szEscapeStringChars = "\\\"\b\f\t\n\r";

  // The formatting command to format a single character using printf style.
  static constexpr const char *s_szFormatChar = "%c";

  static const int s_knMaxNumberLength = 512; // Yeah this is absurd but why not?

  static bool FIsWhitespace(_tyChar _tc)
  {
    return ((' ' == _tc) || ('\t' == _tc) || ('\n' == _tc) || ('\r' == _tc));
  }
  static bool FIsIllegalChar(_tyChar _tc)
  {
    return !_tc; // we could expand this...
  }
  static size_t StrLen(_tyLPCSTR _psz)
  {
    return strlen(_psz);
  }
  static size_t StrNLen(_tyLPCSTR _psz, size_t _stLen = (std::numeric_limits<size_t>::max)())
  {
    return __BIENUTIL_NAMESPACE StrNLen(_psz, _stLen);
  }
  static size_t StrCSpn(_tyLPCSTR _psz, _tyLPCSTR _pszCharSet)
  {
    return strcspn(_psz, _pszCharSet);
  }
  static size_t StrCSpn(_tyLPCSTR _psz, _tyChar _tcBegin, _tyChar _tcEnd, _tyLPCSTR _pszCharSet)
  {
    return __BIENUTIL_NAMESPACE StrCSpn(_psz, _tcBegin, _tcEnd, _pszCharSet);
  }
  static size_t StrRSpn(_tyLPCSTR _pszBegin, _tyLPCSTR _pszEnd, _tyLPCSTR _pszSet)
  {
    return __BIENUTIL_NAMESPACE StrRSpn(_pszBegin, _pszEnd, _pszSet);
  }
  static void MemSet(_tyLPSTR _psz, _tyChar _tc, size_t _n)
  {
    (void)memset(_psz, _tc, _n);
  }
  static int Snprintf(_tyLPSTR _psz, size_t _n, _tyLPCSTR _pszFmt, ...)
  {
    va_list ap;
    va_start(ap, _pszFmt);
    int iRet = ::vsnprintf(_psz, _n, _pszFmt, ap);
    va_end(ap);
    VerifyThrow(iRet >= 0);
    return iRet;
  }
};

// JsonCharTraits< char8_t >:
template <>
struct JsonCharTraits< char8_t >
{
  typedef char8_t _tyChar;
  typedef char8_t _tyPersistAsChar; // This is a hint and each file object can persist as it likes.
  typedef char _tyCharStd; // The char we need to convert to to call standard string methods.
  typedef _tyChar *_tyLPSTR;
  typedef const _tyChar *_tyLPCSTR;
  typedef StrWRsv<std::basic_string<_tyChar>> _tyStdStr;

  static const bool s_fThrowOnUnicodeOverflow = true; // By default we throw when a unicode character value overflows the representation.

  static const _tyChar s_tcLeftSquareBr = u8'[';
  static const _tyChar s_tcRightSquareBr = u8']';
  static const _tyChar s_tcLeftCurlyBr = u8'{';
  static const _tyChar s_tcRightCurlyBr = u8'}';
  static const _tyChar s_tcColon = u8':';
  static const _tyChar s_tcComma = u8',';
  static const _tyChar s_tcPeriod = u8'.';
  static const _tyChar s_tcDoubleQuote = u8'"';
  static const _tyChar s_tcBackSlash = u8'\\';
  static const _tyChar s_tcForwardSlash = u8'/';
  static const _tyChar s_tcMinus = u8'-';
  static const _tyChar s_tcPlus = u8'+';
  static const _tyChar s_tc0 = u8'0';
  static const _tyChar s_tc1 = u8'1';
  static const _tyChar s_tc2 = u8'2';
  static const _tyChar s_tc3 = u8'3';
  static const _tyChar s_tc4 = u8'4';
  static const _tyChar s_tc5 = u8'5';
  static const _tyChar s_tc6 = u8'6';
  static const _tyChar s_tc7 = u8'7';
  static const _tyChar s_tc8 = u8'8';
  static const _tyChar s_tc9 = u8'9';
  static const _tyChar s_tca = u8'a';
  static const _tyChar s_tcb = u8'b';
  static const _tyChar s_tcc = u8'c';
  static const _tyChar s_tcd = u8'd';
  static const _tyChar s_tce = u8'e';
  static const _tyChar s_tcf = u8'f';
  static const _tyChar s_tcA = u8'A';
  static const _tyChar s_tcB = u8'B';
  static const _tyChar s_tcC = u8'C';
  static const _tyChar s_tcD = u8'D';
  static const _tyChar s_tcE = u8'E';
  static const _tyChar s_tcF = u8'F';
  static const _tyChar s_tct = u8't';
  static const _tyChar s_tcr = u8'r';
  static const _tyChar s_tcu = u8'u';
  static const _tyChar s_tcl = u8'l';
  static const _tyChar s_tcs = u8's';
  static const _tyChar s_tcn = u8'n';
  static const _tyChar s_tcBackSpace = u8'\b';
  static const _tyChar s_tcFormFeed = u8'\f';
  static const _tyChar s_tcNewline = u8'\n';
  static const _tyChar s_tcCarriageReturn = u8'\r';
  static const _tyChar s_tcTab = u8'\t';
  static const _tyChar s_tcSpace = u8' ';
  static const _tyChar s_tcDollarSign = u8'$';
  static constexpr _tyLPCSTR s_szCRLF = u8"\r\n";
  static const _tyChar s_tcFirstUnprintableChar = u8'\01';
  static const _tyChar s_tcLastUnprintableChar = u8'\37';
  static constexpr _tyLPCSTR s_szPrintableWhitespace = u8"\t\n\r";
  static constexpr _tyLPCSTR s_szEscapeStringChars = u8"\\\"\b\f\t\n\r";
  // The formatting command to format a single character using printf style.
  static constexpr const char *s_szFormatChar = "%c";

  static const int s_knMaxNumberLength = 512; // Yeah this is absurd but why not?

  static bool FIsWhitespace(_tyChar _tc)
  {
    return ((u8' ' == _tc) || (u8'\t' == _tc) || (u8'\n' == _tc) || (u8'\r' == _tc));
  }
  static bool FIsIllegalChar(_tyChar _tc)
  {
    return !_tc;
  }
  static size_t StrLen(_tyLPCSTR _psz)
  {
    return strlen((const _tyCharStd*)_psz);
  }
  static size_t StrNLen(_tyLPCSTR _psz, size_t _stLen = (std::numeric_limits<size_t>::max)())
  {
    return __BIENUTIL_NAMESPACE StrNLen(_psz, _stLen);
  }
  static size_t StrCSpn(_tyLPCSTR _psz, _tyLPCSTR _pszCharSet)
  {
    return strcspn((const _tyCharStd*)_psz, (const _tyCharStd*)_pszCharSet);
  }
  static size_t StrCSpn(_tyLPCSTR _psz, _tyChar _tcBegin, _tyChar _tcEnd, _tyLPCSTR _pszCharSet)
  {
    return __BIENUTIL_NAMESPACE StrCSpn(_psz, _tcBegin, _tcEnd, _pszCharSet);
  }
  static size_t StrRSpn(_tyLPCSTR _pszBegin, _tyLPCSTR _pszEnd, _tyLPCSTR _pszSet)
  {
    return __BIENUTIL_NAMESPACE StrRSpn(_pszBegin, _pszEnd, _pszSet);
  }
  static void MemSet(_tyLPSTR _psz, _tyChar _tc, size_t _n)
  {
    (void)memset(_psz, _tc, _n);
  }
  static int Snprintf(_tyLPSTR _psz, size_t _n, _tyLPCSTR _pszFmt, ...)
  {
    va_list ap;
    va_start(ap, _pszFmt);
    int iRet = ::vsnprintf((_tyCharStd*)_psz, _n, (const _tyCharStd*)_pszFmt, ap);
    va_end(ap);
    VerifyThrow(iRet >= 0);
    return iRet;
  }
};

// JsonCharTraits< char16_t > : Traits for 16bit char representation. Uses the Windows wc...() methods where possible.
template <>
struct JsonCharTraits<char16_t>
{
  typedef char16_t _tyChar;
  typedef char16_t _tyPersistAsChar; // This is a hint and each file object can persist as it likes.
#ifdef BIEN_WCHAR_16BIT
  typedef wchar_t _tyCharStd; // The char we need to convert to to call standard string methods.
#endif //BIEN_WCHAR_16BIT
  typedef _tyChar *_tyLPSTR;
  typedef const _tyChar *_tyLPCSTR;
  typedef StrWRsv<std::basic_string<_tyChar>> _tyStdStr;

  static const bool s_fThrowOnUnicodeOverflow = true; // By default we throw when a unicode character value overflows the representation.

  static const _tyChar s_tcLeftSquareBr = u'[';
  static const _tyChar s_tcRightSquareBr = u']';
  static const _tyChar s_tcLeftCurlyBr = u'{';
  static const _tyChar s_tcRightCurlyBr = u'}';
  static const _tyChar s_tcColon = u':';
  static const _tyChar s_tcComma = u',';
  static const _tyChar s_tcPeriod = u'.';
  static const _tyChar s_tcDoubleQuote = u'"';
  static const _tyChar s_tcBackSlash = u'\\';
  static const _tyChar s_tcForwardSlash = u'/';
  static const _tyChar s_tcMinus = u'-';
  static const _tyChar s_tcPlus = u'+';
  static const _tyChar s_tc0 = u'0';
  static const _tyChar s_tc1 = u'1';
  static const _tyChar s_tc2 = u'2';
  static const _tyChar s_tc3 = u'3';
  static const _tyChar s_tc4 = u'4';
  static const _tyChar s_tc5 = u'5';
  static const _tyChar s_tc6 = u'6';
  static const _tyChar s_tc7 = u'7';
  static const _tyChar s_tc8 = u'8';
  static const _tyChar s_tc9 = u'9';
  static const _tyChar s_tca = u'a';
  static const _tyChar s_tcb = u'b';
  static const _tyChar s_tcc = u'c';
  static const _tyChar s_tcd = u'd';
  static const _tyChar s_tce = u'e';
  static const _tyChar s_tcf = u'f';
  static const _tyChar s_tcA = u'A';
  static const _tyChar s_tcB = u'B';
  static const _tyChar s_tcC = u'C';
  static const _tyChar s_tcD = u'D';
  static const _tyChar s_tcE = u'E';
  static const _tyChar s_tcF = u'F';
  static const _tyChar s_tct = u't';
  static const _tyChar s_tcr = u'r';
  static const _tyChar s_tcu = u'u';
  static const _tyChar s_tcl = u'l';
  static const _tyChar s_tcs = u's';
  static const _tyChar s_tcn = u'n';
  static const _tyChar s_tcBackSpace = u'\b';
  static const _tyChar s_tcFormFeed = u'\f';
  static const _tyChar s_tcNewline = u'\n';
  static const _tyChar s_tcCarriageReturn = u'\r';
  static const _tyChar s_tcTab = u'\t';
  static const _tyChar s_tcSpace = u' ';
  static const _tyChar s_tcDollarSign = u'$';
  static constexpr _tyLPCSTR s_szCRLF = u"\r\n";
  static const _tyChar s_tcFirstUnprintableChar = u'\01';
  static const _tyChar s_tcLastUnprintableChar = u'\37';
  static constexpr _tyLPCSTR s_szPrintableWhitespace = u"\t\n\r";
  static constexpr _tyLPCSTR s_szEscapeStringChars = u"\\\"\b\f\t\n\r";
  // The formatting command to format a single character using printf style.
  static constexpr const char *s_szFormatChar = "%lc";

  static const int s_knMaxNumberLength = 512; // Yeah this is absurd but why not?

  static bool FIsWhitespace(_tyChar _tc)
  {
    return ((u' ' == _tc) || (u'\t' == _tc) || (u'\n' == _tc) || (u'\r' == _tc));
  }
  static bool FIsIllegalChar(_tyChar _tc)
  {
    return !_tc;
  }
  static size_t StrLen(_tyLPCSTR _psz)
  {
#ifdef BIEN_WCHAR_16BIT
    return wcslen((_tyCharStd*)_psz);
#else //!BIEN_WCHAR_16BIT
    return __BIENUTIL_NAMESPACE StrNLen(_psz);
#endif //!BIEN_WCHAR_16BIT
  }
  static size_t StrNLen(_tyLPCSTR _psz, size_t _stLen = (std::numeric_limits<size_t>::max)())
  {
    return __BIENUTIL_NAMESPACE StrNLen(_psz, _stLen);
  }
  static size_t StrCSpn(_tyLPCSTR _psz, _tyLPCSTR _pszCharSet)
  {
#ifdef BIEN_WCHAR_16BIT
    return wcscspn((const _tyCharStd*)_psz, (const _tyCharStd*)_pszCharSet);
#else //!BIEN_WCHAR_16BIT
    return __BIENUTIL_NAMESPACE StrCSpn(_psz, _pszCharSet);
#endif //!BIEN_WCHAR_16BIT
  }
  static size_t StrCSpn(_tyLPCSTR _psz, _tyChar _tcBegin, _tyChar _tcEnd, _tyLPCSTR _pszCharSet)
  {
    return __BIENUTIL_NAMESPACE StrCSpn(_psz, _tcBegin, _tcEnd, _pszCharSet);
  }
  static size_t StrRSpn(_tyLPCSTR _pszBegin, _tyLPCSTR _pszEnd, _tyLPCSTR _pszSet)
  {
    return __BIENUTIL_NAMESPACE StrRSpn(_pszBegin, _pszEnd, _pszSet);
  }
  static void MemSet(_tyLPSTR _psz, _tyChar _tc, size_t _n)
  {
#ifdef BIEN_WCHAR_16BIT
    (void)wmemset( (_tyCharStd*)_psz, (_tyCharStd)_tc, _n );
#else //!BIEN_WCHAR_16BIT
    __BIENUTIL_NAMESPACE MemSet(_psz, _tc, _n);
#endif //!BIEN_WCHAR_16BIT
  }
  static int Snprintf(_tyLPSTR _rgch, size_t _n, _tyLPCSTR _pszFmt, ...)
  {
    va_list ap;
    va_start(ap, _pszFmt);
#ifdef BIEN_WCHAR_16BIT
    int iRet = ::vswprintf((_tyCharStd*)_rgch, _n, (const _tyCharStd*)_pszFmt, ap);
#else //!BIEN_WCHAR_16BIT
    static_assert( sizeof( _tyChar ) != sizeof( wchar_t ) ); // If this fails then your system is using 16bit wchar_t - the same as Windows and needs to be special cased the same as Windows.
    // We must convert the string to char32_t and then call vswprintf() and then convert back:
    wchar_t * rgwcBuf = (wchar_t*)alloca( _n * sizeof(wchar_t));
    size_t stLenFmt = StrNLen( _pszFmt );
    wchar_t * rgwcFmtConv = (wchar_t*)alloca( stLenFmt * sizeof(wchar_t));
    ConvertAsciiString( rgwcFmtConv, stLenFmt, _pszFmt );
    int iRet = ::vswprintf(rgwcBuf, _n, rgwcFmtConv, ap);
    Assert( iRet >= 0 );
    if ( iRet >= 0 )
      ConvertAsciiString( _rgch, (size_t)iRet, rgwcBuf );
#endif //!BIEN_WCHAR_16BIT
    va_end(ap);
    VerifyThrow(iRet >= 0);
    return iRet;
  }
};

// JsonCharTraits< char32_t > : Traits for 16bit char representation. Uses the Windows wc...() methods where possible.
template <>
struct JsonCharTraits<char32_t>
{
  typedef char32_t _tyChar;
  typedef char32_t _tyPersistAsChar; // This is a hint and each file object can persist as it likes.
#ifdef BIEN_WCHAR_32BIT
  typedef wchar_t _tyCharStd; // The char we need to convert to to call standard string methods.
#endif //BIEN_WCHAR_32BIT
  typedef _tyChar *_tyLPSTR;
  typedef const _tyChar *_tyLPCSTR;
  typedef StrWRsv<std::basic_string<_tyChar>> _tyStdStr;

  static const bool s_fThrowOnUnicodeOverflow = true; // By default we throw when a unicode character value overflows the representation.

  static const _tyChar s_tcLeftSquareBr = U'[';
  static const _tyChar s_tcRightSquareBr = U']';
  static const _tyChar s_tcLeftCurlyBr = U'{';
  static const _tyChar s_tcRightCurlyBr = U'}';
  static const _tyChar s_tcColon = U':';
  static const _tyChar s_tcComma = U',';
  static const _tyChar s_tcPeriod = U'.';
  static const _tyChar s_tcDoubleQuote = U'"';
  static const _tyChar s_tcBackSlash = U'\\';
  static const _tyChar s_tcForwardSlash = U'/';
  static const _tyChar s_tcMinus = U'-';
  static const _tyChar s_tcPlus = U'+';
  static const _tyChar s_tc0 = U'0';
  static const _tyChar s_tc1 = U'1';
  static const _tyChar s_tc2 = U'2';
  static const _tyChar s_tc3 = U'3';
  static const _tyChar s_tc4 = U'4';
  static const _tyChar s_tc5 = U'5';
  static const _tyChar s_tc6 = U'6';
  static const _tyChar s_tc7 = U'7';
  static const _tyChar s_tc8 = U'8';
  static const _tyChar s_tc9 = U'9';
  static const _tyChar s_tca = U'a';
  static const _tyChar s_tcb = U'b';
  static const _tyChar s_tcc = U'c';
  static const _tyChar s_tcd = U'd';
  static const _tyChar s_tce = U'e';
  static const _tyChar s_tcf = U'f';
  static const _tyChar s_tcA = U'A';
  static const _tyChar s_tcB = U'B';
  static const _tyChar s_tcC = U'C';
  static const _tyChar s_tcD = U'D';
  static const _tyChar s_tcE = U'E';
  static const _tyChar s_tcF = U'F';
  static const _tyChar s_tct = U't';
  static const _tyChar s_tcr = U'r';
  static const _tyChar s_tcu = U'U';
  static const _tyChar s_tcl = U'l';
  static const _tyChar s_tcs = U's';
  static const _tyChar s_tcn = U'n';
  static const _tyChar s_tcBackSpace = U'\b';
  static const _tyChar s_tcFormFeed = U'\f';
  static const _tyChar s_tcNewline = U'\n';
  static const _tyChar s_tcCarriageReturn = U'\r';
  static const _tyChar s_tcTab = U'\t';
  static const _tyChar s_tcSpace = U' ';
  static const _tyChar s_tcDollarSign = U'$';
  static constexpr _tyLPCSTR s_szCRLF = U"\r\n";
  static const _tyChar s_tcFirstUnprintableChar = U'\01';
  static const _tyChar s_tcLastUnprintableChar = U'\37';
  static constexpr _tyLPCSTR s_szPrintableWhitespace = U"\t\n\r";
  static constexpr _tyLPCSTR s_szEscapeStringChars = U"\\\"\b\f\t\n\r";
  // The formatting command to format a single character using printf style.
  static constexpr const char *s_szFormatChar = "%lc";

  static const int s_knMaxNumberLength = 512; // Yeah this is absurd but why not?

  static bool FIsWhitespace(_tyChar _tc)
  {
    return ((U' ' == _tc) || (U'\t' == _tc) || (U'\n' == _tc) || (U'\r' == _tc));
  }
  static bool FIsIllegalChar(_tyChar _tc)
  {
    return !_tc;
  }
  static size_t StrLen(_tyLPCSTR _psz)
  {
#ifdef BIEN_WCHAR_32BIT
    return wcslen((const _tyCharStd*)_psz);
#else //!BIEN_WCHAR_32BIT
    return __BIENUTIL_NAMESPACE StrNLen(_psz);
#endif //!BIEN_WCHAR_32BIT
  }
  static size_t StrNLen(_tyLPCSTR _psz, size_t _stLen = (std::numeric_limits<size_t>::max)())
  {
    return __BIENUTIL_NAMESPACE StrNLen(_psz, _stLen);
  }
  static size_t StrCSpn(_tyLPCSTR _psz, _tyLPCSTR _pszCharSet)
  {
#ifdef BIEN_WCHAR_32BIT
    return wcscspn((const _tyCharStd*)_psz, (const _tyCharStd*)_pszCharSet);
#else //!BIEN_WCHAR_32BIT
    return __BIENUTIL_NAMESPACE StrCSpn(_psz, _pszCharSet);
#endif //!BIEN_WCHAR_32BIT
  }
  static size_t StrCSpn(_tyLPCSTR _psz, _tyChar _tcBegin, _tyChar _tcEnd, _tyLPCSTR _pszCharSet)
  {
    return __BIENUTIL_NAMESPACE StrCSpn(_psz, _tcBegin, _tcEnd, _pszCharSet);
  }
  static size_t StrRSpn(_tyLPCSTR _pszBegin, _tyLPCSTR _pszEnd, _tyLPCSTR _pszSet)
  {
    return __BIENUTIL_NAMESPACE StrRSpn(_pszBegin, _pszEnd, _pszSet);
  }
  static void MemSet(_tyLPSTR _psz, _tyChar _tc, size_t _n)
  {
#ifdef BIEN_WCHAR_32BIT
    (void)wmemset((_tyCharStd*)_psz, (_tyCharStd)_tc, _n );
#else //!BIEN_WCHAR_32BIT
    __BIENUTIL_NAMESPACE MemSet(_psz, _tc, _n);
#endif //!BIEN_WCHAR_32BIT
  }
  static int Snprintf(_tyLPSTR _rgch, size_t _n, _tyLPCSTR _pszFmt, ...)
  {
    va_list ap;
    va_start(ap, _pszFmt);
#ifdef BIEN_WCHAR_32BIT
    int iRet = ::vswprintf((_tyCharStd*)_rgch, _n, (const _tyCharStd*)_pszFmt, ap);
#else //!BIEN_WCHAR_32BIT
    static_assert( sizeof( _tyChar ) != sizeof( wchar_t ) ); // If this fails then your system is using 16bit wchar_t - the same as Windows and needs to be special cased the same as Windows.
    // We must convert the string to char32_t and then call vswprintf() and then convert back:
    wchar_t * rgwcBuf = (wchar_t*)alloca( _n * sizeof(wchar_t));
    size_t stLenFmt = StrNLen( _pszFmt );
    wchar_t * rgwcFmtConv = (wchar_t*)alloca( stLenFmt * sizeof(wchar_t));
    ConvertAsciiString( rgwcFmtConv, stLenFmt, _pszFmt );
    int iRet = ::vswprintf(rgwcBuf, _n, rgwcFmtConv, ap);
    Assert( iRet >= 0 );
    if ( iRet >= 0 )
      ConvertAsciiString( _rgch, (min)((size_t)iRet,_n), rgwcBuf );
#endif //!BIEN_WCHAR_32BIT
    va_end(ap);
    VerifyThrow(iRet >= 0);
    return iRet;
  }
};

// JsonCharTraits< wchar_t > : Traits for 16bit(Windows)/32bit(Linux) char representation.
template <>
struct JsonCharTraits<wchar_t>
{
  typedef wchar_t _tyChar;
#ifdef WIN32
  typedef wchar_t _tyPersistAsChar; // This is a hint and each file object can persist as it likes.
#else //!WIN32
  typedef char16_t _tyPersistAsChar; // This is a hint and each file object can persist as it likes.
#endif
  typedef _tyChar *_tyLPSTR;
  typedef const _tyChar *_tyLPCSTR;
  typedef StrWRsv<std::basic_string<_tyChar>> _tyStdStr;

  static const bool s_fThrowOnUnicodeOverflow = true; // By default we throw when a unicode character value overflows the representation.

  static const _tyChar s_tcLeftSquareBr = L'[';
  static const _tyChar s_tcRightSquareBr = L']';
  static const _tyChar s_tcLeftCurlyBr = L'{';
  static const _tyChar s_tcRightCurlyBr = L'}';
  static const _tyChar s_tcColon = L':';
  static const _tyChar s_tcComma = L',';
  static const _tyChar s_tcPeriod = L'.';
  static const _tyChar s_tcDoubleQuote = L'"';
  static const _tyChar s_tcBackSlash = L'\\';
  static const _tyChar s_tcForwardSlash = L'/';
  static const _tyChar s_tcMinus = L'-';
  static const _tyChar s_tcPlus = L'+';
  static const _tyChar s_tc0 = L'0';
  static const _tyChar s_tc1 = L'1';
  static const _tyChar s_tc2 = L'2';
  static const _tyChar s_tc3 = L'3';
  static const _tyChar s_tc4 = L'4';
  static const _tyChar s_tc5 = L'5';
  static const _tyChar s_tc6 = L'6';
  static const _tyChar s_tc7 = L'7';
  static const _tyChar s_tc8 = L'8';
  static const _tyChar s_tc9 = L'9';
  static const _tyChar s_tca = L'a';
  static const _tyChar s_tcb = L'b';
  static const _tyChar s_tcc = L'c';
  static const _tyChar s_tcd = L'd';
  static const _tyChar s_tce = L'e';
  static const _tyChar s_tcf = L'f';
  static const _tyChar s_tcA = L'A';
  static const _tyChar s_tcB = L'B';
  static const _tyChar s_tcC = L'C';
  static const _tyChar s_tcD = L'D';
  static const _tyChar s_tcE = L'E';
  static const _tyChar s_tcF = L'F';
  static const _tyChar s_tct = L't';
  static const _tyChar s_tcr = L'r';
  static const _tyChar s_tcu = L'u';
  static const _tyChar s_tcl = L'l';
  static const _tyChar s_tcs = L's';
  static const _tyChar s_tcn = L'n';
  static const _tyChar s_tcBackSpace = L'\b';
  static const _tyChar s_tcFormFeed = L'\f';
  static const _tyChar s_tcNewline = L'\n';
  static const _tyChar s_tcCarriageReturn = L'\r';
  static const _tyChar s_tcTab = L'\t';
  static const _tyChar s_tcSpace = L' ';
  static const _tyChar s_tcDollarSign = L'$';
  static constexpr _tyLPCSTR s_szCRLF = L"\r\n";
  static const _tyChar s_tcFirstUnprintableChar = L'\01';
  static const _tyChar s_tcLastUnprintableChar = L'\37';
  static constexpr _tyLPCSTR s_szPrintableWhitespace = L"\t\n\r";
  static constexpr _tyLPCSTR s_szEscapeStringChars = L"\\\"\b\f\t\n\r";
  // The formatting command to format a single character using printf style.
  static constexpr const char *s_szFormatChar = "%lc";

  static const int s_knMaxNumberLength = 512; // Yeah this is absurd but why not?

  static bool FIsWhitespace(_tyChar _tc)
  {
    return ((L' ' == _tc) || (L'\t' == _tc) || (L'\n' == _tc) || (L'\r' == _tc));
  }
  static bool FIsIllegalChar(_tyChar _tc)
  {
    return !_tc;
  }
  static size_t StrLen(_tyLPCSTR _psz)
  {
    return wcslen(_psz);
  }
  static size_t StrNLen(_tyLPCSTR _psz, size_t _stLen = (std::numeric_limits<size_t>::max)())
  {
    return __BIENUTIL_NAMESPACE StrNLen(_psz, _stLen);
  }
  static size_t StrCSpn(_tyLPCSTR _psz, _tyLPCSTR _pszCharSet)
  {
    return wcscspn(_psz, _pszCharSet);
  }
  static size_t StrCSpn(_tyLPCSTR _psz, _tyChar _tcBegin, _tyChar _tcEnd, _tyLPCSTR _pszCharSet)
  {
    return __BIENUTIL_NAMESPACE StrCSpn(_psz, _tcBegin, _tcEnd, _pszCharSet);
  }
  static size_t StrRSpn(_tyLPCSTR _pszBegin, _tyLPCSTR _pszEnd, _tyLPCSTR _pszSet)
  {
    return __BIENUTIL_NAMESPACE StrRSpn(_pszBegin, _pszEnd, _pszSet);
  }
  static void MemSet(_tyLPSTR _psz, _tyChar _tc, size_t _n)
  {
    (void)wmemset(_psz, _tc, _n);
  }
  static int Snprintf(_tyLPSTR _psz, size_t _n, _tyLPCSTR _pszFmt, ...)
  {
    va_list ap;
    va_start(ap, _pszFmt);
    int iRet = ::vswprintf(_psz, _n, _pszFmt, ap);
    va_end(ap);
    VerifyThrow(iRet >= 0);
    return iRet;
  }
};

// JsonInputStream: Base class for input streams.
template <class t_tyCharTraits, class t_tyFilePos>
class JsonInputStreamBase
{
public:
  typedef t_tyCharTraits _tyCharTraits;
  typedef t_tyFilePos _tyFilePos;
};

// JsonOutputStream: Base class for output streams.
template <class t_tyCharTraits, class t_tyFilePos>
class JsonOutputStreamBase
{
public:
  typedef t_tyCharTraits _tyCharTraits;
  typedef t_tyFilePos _tyFilePos;
};

// JsonFileInputStream: A class using open(), read(), etc.
template <class t_tyCharTraits, class t_tyPersistAsChar = typename t_tyCharTraits::_tyPersistAsChar>
class JsonFileInputStream : public JsonInputStreamBase<t_tyCharTraits, size_t>
{
  typedef JsonFileInputStream _tyThis;

public:
  typedef t_tyCharTraits _tyCharTraits;
  typedef typename _tyCharTraits::_tyChar _tyChar;
  typedef t_tyPersistAsChar _tyPersistAsChar;
  typedef size_t _tyFilePos;
  typedef JsonReadCursor<_tyThis> _tyJsonReadCursor;

  ~JsonFileInputStream() = default;
  JsonFileInputStream() = default;
  JsonFileInputStream( JsonFileInputStream const & ) = delete;
  JsonFileInputStream & operator = ( JsonFileInputStream const & ) = delete;
  JsonFileInputStream( JsonFileInputStream && _rr ) = default;
  JsonFileInputStream & operator = ( JsonFileInputStream && ) = default;
  void swap( _tyThis & _r )
  {
    m_szFilename.swap( _r.m_szFilename );
    std::swap( m_pos, _r.m_pos );
    m_foFile.swap( _r.m_foFile );
    std::swap( m_tcLookahead, _r.m_tcLookahead );
    std::swap( m_fHasLookahead, _r.m_fHasLookahead );
    std::swap( m_fUseSeek, _r.m_fUseSeek );
  }
  bool FOpened() const
  {
    return m_foFile.FIsOpen();
  }
  // Throws on open failure. This object owns the lifetime of the file descriptor.
  void Open(const char *_szFilename, bool _fUseSeek = true)
  {
    m_foFile.SetHFile( OpenReadOnlyFile( _szFilename ) );
    if (!FOpened())
      THROWBADJSONSTREAMERRNO(GetLastErrNo(), "Unable to OpenReadOnlyFile() file [%s]", _szFilename);
    m_fHasLookahead = false;    // ensure that if we had previously been opened that we don't think we still have a lookahead.
    m_szFilename = _szFilename; // For error reporting and general debugging. Of course we don't need to store this.
    m_fUseSeek = _fUseSeek;
    m_pos = 0;
  }
  // Attach to an FD whose lifetime we do not own. This can be used, for instance, to attach to stdin which is usually at FD 0 unless reopen()ed.
  void AttachFd(vtyFileHandle _hFile, bool _fUseSeek = false, bool _fOwnFileLifetime = false )
  {
    Assert(_hFile != vkhInvalidFileHandle);
    m_foFile.SetHFile( _hFile, _fOwnFileLifetime );
    m_fHasLookahead = false;  // ensure that if we had previously been opened that we don't think we still have a lookahead.
    m_szFilename.clear();     // No filename indicates we are attached to "some hFile".
    m_fUseSeek = _fUseSeek;
    m_pos = 0;
  }
  int Close()
  {
    return m_foFile.Close();
  }
  // Attach to this FOpened() JsonFileInputStream.
  void AttachReadCursor(_tyJsonReadCursor &_rjrc)
  {
    Assert(!_rjrc.FAttached());
    Assert(FOpened());
    _rjrc.AttachRoot(*this);
  }
  void SkipWhitespace()
  {
    Assert(FOpened());
    // We will keep reading characters until we find non-whitespace:
    if (m_fHasLookahead && !_tyCharTraits::FIsWhitespace(m_tcLookahead))
      return;
    size_t stRead;
    for (;;)
    {
      _tyPersistAsChar cpxRead;
      int iReadResult = FileRead( m_foFile.HFileGet(), &cpxRead, sizeof cpxRead, &stRead );
      if (-1 == iReadResult)
        THROWBADJSONSTREAMERRNO(GetLastErrNo(), "FileRead() failed for file [%s]", m_szFilename.c_str());
      m_tcLookahead = cpxRead;
      m_pos += stRead;
      if (stRead != sizeof cpxRead)
        THROWBADJSONSTREAMERRNO(GetLastErrNo(), "FileRead() for file [%s] had [%ld] leftover bytes.", m_szFilename.c_str(), stRead);
      if (!stRead)
      {
        m_fHasLookahead = false;
        return; // We have skipped the whitespace until we hit EOF.
      }
      if (_tyCharTraits::FIsIllegalChar(m_tcLookahead))
        THROWBADJSONSTREAM("Found illegal char [%TC] in file [%s]", m_tcLookahead ? m_tcLookahead : '?', m_szFilename.c_str());
      if (!_tyCharTraits::FIsWhitespace(m_tcLookahead))
      {
        m_fHasLookahead = true;
        return;
      }
    }
  }
  // Throws if lseek() fails.
  _tyFilePos ByPosGet() const
  {
    Assert(FOpened());
    if (m_fUseSeek)
    {
      vtySeekOffset pos;
      int iSeekResult = FileSeek( m_foFile.HFileGet(), 0, vkSeekCur, &pos );
      if (-1 == iSeekResult)
        THROWBADJSONSTREAMERRNO(GetLastErrNo(), "FileSeek() failed for file [%s]", m_szFilename.c_str());
      Assert(pos == m_pos); // These should always match.
      if (m_fHasLookahead)
        pos -= sizeof m_tcLookahead; // Since we have a lookahead we are actually one character before.
      return pos;
    }
    else
      return m_pos - (m_fHasLookahead ? sizeof m_tcLookahead : 0);
  }
  // Read a single character from the file - always throw on EOF.
  _tyChar ReadChar(const char *_pcEOFMessage)
  {
    Assert(FOpened());
    if (m_fHasLookahead)
    {
      m_fHasLookahead = false;
      return m_tcLookahead;
    }
    _tyPersistAsChar cpxRead;
    size_t stRead;
    int iReadResult = FileRead( m_foFile.HFileGet(), &cpxRead, sizeof cpxRead, &stRead );
    if (-1 == iReadResult)
      THROWBADJSONSTREAMERRNO(GetLastErrNo(), "FileRead() failed for file [%s]", m_szFilename.c_str());
    m_tcLookahead = cpxRead;
    if (stRead != sizeof cpxRead)
    {
      Assert(!stRead); // For multibyte characters this could fail but then we would have a bogus file anyway and would want to throw.
      m_pos += stRead;
      THROWBADJSONSTREAM("[%s]: %s", m_szFilename.c_str(), _pcEOFMessage);
    }
    if (_tyCharTraits::FIsIllegalChar(m_tcLookahead))
      THROWBADJSONSTREAM("Found illegal char [%TC] in file [%s]", m_tcLookahead ? m_tcLookahead : '?', m_szFilename.c_str());
    m_pos += stRead;
    Assert(!m_fHasLookahead);
    return m_tcLookahead;
  }
  bool FReadChar(_tyChar &_rtch, bool _fThrowOnEOF, const char *_pcEOFMessage)
  {
    Assert(FOpened());
    if (m_fHasLookahead)
    {
      _rtch = m_tcLookahead;
      m_fHasLookahead = false;
      return true;
    }
    _tyPersistAsChar cpxRead;
    size_t stRead;
    int iReadResult = FileRead( m_foFile.HFileGet(), &cpxRead, sizeof cpxRead, &stRead );
    if (-1 == iReadResult)
      THROWBADJSONSTREAMERRNO(GetLastErrNo(), "FileRead() failed for file [%s]", m_szFilename.c_str());
    m_tcLookahead = cpxRead;
    if (stRead != sizeof cpxRead)
    {
      if (!!stRead)
      {
        m_pos += stRead;
        THROWBADJSONSTREAMERRNO(GetLastErrNo(), "read() for file [%s] had [%ld] leftover bytes.", m_szFilename.c_str(), stRead);
      }
      else
      {
        if (_fThrowOnEOF)
          THROWBADJSONSTREAM("[%s]: %s", m_szFilename.c_str(), _pcEOFMessage);
        return false;
      }
    }
    if (_tyCharTraits::FIsIllegalChar(m_tcLookahead))
      THROWBADJSONSTREAM("Found illegal char [%TC] in file [%s]", m_tcLookahead ? m_tcLookahead : '?', m_szFilename.c_str());
    m_pos += stRead;
    _rtch = m_tcLookahead;
    return true;
  }
  void PushBackLastChar(bool _fMightBeWhitespace = false)
  {
    Assert(!m_fHasLookahead); // support single character lookahead only.
    Assert(_fMightBeWhitespace || !_tyCharTraits::FIsWhitespace(m_tcLookahead));
    if (!_tyCharTraits::FIsWhitespace(m_tcLookahead))
      m_fHasLookahead = true;
  }
protected:
  std::string m_szFilename;
  _tyFilePos m_pos{0};      // We use this if !m_fUseSeek.
  FileObj m_foFile;
  _tyChar m_tcLookahead{0}; // Everytime we read a character we put it in the m_tcLookahead and clear that we have a lookahead.
  bool m_fHasLookahead{false};
  bool m_fUseSeek{true};        // For STDIN we cannot use seek so we just maintain our "current" position.
};

// JsonFixedMemInputStream: Stream a fixed piece o' mem'ry at the JSON parser.
template <class t_tyCharTraits, class t_tyPersistAsChar = typename t_tyCharTraits::_tyPersistAsChar >
class JsonFixedMemInputStream : public JsonInputStreamBase<t_tyCharTraits, size_t>
{
  typedef JsonFixedMemInputStream _tyThis;
  typedef JsonInputStreamBase<t_tyCharTraits, size_t> _tyBase;
public:
  typedef t_tyCharTraits _tyCharTraits;
  typedef typename _tyCharTraits::_tyChar _tyChar;
  typedef t_tyPersistAsChar _tyPersistAsChar;
  typedef size_t _tyFilePos;
  typedef JsonReadCursor<_tyThis> _tyJsonReadCursor;

  JsonFixedMemInputStream(const _tyPersistAsChar *_pcpxBegin, _tyFilePos _stLen)
  {
    Open(_pcpxBegin, _stLen);
  }
  ~JsonFixedMemInputStream() = default;
  JsonFixedMemInputStream() = default;
  JsonFixedMemInputStream( JsonFixedMemInputStream const & ) = default;
  JsonFixedMemInputStream & operator=( JsonFixedMemInputStream const &) = default;
  bool FOpened() const
  {
    return vkpvNullMapping != m_pcpxBegin;
  }
  bool FEOF() const
  {
    Assert(FOpened());
    return (m_pcpxEnd == m_pcpxCur) && !m_fHasLookahead;
  }
  _tyFilePos StLenBytes() const
  {
    Assert(FOpened());
    return (m_pcpxEnd - m_pcpxBegin) * sizeof(_tyPersistAsChar);
  }
  void Open(const _tyPersistAsChar *_pcpxBegin, _tyFilePos _stLen)
  {
    Close();
    m_pcpxCur = m_pcpxBegin = _pcpxBegin;
    m_pcpxEnd = m_pcpxCur + _stLen;
  }
  void Close()
  {
    m_pcpxBegin = (_tyPersistAsChar *)vkpvNullMapping;
    m_pcpxCur = (_tyPersistAsChar *)vkpvNullMapping;
    m_pcpxEnd = (_tyPersistAsChar *)vkpvNullMapping;
    m_fHasLookahead = false;
    m_tcLookahead = 0;
  }
  // Attach to this FOpened() JsonFileInputStream.
  void AttachReadCursor(_tyJsonReadCursor &_rjrc)
  {
    Assert(!_rjrc.FAttached());
    Assert(FOpened());
    _rjrc.AttachRoot(*this);
  }
  void SkipWhitespace(const char *_pcFilename = 0)
  {
    // We will keep reading characters until we find non-whitespace:
    if (m_fHasLookahead && !_tyCharTraits::FIsWhitespace(m_tcLookahead))
      return;
    m_fHasLookahead = false;
    for (;;)
    {
      if (FEOF())
        return; // We have skipped the whitespace until we hit EOF.
      m_tcLookahead = *m_pcpxCur++;
      if (_tyCharTraits::FIsIllegalChar(m_tcLookahead))
        THROWBADJSONSTREAM("Found illegal char [%TC] in file [%s]", m_tcLookahead ? m_tcLookahead : '?', !_pcFilename ? "(no file)" : _pcFilename);
      if (!_tyCharTraits::FIsWhitespace(m_tcLookahead))
      {
        m_fHasLookahead = true;
        return;
      }
    }
  }
  _tyFilePos ByPosGet() const
  {
    Assert(FOpened());
    return ((m_pcpxCur - m_pcpxBegin) - (size_t)m_fHasLookahead) * sizeof(_tyPersistAsChar);
  }
  // Read a single character from the file - always throw on EOF.
  _tyChar ReadChar(const char *_pcEOFMessage, const char *_pcFilename = 0)
  {
    Assert(FOpened());
    if (m_fHasLookahead)
    {
      Assert(!!m_tcLookahead);
      m_fHasLookahead = false;
      return m_tcLookahead;
    }
    if (FEOF())
      THROWBADJSONSTREAM("[%s]: %s", !_pcFilename ? "(no file)" : _pcFilename, _pcEOFMessage);
    Assert(!!*m_pcpxCur);
    m_tcLookahead = *m_pcpxCur++;
    if (_tyCharTraits::FIsIllegalChar(m_tcLookahead))
      THROWBADJSONSTREAM("Found illegal char [%TC] in file [%s]", m_tcLookahead ? m_tcLookahead : '?', !_pcFilename ? "(no file)" : _pcFilename);
    return m_tcLookahead;
  }
  bool FReadChar(_tyChar &_rtch, bool _fThrowOnEOF, const char *_pcEOFMessage, const char *_pcFilename = 0)
  {
    Assert(FOpened());
    if (m_fHasLookahead)
    {
      _rtch = m_tcLookahead;
      m_fHasLookahead = false;
      return true;
    }
    if (FEOF())
    {
      if (_fThrowOnEOF)
        THROWBADJSONSTREAM("[%s]: %s", !_pcFilename ? "(no file)" : _pcFilename, _pcEOFMessage);
      return false;
    }
    _rtch = m_tcLookahead = *m_pcpxCur++;
    if (_tyCharTraits::FIsIllegalChar(m_tcLookahead))
      THROWBADJSONSTREAM("Found illegal char [%TC] in file [%s]", m_tcLookahead ? m_tcLookahead : '?', !_pcFilename ? "(no file)" : _pcFilename);
    return true;
  }
  void PushBackLastChar(bool _fMightBeWhitespace = false)
  {
    Assert(!m_fHasLookahead); // support single character lookahead only.
    Assert(_fMightBeWhitespace || !_tyCharTraits::FIsWhitespace(m_tcLookahead));
    if (!_tyCharTraits::FIsWhitespace(m_tcLookahead))
      m_fHasLookahead = true;
  }
  std::strong_ordering ICompare(_tyThis const &_rOther) const
  {
    Assert(FOpened() && _rOther.FOpened());
    if (StLenBytes() < _rOther.StLenBytes())
      return std::strong_ordering::less;
    else if (StLenBytes() > _rOther.StLenBytes())
      return std::strong_ordering::greater;
    int iComp = memcmp(m_pcpxBegin, _rOther.m_pcpxBegin, StLenBytes());
    return (iComp < 0) ? std::strong_ordering::less : ((iComp > 0) ? std::strong_ordering::greater : std::strong_ordering::equal);
  }
protected:
  const _tyPersistAsChar *m_pcpxBegin{(_tyPersistAsChar *)vkpvNullMapping};
  const _tyPersistAsChar *m_pcpxCur{(_tyPersistAsChar *)vkpvNullMapping};
  const _tyPersistAsChar *m_pcpxEnd{(_tyPersistAsChar *)vkpvNullMapping};
  _tyChar m_tcLookahead{0}; // Everytime we read a character we put it in the m_tcLookahead and clear that we have a lookahead.
  bool m_fHasLookahead{false};
};

// JsonMemMappedInputStream: A class using open(), read(), etc.
template <class t_tyCharTraits, class t_tyPersistAsChar = typename t_tyCharTraits::_tyPersistAsChar>
class JsonMemMappedInputStream : protected JsonFixedMemInputStream<t_tyCharTraits>
{
  typedef JsonFixedMemInputStream<t_tyCharTraits> _tyBase;
  typedef JsonMemMappedInputStream _tyThis;
public:
  typedef t_tyCharTraits _tyCharTraits;
  typedef typename _tyCharTraits::_tyChar _tyChar;
  typedef t_tyPersistAsChar _tyPersistAsChar;
  typedef size_t _tyFilePos;
  typedef JsonReadCursor<_tyThis> _tyJsonReadCursor;

  JsonMemMappedInputStream() = default;
  ~JsonMemMappedInputStream() = default;
  // We allow copying but the copy doesn't own the mapping.
  JsonMemMappedInputStream( JsonMemMappedInputStream const & _r )
    : m_fmoMapping( _r.m_fmoMapping, false )
  {
  }
  JsonMemMappedInputStream & operator = ( JsonMemMappedInputStream const & _r )
  {
    _tyThis copy( _r );
    swap( copy );
  }
  JsonMemMappedInputStream( JsonMemMappedInputStream && _rr )
  {
    swap( _rr );
  }
  JsonMemMappedInputStream & operator = ( JsonMemMappedInputStream && _rr )
  {
    _tyThis acquire( std::move( _rr ) );
    swap( acquire );
  }
  void swap( _tyThis & _r )
  {
    _tyBase::swap( _r );
    m_szFilename.swap( _r.m_szFilename );
    m_fmoMapping.swap( _r.m_fmoMapping );
  }
  bool FOpened() const
  {
    return FFileMapped();
  }
  bool FFileMapped() const
  {
    return m_fmoMapping.FIsOpen();
  }
  using _tyBase::FEOF;
  using _tyBase::StLenBytes;
  // Throws on open failure. This object owns the lifetime of the file descriptor.
  void Open(const char *_szFilename)
  {
    Close();
    FileObj fileLocal( OpenReadOnlyFile(_szFilename) ); // We will close the file after we map it since that is fine and results in easier cleanup.
    if ( !fileLocal.FIsOpen() )
      THROWBADJSONSTREAMERRNO(GetLastErrNo(), "Unable to OpenReadOnlyFile() file [%s]", _szFilename);
    // Now get the size of the file and then map it.
    vtyHandleAttr attrFile;
    int iResult = GetHandleAttrs( fileLocal.HFileGet(), attrFile );
    if (-1 == iResult)
      THROWBADJSONSTREAMERRNO(GetLastErrNo(), "GetHandleAttrs() failed for [%s]", _szFilename);
    size_t stSize = GetSize_HandleAttr( attrFile );
    if (0 == stSize )
      THROWBADJSONSTREAM("File [%s] is empty - it contains no JSON value.", _szFilename);
    if (!!(stSize % sizeof(t_tyPersistAsChar)))
      THROWBADJSONSTREAM("File [%s]'s size not multiple of char size stSize[%ld].", _szFilename, stSize);
    if ( !FIsRegularFile_HandleAttr( attrFile ) )
      THROWBADJSONSTREAM("File [%s] is not a regular file.", _szFilename);
    m_fmoMapping.SetHMMFile( MapReadOnlyHandle( fileLocal.HFileGet(), nullptr ) );
    if ( !m_fmoMapping.FIsOpen() )
      THROWBADJSONSTREAMERRNO(GetLastErrNo(), "MapReadOnlyHandle() failed to map [%s], size [%ld].", _szFilename, stSize);
    m_pcpxBegin = m_pcpxCur = (const t_tyPersistAsChar *)m_fmoMapping.Pv();
    m_pcpxEnd = m_pcpxCur + (stSize / sizeof(t_tyPersistAsChar));
    m_fHasLookahead = false;    // ensure that if we had previously been opened that we don't think we still have a lookahead.
    m_szFilename = _szFilename; // For error reporting and general debugging. Of course we don't need to store this.
  }
  int Close()
  {
    return m_fmoMapping.Close();
  }
  // Attach to this FOpened() JsonFileInputStream.
  // We *could* use the base's impl for this but then the read cursor only see's the base's type and not the full type.
  // In this case that works fine but in the general case it might not. Anyway, it adds very little code to do it this way so no worries.
  void AttachReadCursor(_tyJsonReadCursor &_rjrc)
  {
    Assert(!_rjrc.FAttached());
    Assert(FOpened());
    _rjrc.AttachRoot(*this);
  }
  void SkipWhitespace()
  {
    return _tyBase::SkipWhitespace(m_szFilename.c_str());
  }
  using _tyBase::ByPosGet;
  // Read a single character from the file - always throw on EOF.
  _tyChar ReadChar(const char *_pcEOFMessage)
  {
    return _tyBase::ReadChar(_pcEOFMessage, m_szFilename.c_str());
  }
  bool FReadChar(_tyChar &_rtch, bool _fThrowOnEOF, const char *_pcEOFMessage)
  {
    return _tyBase::FReadChar(_rtch, _fThrowOnEOF, _pcEOFMessage, m_szFilename.c_str());
  }
  using _tyBase::PushBackLastChar;
  std::strong_ordering ICompare(_tyThis const &_rOther) const
  {
    return _tyBase::ICompare(_rOther);
  }
protected:
  using _tyBase::m_fHasLookahead;
  using _tyBase::m_pcpxBegin;
  using _tyBase::m_pcpxCur;
  using _tyBase::m_pcpxEnd;
  using _tyBase::m_tcLookahead;
  std::string m_szFilename;
  FileMappingObj m_fmoMapping; // Use the smart obj...
};

// Utility method used by the various output streams.
// If <_fEscape> then we escape all special characters when writing.
template < class t_tyJsonOutputStream >
void JsonOutputStream_WriteString( t_tyJsonOutputStream & _rjos, bool _fEscape, 
  typename t_tyJsonOutputStream::_tyLPCSTR _psz, ssize_t _sstLen = -1, const typename t_tyJsonOutputStream::_tyJsonFormatSpec *_pjfs = 0 )
{
  typedef t_tyJsonOutputStream _tyJsonOutputStream;
  typedef typename _tyJsonOutputStream::_tyCharTraits _tyCharTraits;
  typedef typename _tyJsonOutputStream::_tyChar _tyChar;
  typedef typename _tyJsonOutputStream::_tyLPCSTR _tyLPCSTR;
  Assert(!_pjfs || _fEscape); // only pass formatting when we are escaping the string.
  if (!_sstLen)
    return;
  size_t stLen = (_sstLen < 0) ? _tyCharTraits::StrLen(_psz) : (size_t)_sstLen;
  if (!_fEscape) // We shouldn't write such characters to strings so we had better know that we don't have any.
    Assert(stLen <= _tyCharTraits::StrCSpn(_psz, _tyCharTraits::s_tcFirstUnprintableChar, _tyCharTraits::s_tcLastUnprintableChar + 1,
                                            _tyCharTraits::s_szEscapeStringChars));
  _tyLPCSTR pszPrintableWhitespaceAtEnd = _psz + stLen;
  if (_fEscape && !!_pjfs && !_pjfs->m_fEscapePrintableWhitespace && _pjfs->m_fEscapePrintableWhitespaceAtEndOfLine)
    pszPrintableWhitespaceAtEnd -= _tyCharTraits::StrRSpn(_psz, pszPrintableWhitespaceAtEnd, _tyCharTraits::s_szPrintableWhitespace);

  // When we are escaping the string we write piecewise.
  _tyLPCSTR pszWrite = _psz;
  while (!!stLen)
  {
    if (pszWrite < pszPrintableWhitespaceAtEnd)
    {
      ssize_t sstLenWrite = stLen;
      if (_fEscape)
      {
        if (!_pjfs || _pjfs->m_fEscapePrintableWhitespace) // escape everything.
          sstLenWrite = _tyCharTraits::StrCSpn(pszWrite,
                                                _tyCharTraits::s_tcFirstUnprintableChar, _tyCharTraits::s_tcLastUnprintableChar + 1,
                                                _tyCharTraits::s_szEscapeStringChars);
        else
        {
          // More complex escaping of whitespace is possible here:
          _tyLPCSTR pszNoEscape = pszWrite;
          for (; pszPrintableWhitespaceAtEnd != pszNoEscape; ++pszNoEscape)
          {
            _tyLPCSTR pszPrintableWS = _tyCharTraits::s_szPrintableWhitespace;
            for (; !!*pszPrintableWS; ++pszPrintableWS)
            {
              if (*pszNoEscape == *pszPrintableWS)
                break;
            }
            if (!!*pszPrintableWS)
              continue; // found printable WS that we want to print.
            if ((*pszNoEscape < _tyCharTraits::s_tcFirstUnprintableChar) ||
                (*pszNoEscape > _tyCharTraits::s_tcLastUnprintableChar))
            {
              _tyLPCSTR pszEscapeChar = _tyCharTraits::s_szEscapeStringChars;
              for (; !!*pszEscapeChar && (*pszNoEscape != *pszEscapeChar); ++pszEscapeChar)
                ;
              if (!!*pszEscapeChar)
                continue; // found printable WS that we want to print.
            }
          }
          sstLenWrite = pszNoEscape - pszWrite;
        }
      }
      if (sstLenWrite)
      {
        _rjos.WriteRawChars(pszWrite, sstLenWrite);
        stLen -= sstLenWrite;
        pszWrite += sstLenWrite;
        if (!stLen)
          return; // the overwhelming case is we return here.
      }
      else
        Assert(!!stLen); // invariant.
    }
    // else otherwise we will escape all remaining characters.
    _rjos.WriteChar(_tyCharTraits::s_tcBackSlash); // use for error handling.
    // If we haven't written the whole string then we encountered a character we need to escape.
    switch (*pszWrite)
    {
    case _tyCharTraits::s_tcBackSpace:
      _rjos.WriteChar(_tyCharTraits::s_tcb);
      break;
    case _tyCharTraits::s_tcFormFeed:
      _rjos.WriteChar(_tyCharTraits::s_tcf);
      break;
    case _tyCharTraits::s_tcNewline:
      _rjos.WriteChar(_tyCharTraits::s_tcn);
      break;
    case _tyCharTraits::s_tcCarriageReturn:
      _rjos.WriteChar(_tyCharTraits::s_tcr);
      break;
    case _tyCharTraits::s_tcTab:
      _rjos.WriteChar(_tyCharTraits::s_tct);
      break;
    case _tyCharTraits::s_tcBackSlash:
    case _tyCharTraits::s_tcDoubleQuote:
      _rjos.WriteChar(*pszWrite);
      break;
    default:
      // Unprintable character between 0 and 31. We'll use the \uXXXX method for this character.
      _rjos.WriteChar(_tyCharTraits::s_tcu);
      _rjos.WriteChar(_tyCharTraits::s_tc0);
      _rjos.WriteChar(_tyCharTraits::s_tc0);
      _rjos.WriteChar(_tyChar('0' + (*pszWrite / 16)));
      _rjos.WriteChar(_tyChar((*pszWrite % 16) < 10 ? ('0' + (*pszWrite % 16)) : ('A' + (*pszWrite % 16) - 10)));
      break;
    }
    ++pszWrite;
    --stLen;
  }
}

// JsonFileOutputStream: A class using open(), read(), etc.
template <class t_tyCharTraits, class t_tyPersistAsChar = typename t_tyCharTraits::_tyPersistAsChar>
class JsonFileOutputStream : public JsonOutputStreamBase<t_tyCharTraits, size_t>
{
  typedef JsonFileOutputStream _tyThis;
  typedef JsonOutputStreamBase<t_tyCharTraits, size_t> _tyBase;
public:
  typedef t_tyCharTraits _tyCharTraits;
  typedef typename _tyCharTraits::_tyChar _tyChar;
  typedef t_tyPersistAsChar _tyPersistAsChar;
  typedef std::basic_string<_tyPersistAsChar> _tyStdStrPersist;
  typedef typename _tyCharTraits::_tyLPCSTR _tyLPCSTR;
  typedef size_t _tyFilePos;
  typedef JsonReadCursor<_tyThis> _tyJsonReadCursor;
  typedef JsonFormatSpec<_tyCharTraits> _tyJsonFormatSpec;

  ~JsonFileOutputStream() = default;
  JsonFileOutputStream() = default;
  JsonFileOutputStream( JsonFileOutputStream const & ) = delete;
  JsonFileOutputStream & operator = ( JsonFileOutputStream const &) = delete;
  JsonFileOutputStream( JsonFileOutputStream && _rr ) = default;
  JsonFileOutputStream & operator = ( JsonFileOutputStream &&) = default;
  void swap( JsonFileOutputStream & _r )
  {
    _r.m_szFilename.swap(m_szFilename);
    _r.m_szExceptionString.swap(m_szExceptionString);
    m_foFile.swap( _r.m_foFile );
  }
  // This is a manner of indicating that something happened during streaming.
  // Since we use object destruction to finalize writes to a file and cannot throw out of a destructor.
  void SetExceptionString(const char *_szWhat)
  {
    m_szExceptionString = _szWhat;
  }
  void GetExceptionString(std::string &_rstrExceptionString)
  {
    _rstrExceptionString = m_szExceptionString;
  }
  bool FOpened() const
  {
    return m_foFile.FIsOpen();
  }
  // Throws on open failure. This object owns the lifetime of the file descriptor.
  void Open(const char *_szFilename, FileSharing _fs = FileSharing::NoSharing)
  {
    m_szExceptionString.clear();
    m_foFile.SetHFile( CreateWriteOnlyFile( _szFilename, _fs) );
    if (!FOpened())
      THROWBADJSONSTREAMERRNO(GetLastErrNo(), "Unable to CreateWriteOnlyFile() file [%s]", _szFilename);
    m_szFilename = _szFilename; // For error reporting and general debugging. Of course we don't need to store this.
  }
  // Attach to an FD whose lifetime we do not own. This can be used, for instance, to attach to stdout which is usually at FD 1 (unless reopen()ed).
  void AttachFd(vtyFileHandle _hFile, bool _fOwnFdLifetime = false)
  {
    Assert(_hFile != vkhInvalidFileHandle);
    m_foFile.SetHFile( _hFile, _fOwnFdLifetime );
    m_szFilename.clear();     // No filename indicates we are attached to "some hFile".
  }
  int Close()
  {
    return m_foFile.Close();
  }
  void WriteByteOrderMark() const
  {
    Assert( FOpened() );
    Assert( 0 == NFileSeekAndThrow( m_foFile.HFileGet(), 0, vkSeekCur ) );
    uint8_t rgBOM[] = {0xFF, 0xFE};
    size_t stWrote;
    int iWriteResult = FileWrite( m_foFile.HFileGet(), &rgBOM, sizeof rgBOM, &stWrote );
    if (!!iWriteResult)
    {
      Assert( -1 == iWriteResult );
      THROWBADJSONSTREAMERRNO(GetLastErrNo(), "FileWrite() failed for file [%s]", m_szFilename.c_str());
    }
    Assert( stWrote == sizeof rgBOM );
  }
  // Write a single character from the file - always throw on EOF.
  void WriteChar(_tyChar _tc)
  {
    Assert(FOpened());
    if (sizeof(_tyChar) == sizeof(_tyPersistAsChar))
    {
      size_t stWrote;
      int iWriteResult = FileWrite( m_foFile.HFileGet(), &_tc, sizeof _tc, &stWrote );
      if (!!iWriteResult)
      {
        Assert( -1 == iWriteResult );
        THROWBADJSONSTREAMERRNO(GetLastErrNo(), "FileWrite() failed for file [%s]", m_szFilename.c_str());
      }
      Assert( stWrote == sizeof _tc );
    }
    else
    {
      _tyStdStrPersist strPersist;
      ConvertString(strPersist, &_tc, 1);
      size_t stWrite = strPersist.length() * sizeof(_tyPersistAsChar);
      size_t stWrote;
      int iWriteResult = FileWrite( m_foFile.HFileGet(), &strPersist[0], stWrite, &stWrote );
      if (!!iWriteResult)
      {
        Assert( -1 == iWriteResult );
        THROWBADJSONSTREAMERRNO(GetLastErrNo(), "FileWrite() failed for file [%s]", m_szFilename.c_str());
      }
      Assert( stWrote == stWrite );
    }
  }
  void WriteRawChars(_tyLPCSTR _psz, ssize_t _sstLen = -1)
  {
    Assert(FOpened());
    if (_sstLen < 0)
      _sstLen = _tyCharTraits::StrLen(_psz);
    if (sizeof(_tyChar) != sizeof(_tyPersistAsChar))
    {
      _tyStdStrPersist strPersist;
      ConvertString(strPersist, _psz, _sstLen);
      size_t stWrite = strPersist.length() * sizeof(_tyPersistAsChar);
      size_t stWrote;
      int iWriteResult = FileWrite( m_foFile.HFileGet(), &strPersist[0], stWrite, &stWrote );
      if (!!iWriteResult)
      {
        Assert( -1 == iWriteResult );
        THROWBADJSONSTREAMERRNO(GetLastErrNo(), "FileWrite() failed for file [%s]", m_szFilename.c_str());
      }
      Assert( stWrote == stWrite );
    }
    else
    {
      size_t stWrote;
      int iWriteResult = FileWrite( m_foFile.HFileGet(), _psz, _sstLen, &stWrote );
      if (!!iWriteResult)
      {
        Assert( -1 == iWriteResult );
        THROWBADJSONSTREAMERRNO(GetLastErrNo(), "FileWrite() failed for file [%s]", m_szFilename.c_str());
      }
      Assert( stWrote == _sstLen );
    }
  }
  // If <_fEscape> then we escape all special characters when writing.
  void WriteString(bool _fEscape, _tyLPCSTR _psz, ssize_t _sstLen = -1, const _tyJsonFormatSpec *_pjfs = 0)
  {
    JsonOutputStream_WriteString( *this, _fEscape, _psz, _sstLen, _pjfs );
  }
protected:
  std::string m_szFilename;
  std::string m_szExceptionString;
  FileObj m_foFile;
};

// JsonMemMappedOutputStream: A class using open(), read(), etc.
template <class t_tyCharTraits, class t_tyPersistAsChar = typename t_tyCharTraits::_tyPersistAsChar>
class JsonMemMappedOutputStream : public JsonOutputStreamBase<t_tyCharTraits, size_t>
{
  typedef JsonMemMappedOutputStream _tyThis;
  typedef JsonOutputStreamBase<t_tyCharTraits, size_t> _tyBase;
public:
  typedef t_tyCharTraits _tyCharTraits;
  typedef typename _tyCharTraits::_tyChar _tyChar;
  typedef t_tyPersistAsChar _tyPersistAsChar;
  typedef typename _tyCharTraits::_tyLPCSTR _tyLPCSTR;
  typedef std::basic_string<_tyPersistAsChar> _tyStdStrPersist;
  typedef size_t _tyFilePos;
  typedef JsonReadCursor<_tyThis> _tyJsonReadCursor;
  typedef JsonFormatSpec<_tyCharTraits> _tyJsonFormatSpec;
  static const _tyFilePos s_knGrowFileByBytes = 65536 * 4;

  JsonMemMappedOutputStream() = default;
  ~JsonMemMappedOutputStream() noexcept(false)
  {
    bool fInUnwinding = !!std::uncaught_exceptions();
    (void)Close(!fInUnwinding); // Only throw on error from close if we are not currently unwinding.
  }
  // We can't allow copying because the mapping may be resized under the copy.
  JsonMemMappedOutputStream( JsonMemMappedOutputStream const & _r ) = delete;
  JsonMemMappedOutputStream & operator = ( JsonMemMappedOutputStream const & _r ) = delete;
  JsonMemMappedOutputStream( JsonMemMappedOutputStream && _rr )
  {
    swap( _rr );
  }
  JsonMemMappedOutputStream & operator = ( JsonMemMappedOutputStream && _rr )
  {
    _tyThis acquire( std::move( _rr ) );
    swap( acquire );
  }
  void swap(JsonMemMappedOutputStream &_r)
  {
    _r.m_szFilename.swap(m_szFilename);
    _r.m_szExceptionString.swap(m_szExceptionString);
    m_foFile.swap( _r.m_foFile );
    m_fmoFile.swap( _r.m_fmoFile );
    std::swap(_r.m_cpxMappedCur, m_cpxMappedCur);
    std::swap(_r.m_cpxMappedEnd, m_cpxMappedEnd);
    std::swap(_r.m_stMapped, m_stMapped);
  }
  // This is a manner of indicating that something happened during streaming.
  // Since we use object destruction to finalize writes to a file and cannot throw out of a destructor.
  void SetExceptionString(const char *_szWhat)
  {
    m_szExceptionString = _szWhat;
  }
  void GetExceptionString(std::string &_rstrExceptionString)
  {
    _rstrExceptionString = m_szExceptionString;
  }
  bool FOpened() const
  {
    Assert( m_foFile.FIsOpen() == m_fmoFile.FIsOpen() );
    return m_fmoFile.FIsOpen();
  }
  // Throws on open failure. This object owns the lifetime of the file descriptor.
  void Open(const char *_szFilename)
  {
    Close(true); // If truncating the previously open file fails then we want to know.
    FileObj foFile( CreateReadWriteFile( _szFilename ) );
    if ( !foFile.FIsOpen() )
      THROWBADJSONSTREAMERRNO(GetLastErrNo(), "Unable to CreateReadWriteFile() file [%s]", _szFilename);
    int iResult = FileSetSize( foFile.HFileGet(), s_knGrowFileByBytes ); // Set initial size.
    if ( !!iResult )
      THROWBADJSONSTREAMERRNO(GetLastErrNo(), "Unable to FileSetSize() file [%s]", _szFilename);
    FileMappingObj fmoFile( MapReadWriteHandle( foFile.HFileGet() ) );
    if ( !fmoFile.FIsOpen() )
      THROWBADJSONSTREAMERRNO(GetLastErrNo(), "Mapping failed for [%s]", _szFilename);
    m_stMapped = s_knGrowFileByBytes;
    m_cpxMappedCur = (_tyPersistAsChar *)fmoFile.Pv();
    m_cpxMappedEnd = m_cpxMappedCur + (m_stMapped / sizeof(_tyPersistAsChar));
    m_szFilename = _szFilename; // For error reporting and general debugging. Of course we don't need to store this.
    m_foFile.swap( foFile );
    m_fmoFile.swap( fmoFile );
  }

  // Allow throwing on error because closing actually does something and we need to know if it succeeds.
  int Close( bool _fThrowOnError ) noexcept(false)
  {
    if ( !m_fmoFile.FIsOpen() )
    {
      // Not much to do cuz we don't know about the size of the mapping, etc. Just close the file.
      m_foFile.Close();
      return 0;
    }
    void * pvMappedSave = m_fmoFile.Pv();
    int iCloseFileMapping = m_fmoFile.Close();
    vtyErrNo errCloseFileMapping = !iCloseFileMapping ? vkerrNullErrNo : GetLastErrNo();
    vtyErrNo errTruncate = vkerrNullErrNo;
    vtyErrNo errCloseFile = vkerrNullErrNo;
    if ( m_foFile.FIsOpen() )
    {
      // We need to truncate the file to m_cpxMappedCur - m_pvMapped bytes.
      size_t stSizeTruncate = (m_cpxMappedCur - (_tyPersistAsChar *)pvMappedSave) * sizeof(_tyPersistAsChar);
      int iTruncate = FileSetSize(m_foFile.HFileGet(), stSizeTruncate);
      vtyErrNo errTruncate = !iTruncate ? vkerrNullErrNo : GetLastErrNo();
      int iClose = m_foFile.Close();
      vtyErrNo errCloseFile = !iClose ? vkerrNullErrNo : GetLastErrNo();
    }
    vtyErrNo errFirst;
    unsigned nError;
    if ( ( (nError = 1), ( vkerrNullErrNo != ( errFirst = errTruncate ) ) ) || 
         ( (nError = 2), ( vkerrNullErrNo != ( errFirst = errCloseFileMapping ) ) ) ||
         ( (nError = 3), ( vkerrNullErrNo != ( errFirst = errCloseFile ) ) ) )
    {
      // Ensure that the errno is represented in the last error:
      SetLastErrNo( errFirst );
      if ( _fThrowOnError )
      {
        const char * pcThrow;
        switch( nError )
        {
          case 1: pcThrow = "Error encountered truncating [%s]"; break;
          case 2: pcThrow = "Error encountered closing file mapping [%s]"; break;
          case 3: pcThrow = "Error encountered closing file [%s]"; break;
          default: pcThrow = "Wh-what?! [%s]"; break;
        }
        THROWBADJSONSTREAMERRNO( errFirst, pcThrow, m_szFilename.c_str() );
      }
    }
    return errFirst == vkerrNullErrNo ? 0 : -1;
  }
  // Read a single character from the file - always throw on EOF.
  void WriteChar(_tyChar _tc)
  {
    if (sizeof(_tyChar) == sizeof(_tyPersistAsChar))
    {
      _CheckGrowMap(1);
      *m_cpxMappedCur++ = _tc;
    }
    else
    {
      _tyStdStrPersist strPersist;
      ConvertString(strPersist, &_tc, 1);
      _CheckGrowMap(strPersist.length());
      memcpy(m_cpxMappedCur, &strPersist[0], strPersist.length() * sizeof( _tyPersistAsChar ));
      m_cpxMappedCur += strPersist.length();
    }
  }
  void WriteRawChars(_tyLPCSTR _psz, ssize_t _sstLen = -1)
  {
    if (!_sstLen)
      return;
    if (_sstLen < 0)
      _sstLen = _tyCharTraits::StrLen(_psz);
    if (sizeof(_tyChar) != sizeof(_tyPersistAsChar))
    {
      _tyStdStrPersist strPersist;
      ConvertString(strPersist, _psz, _sstLen);
      _CheckGrowMap(strPersist.length());
      memcpy(m_cpxMappedCur, &strPersist[0], strPersist.length() * sizeof(_tyPersistAsChar ));
      m_cpxMappedCur += strPersist.length();
    }
    else
    {
      _CheckGrowMap((size_t)_sstLen);
      memcpy(m_cpxMappedCur, _psz, _sstLen * sizeof(_tyChar));
      m_cpxMappedCur += _sstLen;
    }
  }
  // If <_fEscape> then we escape all special characters when writing.
  void WriteString(bool _fEscape, _tyLPCSTR _psz, ssize_t _sstLen = -1, const _tyJsonFormatSpec *_pjfs = 0)
  {
    JsonOutputStream_WriteString( *this, _fEscape, _psz, _sstLen, _pjfs );
  }
protected:
  void _CheckGrowMap(size_t _charsByAtLeast)
  {
    if (size_t(m_cpxMappedEnd - m_cpxMappedCur) < _charsByAtLeast)
      _GrowMap(_charsByAtLeast);
  }
  void _GrowMap(size_t _charsByAtLeast)
  {
    VerifyThrow( m_foFile.FIsOpen() && m_fmoFile.FIsOpen() );
    // unmap, grow file, remap. That was easy!
    _charsByAtLeast *= sizeof(_tyPersistAsChar); // scale from chars to bytes.
    size_t stGrowBy = (((_charsByAtLeast - 1) / s_knGrowFileByBytes) + 1) * s_knGrowFileByBytes;
    void *pvOldMapping = m_fmoFile.Pv();
    (void)m_fmoFile.Close();
    m_stMapped += stGrowBy;
    int iFileSetSize = FileSetSize(m_foFile.HFileGet(), m_stMapped);
    if (-1 == iFileSetSize)
      THROWBADJSONSTREAMERRNO(GetLastErrNo(), "FileSetSize() failed for file [%s].", m_szFilename.c_str());
    m_fmoFile.SetHMMFile( MapReadWriteHandle( m_foFile.HFileGet() ) );
    if ( !m_fmoFile.FIsOpen() )
      THROWBADJSONSTREAMERRNO(GetLastErrNo(), "Remapping the failed for file [%s].", m_szFilename.c_str());
    m_cpxMappedEnd = (_tyPersistAsChar *)m_fmoFile.Pv() + (m_stMapped / sizeof(_tyPersistAsChar));
    m_cpxMappedCur = (_tyPersistAsChar *)m_fmoFile.Pv() + (m_cpxMappedCur - (_tyPersistAsChar *)pvOldMapping);
  }
  std::string m_szFilename;
  std::string m_szExceptionString;
  FileObj m_foFile; // We must keep the file open for writing to it mapped so we can resize it.
  FileMappingObj m_fmoFile;
  _tyPersistAsChar *m_cpxMappedCur{(_tyPersistAsChar*)vkpvNullMapping};
  _tyPersistAsChar *m_cpxMappedEnd{(_tyPersistAsChar*)vkpvNullMapping};
  size_t m_stMapped{};
};

// This just writes to an in-memory stream which then can be done anything with.
template <class t_tyCharTraits, class t_tyPersistAsChar = typename t_tyCharTraits::_tyPersistAsChar, size_t t_kstBlockSize = 65536>
class JsonOutputMemStream : public JsonOutputStreamBase<t_tyCharTraits, size_t>
{
  typedef JsonOutputMemStream _tyThis;
public:
  typedef t_tyCharTraits _tyCharTraits;
  typedef typename _tyCharTraits::_tyChar _tyChar;
  typedef t_tyPersistAsChar _tyPersistAsChar;
  typedef typename _tyCharTraits::_tyLPCSTR _tyLPCSTR;
  typedef std::basic_string<_tyPersistAsChar> _tyStdStrPersist;
  typedef size_t _tyFilePos;
  typedef JsonReadCursor<_tyThis> _tyJsonReadCursor;
  typedef JsonFormatSpec<_tyCharTraits> _tyJsonFormatSpec;
  typedef MemFileContainer<_tyFilePos, false> _tyMemFileContainer;
  typedef MemStream<_tyFilePos, false> _tyMemStream;

  JsonOutputMemStream()
  {
    _tyMemFileContainer mfcMemFile(t_kstBlockSize); // This creates the MemFile but then it hands it off to the MemStream and is no longer necessary.
    mfcMemFile.OpenStream(m_msMemStream);           // get an empty stream ready for writing.
  }
  ~JsonOutputMemStream()
  {
  }
  void swap(JsonOutputMemStream &_r)
  {
    _r.m_szExceptionString.swap(m_szExceptionString);
    m_msMemStream.swap(_r.m_msMemStream);
  }
  size_t GetLengthChars() const
  {
    return m_msMemStream.GetEndPos() / sizeof(_tyPersistAsChar);
  }
  size_t GetLengthBytes() const
  {
    return m_msMemStream.GetEndPos();
  }
  const _tyMemStream &GetMemStream() const
  {
    return m_msMemStream;
  }
  _tyMemStream &GetMemStream()
  {
    return m_msMemStream;
  }
  // This is a manner of indicating that something happened during streaming.
  // Since we use object destruction to finalize writes to a file and cannot throw out of a destructor.
  void SetExceptionString(const char *_szWhat)
  {
    m_szExceptionString = _szWhat;
  }
  void GetExceptionString(std::string &_rstrExceptionString)
  {
    _rstrExceptionString = m_szExceptionString;
  }
  // Read a single character from the file - always throw on EOF.
  void WriteChar(_tyChar _tc)
  {
    if (sizeof(_tyChar) == sizeof(_tyPersistAsChar))
    {
      PrepareErrNo();
      ssize_t sstWrote = m_msMemStream.Write(&_tc, sizeof _tc);
      if (-1 == sstWrote)
        THROWBADJSONSTREAMERRNO(GetLastErrNo(), "Write() failed for MemFile.");
      Assert(sstWrote == sizeof _tc);
    }
    else
    {
      _tyStdStrPersist strPersist;
      ConvertString(strPersist, &_tc, 1);
      ssize_t sstWrite = strPersist.length() * sizeof(_tyPersistAsChar);
      PrepareErrNo();
      ssize_t sstWrote = m_msMemStream.Write(&strPersist[0], sstWrite);
      if (sstWrote != sstWrite)
      {
        if (-1 == sstWrote)
          THROWBADJSONSTREAMERRNO(GetLastErrNo(), "Write() failed for MemFile.");
        else
          THROWBADJSONSTREAM("write() only wrote [%ld] bytes of [%ld] to MemFile.", sstWrote, sstWrite);
      }
    }
  }
  void WriteRawChars(_tyLPCSTR _psz, ssize_t _sstLen = -1)
  {
    if (_sstLen < 0)
      _sstLen = _tyCharTraits::StrLen(_psz);
    if (sizeof(_tyChar) != sizeof(_tyPersistAsChar))
    {
      _tyStdStrPersist strPersist;
      ConvertString(strPersist, _psz, _sstLen);
      ssize_t sstWrite = strPersist.length() * sizeof(_tyPersistAsChar);
      PrepareErrNo();
      ssize_t sstWrote = m_msMemStream.Write(&strPersist[0], sstWrite);
      if (sstWrote != sstWrite)
      {
        if (-1 == sstWrote)
          THROWBADJSONSTREAMERRNO(GetLastErrNo(), "Write() failed for MemFile.");
        else
          THROWBADJSONSTREAM("write() only wrote [%ld] bytes of [%ld] to MemFile.", sstWrote, sstWrite);
      }
    }
    else
    {
      PrepareErrNo();
      ssize_t sstWrote = m_msMemStream.Write(_psz, _sstLen * sizeof(_tyChar));
      if (-1 == sstWrote)
        THROWBADJSONSTREAMERRNO(GetLastErrNo(), "write() failed for MemFile.");
      Assert(sstWrote == _sstLen * sizeof(_tyChar)); // else we would have thrown.
    }
  }
  // If <_fEscape> then we escape all special characters when writing.
  void WriteString(bool _fEscape, _tyLPCSTR _psz, ssize_t _sstLen = -1, const _tyJsonFormatSpec *_pjfs = 0)
  {
    JsonOutputStream_WriteString( *this, _fEscape, _psz, _sstLen, _pjfs );
  }
  int IWriteMemStreamToFile(vtyFileHandle _hFile, bool _fAllowThrows) noexcept(false)
  {
    try
    {
      m_msMemStream.WriteToFile(_hFile, 0);
      return 0;
    }
    catch (std::exception const &rexc)
    {
      if (_fAllowThrows)
        throw; // rethrow and let caller handle cuz he wants to.
      LOGSYSLOG(eslmtError, "IWriteMemStreamToFile(): Caught exception [%s].", rexc.what());
      return -1;
    }
  }
protected:
  std::string m_szExceptionString;
  _tyMemStream m_msMemStream;
};

// This writes to a memstream during the write and then writes the OS file on close.
template <class t_tyCharTraits, class t_tyPersistAsChar = typename t_tyCharTraits::_tyPersistAsChar, size_t t_kstBlockSize = 65536>
class JsonFileOutputMemStream : protected JsonOutputMemStream<t_tyCharTraits, t_tyPersistAsChar, t_kstBlockSize>
{
  typedef JsonFileOutputMemStream _tyThis;
  typedef JsonOutputMemStream<t_tyCharTraits, t_tyPersistAsChar, t_kstBlockSize> _tyBase;

public:
  typedef t_tyCharTraits _tyCharTraits;
  typedef typename _tyCharTraits::_tyChar _tyChar;
  typedef t_tyPersistAsChar _tyPersistAsChar;
  typedef typename _tyCharTraits::_tyLPCSTR _tyLPCSTR;
  typedef std::basic_string<_tyPersistAsChar> _tyStdStrPersist;
  typedef size_t _tyFilePos;
  typedef JsonReadCursor<_tyThis> _tyJsonReadCursor;
  typedef JsonFormatSpec<_tyCharTraits> _tyJsonFormatSpec;

  JsonFileOutputMemStream() = default;
  ~JsonFileOutputMemStream() noexcept(false)
  {
    if (FOpened())
      (void)Close(!std::uncaught_exceptions());
  }
  void swap(JsonFileOutputMemStream &_r)
  {
    _tyBase::swap(_r);
    _r.m_szFilename.swap(m_szFilename);
    m_foFile.swap( _r.m_foFile );
  }
  // This is a manner of indicating that something happened during streaming.
  // Since we use object destruction to finalize writes to a file and cannot throw out of a destructor.
  void SetExceptionString(const char *_szWhat)
  {
    m_szExceptionString = _szWhat;
  }
  void GetExceptionString(std::string &_rstrExceptionString)
  {
    _rstrExceptionString = m_szExceptionString;
  }
  bool FOpened() const
  {
    return m_foFile.FIsOpen();
  }
  // Throws on open failure. This object owns the lifetime of the file descriptor.
  void Open(const char *_szFilename)
  {
    Close();
    Assert(!FOpened());
    m_foFile.SetHFile(CreateWriteOnlyFile(_szFilename));
    if (!FOpened())
      THROWBADJSONSTREAMERRNO(GetLastErrNo(), "Unable to CreateWriteOnlyFile() file [%s]", _szFilename);
    m_szFilename = _szFilename; // For error reporting and general debugging. Of course we don't need to store this.
  }
  // Attach to an FD whose lifetime we do not own. This can be used, for instance, to attach to stdout which is usually at FD 1 (unless reopen()ed).
  void AttachFd(vtyFileHandle _hFile)
  {
    m_foFile.SetHFile(_hFile, false);
    m_szFilename.clear();     // No filename indicates we are attached to "some hFile".
  }
  using _tyBase::IWriteMemStreamToFile;
  int Close(bool _fAllowThrows = true)
  {
    if ( FOpened() )
    {
      FileObj foFile( std::move( m_foFile ) );
      return IWriteMemStreamToFile(foFile.HFileGet(), _fAllowThrows); // Catches any exception when !_fAllowThrows otherwise throws through.
    }
    return 0;
  }
  using _tyBase::WriteChar;
  using _tyBase::WriteRawChars;
  using _tyBase::WriteString;
protected:
  std::string m_szFilename;
  std::string m_szExceptionString;
  FileObj m_foFile;
};

// Write to a basic_ostream.
template <class t_tyCharTraits, class t_tyOStream = std::ostream>
class JsonOutputOStream : public JsonOutputStreamBase<t_tyCharTraits, size_t>
{
  typedef JsonOutputOStream _tyThis;
public:
  typedef t_tyCharTraits _tyCharTraits;
  typedef t_tyOStream _tyOStream;
  typedef typename _tyCharTraits::_tyChar _tyChar;
  typedef typename _tyOStream::char_type _tyPersistAsChar;
  typedef typename _tyCharTraits::_tyLPCSTR _tyLPCSTR;
  typedef std::basic_string<_tyPersistAsChar> _tyStdStrPersist;
  typedef size_t _tyFilePos;
  typedef JsonReadCursor<_tyThis> _tyJsonReadCursor;
  typedef JsonFormatSpec<_tyCharTraits> _tyJsonFormatSpec;
  typedef MemFileContainer<_tyFilePos, false> _tyMemFileContainer;

  JsonOutputOStream( _tyOStream & _ros )
    : m_ros( _ros )
  {
  }
  ~JsonOutputOStream() = default;
  JsonOutputOStream() = delete;
  JsonOutputOStream( JsonOutputOStream const & ) = delete;
  JsonOutputOStream & operator=( JsonOutputOStream const & ) = delete;
  JsonOutputOStream( JsonOutputOStream && ) = default;
  JsonOutputOStream & operator=( JsonOutputOStream && _rr )
  {
    JsonOutputOStream jos( std::move( _rr ) );
    swap( *this );
  }
  void swap(JsonOutputOStream &_r)
  {
    _r.m_szExceptionString.swap(m_szExceptionString);
    VerifyThrowSz(&_r.m_ros == &m_ros, "swap only support for JsonOutputOStreams referencing the same ostream object.");
  }
  const _tyOStream &GetOStream() const
  {
    return m_ros;
  }
  _tyOStream &GetOStream()
  {
    return m_ros;
  }
  // This is a manner of indicating that something happened during streaming.
  // Since we use object destruction to finalize writes to a file and cannot throw out of a destructor.
  void SetExceptionString(const char *_szWhat)
  {
    m_szExceptionString = _szWhat;
  }
  void GetExceptionString(std::string &_rstrExceptionString)
  {
    _rstrExceptionString = m_szExceptionString;
  }
  // Read a single character from the file - always throw on EOF.
  void WriteChar(_tyChar _tc)
  {
    typename _tyOStream::sentry s(m_ros);
    if (s)
    {
      if (sizeof(_tyChar) == sizeof(_tyPersistAsChar))
      {
        (void)m_ros.write(&_tc, sizeof _tc); // may throw on failure subject to the caller's whims.
      }
      else
      {
        _tyStdStrPersist strPersist;
        ConvertString(strPersist, &_tc, 1);
        (void)m_ros.write(&strPersist[0], strPersist.length() ); // may throw on failure subject to the caller's whims.
      }
    }
  }
  void WriteRawChars(_tyLPCSTR _psz, ssize_t _sstLen = -1)
  {
    typename _tyOStream::sentry s(m_ros);
    if (s)
    {
      if (_sstLen < 0)
        _sstLen = _tyCharTraits::StrLen(_psz);
      if (sizeof(_tyChar) != sizeof(_tyPersistAsChar))
      {
        _tyStdStrPersist strPersist;
        ConvertString(strPersist, _psz, _sstLen);
        (void)m_ros.write(&strPersist[0], strPersist.length() ); // may throw on failure subject to the caller's whims.
      }
      else
        (void)m_ros.write(_psz, _sstLen ); // may throw on failure subject to the caller's whims.
    }
  }
  // If <_fEscape> then we escape all special characters when writing.
  void WriteString(bool _fEscape, _tyLPCSTR _psz, ssize_t _sstLen = -1, const _tyJsonFormatSpec *_pjfs = 0)
  {
    JsonOutputStream_WriteString( *this, _fEscape, _psz, _sstLen, _pjfs );
  }
protected:
  std::string m_szExceptionString;
  _tyOStream & m_ros;
};

// _EJsonValueType: The types of values in a JsonValue object.
enum _EJsonValueType : uint8_t
{
  ejvtObject,
  ejvtArray,
  ejvtNumber,
  ejvtFirstJsonSimpleValue = ejvtNumber,
  ejvtString,
  ejvtTrue,
  ejvtFirstJsonValueless = ejvtTrue,
  ejvtFalse,
  ejvtNull,
  ejvtLastJsonSpecifiedValue = ejvtNull,
  ejvtEndOfObject, // This pseudo value type indicates that the iteration of the parent object has reached its end.
  ejvtEndOfArray,  // This pseudo value type indicates that the iteration of the parent array has reached its end.
  ejvtJsonValueTypeCount
};
typedef _EJsonValueType EJsonValueType;

// class JsonValue:
// The base level object for a Json file is a JsonValue - i.e. a JSON file contains a JsonValue as its root object.
template <class t_tyCharTraits>
class JsonValue
{
  typedef JsonValue _tyThis;
public:
  using _tyCharTraits = t_tyCharTraits;
  using _tyChar = typename _tyCharTraits::_tyChar;
  using _tyStdStr = typename _tyCharTraits::_tyStdStr;
  using _tyJsonObject = JsonObject<t_tyCharTraits>;
  using _tyJsonArray = JsonArray<t_tyCharTraits>;
  using _tyJsonFormatSpec = JsonFormatSpec<t_tyCharTraits>;

  // Note that this JsonValue very well may be referred to by a parent JsonValue.
  // JsonValues must be destructed carefully.

  JsonValue &operator=(const JsonValue &) = delete;

  JsonValue(JsonValue *_pjvParent = nullptr, EJsonValueType _jvtType = ejvtJsonValueTypeCount)
      : m_pjvParent(_pjvParent),
        m_jvtType(_jvtType)
  {
    Assert(!m_pvValue); // Should have been zeroed by default member initialization on site.
  }
  JsonValue(JsonValue const &_r)
      : m_pjvParent(_r.m_pjvParent),
        m_jvtType(_r.m_jvtType)
  {
    if (!!_r.m_pvValue) // If the value is populated then copy it.
      _CreateValue(_r);
  }
  JsonValue(JsonValue &&_rr)
  {
    swap(_rr);
  }
  ~JsonValue()
  {
    if (m_pvValue)
      _DestroyValue();
  }

  // Take ownership of the value passed by the caller - just swap it in.
  void SetValue(JsonValue &&_rr)
  {
    std::swap(_rr.m_jvtType, m_jvtType);
    std::swap(_rr.m_pvValue, m_pvValue);
  }
  JsonValue &operator=(JsonValue &&_rr)
  {
    if (this != &_rr)
    {
      Destroy();
      swap(_rr);
    }
    return *this;
  }
  void swap(JsonValue &_r)
  {
    std::swap(_r.m_pjvParent, m_pjvParent);
    std::swap(_r.m_pvValue, m_pvValue);
    std::swap(_r.m_jvtType, m_jvtType);
  }
  void Destroy() // Destroy and make null.
  {
    if (m_pvValue)
      _DestroyValue();
    m_pjvParent = 0;
    m_jvtType = ejvtJsonValueTypeCount;
    Assert(FIsNull());
  }
  void DestroyValue() // Just destroy and nullify any associated dynamically allocated value.
  {
    if (m_pvValue)
      _DestroyValue();
  }

  // Set that this value is at the end of the iteration according to <_fObject>.
  void SetEndOfIteration(bool _fObject)
  {
    DestroyValue();
    m_jvtType = _fObject ? ejvtEndOfObject : ejvtEndOfArray;
  }

  void SetPjvParent(const JsonValue *_pjvParent)
  {
    m_pjvParent = _pjvParent;
  }
  const JsonValue *PjvGetParent() const
  {
    return m_pjvParent == this ? 0 : m_pjvParent;
  }
  bool FCheckValidParent() const
  {
    return !!m_pjvParent; // Should always have some parent - it might be ourselves.
  }

  bool FEmptyValue() const
  {
    return !m_pvValue;
  }
  bool FIsNull() const
  {
    return !m_pjvParent && !m_pvValue && (ejvtJsonValueTypeCount == m_jvtType);
  }
  bool FAtRootValue() const
  {
    return !PjvGetParent();
  }

  EJsonValueType JvtGetValueType() const
  {
    return m_jvtType;
  }
  void SetValueType(EJsonValueType _jvt)
  {
    m_jvtType = _jvt;
  }
  bool FAtAggregateValue() const
  {
    return (m_jvtType == ejvtObject) || (m_jvtType == ejvtArray);
  }
  bool FAtObjectValue() const
  {
    return (m_jvtType == ejvtObject);
  }
  bool FAtArrayValue() const
  {
    return (m_jvtType == ejvtArray);
  }

  // We assume that we have sole use of this input stream - as any such sharing would need complex guarding otherwise.
  // We should never need to backtrack when reading from the stream - we may need to fast forward for sure but not backtrack.
  // As such we should always be right at the point we want to be within the file and should never need to seek backward to reach the point we need to.
  // This means that we shouldn't need to actually store any position context within the read cursor since the presumption is that we are always where we
  //  should be. We will store this info in debug and use this to ensure that we are where we think we should be.
  template <class t_tyInputStream, class t_tyJsonReadCursor>
  void AttachInputStream(t_tyInputStream &_ris, t_tyJsonReadCursor &_rjrc)
  {
    Assert(FIsNull()); // We should be null here - we should be at the root and have no parent.
    // Set the parent to ourselves as a sanity check.
    m_pjvParent = this;
    _rjrc.AttachInputStream(_ris, this);
  }

  // Return the type of the upcoming value from the first character.
  static EJsonValueType GetJvtTypeFromChar(_tyChar _tc)
  {
    switch (_tc)
    {
    case _tyCharTraits::s_tcLeftSquareBr:
      return ejvtArray;
    case _tyCharTraits::s_tcLeftCurlyBr:
      return ejvtObject;
    case _tyCharTraits::s_tcMinus:
    case _tyCharTraits::s_tc0:
    case _tyCharTraits::s_tc1:
    case _tyCharTraits::s_tc2:
    case _tyCharTraits::s_tc3:
    case _tyCharTraits::s_tc4:
    case _tyCharTraits::s_tc5:
    case _tyCharTraits::s_tc6:
    case _tyCharTraits::s_tc7:
    case _tyCharTraits::s_tc8:
    case _tyCharTraits::s_tc9:
      return ejvtNumber;
    case _tyCharTraits::s_tcDoubleQuote:
      return ejvtString;
    case _tyCharTraits::s_tcf:
      return ejvtFalse;
    case _tyCharTraits::s_tct:
      return ejvtTrue;
    case _tyCharTraits::s_tcn:
      return ejvtNull;
    default:
      return ejvtJsonValueTypeCount;
    }
  }

  template <class t_tyJsonOutputStream>
  void WriteSimpleValue(t_tyJsonOutputStream &_rjos, const _tyJsonFormatSpec *_pjfs = 0) const
  {
    switch (m_jvtType)
    {
    case ejvtNumber:
    case ejvtString:
      Assert(!!m_pvValue); // We expect this to be populated but we will gracefully fail.
      if (ejvtString == m_jvtType)
        _rjos.WriteChar(_tyCharTraits::s_tcDoubleQuote);
      _rjos.WriteString(ejvtString == m_jvtType, PGetStringValue()->c_str(), PGetStringValue()->length(), ejvtString == m_jvtType ? _pjfs : 0);
      if (ejvtString == m_jvtType)
        _rjos.WriteChar(_tyCharTraits::s_tcDoubleQuote);
      break;
    case ejvtTrue:
      _rjos.WriteString(false, str_array_cast<_tyChar>("true"), 4);
      break;
    case ejvtFalse:
      _rjos.WriteString(false, str_array_cast<_tyChar>("false"), 5);
      break;
    case ejvtNull:
      _rjos.WriteString(false, str_array_cast<_tyChar>("null"), 4);
      break;
    default:
      Assert(0);
      break;
    }
  }

  _tyJsonObject *PCreateJsonObject()
  {
    Assert(ejvtObject == m_jvtType);
    _CreateValue();
    return (ejvtObject == m_jvtType) ? (_tyJsonObject *)m_pvValue : 0;
  }
  _tyJsonObject *PGetJsonObject() const
  {
    Assert(ejvtObject == m_jvtType);
    if (!m_pvValue)
      _CreateValue();
    return (ejvtObject == m_jvtType) ? (_tyJsonObject *)m_pvValue : 0;
  }
  _tyJsonArray *PCreateJsonArray()
  {
    Assert(ejvtArray == m_jvtType);
    _CreateValue();
    return (ejvtArray == m_jvtType) ? (_tyJsonArray *)m_pvValue : 0;
  }
  _tyJsonArray *PGetJsonArray() const
  {
    Assert(ejvtArray == m_jvtType);
    if (!m_pvValue)
      _CreateValue();
    return (ejvtArray == m_jvtType) ? (_tyJsonArray *)m_pvValue : 0;
  }
  _tyStdStr *PCreateStringValue()
  {
    Assert((ejvtNumber == m_jvtType) || (ejvtString == m_jvtType));
    _CreateValue();
    return ((ejvtNumber == m_jvtType) || (ejvtString == m_jvtType)) ? (_tyStdStr *)m_pvValue : 0;
  }
  _tyStdStr *PGetStringValue() const
  {
    Assert((ejvtNumber == m_jvtType) || (ejvtString == m_jvtType));
    if (!m_pvValue)
      _CreateValue();
    return ((ejvtNumber == m_jvtType) || (ejvtString == m_jvtType)) ? (_tyStdStr *)m_pvValue : 0;
  }

protected:
  void _DestroyValue()
  {
    void *pvValue = m_pvValue;
    m_pvValue = 0;
    _DestroyValue(pvValue);
  }
  void _DestroyValue(void *_pvValue)
  {
    // We need to destroy any connected objects:
    switch (m_jvtType)
    {
    case ejvtObject:
      delete (_tyJsonObject *)_pvValue;
      break;
    case ejvtArray:
      delete (_tyJsonArray *)_pvValue;
      break;
    case ejvtNumber:
    case ejvtString:
      delete (_tyStdStr *)_pvValue;
      break;
    case ejvtTrue:
    case ejvtFalse:
    case ejvtNull:
    default:
      Assert(0);
      break;
    }
  }
  void _CreateValue() const
  {
    Assert(!m_pvValue);
    // We need to create a connected value object depending on type:
    switch (m_jvtType)
    {
    case ejvtObject:
      m_pvValue = DBG_NEW _tyJsonObject(this);
      break;
    case ejvtArray:
      m_pvValue = DBG_NEW _tyJsonArray(this);
      break;
    case ejvtNumber:
    case ejvtString:
      m_pvValue = DBG_NEW _tyStdStr();
      break;
    case ejvtTrue:
    case ejvtFalse:
    case ejvtNull:
    default:
      Assert(0); // Shouldn't be calling for cases for which there is no value.
      break;
    }
  }
  // Create a new value that is a copy of from the passed JsonValue.
  void _CreateValue(JsonValue const &_r) const
  {
    Assert(!m_pvValue);
    Assert(m_jvtType == _r.m_jvtType); // handled by caller.
    Assert(!!_r.m_pvValue);

    // We need to create a connected value object depending on type:
    switch (m_jvtType)
    {
    case ejvtObject:
      m_pvValue = DBG_NEW _tyJsonObject(*_r.PGetJsonObject());
      break;
    case ejvtArray:
      m_pvValue = DBG_NEW _tyJsonArray(*_r.PGetJsonArray());
      break;
    case ejvtNumber:
    case ejvtString:
      m_pvValue = DBG_NEW _tyStdStr(*_r.PGetStringValue());
      break;
    case ejvtTrue:
    case ejvtFalse:
    case ejvtNull:
    default:
      Assert(0); // Shouldn't be calling for cases for which there is no value.
      break;
    }
  }

  const JsonValue *m_pjvParent{}; // We make this const and then const_cast<> as needed.
  union
  {
    mutable void *m_pvValue{};
    mutable _tyStdStr *m_pstrValue;
    mutable _tyJsonObject *m_pjoValue;
    mutable _tyJsonArray *m_pjaValue;
  };

  //mutable void * m_pvValue{}; // Just use void* since all our values are pointers to objects - the above works better in the debugger...
  // m_pvValue is one of:
  // for ejvtTrue, ejvtFalse, ejvtNull: nullptr
  // for ejvtObject: JsonObject *
  // for ejvtArray: JsonArray *
  // for ejvtNumber, ejvtString: _tyStdStr *
  EJsonValueType m_jvtType{ejvtJsonValueTypeCount}; // Type of the JsonValue.
};

// JsonFormatSpec:
// Formatting specifications for pretty printing JSON files.
template <class t_tyCharTraits>
class JsonFormatSpec
{
  typedef JsonFormatSpec _tyThis;

public:
  typedef t_tyCharTraits _tyCharTraits;
  typedef typename t_tyCharTraits::_tyChar _tyChar;

  template <class t_tyJsonOutputStream>
  void WriteLinefeed(t_tyJsonOutputStream &_rjos) const
  {
    if (m_fUseCRLF)
      _rjos.WriteRawChars(_tyCharTraits::s_szCRLF);
    else
      _rjos.WriteChar(_tyCharTraits::s_tcNewline);
  }

  template <class t_tyJsonOutputStream>
  void WriteWhitespaceIndent(t_tyJsonOutputStream &_rjos, unsigned int _nLevel) const
  {
    Assert(_nLevel);
    if (_nLevel)
    {
      // Prevent crashing:
      unsigned int nIndent = _nLevel * m_nWhitespacePerIndent;
      if (nIndent > m_nMaximumIndent)
        nIndent = m_nMaximumIndent;
      _tyChar *pcSpace = (_tyChar *)alloca(nIndent * sizeof(_tyChar));
      _tyCharTraits::MemSet(pcSpace, m_fUseTabs ? _tyCharTraits::s_tcTab : _tyCharTraits::s_tcSpace, nIndent);
      _rjos.WriteRawChars(pcSpace, nIndent);
    }
  }
  template <class t_tyJsonOutputStream>
  void WriteSpace(t_tyJsonOutputStream &_rjos) const
  {
    _rjos.WriteChar(_tyCharTraits::s_tcSpace);
  }

  JsonFormatSpec() = default;
  JsonFormatSpec(JsonFormatSpec const &_r) = default;

  // Use tabs or spaces.
  bool m_fUseTabs{false};
  // The number of tabs or spaces to use for each indentation.
  unsigned int m_nWhitespacePerIndent{4};
  // The maximum indentation to keep things from getting out of view.
  unsigned int m_nMaximumIndent{60};
  // Should be use CR+LF (Windows style) for EOL?
  bool m_fUseCRLF{
#ifdef WIN32
    true
#else //!WIN32
    false
#endif //!WIN32
  };
  // Should we use escape sequences for newlines, carriage returns and tabs:
  bool m_fEscapePrintableWhitespace{true}; // Note that setting this to false violates the JSON standard - but it is useful for reading files sometimes.
  // Should we escape such whitespace when it appears at the end of a value or the whole value is whitespace?
  bool m_fEscapePrintableWhitespaceAtEndOfLine{true};
};

// JsonValueLife:
// This class is used to write JSON files.
template <class t_tyJsonOutputStream>
class JsonValueLife
{
  typedef JsonValueLife _tyThis;

public:
  typedef t_tyJsonOutputStream _tyJsonOutputStream;
  typedef typename t_tyJsonOutputStream::_tyCharTraits _tyCharTraits;
  typedef typename _tyCharTraits::_tyChar _tyChar;
  typedef typename _tyCharTraits::_tyLPCSTR _tyLPCSTR;
  typedef typename _tyCharTraits::_tyStdStr _tyStdStr;
  typedef JsonValue<_tyCharTraits> _tyJsonValue;
  typedef JsonFormatSpec<_tyCharTraits> _tyJsonFormatSpec;

  JsonValueLife() = delete; // Must have a reference to the JsonOutputStream - though we may change this later.
  JsonValueLife(JsonValueLife const &) = delete;
  JsonValueLife &operator=(JsonValueLife const &) = delete;

  JsonValueLife(JsonValueLife &&_rr, JsonValueLife *_pjvlParent = 0)
      : m_rjos(_rr.m_rjos),
        m_jv(std::move(_rr.m_jv)),
        m_pjvlParent(!_pjvlParent ? _rr.m_pjvlParent : _pjvlParent),
        m_nCurAggrLevel(_rr.m_nCurAggrLevel),
        m_nSubValuesWritten(_rr.m_nSubValuesWritten),
        m_optJsonFormatSpec(_rr.m_optJsonFormatSpec)
  {
    _rr.SetDontWritePostAmble(); // We own this things lifetime now.
  }
  void SetDontWritePostAmble()
  {
    m_nCurAggrLevel = UINT_MAX;
  }
  bool FDontWritePostAmble() const
  {
    return m_nCurAggrLevel == UINT_MAX;
  }

  JsonValueLife(t_tyJsonOutputStream &_rjos, EJsonValueType _jvt, const _tyJsonFormatSpec *_pjfs = 0)
      : m_rjos(_rjos),
        m_jv(nullptr, _jvt)
  {
    if (!!_pjfs)
      m_optJsonFormatSpec.emplace(*_pjfs);
    _WritePreamble(_jvt);
  }
  JsonValueLife(JsonValueLife &_jvl, EJsonValueType _jvt)
      : m_rjos(_jvl.m_rjos),
        m_pjvlParent(&_jvl),
        m_nCurAggrLevel(_jvl.m_nCurAggrLevel + 1),
        m_jv(&_jvl.RJvGet(), _jvt), // Link to parent JsonValue.
        m_optJsonFormatSpec(_jvl.m_optJsonFormatSpec)
  {
    _WritePreamble(_jvt);
  }
  void _WritePreamble(EJsonValueType _jvt)
  {
    if (m_pjvlParent && !!m_pjvlParent->NSubValuesWritten()) // Write a comma if we aren't the first sub value.
      m_rjos.WriteChar(_tyCharTraits::s_tcComma);
    if (!!m_optJsonFormatSpec && !!NCurAggrLevel())
    {
      m_optJsonFormatSpec->WriteLinefeed(m_rjos);
      m_optJsonFormatSpec->WriteWhitespaceIndent(m_rjos, NCurAggrLevel());
    }
    // For aggregate types we will need to write something to the output stream right away.
    if (ejvtObject == _jvt)
      m_rjos.WriteChar(_tyCharTraits::s_tcLeftCurlyBr);
    else if (ejvtArray == _jvt)
      m_rjos.WriteChar(_tyCharTraits::s_tcLeftSquareBr);
  }
  JsonValueLife(JsonValueLife &_jvl, _tyLPCSTR _pszKey, EJsonValueType _jvt)
      : m_rjos(_jvl.m_rjos),
        m_pjvlParent(&_jvl),
        m_nCurAggrLevel(_jvl.m_nCurAggrLevel + 1),
        m_jv(&_jvl.RJvGet(), _jvt), // Link to parent JsonValue.
        m_optJsonFormatSpec(_jvl.m_optJsonFormatSpec)
  {
    _WritePreamble(_pszKey, -1, _jvt);
  }
  JsonValueLife(JsonValueLife &_jvl, _tyLPCSTR _pszKey, ssize_t _stLenKey, EJsonValueType _jvt)
      : m_rjos(_jvl.m_rjos),
        m_pjvlParent(&_jvl),
        m_nCurAggrLevel(_jvl.m_nCurAggrLevel + 1),
        m_jv(&_jvl.RJvGet(), _jvt), // Link to parent JsonValue.
        m_optJsonFormatSpec(_jvl.m_optJsonFormatSpec)
  {
    _WritePreamble(_pszKey, _stLenKey, _jvt);
  }
  void _WritePreamble(_tyLPCSTR _pszKey, ssize_t _stLenKey, EJsonValueType _jvt)
  {
    if (m_pjvlParent && !!m_pjvlParent->NSubValuesWritten()) // Write a comma if we aren't the first sub value.
      m_rjos.WriteChar(_tyCharTraits::s_tcComma);
    if (!!m_optJsonFormatSpec)
    {
      if (NCurAggrLevel())
      {
        m_optJsonFormatSpec->WriteLinefeed(m_rjos);
        m_optJsonFormatSpec->WriteWhitespaceIndent(m_rjos, NCurAggrLevel());
      }
    }
    m_rjos.WriteChar(_tyCharTraits::s_tcDoubleQuote);
    m_rjos.WriteString(true, _pszKey, _stLenKey);
    m_rjos.WriteChar(_tyCharTraits::s_tcDoubleQuote);
    m_rjos.WriteChar(_tyCharTraits::s_tcColon);

    if (!!m_optJsonFormatSpec)
      m_optJsonFormatSpec->WriteSpace(m_rjos);

    // For aggregate types we will need to write something to the output stream right away.
    if (ejvtObject == _jvt)
      m_rjos.WriteChar(_tyCharTraits::s_tcLeftCurlyBr);
    else if (ejvtArray == _jvt)
      m_rjos.WriteChar(_tyCharTraits::s_tcLeftSquareBr);
  }
  ~JsonValueLife() noexcept(false) // We might throw from here.
  {
    // If we within a stack unwinding due to another exception then we don't want to throw, otherwise we do.
    bool fInUnwinding = !!std::uncaught_exceptions();
    if (fInUnwinding)
      return; // We can't be sure of our state and indeed we were seeing this during testing with purposefully corrupt files - which would lead to an AV.
    if (!FDontWritePostAmble())
      _WritePostamble(); // The postamble is written here for all objects.
  }
  void _WritePostamble()
  {
    if (!!m_nSubValuesWritten && !!m_optJsonFormatSpec)
    {
      Assert((ejvtObject == m_jv.JvtGetValueType()) || (ejvtArray == m_jv.JvtGetValueType()));
      m_optJsonFormatSpec->WriteLinefeed(m_rjos);
      if (NCurAggrLevel())
        m_optJsonFormatSpec->WriteWhitespaceIndent(m_rjos, NCurAggrLevel());
    }
    if (ejvtObject == m_jv.JvtGetValueType())
    {
      m_rjos.WriteChar(_tyCharTraits::s_tcRightCurlyBr);
    }
    else if (ejvtArray == m_jv.JvtGetValueType())
    {
      m_rjos.WriteChar(_tyCharTraits::s_tcRightSquareBr);
    }
    else
    {
      // Then we will write the simple value in m_jv. There should be a non-empty value there.
      m_jv.WriteSimpleValue(m_rjos, !m_optJsonFormatSpec ? 0 : &*m_optJsonFormatSpec);
    }
    if (!!m_pjvlParent)
      m_pjvlParent->IncSubValuesWritten(); // We have successfully written a subobject.
  }

  // Accessors:
  _tyJsonValue const &RJvGet() const { return m_jv; }
  _tyJsonValue &RJvGet() { return m_jv; }
  EJsonValueType JvtGetValueType() const
  {
    return m_jv.JvtGetValueType();
  }
  bool FAtAggregateValue() const
  {
    return m_jv.FAtAggregateValue();
  }
  bool FAtObjectValue() const
  {
    return m_jv.FAtObjectValue();
  }
  bool FAtArrayValue() const
  {
    return m_jv.FAtArrayValue();
  }
  void SetValue(_tyJsonValue &&_rrjvValue)
  {
    m_jv.SetValue(std::move(_rrjvValue));
  }
  void IncSubValuesWritten()
  {
    ++m_nSubValuesWritten;
  }
  unsigned int NSubValuesWritten() const
  {
    return m_nSubValuesWritten;
  }
  unsigned int NCurAggrLevel() const
  {
    return m_nCurAggrLevel;
  }

  // Operations:
  // Object (key,value) operations:
  void WriteNullValue(_tyLPCSTR _pszKey)
  {
    Assert(FAtObjectValue());
    if (!FAtObjectValue())
      THROWBADJSONSEMANTICUSE("Writing a (key,value) pair to a non-object.");
    JsonValueLife jvlObjectElement(*this, _pszKey, ejvtNull);
  }
  void WriteBoolValue(_tyLPCSTR _pszKey, bool _f)
  {
    Assert(FAtObjectValue());
    if (!FAtObjectValue())
      THROWBADJSONSEMANTICUSE("Writing a (key,value) pair to a non-object.");
    JsonValueLife jvlObjectElement(*this, _pszKey, _f ? ejvtTrue : ejvtFalse);
  }
  void WriteValueType(_tyLPCSTR _pszKey, EJsonValueType _jvt)
  {
    switch (_jvt)
    {
    case ejvtNull:
    case ejvtTrue:
    case ejvtFalse:
    {
      Assert(FAtArrayValue());
      if (!FAtArrayValue())
        THROWBADJSONSEMANTICUSE("Writing a (key,value) pair to a non-object.");
      JsonValueLife jvlArrayElement(*this, _pszKey, _jvt);
    }
    break;
    default:
      THROWBADJSONSEMANTICUSE("This method only for null, true, or false.");
      break;
    }
  }

  // Only support one overload of the method that moves the string value in.
  template <class t_tyKeyChar>
  void WriteStringValue(const t_tyKeyChar *_pszKey, _tyStdStr &&_rrstrVal) // take ownership of passed string.
    requires( sizeof( t_tyKeyChar ) == sizeof( _tyChar ) )
  {
    _WriteValue(ejvtString, (_tyLPCSTR)_pszKey, StrNLen((_tyLPCSTR)_pszKey), std::move(_rrstrVal));
  }
  template <class t_tyKeyChar>
  void WriteStringValue(const t_tyKeyChar *_pszKey, ssize_t _stLenKey, _tyStdStr &&_rrstrVal) // take ownership of passed string.
    requires( sizeof( t_tyKeyChar ) == sizeof( _tyChar ) )
  {
    if (_stLenKey < 0)
      _stLenKey = StrNLen((_tyLPCSTR)_pszKey);
    _WriteValue(ejvtString, (_tyLPCSTR)_pszKey, _stLenKey, std::move(_rrstrVal));
  }
  // Allow conversion of the key and movement of the value.
  template <class t_tyKeyChar>
  void WriteStringValue(const t_tyKeyChar *_pszKey, ssize_t _stLenKey, _tyStdStr &&_rrstrVal) 
    requires( sizeof( t_tyKeyChar ) != sizeof( _tyChar ) )
  {
    _tyStdStr strConvertKey;
    ConvertString(strConvertKey, _pszKey, _stLenKey);
    _WriteValue(ejvtString, &strConvertKey[0], strConvertKey.length(), std::move(_rrstrVal));
  }

  template <class t_tyKeyStr, class t_tyValueStr>
  void WriteStringValue(t_tyKeyStr const &_rstrKey, t_tyValueStr const &_rstrVal) 
    requires(TIsCharType_v<typename t_tyKeyStr::value_type> &&TIsCharType_v<typename t_tyValueStr::value_type>)
  {
    WriteStringValue(&_rstrKey[0], _rstrKey.length(), &_rstrVal[0], _rstrVal.length());
  }
  template <class t_tyKeyChar, class t_tyValueStr>
  void WriteStringValue(const t_tyKeyChar *_pszKey, t_tyValueStr const &_rstrVal) 
    requires(TIsCharType_v<typename t_tyValueStr::value_type>)
  {
    WriteStringValue(_pszKey, -1, &_rstrVal[0], _rstrVal.length());
  }
  template <class t_tyKeyChar, class t_tyValueStr>
  void WriteStringValue(const t_tyKeyChar *_pszKey, ssize_t _stLenKey, t_tyValueStr const &_rstrVal) 
    requires(TIsCharType_v<typename t_tyValueStr::value_type>)
  {
    WriteStringValue(_pszKey, _stLenKey, &_rstrVal[0], _rstrVal.length());
  }
  template <class t_tyKeyStr, class t_tyValueChar>
  void WriteStringValue(t_tyKeyStr const &_rstrKey, const t_tyValueChar *_pszVal, ssize_t _stLenValue = -1) 
    requires(TIsCharType_v<typename t_tyKeyStr::value_type>)
  {
    WriteStringValue(&_rstrKey[0], _rstrKey.length(), _pszVal, _stLenValue);
  }
  // Base level: No conversion - but yes to casting for same size character types.
  template <class t_tyKeyChar, class t_tyValueChar >
  void WriteStringValue(const t_tyKeyChar * _pszKey, ssize_t _stLenKey, const t_tyValueChar * _pszValue, ssize_t _stLenValue = -1)
    requires( ( sizeof( t_tyKeyChar ) == sizeof( _tyChar ) ) && ( sizeof( t_tyValueChar ) == sizeof( _tyChar ) ) )
  {
    if (_stLenKey < 0)
      _stLenKey = StrNLen((_tyLPCSTR)_pszKey);
    if (_stLenValue < 0)
      _stLenValue = StrNLen((_tyLPCSTR)_pszValue);
    _WriteValue(ejvtString, (_tyLPCSTR)_pszKey, (size_t)_stLenKey, (_tyLPCSTR)_pszValue, (size_t)_stLenValue);
  }
  // Key conversion only:
  template <class t_tyKeyChar, class t_tyValueChar >
  void WriteStringValue(const t_tyKeyChar *_pszKey, ssize_t _stLenKey, const t_tyValueChar * _pszValue, ssize_t _stLenValue = -1) 
    requires( ( sizeof( t_tyKeyChar ) != sizeof( _tyChar ) ) && ( sizeof( t_tyValueChar ) == sizeof( _tyChar ) ) )
  {
    _tyStdStr strConvertKey;
    ConvertString(strConvertKey, _pszKey, _stLenKey);
    if (_stLenValue < 0)
      _stLenValue = StrNLen((_tyLPCSTR)_pszValue);
    _WriteValue(ejvtString, &strConvertKey[0], strConvertKey.length(), (_tyLPCSTR)_pszValue, (size_t)_stLenValue);
  }
  // Value conversion only:
  template <class t_tyKeyChar, class t_tyValueChar >
  void WriteStringValue(const t_tyKeyChar *_pszKey, ssize_t _stLenKey, const t_tyValueChar *_pszValue, ssize_t _stLenValue = -1)
    requires( ( sizeof( t_tyKeyChar ) == sizeof( _tyChar ) ) && ( sizeof( t_tyValueChar ) != sizeof( _tyChar ) ) )
  {
    if (_stLenKey < 0)
      _stLenKey = StrNLen((_tyLPCSTR)_pszKey);
    _tyStdStr strConvertValue;
    ConvertString(strConvertValue, _pszValue, _stLenValue);
    _WriteValue(ejvtString, (_tyLPCSTR)_pszKey, (size_t)_stLenKey, &strConvertValue[0], strConvertValue.length());
  }
  // Key and value conversion:
  template <class t_tyKeyChar, class t_tyValueChar>
  void WriteStringValue(const t_tyKeyChar *_pszKey, ssize_t _stLenKey, const t_tyValueChar *_pszValue, ssize_t _stLenValue = -1)
    requires( ( sizeof( t_tyKeyChar ) != sizeof( _tyChar ) ) && ( sizeof( t_tyValueChar ) != sizeof( _tyChar ) ) )
  {
    _tyStdStr strConvertKey;
    ConvertString(strConvertKey, _pszKey, _stLenKey);
    _tyStdStr strConvertValue;
    ConvertString(strConvertValue, _pszValue, _stLenValue);
    _WriteValue(ejvtString, &strConvertKey[0], strConvertKey.length(), &strConvertValue[0], strConvertValue.length());
  }

  // For printf formatting directly into a value we can only support two character types natively, char and wchar_t.
  // This is fine because the arguments and formating commands are only correct for those types.
  template <class t_tyConvertFromChar>
  void PrintfStringKeyValue(_tyLPCSTR _pszKey, const t_tyConvertFromChar *_pszValue, ...)
    requires( ( sizeof( t_tyConvertFromChar ) == sizeof( char ) ) || ( sizeof( t_tyConvertFromChar ) == sizeof( wchar_t ) ) ) // reuse this template easily while also limiting application.
  {
    va_list ap;
    va_start(ap, _pszValue);
    try
    {
      VPrintfStringKeyValue(_pszKey, _pszValue, ap);
    }
    catch (...)
    {
      va_end(ap);
      throw;
    }
    va_end(ap);
  }
  template <class t_tyConvertFromChar>
  void VPrintfStringKeyValue(_tyLPCSTR _pszKey, const t_tyConvertFromChar *_pszValue, va_list _ap)
    requires( ( sizeof( t_tyConvertFromChar ) == sizeof( char ) ) || ( sizeof( t_tyConvertFromChar ) == sizeof( wchar_t ) ) ) // reuse this template easily while also limiting application.
  {
    std::basic_string<t_tyConvertFromChar> str;
    VPrintfStdStr(str, _pszValue, _ap);
    WriteStringValue(_pszKey, -1, &str[0], str.length());
  }

  void WriteValue(_tyLPCSTR _pszKey, uint8_t _by)
  {
    _WriteValue(_pszKey, str_array_cast<_tyChar>("%hhu"), _by);
  }
  void WriteValue(_tyLPCSTR _pszKey, int8_t _sby)
  {
    _WriteValue(_pszKey, str_array_cast<_tyChar>("%hhd"), _sby);
  }
  void WriteValue(_tyLPCSTR _pszKey, uint16_t _us)
  {
    _WriteValue(_pszKey, str_array_cast<_tyChar>("%hu"), _us);
  }
  void WriteValue(_tyLPCSTR _pszKey, int16_t _ss)
  {
    _WriteValue(_pszKey, str_array_cast<_tyChar>("%hd"), _ss);
  }
  void WriteValue(_tyLPCSTR _pszKey, uint32_t _ui)
  {
    _WriteValue(_pszKey, str_array_cast<_tyChar>("%u"), _ui);
  }
  void WriteValue(_tyLPCSTR _pszKey, int32_t _si)
  {
    _WriteValue(_pszKey, str_array_cast<_tyChar>("%d"), _si);
  }
  void WriteValue(_tyLPCSTR _pszKey, uint64_t _ul)
  {
    _WriteValue(_pszKey, str_array_cast<_tyChar>("%lu"), _ul);
  }
  void WriteValue(_tyLPCSTR _pszKey, int64_t _sl)
  {
    _WriteValue(_pszKey, str_array_cast<_tyChar>("%ld"), _sl);
  }
  void WriteValue(_tyLPCSTR _pszKey, double _dbl)
  {
    _WriteValue(_pszKey, str_array_cast<_tyChar>("%f"), _dbl);
  }
  void WriteValue(_tyLPCSTR _pszKey, long double _ldbl)
  {
    _WriteValue(_pszKey, str_array_cast<_tyChar>("%Lf"), _ldbl);
  }
#ifdef WIN32 // Linux seems to handle this correctly without - and there's an endless loop with...
  // Translate long and unsigned long appropriately by platform size:
  void WriteValue(_tyLPCSTR _pszKey, long _l)
     requires ( sizeof( int32_t ) == sizeof( long ) )
  {
    return WriteValue(_pszKey, (int32_t)_l);
  }
  void WriteValue(_tyLPCSTR _pszKey, long _l)
    requires (sizeof(int64_t) == sizeof(long))
  {
    return WriteValue(_pszKey, (int64_t)_l);
  }
  void WriteValue(_tyLPCSTR _pszKey, unsigned long _l)
    requires (sizeof(uint32_t) == sizeof(unsigned long))
  {
    return WriteValue(_pszKey, (uint32_t)_l);
  }
  void WriteValue(_tyLPCSTR _pszKey, unsigned long _l)
    requires (sizeof(uint64_t) == sizeof(unsigned long))
  {
    return WriteValue(_pszKey, (uint64_t)_l);
  }
#endif //WIN32

  void WriteTimeStringValue(_tyLPCSTR _pszKey, time_t const &_tt)
  {
    _tyStdStr strTime;
    n_TimeUtil::TimeToString(_tt, strTime);
    WriteStringValue(_pszKey, -1, std::move(strTime));
  }
  void WriteUuidStringValue(_tyLPCSTR _pszKey, uuid_t const &_uuidt)
  {
    vtyUUIDString uusOut;
    int iRes = UUIDToString(_uuidt, uusOut, sizeof uusOut);
    Assert(!iRes);
    WriteStringValue(_pszKey, -1, uusOut, -1);
  }

  // Array (value) operations:
  void WriteNullValue()
  {
    Assert(FAtArrayValue());
    if (!FAtArrayValue())
      THROWBADJSONSEMANTICUSE("Writing a value to a non-array.");
    JsonValueLife jvlArrayElement(*this, ejvtNull);
  }
  void WriteBoolValue(bool _f)
  {
    Assert(FAtArrayValue());
    if (!FAtArrayValue())
      THROWBADJSONSEMANTICUSE("Writing a value to a non-array.");
    JsonValueLife jvlArrayElement(*this, _f ? ejvtTrue : ejvtFalse);
  }
  void WriteValueType(EJsonValueType _jvt)
  {
    switch (_jvt)
    {
    case ejvtNull:
    case ejvtTrue:
    case ejvtFalse:
    {
      Assert(FAtArrayValue());
      if (!FAtArrayValue())
        THROWBADJSONSEMANTICUSE("Writing a value to a non-array.");
      JsonValueLife jvlArrayElement(*this, _jvt);
    }
    break;
    default:
      THROWBADJSONSEMANTICUSE("This method only for null, true, or false.");
      break;
    }
  }

  template <class t_tyValueChar>
  void WriteStringValue(const t_tyValueChar *_pszValue, ssize_t _stLenValue = -1)
    requires( sizeof( t_tyValueChar ) == sizeof( _tyChar ) )
  {
    if (_stLenValue < 0)
      _stLenValue = _tyCharTraits::StrLen((_tyLPCSTR)_pszValue);
    _WriteValue(ejvtString, (_tyLPCSTR)_pszValue, (size_t)_stLenValue);
  }
  template <class t_tyValueChar>
  void WriteStringValue(const t_tyValueChar *_pszValue, ssize_t _stLenValue = -1) 
    requires( sizeof( t_tyValueChar ) != sizeof( _tyChar ) )
  {
    _tyStdStr strConvertValue;
    ConvertString(strConvertValue, _pszValue, _stLenValue);
    _WriteValue(ejvtString, &strConvertValue[0], strConvertValue.length());
  }
  template <class t_tyStr>
  void WriteStringValue(t_tyStr const &_rstrVal)
  {
    WriteStringValue(&_rstrVal[0], _rstrVal.length());
  }
  template <class t_tyStr>
  void WriteStrOrNumValue(EJsonValueType _jvt, t_tyStr const &_rstrVal)
    requires( sizeof( t_tyStr::value_type ) == sizeof( _tyChar ) )
  {
    if ((ejvtString != _jvt) && (ejvtNumber != _jvt))
      THROWBADJSONSEMANTICUSE("This method only for numbers and strings.");
    _WriteValue(_jvt, (_tyLPCSTR)&_rstrVal[0], _rstrVal.length());
  }
  template <class t_tyStr>
  void WriteStrOrNumValue(EJsonValueType _jvt, t_tyStr const &_rstrVal) 
    requires( sizeof( t_tyStr::value_type ) != sizeof( _tyChar ) )
  {
    if ((ejvtString != _jvt) && (ejvtNumber != _jvt))
      THROWBADJSONSEMANTICUSE("This method only for numbers and strings.");
    std::basic_string<_tyChar> strConvertValue;
    ConvertString(strConvertValue, _rstrVal);
    _WriteValue(_jvt, &strConvertValue[0], strConvertValue.length());
  }
  void WriteStringValue(_tyStdStr &&_rrstrVal) // take ownership of passed string.
  {
    _WriteValue(ejvtString, std::move(_rrstrVal));
  }
  template <class t_tyConvertFromChar>
  void PrintfStringValue(const t_tyConvertFromChar* _pszValue, ...)
    requires( ( sizeof( t_tyConvertFromChar ) == sizeof( char ) ) || ( sizeof( t_tyConvertFromChar ) == sizeof( wchar_t ) ) ) // reuse this template easily while also limiting application.
  {
    va_list ap;
    va_start(ap, _pszValue);
    try
    {
      VPrintfStringValue(_pszValue, ap);
    }
    catch (...)
    {
      va_end(ap);
      throw;
    }
    va_end(ap);
  }
  template <class t_tyConvertFromChar>
  void VPrintfStringValue(const t_tyConvertFromChar* _pszValue, va_list _ap)
    requires( ( sizeof( t_tyConvertFromChar ) == sizeof( char ) ) || ( sizeof( t_tyConvertFromChar ) == sizeof( wchar_t ) ) ) // reuse this template easily while also limiting application.
  {
    std::basic_string<t_tyConvertFromChar> str;
    VPrintfStdStr(str, _pszValue, _ap);
    WriteStringValue(&str[0], str.length());
  }
  void WriteTimeStringValue(time_t const &_tt)
  {
    std::string strTime;
    n_TimeUtil::TimeToString(_tt, strTime);
    _WriteValue(ejvtString, std::move(strTime));
  }
  void WriteUuidStringValue(uuid_t const &_uuidt)
  {
    vtyUUIDString uusOut;
    int iRes = UUIDToString(_uuidt, uusOut, sizeof uusOut);
    Assert(!!iRes);
    _WriteValue(ejvtString, uusOut);
  }

  void WriteValue(uint8_t _by)
  {
    _WriteValue(str_array_cast<_tyChar>("%hhu"), _by);
  }
  void WriteValue(int8_t _sby)
  {
    _WriteValue(str_array_cast<_tyChar>("%hhd"), _sby);
  }
  void WriteValue(uint16_t _us)
  {
    _WriteValue(str_array_cast<_tyChar>("%hu"), _us);
  }
  void WriteValue(int16_t _ss)
  {
    _WriteValue(str_array_cast<_tyChar>("%hd"), _ss);
  }
  void WriteValue(uint32_t _ui)
  {
    _WriteValue(str_array_cast<_tyChar>("%u"), _ui);
  }
  void WriteValue(int32_t _si)
  {
    _WriteValue(str_array_cast<_tyChar>("%d"), _si);
  }
  void WriteValue(uint64_t _ul)
  {
    _WriteValue(str_array_cast<_tyChar>("%lu"), _ul);
  }
  void WriteValue(int64_t _sl)
  {
    _WriteValue(str_array_cast<_tyChar>("%ld"), _sl);
  }
  void WriteValue(double _dbl)
  {
    _WriteValue(str_array_cast<_tyChar>("%lf"), _dbl);
  }
  void WriteValue(long double _ldbl)
  {
    _WriteValue(str_array_cast<_tyChar>("%Lf"), _ldbl);
  }

protected:
  template <class t_tyNum>
  void _WriteValue(_tyLPCSTR _pszKey, _tyLPCSTR _pszFmt, t_tyNum _num)
  {
    const int knNum = 512;
    _tyChar rgcNum[knNum];
    int nPrinted = _tyCharTraits::Snprintf(rgcNum, knNum, _pszFmt, _num);
    Assert(nPrinted < knNum);
    _WriteValue(ejvtNumber, _pszKey, StrNLen(_pszKey), rgcNum, (std::min)(nPrinted, knNum - 1));
  }
  void _WriteValue(EJsonValueType _ejvt, _tyLPCSTR _pszKey, size_t _stLenKey, _tyLPCSTR _pszValue, size_t _stLenValue)
  {
    Assert(FAtObjectValue());
    if (!FAtObjectValue())
      THROWBADJSONSEMANTICUSE("Writing a (key,value) pair to a non-object.");
    Assert(_tyCharTraits::StrNLen(_pszValue, _stLenValue) == _stLenValue);
    Assert(_tyCharTraits::StrNLen(_pszKey, _stLenKey) == _stLenKey);
    JsonValueLife jvlObjectElement(*this, _pszKey, _stLenKey, _ejvt);
    jvlObjectElement.RJvGet().PGetStringValue()->assign(_pszValue, _stLenValue);
  }
  void _WriteValue(EJsonValueType _ejvt, _tyLPCSTR _pszKey, size_t _stLenKey, _tyStdStr &&_rrstrVal)
  {
    Assert(FAtObjectValue());
    if (!FAtObjectValue())
      THROWBADJSONSEMANTICUSE("Writing a (key,value) pair to a non-object.");
    Assert(_tyCharTraits::StrNLen(_pszKey, _stLenKey) == _stLenKey);
    JsonValueLife jvlObjectElement(*this, _pszKey, _stLenKey, _ejvt);
    *jvlObjectElement.RJvGet().PCreateStringValue() = std::move(_rrstrVal);
  }

  template <class t_tyNum>
  void _WriteValue(_tyLPCSTR _pszFmt, t_tyNum _num)
  {
    const int knNum = 512;
    _tyChar rgcNum[knNum];
    int nPrinted = _tyCharTraits::Snprintf(rgcNum, knNum, _pszFmt, _num);
    Assert(nPrinted < knNum);
    _WriteValue(ejvtNumber, rgcNum, (std::min)(nPrinted, knNum - 1));
  }
  void _WriteValue(EJsonValueType _ejvt, _tyLPCSTR _pszValue, size_t _stLen)
  {
    Assert(FAtArrayValue());
    if (!FAtArrayValue())
      THROWBADJSONSEMANTICUSE("Writing a value to a non-array.");
    Assert(_tyCharTraits::StrNLen(_pszValue, _stLen) == _stLen);
    JsonValueLife jvlArrayElement(*this, _ejvt);
    jvlArrayElement.RJvGet().PGetStringValue()->assign(_pszValue, _stLen);
  }
  void _WriteValue(EJsonValueType _ejvt, _tyStdStr &&_rrstrVal)
  {
    Assert(FAtArrayValue());
    if (!FAtArrayValue())
      THROWBADJSONSEMANTICUSE("Writing a value to a non-array.");
    JsonValueLife jvlArrayElement(*this, _ejvt);
    *jvlArrayElement.RJvGet().PCreateStringValue() = std::move(_rrstrVal);
  }

  _tyJsonOutputStream &m_rjos;
  std::optional<const _tyJsonFormatSpec> m_optJsonFormatSpec;
  JsonValueLife *m_pjvlParent{}; // If we are at the root this will be zero.
  _tyJsonValue m_jv;
  unsigned int m_nSubValuesWritten{}; // When a sub-value finishes its writing it will cause this number to be increment. This allows us to place commas correctly, etc.
  unsigned int m_nCurAggrLevel{};
};

// JsonValueLifeAbstractBase:
// This will contain a polymorphic JsonValueLife<SomeOutputType>.
// Then this object can be used with virtual methods in various scenarios - since templated member methods may not be virtual.
template <class t_tyChar>
class JsonValueLifeAbstractBase
{
  typedef JsonValueLifeAbstractBase _tyThis;

public:
  typedef t_tyChar _tyChar;
  typedef JsonCharTraits<_tyChar> _tyCharTraits;
  typedef JsonValue<_tyCharTraits> _tyJsonValue;
  typedef const _tyChar *_tyLPCSTR;

  JsonValueLifeAbstractBase() = default;
  virtual ~JsonValueLifeAbstractBase() noexcept(false) = default;

  virtual void NewSubValue(EJsonValueType _jvt, std::unique_ptr<_tyThis> &_rNewSubValue) = 0;
  virtual void NewSubValue(_tyLPCSTR _pszKey, EJsonValueType _jvt, std::unique_ptr<_tyThis> &_rNewSubValue) = 0;

  virtual _tyJsonValue const &RJvGet() const = 0;
  virtual _tyJsonValue &RJvGet() = 0;
  virtual EJsonValueType JvtGetValueType() const = 0;
  virtual bool FAtAggregateValue() const = 0;
  virtual bool FAtObjectValue() const = 0;
  virtual bool FAtArrayValue() const = 0;
  virtual void SetValue(_tyJsonValue &&_rrjvValue) = 0;
  virtual unsigned int NSubValuesWritten() const = 0;
  virtual unsigned int NCurAggrLevel() const = 0;

  virtual void WriteNullValue(_tyLPCSTR _pszKey) = 0;
  virtual void WriteBoolValue(_tyLPCSTR _pszKey, bool _f) = 0;
  virtual void WriteValueType(_tyLPCSTR _pszKey, EJsonValueType _jvt) = 0;
  virtual void PrintfStringKeyValue(_tyLPCSTR _pszKey, const char * _pszValue, ...) = 0;
  virtual void PrintfStringKeyValue(_tyLPCSTR _pszKey, const wchar_t * _pszValue, ...) = 0;
  virtual void WriteValue(_tyLPCSTR _pszKey, uint8_t _by) = 0;
  virtual void WriteValue(_tyLPCSTR _pszKey, int8_t _sby) = 0;
  virtual void WriteValue(_tyLPCSTR _pszKey, uint16_t _us) = 0;
  virtual void WriteValue(_tyLPCSTR _pszKey, int16_t _ss) = 0;
  virtual void WriteValue(_tyLPCSTR _pszKey, uint32_t _ui) = 0;
  virtual void WriteValue(_tyLPCSTR _pszKey, int32_t _si) = 0;
  virtual void WriteValue(_tyLPCSTR _pszKey, uint64_t _ul) = 0;
  virtual void WriteValue(_tyLPCSTR _pszKey, int64_t _sl) = 0;
  virtual void WriteValue(_tyLPCSTR _pszKey, double _dbl) = 0;
  virtual void WriteValue(_tyLPCSTR _pszKey, long double _ldbl) = 0;

  virtual void WriteNullValue() = 0;
  virtual void WriteBoolValue(bool _f) = 0;
  virtual void WriteValueType(EJsonValueType _jvt) = 0;
  virtual void WriteStringValue(_tyLPCSTR _pszValue, ssize_t _stLen = -1) = 0;
  template <class t_tyStr>
  void WriteStringValue(t_tyStr const &_rstrVal)
  {
    WriteStringValue(&_rstrVal[0], _rstrVal.length());
  }
  virtual void PrintfStringValue( const char * _pszValue, ...) = 0;
  virtual void PrintfStringValue( const wchar_t * _pszValue, ...) = 0;
  virtual void WriteValue(uint8_t _by) = 0;
  virtual void WriteValue(int8_t _sby) = 0;
  virtual void WriteValue(uint16_t _us) = 0;
  virtual void WriteValue(int16_t _ss) = 0;
  virtual void WriteValue(uint32_t _ui) = 0;
  virtual void WriteValue(int32_t _si) = 0;
  virtual void WriteValue(uint64_t _ul) = 0;
  virtual void WriteValue(int64_t _sl) = 0;
  virtual void WriteValue(double _dbl) = 0;
  virtual void WriteValue(long double _ldbl) = 0;

  // TODO: Add a lot more virtual methods to implement various things.
};

template <class t_tyJsonOutputStream>
class JsonValueLifePoly : public JsonValueLifeAbstractBase<typename t_tyJsonOutputStream::_tyChar>,
                          public JsonValueLife<t_tyJsonOutputStream>
{
  typedef JsonValueLifeAbstractBase<typename t_tyJsonOutputStream::_tyChar> _tyAbstractBase;
  typedef JsonValueLife<t_tyJsonOutputStream> _tyBase;
  typedef JsonValueLifePoly _tyThis;

public:
  using typename _tyBase::_tyJsonValue; // Interesting if this inits the vtable correctly... hoping it will of course.
  using typename _tyBase::_tyLPCSTR;

  using JsonValueLife<t_tyJsonOutputStream>::JsonValueLife;

  // Construct the same type of JsonValueLife that we are... duh.
  void NewSubValue(EJsonValueType _jvt, std::unique_ptr<_tyAbstractBase> &_rNewSubValue) override
  {
    _rNewSubValue.reset(DBG_NEW _tyThis(*this, _jvt));
  }
  void NewSubValue(_tyLPCSTR _pszKey, EJsonValueType _jvt, std::unique_ptr<_tyAbstractBase> &_rNewSubValue) override
  {
    _rNewSubValue.reset(DBG_NEW _tyThis(*this, _pszKey, _jvt));
  }

  _tyJsonValue const &RJvGet() const override
  {
    return _tyBase::RJvGet();
  }
  _tyJsonValue &RJvGet() override
  {
    return _tyBase::RJvGet();
  }
  EJsonValueType JvtGetValueType() const override
  {
    return _tyBase::JvtGetValueType();
  }
  bool FAtAggregateValue() const override
  {
    return _tyBase::FAtAggregateValue();
  }
  bool FAtObjectValue() const override
  {
    return _tyBase::FAtObjectValue();
  }
  bool FAtArrayValue() const override
  {
    return _tyBase::FAtArrayValue();
  }
  void SetValue(_tyJsonValue &&_rrjvValue) override
  {
    _tyBase::SetValue(std::move(_rrjvValue));
  }
  unsigned int NSubValuesWritten() const override
  {
    return _tyBase::NSubValuesWritten();
  }
  unsigned int NCurAggrLevel() const override
  {
    return _tyBase::NCurAggrLevel();
  }

  void WriteNullValue(_tyLPCSTR _pszKey) override
  {
    _tyBase::WriteNullValue(_pszKey);
  }
  void WriteBoolValue(_tyLPCSTR _pszKey, bool _f) override
  {
    _tyBase::WriteBoolValue(_pszKey, _f);
  }
  void WriteValueType(_tyLPCSTR _pszKey, EJsonValueType _jvt) override
  {
    _tyBase::WriteValueType(_pszKey, _jvt);
  }
  void PrintfStringKeyValue(_tyLPCSTR _pszKey, const char * _pszValue, ...) override
  {
    va_list ap;
    va_start(ap, _pszValue);
    _tyBase::VPrintfStringKeyValue(_pszKey, _pszValue, ap);
    va_end(ap);
  }
  void PrintfStringKeyValue(_tyLPCSTR _pszKey, const wchar_t* _pszValue, ...) override
  {
    va_list ap;
    va_start(ap, _pszValue);
    _tyBase::VPrintfStringKeyValue(_pszKey, _pszValue, ap);
    va_end(ap);
  }
  void WriteValue(_tyLPCSTR _pszKey, uint8_t _v) override
  {
    _tyBase::WriteValue(_pszKey, _v);
  }
  void WriteValue(_tyLPCSTR _pszKey, int8_t _v) override
  {
    _tyBase::WriteValue(_pszKey, _v);
  }
  void WriteValue(_tyLPCSTR _pszKey, uint16_t _v) override
  {
    _tyBase::WriteValue(_pszKey, _v);
  }
  void WriteValue(_tyLPCSTR _pszKey, int16_t _v) override
  {
    _tyBase::WriteValue(_pszKey, _v);
  }
  void WriteValue(_tyLPCSTR _pszKey, uint32_t _v) override
  {
    _tyBase::WriteValue(_pszKey, _v);
  }
  void WriteValue(_tyLPCSTR _pszKey, int32_t _v) override
  {
    _tyBase::WriteValue(_pszKey, _v);
  }
  void WriteValue(_tyLPCSTR _pszKey, uint64_t _v) override
  {
    _tyBase::WriteValue(_pszKey, _v);
  }
  void WriteValue(_tyLPCSTR _pszKey, int64_t _v) override
  {
    _tyBase::WriteValue(_pszKey, _v);
  }
  void WriteValue(_tyLPCSTR _pszKey, double _v) override
  {
    _tyBase::WriteValue(_pszKey, _v);
  }
  void WriteValue(_tyLPCSTR _pszKey, long double _v) override
  {
    _tyBase::WriteValue(_pszKey, _v);
  }

  void WriteNullValue() override
  {
    _tyBase::WriteNullValue();
  }
  void WriteBoolValue(bool _f) override
  {
    _tyBase::WriteBoolValue(_f);
  }
  void WriteValueType(EJsonValueType _jvt) override
  {
    _tyBase::WriteValueType(_jvt);
  }
  void WriteStringValue(_tyLPCSTR _pszValue, ssize_t _stLen = -1) override
  {
    _tyBase::WriteStringValue(_pszValue, _stLen);
  }
  void PrintfStringValue(const char * _pszValue, ...) override
  {
    va_list ap;
    va_start(ap, _pszValue);
    _tyBase::VPrintfStringValue(_pszValue, ap);
    va_end(ap);
  }
  void PrintfStringValue(const wchar_t* _pszValue, ...) override
  {
    va_list ap;
    va_start(ap, _pszValue);
    _tyBase::VPrintfStringValue(_pszValue, ap);
    va_end(ap);
  }
  void WriteValue(uint8_t _v) override
  {
    _tyBase::WriteValue(_v);
  }
  void WriteValue(int8_t _v) override
  {
    _tyBase::WriteValue(_v);
  }
  void WriteValue(uint16_t _v) override
  {
    _tyBase::WriteValue(_v);
  }
  void WriteValue(int16_t _v) override
  {
    _tyBase::WriteValue(_v);
  }
  void WriteValue(uint32_t _v) override
  {
    _tyBase::WriteValue(_v);
  }
  void WriteValue(int32_t _v) override
  {
    _tyBase::WriteValue(_v);
  }
  void WriteValue(uint64_t _v) override
  {
    _tyBase::WriteValue(_v);
  }
  void WriteValue(int64_t _v) override
  {
    _tyBase::WriteValue(_v);
  }
  void WriteValue(double _v) override
  {
    _tyBase::WriteValue(_v);
  }
  void WriteValue(long double _v) override
  {
    _tyBase::WriteValue(_v);
  }
};

// JsonAggregate:
// This may be JsonObject or JsonArray
template <class t_tyCharTraits>
class JsonAggregate
{
  typedef JsonAggregate _tyThis;

public:
  typedef JsonValue<t_tyCharTraits> _tyJsonValue;

  JsonAggregate(const _tyJsonValue *_pjvParent)
  {
    m_jvCur.SetPjvParent(_pjvParent); // Link the contained value to the parent value. This is a soft reference - we assume any parent will always exist.
  }
  JsonAggregate(const JsonAggregate &_r) = default; // standard copy constructor works.

  const _tyJsonValue &RJvGet() const
  {
    return m_jvCur;
  }
  _tyJsonValue &RJvGet()
  {
    return m_jvCur;
  }
#if 0
  int NElement() const
  {
    return m_nObjectOrArrayElement;
  }
  void SetNElement(int _nObjectOrArrayElement) const
  {
    m_nObjectOrArrayElement = _nObjectOrArrayElement;
  }
  void IncElement()
  {
    ++m_nObjectOrArrayElement;
  }
#endif //0
  void SetEndOfIteration(bool _fObject)
  {
    m_jvCur.SetEndOfIteration(_fObject);
  }

protected:
  _tyJsonValue m_jvCur;          // The current JsonValue for this object.
#if 0
  int m_nObjectOrArrayElement{}; // The index of the object or array that this context's value corresponds to.
#endif //0
};

// class JsonObject:
// This represents an object containing key:value pairs.
template <class t_tyCharTraits>
class JsonObject : protected JsonAggregate<t_tyCharTraits> // use protected because we don't want a virtual destructor.
{
  typedef JsonAggregate<t_tyCharTraits> _tyBase;
  typedef JsonObject _tyThis;

public:
  using _tyCharTraits = t_tyCharTraits;
  typedef JsonValue<t_tyCharTraits> _tyJsonValue;
  using _tyStdStr = typename _tyCharTraits::_tyStdStr;

  JsonObject(const _tyJsonValue *_pjvParent)
      : _tyBase(_pjvParent)
  {
  }
  JsonObject(JsonObject const &_r) = default;
  using _tyBase::RJvGet;
#if 0
  using _tyBase::IncElement;
  using _tyBase::NElement;
  using _tyBase::SetNElement;
#endif //0

  const _tyStdStr &RStrKey() const
  {
    return m_strCurKey;
  }
  void GetKey(_tyStdStr &_strCurKey) const
  {
    _strCurKey = m_strCurKey;
  }
  void ClearKey()
  {
    m_strCurKey.clear();
  }
  void SwapKey(_tyStdStr &_rstr)
  {
    m_strCurKey.swap(_rstr);
  }

  // Set the JsonObject for the end of iteration.
  void SetEndOfIteration()
  {
    _tyBase::SetEndOfIteration(true);
    m_strCurKey.clear();
  }
  bool FEndOfIteration() const
  {
    return m_jvCur.JvtGetValueType() == ejvtEndOfObject;
  }

protected:
  using _tyBase::m_jvCur;
  _tyStdStr m_strCurKey; // The current label for this object.
};

// JsonArray:
// This represents an JSON array containing multiple values.
template <class t_tyCharTraits>
class JsonArray : protected JsonAggregate<t_tyCharTraits>
{
  typedef JsonAggregate<t_tyCharTraits> _tyBase;
  typedef JsonArray _tyThis;

public:
  using _tyCharTraits = t_tyCharTraits;
  typedef JsonValue<t_tyCharTraits> _tyJsonValue;
  using _tyStdStr = typename _tyCharTraits::_tyStdStr;

  JsonArray(const _tyJsonValue *_pjvParent)
      : _tyBase(_pjvParent)
  {
  }
  JsonArray(JsonArray const &_r) = default;
  using _tyBase::RJvGet;
#if 0
  using _tyBase::IncElement;
  using _tyBase::NElement;
  using _tyBase::SetNElement;
#endif //0

  // Set the JsonArray for the end of iteration.
  void SetEndOfIteration()
  {
    _tyBase::SetEndOfIteration(false);
  }
  using _tyBase::m_jvCur;
  bool FEndOfIteration() const
  {
    return m_jvCur.JvtGetValueType() == ejvtEndOfArray;
  }
};

// JsonReadContext:
// This is the context maintained for a read at a given point along the current path.
// The JsonReadCursor maintains a stack of these.
template <class t_tyJsonInputStream>
class JsonReadContext
{
  typedef JsonReadContext _tyThis;
  friend JsonReadCursor<t_tyJsonInputStream>;

public:
  typedef t_tyJsonInputStream _tyJsonInputStream;
  typedef typename t_tyJsonInputStream::_tyCharTraits _tyCharTraits;
  typedef typename _tyCharTraits::_tyChar _tyChar;
  typedef JsonValue<_tyCharTraits> _tyJsonValue;
  typedef JsonObject<_tyCharTraits> _tyJsonObject;
  typedef JsonArray<_tyCharTraits> _tyJsonArray;
  typedef typename t_tyJsonInputStream::_tyFilePos _tyFilePos;
  using _tyStdStr = typename _tyCharTraits::_tyStdStr;

  JsonReadContext(_tyJsonValue *_pjvCur = nullptr, JsonReadContext *_pjrxPrev = nullptr)
      : m_pjrxPrev(_pjrxPrev),
        m_pjvCur(_pjvCur)
  {
  }

  _tyJsonValue *PJvGet() const
  {
    return m_pjvCur;
  }
  _tyJsonObject *PGetJsonObject() const
  {
    Assert(!!m_pjvCur);
    return m_pjvCur->PGetJsonObject();
  }
  _tyJsonArray *PGetJsonArray() const
  {
    Assert(!!m_pjvCur);
    return m_pjvCur->PGetJsonArray();
  }

  JsonReadContext *PJrcGetNext()
  {
    return &*m_pjrxNext;
  }
  const JsonReadContext *PJrcGetNext() const
  {
    return &*m_pjrxNext;
  }

  EJsonValueType JvtGetValueType() const
  {
    Assert(!!m_pjvCur);
    if (!m_pjvCur)
      return ejvtJsonValueTypeCount;
    return m_pjvCur->JvtGetValueType();
  }
  void SetValueType(EJsonValueType _jvt)
  {
    Assert(!!m_pjvCur);
    if (!!m_pjvCur)
      m_pjvCur->SetValueType(_jvt);
  }

  _tyStdStr *PGetStringValue() const
  {
    Assert(!!m_pjvCur);
    return !m_pjvCur ? 0 : m_pjvCur->PGetStringValue();
  }

  void SetEndOfIteration(_tyFilePos _pos)
  {
    m_tcFirst = 0;
    m_posStartValue = m_posEndValue = _pos;
    // The value is set to EndOfIteration separately.
  }
  bool FEndOfIteration() const
  {
    bool fEnd = (((JvtGetValueType() == ejvtObject) && !!m_pjvCur->PGetJsonObject() && m_pjvCur->PGetJsonObject()->FEndOfIteration()) || ((JvtGetValueType() == ejvtArray) && !!m_pjvCur->PGetJsonArray() && m_pjvCur->PGetJsonArray()->FEndOfIteration()));
    //Assert( !fEnd || !m_tcFirst ); - dbien: should figure out where I am setting end-of and also reset this to 0.
    return fEnd;
  }

  // Push _pjrxNewHead as the new head of stack before _pjrxHead.
  static void PushStack(std::unique_ptr<JsonReadContext> &_pjrxHead, std::unique_ptr<JsonReadContext> &_pjrxNewHead)
  {
    if (!!_pjrxHead)
    {
      Assert(!_pjrxHead->m_pjrxPrev);
      Assert(!_pjrxNewHead->m_pjrxPrev);
      Assert(!_pjrxNewHead->m_pjrxNext);
      _pjrxHead->m_pjrxPrev = &*_pjrxNewHead; // soft link.
      _pjrxNewHead->m_pjrxNext.swap(_pjrxHead);
    }
    _pjrxHead.swap(_pjrxNewHead); // new head is now the head and it points at old head.
  }
  static void PopStack(std::unique_ptr<JsonReadContext> &_pjrxHead)
  {
    std::unique_ptr<JsonReadContext> pjrxOldHead;
    _pjrxHead.swap(pjrxOldHead);
    pjrxOldHead->m_pjrxNext.swap(_pjrxHead);
    _pjrxHead->m_pjrxPrev = 0;
  }

protected:
  _tyJsonValue *m_pjvCur{};                    // We maintain a soft reference to the current JsonValue at this level.
  std::unique_ptr<JsonReadContext> m_pjrxNext; // Implement a simple doubly linked list.
  JsonReadContext *m_pjrxPrev{};               // soft reference to parent in list.
  _tyFilePos m_posStartValue{};                // The start of the value for this element - after parsing WS.
  _tyFilePos m_posEndValue{};                  // The end of the value for this element - before parsing WS beyond.
  _tyChar m_tcFirst{};                         // Only for the number type does this matter but since it does...
};

// JsonRestoreContext:
// Restores the context to formerly current context upon end object lifetime.
template <class t_tyJsonInputStream>
class JsonRestoreContext
{
  typedef JsonRestoreContext _tyThis;

public:
  typedef t_tyJsonInputStream _tyJsonInputStream;
  typedef typename t_tyJsonInputStream::_tyCharTraits _tyCharTraits;
  typedef JsonReadCursor<t_tyJsonInputStream> _tyJsonReadCursor;
  typedef JsonReadContext<t_tyJsonInputStream> _tyJsonReadContext;

  JsonRestoreContext() = default;
  JsonRestoreContext(_tyJsonReadCursor &_jrc)
      : m_pjrc(&_jrc),
        m_pjrx(&_jrc.GetCurrentContext())
  {
  }
  ~JsonRestoreContext() noexcept(false)
  {
    if (!!m_pjrx && !!m_pjrc)
    {
      bool fInUnwinding = !!std::uncaught_exceptions();
      try
      {
        m_pjrc->MoveToContext(*m_pjrx); // There really no reason this should throw unless someone does something stupid - which of course happens.
      }
      catch (std::exception &rexc)
      {
        if (!fInUnwinding)
          throw;
        LOGSYSLOG(eslmtError, "~JsonRestoreContext(): Caught exception [%s].", rexc.what());
      }
    }
  }

  // This one does potentially throw since we are not within a destructor.
  void Release()
  {
    if (!!m_pjrx && !!m_pjrc)
    {
      _tyJsonReadCursor *pjrc = m_pjrc;
      m_pjrc = 0;
      pjrc->MoveToContext(*m_pjrx);
    }
  }

protected:
  _tyJsonReadCursor *m_pjrc{};
  const _tyJsonReadContext *m_pjrx{};
};

// class JsonReadCursor:
template <class t_tyJsonInputStream>
class JsonReadCursor
{
  typedef JsonReadCursor _tyThis;

public:
  typedef t_tyJsonInputStream _tyJsonInputStream;
  typedef typename t_tyJsonInputStream::_tyFilePos _tyFilePos;
  typedef typename t_tyJsonInputStream::_tyCharTraits _tyCharTraits;
  typedef typename _tyCharTraits::_tyChar _tyChar;
  typedef JsonValue<_tyCharTraits> _tyJsonValue;
  typedef JsonObject<_tyCharTraits> _tyJsonObject;
  typedef JsonArray<_tyCharTraits> _tyJsonArray;
  typedef JsonReadContext<t_tyJsonInputStream> _tyJsonReadContext;
  typedef JsonRestoreContext<t_tyJsonInputStream> _tyJsonRestoreContext;
  using _tyStdStr = typename _tyCharTraits::_tyStdStr;
  using _tyLPCSTR = typename _tyCharTraits::_tyLPCSTR;

  JsonReadCursor() = default;
  JsonReadCursor(JsonReadCursor const &) = delete;
  JsonReadCursor &operator=(JsonReadCursor const &) = delete;

  void AssertValid() const
  {
#if ASSERTSENABLED
    Assert(!m_pjrxCurrent == !m_pis);
    Assert(!m_pjrxRootVal == !m_pis);
    Assert(!m_pjrxContextStack == !m_pis);
    // Make sure that the current context is in the context stack.
    if (!!m_pjrxContextStack)
    {
      const _tyJsonReadContext *pjrxCheck = &*m_pjrxContextStack;
      for (; !!pjrxCheck && (pjrxCheck != m_pjrxCurrent); pjrxCheck = pjrxCheck->PJrcGetNext())
        ;
      Assert(!!pjrxCheck); // If this fails then the current context is not in the content stack - this is bad.
    }
#endif //ASSERTSENABLED
  }
  bool FAttached() const
  {
    AssertValid();
    return !!m_pis;
  }

  const _tyJsonReadContext &GetCurrentContext() const
  {
    Assert(!!m_pjrxCurrent);
    return *m_pjrxCurrent;
  }

  // Returns true if the current value is an aggregate value - object or array.
  bool FAtAggregateValue() const
  {
    AssertValid();
    return !!m_pjrxCurrent &&
           ((ejvtArray == m_pjrxCurrent->JvtGetValueType()) || (ejvtObject == m_pjrxCurrent->JvtGetValueType()));
  }
  bool FAtObjectValue() const
  {
    AssertValid();
    return !!m_pjrxCurrent && (ejvtObject == m_pjrxCurrent->JvtGetValueType());
  }
  bool FAtArrayValue() const
  {
    AssertValid();
    return !!m_pjrxCurrent && (ejvtArray == m_pjrxCurrent->JvtGetValueType());
  }
  // Return if we are at the end of the iteration of an aggregate (object or array).
  bool FAtEndOfAggregate() const
  {
    AssertValid();
    return !!m_pjrxCurrent &&
           ((ejvtEndOfArray == m_pjrxCurrent->JvtGetValueType()) || (ejvtEndOfObject == m_pjrxCurrent->JvtGetValueType()));
  }

  const _tyStdStr &RStrKey(EJsonValueType *_pjvt = 0) const
  {
    Assert(FAttached());
    if (FAtEndOfAggregate() || !m_pjrxCurrent || !m_pjrxCurrent->m_pjrxNext || (ejvtObject != m_pjrxCurrent->m_pjrxNext->JvtGetValueType()))
      THROWBADJSONSEMANTICUSE("No key available.");
    _tyJsonObject *pjoCur = m_pjrxCurrent->m_pjrxNext->PGetJsonObject();
    Assert(!pjoCur->FEndOfIteration()); // sanity
    if (!!_pjvt)
      *_pjvt = m_pjrxCurrent->JvtGetValueType();
    return pjoCur->RStrKey();
  }
  // Get the current key if there is a current key.
  bool FGetKeyCurrent(_tyStdStr &_rstrKey, EJsonValueType &_rjvt) const
  {
    Assert(FAttached());
    if (FAtEndOfAggregate())
      return false;
    // This should only be called when we are inside of an object's value.
    // So, we need to have a parent value and that parent value should correspond to an object.
    // Otherwise we throw an "invalid semantic use" exception.
    if (!m_pjrxCurrent || !m_pjrxCurrent->m_pjrxNext || (ejvtObject != m_pjrxCurrent->m_pjrxNext->JvtGetValueType()))
      THROWBADJSONSEMANTICUSE("Not located at an object so no key available.");
    // If the key value in the object is the empty string then this is an empty object and we return false.
    _tyJsonObject *pjoCur = m_pjrxCurrent->m_pjrxNext->PGetJsonObject();
    Assert(!pjoCur->FEndOfIteration());
    pjoCur->GetKey(_rstrKey);
    _rjvt = m_pjrxCurrent->JvtGetValueType(); // This is the value type for this key string. We may not have read the actual value yet.
    return true;
  }
  EJsonValueType JvtGetValueType() const
  {
    Assert(FAttached());
    return m_pjrxCurrent->JvtGetValueType();
  }
  bool FIsValueNull() const
  {
    return ejvtNull == JvtGetValueType();
  }

  // Get a full copy of the JsonValue. We allow this to work with any type of JsonValue for completeness.
  void GetValue(_tyJsonValue &_rjvValue) const
  {
    Assert(FAttached());
    EJsonValueType jvt = JvtGetValueType();
    if ((ejvtEndOfObject == jvt) || (ejvtEndOfArray == jvt))
    {
      _rjvValue.SetEndOfIteration(ejvtEndOfObject == jvt);
      return;
    }

    // Now check if we haven't read the simple value yet because then we gotta read it.
    if ((ejvtObject != jvt) && (ejvtArray != jvt) && !m_pjrxCurrent->m_posEndValue)
      const_cast<_tyThis *>(this)->_ReadSimpleValue();

    _tyJsonValue jvLocal(*m_pjrxCurrent->PJvGet()); // copy into local then swap values with passed value - solves all sorts of potential issues with initial conditions.
    _rjvValue.swap(jvLocal);
  }

  // Get a string representation of the value.
  // This is a semantic error if this is an aggregate value (object or array).
  void GetValue(_tyStdStr &_strValue) const
  {
    Assert(FAttached());
    if (FAtAggregateValue())
      THROWBADJSONSEMANTICUSE("At an aggregate value - object or array.");

    EJsonValueType jvt = JvtGetValueType();
    if ((ejvtEndOfObject == jvt) || (ejvtEndOfArray == jvt))
      THROWBADJSONSEMANTICUSE("No value located at end of object or array.");

    // Now check if we haven't read the value yet because then we gotta read it.
    if (!m_pjrxCurrent->m_posEndValue)
      const_cast<_tyThis *>(this)->_ReadSimpleValue();

    switch (JvtGetValueType())
    {
    case ejvtNumber:
    case ejvtString:
      _strValue = *m_pjrxCurrent->PGetStringValue();
      break;
    case ejvtTrue:
      _strValue = "true";
      break;
    case ejvtFalse:
      _strValue = "false";
      break;
    case ejvtNull:
      _strValue = "null";
      break;
    default:
      Assert(0);
      _strValue = "***ERROR***";
    }
  }
  void GetValue(bool &_rf) const
  {
    // Now check if we haven't read the value yet because then we gotta read it.
    if (!m_pjrxCurrent->m_posEndValue)
      const_cast<_tyThis *>(this)->_ReadSimpleValue();

    switch (JvtGetValueType())
    {
    default:
      THROWBADJSONSEMANTICUSE("Not at a boolean value type.");
      break;
    case ejvtTrue:
      _rf = true;
      break;
    case ejvtFalse:
      _rf = false;
      break;
    }
  }
  template <class t_tyNum>
  void _GetValue(_tyLPCSTR _pszFmt, t_tyNum &_rNumber) const
  {
    if (ejvtNumber != JvtGetValueType())
      THROWBADJSONSEMANTICUSE("Not at a numeric value type.");

    // Now check if we haven't read the value yet because then we gotta read it.
    if (!m_pjrxCurrent->m_posEndValue)
      const_cast<_tyThis *>(this)->_ReadSimpleValue();

    // The presumption is that sscanf won't read past any decimal point if scanning a non-floating point number.
    int iRet = sscanf(m_pjrxCurrent->PGetStringValue()->c_str(), _pszFmt, &_rNumber);
    Assert(1 == iRet); // Due to the specification of number we expect this to always succeed.
  }
  void GetValue(uint8_t &_rby) const { _GetValue("%hhu", _rby); }
  void GetValue(int8_t &_rsby) const { _GetValue("%hhd", _rsby); }
  void GetValue(uint16_t &_rus) const { _GetValue("%hu", _rus); }
  void GetValue(int16_t &_rss) const { _GetValue("%hd", _rss); }
  void GetValue(uint32_t &_rui) const { _GetValue("%u", _rui); }
  void GetValue(int32_t &_rsi) const { _GetValue("%d", _rsi); }
  void GetValue(uint64_t &_rul) const { _GetValue("%lu", _rul); }
  void GetValue(int64_t &_rsl) const { _GetValue("%ld", _rsl); }
  void GetValue(float &_rfl) const { _GetValue("%e", _rfl); }
  void GetValue(double &_rdbl) const { _GetValue("%le", _rdbl); }
  void GetValue(long double &_rldbl) const { _GetValue("%Le", _rldbl); }

  // Speciality values:
  // Human readable date/time - implemented on ejvtString.
  void GetTimeStringValue(time_t &_tt) const
  {
    if (ejvtString != JvtGetValueType())
      THROWBADJSONSEMANTICUSE("Not at a string value type.");
    int iRet = n_TimeUtil::ITimeFromString(m_pjrxCurrent->PGetStringValue()->c_str(), _tt);
    if (!!iRet)
      THROWBADJSONSEMANTICUSE("Failed to parse a date/time, iRet[%d].", iRet);
  }
  // Human readable date/time - implemented on ejvtString.
  void GetUuidStringValue(uuid_t &_uuidt) const
  {
    if (ejvtString != JvtGetValueType())
      THROWBADJSONSEMANTICUSE("Not at a string value type.");
    if (m_pjrxCurrent->PGetStringValue()->length() < vkstUUIDNChars)
      THROWBADJSONSEMANTICUSE("Not enough characters in the string for a UUID string - 36 chars are required.");
    int iRet = UUIDFromString(m_pjrxCurrent->PGetStringValue()->c_str(), _uuidt);
    if (!!iRet)
      THROWBADJSONSEMANTICUSE("Failed to parse a uuid from string [%s].", m_pjrxCurrent->PGetStringValue()->c_str());
  }

  // This will read any unread value into the value object
  void _ReadSimpleValue()
  {
    Assert(!m_pjrxCurrent->m_posEndValue);
    EJsonValueType jvt = JvtGetValueType();
    Assert((jvt >= ejvtFirstJsonSimpleValue) && (jvt <= ejvtLastJsonSpecifiedValue)); // Should be handled by caller.
    switch (jvt)
    {
    case ejvtNumber:
      _ReadNumber(m_pjrxCurrent->m_tcFirst, !m_pjrxCurrent->m_pjrxNext, *m_pjrxCurrent->PGetStringValue());
      break;
    case ejvtString:
      _ReadString(*m_pjrxCurrent->PGetStringValue());
      break;
    case ejvtTrue:
    case ejvtFalse:
    case ejvtNull:
      _SkipSimpleValue(jvt, m_pjrxCurrent->m_tcFirst, !m_pjrxCurrent->m_pjrxNext); // We just skip remaining value checking that it is correct.
      break;
    default:
      Assert(0);
      break;
    }
    m_pjrxCurrent->m_posEndValue = m_pis->ByPosGet();
  }

  // Read a JSON number according to the specifications of such.
  // The interesting thing about a number is that there is no character indicating the ending
  //  - whereas for all other value types there is a specific character ending the value.
  // A JSON number ends when there is a character that is not a JSON number - or the EOF in the case of a JSON stream containing a single number value.
  // For all other JSON values you wouldn't have to worry about perhaps encountering EOF in the midst of reading the value.
  void _ReadNumber(_tyChar _tcFirst, bool _fAtRootElement, _tyStdStr &_rstrRead) const
  {
    // We will only read a number up to _tyCharTraits::s_knMaxNumberLength in string length.
    const unsigned int knMaxLength = _tyCharTraits::s_knMaxNumberLength;
    _tyChar rgtcBuffer[knMaxLength + 1];
    _tyChar *ptcCur = rgtcBuffer;
    *ptcCur++ = _tcFirst;

    auto lambdaAddCharNum = [&](_tyChar _tch) {
      if (ptcCur == rgtcBuffer + knMaxLength)
        THROWBADJSONSTREAM("Overflow of max JSON number length of [%u].", knMaxLength);
      *ptcCur++ = _tch;
    };

    // We will have read the first character of a number and then will need to satisfy the state machine for the number appropriately.
    bool fZeroFirst = (_tyCharTraits::s_tc0 == _tcFirst);
    _tyChar tchCurrentChar; // maintained as the current character.
    if (_tyCharTraits::s_tcMinus == _tcFirst)
    {
      tchCurrentChar = m_pis->ReadChar("Hit EOF looking for a digit after a minus."); // This may not be EOF.
      *ptcCur++ = tchCurrentChar;
      if ((tchCurrentChar < _tyCharTraits::s_tc0) || (tchCurrentChar > _tyCharTraits::s_tc9))
        THROWBADJSONSTREAM("Found [%TC] when looking for digit after a minus.", tchCurrentChar);
      fZeroFirst = (_tyCharTraits::s_tc0 == tchCurrentChar);
    }
    // If we are the root element then we may encounter EOF and have that not be an error when reading some of the values.
    if (fZeroFirst)
    {
      bool fFoundChar = m_pis->FReadChar(tchCurrentChar, !_fAtRootElement, "Hit EOF looking for something after a leading zero."); // Throw on EOF if we aren't at the root element.
      if (!fFoundChar)
        goto Label_DreadedLabel; // Then we read until the end of file for a JSON file containing a single number as its only element.
    }
    else // ( !fZeroFirst )
    {
      // Then we expect more digits or a period or an 'e' or an 'E' or EOF if we are the root element.
      for (;;)
      {
        bool fFoundChar = m_pis->FReadChar(tchCurrentChar, !_fAtRootElement, "Hit EOF looking for a non-number."); // Throw on EOF if we aren't at the root element.
        if (!fFoundChar)
          goto Label_DreadedLabel; // Then we read until the end of file for a JSON file containing a single number as its only element.
        if ((tchCurrentChar >= _tyCharTraits::s_tc0) && (tchCurrentChar <= _tyCharTraits::s_tc9))
          lambdaAddCharNum(tchCurrentChar);
        else
          break;
      }
    }

    if (tchCurrentChar == _tyCharTraits::s_tcPeriod)
    {
      lambdaAddCharNum(tchCurrentChar); // Don't miss your period.
      // Then according to the JSON spec we must have at least one digit here.
      tchCurrentChar = m_pis->ReadChar("Hit EOF looking for a digit after a decimal point."); // throw on EOF.
      if ((tchCurrentChar < _tyCharTraits::s_tc0) || (tchCurrentChar > _tyCharTraits::s_tc9))
        THROWBADJSONSTREAM("Found [%TC] when looking for digit after a decimal point.", tchCurrentChar);
      // Now we expect a digit, 'e', 'E', EOF or something else that our parent can check out.
      lambdaAddCharNum(tchCurrentChar);
      for (;;)
      {
        bool fFoundChar = m_pis->FReadChar(tchCurrentChar, !_fAtRootElement, "Hit EOF looking for a non-number after the period."); // Throw on EOF if we aren't at the root element.
        if (!fFoundChar)
          goto Label_DreadedLabel; // Then we read until the end of file for a JSON file containing a single number as its only element.
        if ((tchCurrentChar >= _tyCharTraits::s_tc0) && (tchCurrentChar <= _tyCharTraits::s_tc9))
          lambdaAddCharNum(tchCurrentChar);
        else
          break;
      }
    }
    if ((tchCurrentChar == _tyCharTraits::s_tcE) ||
        (tchCurrentChar == _tyCharTraits::s_tce))
    {
      lambdaAddCharNum(tchCurrentChar);
      // Then we might see a plus or a minus or a number - but we cannot see EOF here correctly:
      tchCurrentChar = m_pis->ReadChar("Hit EOF looking for a digit after an exponent indicator."); // Throws on EOF.
      lambdaAddCharNum(tchCurrentChar);
      if ((tchCurrentChar == _tyCharTraits::s_tcMinus) ||
          (tchCurrentChar == _tyCharTraits::s_tcPlus))
      {
        tchCurrentChar = m_pis->ReadChar("Hit EOF looking for a digit after an exponent indicator with plus/minus."); // Then we must still find a number after - throws on EOF.
        lambdaAddCharNum(tchCurrentChar);
      }
      // Now we must see at least one digit so for the first read we throw.
      if ((tchCurrentChar < _tyCharTraits::s_tc0) || (tchCurrentChar > _tyCharTraits::s_tc9))
        THROWBADJSONSTREAM("Found [%TC] when looking for digit after a exponent indicator (e/E).", tchCurrentChar);
      // We have satisfied "at least a digit after an exponent indicator" - now we might just hit EOF or anything else after this...
      for (;;)
      {
        bool fFoundChar = m_pis->FReadChar(tchCurrentChar, !_fAtRootElement, "Hit EOF looking for a non-number after the exponent indicator."); // Throw on EOF if we aren't at the root element.
        if (!fFoundChar)
          goto Label_DreadedLabel; // Then we read until the end of file for a JSON file containing a single number as its only element.
        if ((tchCurrentChar >= _tyCharTraits::s_tc0) && (tchCurrentChar <= _tyCharTraits::s_tc9))
          lambdaAddCharNum(tchCurrentChar);
        else
          break;
      }
    }
    m_pis->PushBackLastChar(true); // Let caller read this and decide what to do depending on context - we don't expect a specific character.
  Label_DreadedLabel:              // Just way too easy to do it this way.
    *ptcCur++ = _tyChar(0);
    _rstrRead = rgtcBuffer; // Copy the number into the return buffer.
  }

  // Skip a JSON number according to the specifications of such.
  void _SkipNumber(_tyChar _tcFirst, bool _fAtRootElement) const
  {
    // We will have read the first character of a number and then will need to satisfy the state machine for the number appropriately.
    bool fZeroFirst = (_tyCharTraits::s_tc0 == _tcFirst);
    _tyChar tchCurrentChar; // maintained as the current character.
    if (_tyCharTraits::s_tcMinus == _tcFirst)
    {
      tchCurrentChar = m_pis->ReadChar("Hit EOF looking for a digit after a minus."); // This may not be EOF.
      if ((tchCurrentChar < _tyCharTraits::s_tc0) || (tchCurrentChar > _tyCharTraits::s_tc9))
        THROWBADJSONSTREAM("Found [%TC] when looking for digit after a minus.", tchCurrentChar);
      fZeroFirst = (_tyCharTraits::s_tc0 == tchCurrentChar);
    }
    // If we are the root element then we may encounter EOF and have that not be an error when reading some of the values.
    if (fZeroFirst)
    {
      bool fFoundChar = m_pis->FReadChar(tchCurrentChar, !_fAtRootElement, "Hit EOF looking for something after a leading zero."); // Throw on EOF if we aren't at the root element.
      if (!fFoundChar)
        return; // Then we read until the end of file for a JSON file containing a single number as its only element.
    }
    else // ( !fZeroFirst )
    {
      // Then we expect more digits or a period or an 'e' or an 'E' or EOF if we are the root element.
      do
      {
        bool fFoundChar = m_pis->FReadChar(tchCurrentChar, !_fAtRootElement, "Hit EOF looking for a non-number."); // Throw on EOF if we aren't at the root element.
        if (!fFoundChar)
          return; // Then we read until the end of file for a JSON file containing a single number as its only element.
      } while ((tchCurrentChar >= _tyCharTraits::s_tc0) && (tchCurrentChar <= _tyCharTraits::s_tc9));
    }

    if (tchCurrentChar == _tyCharTraits::s_tcPeriod)
    {
      // Then according to the JSON spec we must have at least one digit here.
      tchCurrentChar = m_pis->ReadChar("Hit EOF looking for a digit after a decimal point."); // throw on EOF.
      if ((tchCurrentChar < _tyCharTraits::s_tc0) || (tchCurrentChar > _tyCharTraits::s_tc9))
        THROWBADJSONSTREAM("Found [%TC] when looking for digit after a decimal point.", tchCurrentChar);
      // Now we expect a digit, 'e', 'E', EOF or something else that our parent can check out.
      do
      {
        bool fFoundChar = m_pis->FReadChar(tchCurrentChar, !_fAtRootElement, "Hit EOF looking for a non-number after the period."); // Throw on EOF if we aren't at the root element.
        if (!fFoundChar)
          return; // Then we read until the end of file for a JSON file containing a single number as its only element.
      } while ((tchCurrentChar >= _tyCharTraits::s_tc0) && (tchCurrentChar <= _tyCharTraits::s_tc9));
    }
    if ((tchCurrentChar == _tyCharTraits::s_tcE) ||
        (tchCurrentChar == _tyCharTraits::s_tce))
    {
      // Then we might see a plus or a minus or a number - but we cannot see EOF here correctly:
      tchCurrentChar = m_pis->ReadChar("Hit EOF looking for a digit after an exponent indicator."); // Throws on EOF.
      if ((tchCurrentChar == _tyCharTraits::s_tcMinus) ||
          (tchCurrentChar == _tyCharTraits::s_tcPlus))
        tchCurrentChar = m_pis->ReadChar("Hit EOF looking for a digit after an exponent indicator with plus/minus."); // Then we must still find a number after - throws on EOF.
      // Now we must see at least one digit so for the first read we throw.
      if ((tchCurrentChar < _tyCharTraits::s_tc0) || (tchCurrentChar > _tyCharTraits::s_tc9))
        THROWBADJSONSTREAM("Found [%TC] when looking for digit after an exponent indicator (e/E).", tchCurrentChar);
      // We have satisfied "at least a digit after an exponent indicator" - now we might just hit EOF or anything else after this...
      do
      {
        bool fFoundChar = m_pis->FReadChar(tchCurrentChar, !_fAtRootElement, "Hit EOF looking for a non-number after the exponent indicator."); // Throw on EOF if we aren't at the root element.
        if (!fFoundChar)
          return; // Then we read until the end of file for a JSON file containing a single number as its only element.
      } while ((tchCurrentChar >= _tyCharTraits::s_tc0) && (tchCurrentChar <= _tyCharTraits::s_tc9));
    }
    m_pis->PushBackLastChar(true); // Let caller read this and decide what to do depending on context - we don't expect a specific character here.
  }

  // Read the string starting at the current position. The '"' has already been read.
  void _ReadString(_tyStdStr &_rstrRead) const
  {
    const int knLenBuffer = 1023;
    _tyChar rgtcBuffer[knLenBuffer + 1]; // We will append a zero when writing to the string.
    rgtcBuffer[knLenBuffer] = 0;         // preterminate end.
    _tyChar *ptchCur = rgtcBuffer;
    _rstrRead.clear();

    // We know we will see a '"' at the end. Along the way we may see multiple excape '\' characters.
    // So we move along checking each character, throwing if we hit EOF before the end of the string is found.

    for (;;)
    {
      _tyChar tchCur = m_pis->ReadChar("EOF found looking for end of string."); // throws on EOF.
      if (_tyCharTraits::s_tcDoubleQuote == tchCur)
        break; // We have reached EOS.
      if (_tyCharTraits::s_tcBackSlash == tchCur)
      {
        tchCur = m_pis->ReadChar("EOF finding completion of backslash escape for string."); // throws on EOF.
        switch (tchCur)
        {
        case _tyCharTraits::s_tcDoubleQuote:
        case _tyCharTraits::s_tcBackSlash:
        case _tyCharTraits::s_tcForwardSlash:
          break; // Just use tchCur.
        case _tyCharTraits::s_tcb:
          tchCur = _tyCharTraits::s_tcBackSpace;
          break;
        case _tyCharTraits::s_tcf:
          tchCur = _tyCharTraits::s_tcFormFeed;
          break;
        case _tyCharTraits::s_tcn:
          tchCur = _tyCharTraits::s_tcNewline;
          break;
        case _tyCharTraits::s_tcr:
          tchCur = _tyCharTraits::s_tcCarriageReturn;
          break;
        case _tyCharTraits::s_tct:
          tchCur = _tyCharTraits::s_tcTab;
          break;
        case _tyCharTraits::s_tcu:
        {
          unsigned int uHex = 0; // Accumulate the hex amount.
          unsigned int uCurrentMultiplier = (1u << 12);
          // Must find 4 hex digits:
          for (int n = 0; n < 4; ++n, (uCurrentMultiplier >>= 4))
          {
            tchCur = m_pis->ReadChar("EOF found looking for 4 hex digits following \\u."); // throws on EOF.
            if ((tchCur >= _tyCharTraits::s_tc0) && (tchCur <= _tyCharTraits::s_tc9))
              uHex += uCurrentMultiplier * (tchCur - _tyCharTraits::s_tc0);
            else if ((tchCur >= _tyCharTraits::s_tca) && (tchCur <= _tyCharTraits::s_tcf))
              uHex += uCurrentMultiplier * (10 + (tchCur - _tyCharTraits::s_tca));
            else if ((tchCur >= _tyCharTraits::s_tcA) && (tchCur <= _tyCharTraits::s_tcF))
              uHex += uCurrentMultiplier * (10 + (tchCur - _tyCharTraits::s_tcA));
            else
              THROWBADJSONSTREAM("Found [%TC] when looking for digit following \\u.", tchCur);
          }
          // If we are supposed to throw on overflow then check for it, otherwise just truncate to the character type silently.
          if (_tyCharTraits::s_fThrowOnUnicodeOverflow && (sizeof(_tyChar) < sizeof(uHex)))
          {
            if (uHex >= (1u << (CHAR_BIT * sizeof(_tyChar))))
              THROWBADJSONSTREAM("Unicode hex overflow [%u].", uHex);
          }
          tchCur = (_tyChar)uHex;
          break;
        }
        default:
          THROWBADJSONSTREAM("Found [%TC] when looking for competetion of backslash when reading string.", tchCur);
          break;
        }
      }
      if (ptchCur == rgtcBuffer + knLenBuffer)
      {
        _rstrRead += rgtcBuffer;
        ptchCur = rgtcBuffer;
      }
      *ptchCur++ = tchCur;
    }
    // Add in the rest of the string if any.
    *ptchCur = 0;
    _rstrRead += rgtcBuffer;
  }
  // Skip the string starting at the current position. The '"' has already been read.
  void _SkipRemainingString() const
  {
    // We know we will see a '"' at the end. Along the way we may see multiple excape '\' characters.
    // So we move along checking each character, throwing if we hit EOF before the end of the string is found.

    for (;;)
    {
      _tyChar tchCur = m_pis->ReadChar("EOF found looking for end of string."); // throws on EOF.
      if (_tyCharTraits::s_tcDoubleQuote == tchCur)
        break; // We have reached EOS.
      if (_tyCharTraits::s_tcBackSlash == tchCur)
      {
        tchCur = m_pis->ReadChar("EOF finding completion of backslash escape for string."); // throws on EOF.
        switch (tchCur)
        {
        case _tyCharTraits::s_tcDoubleQuote:
        case _tyCharTraits::s_tcBackSlash:
        case _tyCharTraits::s_tcForwardSlash:
        case _tyCharTraits::s_tcb:
        case _tyCharTraits::s_tcf:
        case _tyCharTraits::s_tcn:
        case _tyCharTraits::s_tcr:
        case _tyCharTraits::s_tct:
          break;
        case _tyCharTraits::s_tcu:
        {
          // Must find 4 hex digits:
          for (int n = 0; n < 4; ++n)
          {
            tchCur = m_pis->ReadChar("EOF found looking for 4 hex digits following \\u."); // throws on EOF.
            if (!(((tchCur >= _tyCharTraits::s_tc0) && (tchCur <= _tyCharTraits::s_tc9)) ||
                  ((tchCur >= _tyCharTraits::s_tca) && (tchCur <= _tyCharTraits::s_tcf)) ||
                  ((tchCur >= _tyCharTraits::s_tcA) && (tchCur <= _tyCharTraits::s_tcF))))
              THROWBADJSONSTREAM("Found [%TC] when looking for digit following \\u.", tchCur);
          }
        }
        default:
          THROWBADJSONSTREAM("Found [%TC] when looking for competetion of backslash when reading string.", tchCur);
          break;
        }
      }
      // Otherwise we just continue reading.
    }
  }

  // Just skip the zero terminated string starting at the current point in the JSON stream.
  void _SkipFixed(const char *_pcSkip) const
  {
    // We just cast the chars to _tyChar since that is valid for JSON files.
    const char *pcCur = _pcSkip;
    _tyChar tchCur;
    for (; !!*pcCur && (_tyChar(*pcCur) == (tchCur = m_pis->ReadChar("Hit EOF trying to skip fixed string."))); ++pcCur)
      ;
    if (*pcCur)
      THROWBADJSONSTREAM("Trying to skip[%s], instead of [%c] found [%TC].", _pcSkip, *pcCur, tchCur);
  }

  void _SkipSimpleValue(EJsonValueType _jvtCur, _tyChar _tcFirst, bool _fAtRootElement) const
  {
    switch (_jvtCur)
    {
    case ejvtNumber:
      _SkipNumber(_tcFirst, _fAtRootElement);
      break;
    case ejvtString:
      _SkipRemainingString();
      break;
    case ejvtTrue:
      _SkipFixed("rue");
      break;
    case ejvtFalse:
      _SkipFixed("alse");
      break;
    case ejvtNull:
      _SkipFixed("ull");
      break;
    default:
      Assert(false);
      break;
    }
  }

  // Skip the value located at the current point in the stream.
  // Expects that the entire value is present on the JSON stream - i.e. we will not have read the first character...
  void _SkipValue()
  {
    m_pis->SkipWhitespace();
    // We don't expect EOF here.
    _tyChar tchCur = m_pis->ReadChar("EOF on first value char."); // throws on EOF.
    EJsonValueType jvtCur = _tyJsonValue::GetJvtTypeFromChar(tchCur);
    if (ejvtJsonValueTypeCount == jvtCur)
      THROWBADJSONSTREAM("Found [%TC] when looking for value starting character.", tchCur);
    if (ejvtObject == jvtCur)
      _SkipWholeObject();
    else if (ejvtArray == jvtCur)
      _SkipWholeArray();
    else
      _SkipSimpleValue(jvtCur, tchCur, false); // we assume we aren't at the root element here since we shouldn't get here - we would have a context.
  }

  // We will have read the first '{' of the object.
  void _SkipWholeObject()
  {
    m_pis->SkipWhitespace();
    _tyChar tchCur = m_pis->ReadChar("EOF after begin bracket."); // throws on EOF.
    while (_tyCharTraits::s_tcDoubleQuote == tchCur)
    {
      _SkipRemainingString(); // Skip the key.
      m_pis->SkipWhitespace();
      tchCur = m_pis->ReadChar("EOF looking for colon."); // throws on EOF.
      if (_tyCharTraits::s_tcColon != tchCur)
        THROWBADJSONSTREAM("Found [%TC] when looking for colon.", tchCur);
      _SkipValue(); // Skip the value.
      m_pis->SkipWhitespace();
      tchCur = m_pis->ReadChar("EOF looking comma or end of object."); // throws on EOF.
      if (_tyCharTraits::s_tcComma == tchCur)
      {
        m_pis->SkipWhitespace();
        tchCur = m_pis->ReadChar("EOF looking double quote."); // throws on EOF.
        if (_tyCharTraits::s_tcDoubleQuote != tchCur)
          THROWBADJSONSTREAM("Found [%TC] when looking for double quote.", tchCur);
      }
      else
        break;
    }
    if (_tyCharTraits::s_tcRightCurlyBr != tchCur)
      THROWBADJSONSTREAM("Found [%TC] when looking for double quote or comma or right bracket.", tchCur);
  }
  // We will have read the first '[' of the array.
  void _SkipWholeArray()
  {
    m_pis->SkipWhitespace();
    _tyChar tchCur = m_pis->ReadChar("EOF after begin bracket."); // throws on EOF.
    if (_tyCharTraits::s_tcRightSquareBr != tchCur)
      m_pis->PushBackLastChar(); // Don't send an expected character as it is from the set of value-starting characters.
    while (_tyCharTraits::s_tcRightSquareBr != tchCur)
    {
      _SkipValue();
      m_pis->SkipWhitespace();
      tchCur = m_pis->ReadChar("EOF looking for comma."); // throws on EOF.
      if (_tyCharTraits::s_tcComma == tchCur)
        continue;
      if (_tyCharTraits::s_tcRightSquareBr != tchCur)
        THROWBADJSONSTREAM("Found [%TC] when looking for comma or array end.", tchCur);
    }
  }

  // Skip an object with an associated context. This will potentially recurse into SkipWholeObject(), SkipWholeArray(), etc. which will not have an associated context.
  void _SkipObject(_tyJsonReadContext &_rjrx)
  {
    // We have a couple of possible starting states here:
    // 1) We haven't read the data for this object ( !_rjrx.m_posEndValue ) in this case we expect to see "key":value, ..., "key":value }
    //      Note that there may also be no (key,value) pairs at all - we may have an empty object.
    // 2) We have read the data. In this case we will have also read the value and we expect to see a ',' or a '}'.
    // 3) We have an empty object and we have read the data - in which case we have already read the final '}' of the object
    //      and we have nothing at all to do.

    if (ejvtEndOfObject == _rjrx.JvtGetValueType())
      return; // Then we have already read to the end of this object.

    if (!_rjrx.m_posEndValue)
    {
      _SkipWholeObject(); // this expects that we have read the first '{'.
      // Now indicate that we are at the end of the object.
      _rjrx.m_posEndValue = m_pis->ByPosGet(); // Indicate that we have read the value.
      return;
    }

    // Then we have partially iterated the object. All contexts above us are closed which means we should find
    //  either a comma or end curly bracket.
    m_pis->SkipWhitespace();
    _tyChar tchCur = m_pis->ReadChar("EOF looking for end object } or comma."); // throws on EOF.
    while (_tyCharTraits::s_tcRightCurlyBr != tchCur)
    {
      if (_tyCharTraits::s_tcComma != tchCur)
        THROWBADJSONSTREAM("Found [%TC] when looking for comma or object end.", tchCur);
      m_pis->SkipWhitespace();
      tchCur = m_pis->ReadChar("EOF looking for double quote."); // throws on EOF.
      if (_tyCharTraits::s_tcDoubleQuote != tchCur)
        THROWBADJSONSTREAM("Found [%TC] when looking key start double quote.", tchCur);
      _SkipRemainingString();
      m_pis->SkipWhitespace();
      tchCur = m_pis->ReadChar("EOF looking for colon."); // throws on EOF.
      if (_tyCharTraits::s_tcColon != tchCur)
        THROWBADJSONSTREAM("Found [%TC] when looking for colon.", tchCur);
      _SkipValue();
      m_pis->SkipWhitespace();
      tchCur = m_pis->ReadChar("EOF looking for end object } or comma."); // throws on EOF.
    }
    _tyJsonObject *pjoCur = _rjrx.PGetJsonObject();
    _rjrx.SetEndOfIteration(m_pis->ByPosGet()); // Indicate that we have iterated to the end of this object.
    pjoCur->SetEndOfIteration();
  }
  // Skip an array with an associated context. This will potentially recurse into SkipWholeObject(), SkipWholeArray(), etc. which will not have an associated context.
  void _SkipArray(_tyJsonReadContext &_rjrx)
  {
    // We have a couple of possible starting states here:
    // 1) We haven't read the data for this array ( !_rjrx.m_posEndValue ) in this case we expect to see value, ..., value }
    //      Note that there may also be no values at all - we may have an empty array.
    // 2) We have read the data. In this case we will have also read the value and we expect to see a ',' or a ']'.
    // 3) We have an empty object and we have read the data - in which case we have already read the final ']' of the object
    //      and we have nothing at all to do.

    if (ejvtEndOfArray == _rjrx.JvtGetValueType())
      return; // Then we have already read to the end of this object.

    if (!_rjrx.m_posEndValue)
    {
      _SkipWholeArray(); // this expects that we have read the first '['.
      // Now indicate that we are at the end of the array.
      _rjrx.m_posEndValue = m_pis->ByPosGet(); // Indicate that we have read the value.
      //_rjrx.SetValueType( ejvtEndOfArray );
      return;
    }

    // Then we have partially iterated the object. All contexts above us are closed which means we should find
    //  either a comma or end square bracket.
    m_pis->SkipWhitespace();
    _tyChar tchCur = m_pis->ReadChar("EOF looking for end array ] or comma."); // throws on EOF.
    while (_tyCharTraits::s_tcRightSquareBr != tchCur)
    {
      if (_tyCharTraits::s_tcComma != tchCur)
        THROWBADJSONSTREAM("Found [%TC] when looking for comma or array end.", tchCur);
      m_pis->SkipWhitespace();
      _SkipValue();
      m_pis->SkipWhitespace();
      tchCur = m_pis->ReadChar("EOF looking for end array ] or comma."); // throws on EOF.
    }
    _tyJsonArray *pjaCur = _rjrx.PGetJsonArray();
    _rjrx.SetEndOfIteration(m_pis->ByPosGet()); // Indicate that we have iterated to the end of this object.
    pjaCur->SetEndOfIteration();
  }

  // Skip this context - must be at the top of the context stack.
  void _SkipContext(_tyJsonReadContext &_rjrx)
  {
    // Skip this context which is at the top of the stack.
    // A couple of cases here:
    // 1) We are a non-hierarchical type: i.e. not array or object. In this case we merely have to read the value.
    // 2) We are an object or array. In this case we have to read until the end of the object or array.
    if (ejvtObject == _rjrx.JvtGetValueType())
      _SkipObject(_rjrx);
    else if (ejvtArray == _rjrx.JvtGetValueType())
      _SkipArray(_rjrx);
    else
    {
      // If we have already the value then we are done with this type.
      if (!_rjrx.m_posEndValue)
      {
        _SkipSimpleValue(_rjrx.JvtGetValueType(), _rjrx.m_tcFirst, !_rjrx.m_pjrxNext); // skip to the end of this value without saving the info.
        _rjrx.m_posEndValue = m_pis->ByPosGet();                                         // Update this for completeness in all scenarios.
      }
      return; // We have already fully processed this context.
    }
  }

  // Skip and close all contexts above the current context in the context stack.
  void SkipAllContextsAboveCurrent()
  {
    AssertValid();
    while (&*m_pjrxContextStack != m_pjrxCurrent)
    {
      _SkipContext(*m_pjrxContextStack);
      _tyJsonReadContext::PopStack(m_pjrxContextStack);
    }
  }
  void SkipTopContext()
  {
    _SkipContext(*m_pjrxCurrent);
  }
  // Move to the given context which must be in the context stack.
  // If it isn't in the context stack then a json_stream_bad_semantic_usage_exception() is thrown.
  void MoveToContext(const _tyJsonReadContext &_rjrx)
  {
    _tyJsonReadContext *pjrxFound = &*m_pjrxContextStack;
    for (; !!pjrxFound && (pjrxFound != &_rjrx); pjrxFound = pjrxFound->PJrcGetNext())
      ;
    if (!pjrxFound)
      THROWBADJSONSEMANTICUSE("Context not found in context stack.");
    m_pjrxCurrent = pjrxFound;
  }

  // Move the the next element of the object or array. Return false if this brings us to the end of the entity.
  bool FNextElement()
  {
    Assert(FAttached());
    // This should only be called when we are inside of an object or array.
    // So, we need to have a parent value and that parent value should correspond to an object or array.
    // Otherwise we throw an "invalid semantic use" exception.
    if (!m_pjrxCurrent || !m_pjrxCurrent->m_pjrxNext ||
        ((ejvtObject != m_pjrxCurrent->m_pjrxNext->JvtGetValueType()) &&
         (ejvtArray != m_pjrxCurrent->m_pjrxNext->JvtGetValueType())))
      THROWBADJSONSEMANTICUSE("Not located at an object or array.");

    if ((m_pjrxCurrent->JvtGetValueType() == ejvtEndOfObject) ||
        (m_pjrxCurrent->JvtGetValueType() == ejvtEndOfArray))
    {
      Assert((m_pjrxCurrent->JvtGetValueType() == ejvtEndOfObject) == (ejvtObject != m_pjrxCurrent->m_pjrxNext->JvtGetValueType())); // Would be weird if it didn't match.
      return false;                                                                                                                  // We are already at the end of the iteration.
    }
    // Perform common tasks:
    // We may be at the leaf of the current pathway when calling this or we may be in the middle of some pathway.
    // We must close any contexts above this object first.
    SkipAllContextsAboveCurrent(); // Skip and close all the contexts above the current context.
    // Close the top context if it is not read already:
    if (!m_pjrxCurrent->m_posEndValue ||
        (((ejvtObject == m_pjrxCurrent->JvtGetValueType()) ||
          (ejvtArray == m_pjrxCurrent->JvtGetValueType())) &&
         !m_pjrxCurrent->FEndOfIteration()))
      SkipTopContext(); // Then skip the value at the current context.
    // Destroy the current object regardless - we are going to the next one.
    m_pjrxCurrent->PJvGet()->Destroy();

    // Now we are going to look for a comma or an right curly/square bracket:
    m_pis->SkipWhitespace();
    _tyChar tchCur = m_pis->ReadChar("EOF looking for end object/array }/] or comma."); // throws on EOF.

    if (ejvtObject == m_pjrxCurrent->m_pjrxNext->JvtGetValueType())
    {
      _tyJsonObject *pjoCur = m_pjrxCurrent->m_pjrxNext->PGetJsonObject();
      Assert(!pjoCur->FEndOfIteration());
      if (_tyCharTraits::s_tcRightCurlyBr == tchCur)
      {
        m_pjrxCurrent->SetEndOfIteration(m_pis->ByPosGet());
        pjoCur->SetEndOfIteration();
        return false; // We reached the end of the iteration.
      }
      if (_tyCharTraits::s_tcComma != tchCur)
        THROWBADJSONSTREAM("Found [%TC] when looking for comma or object end.", tchCur);
      // Now we will read the key string of the next element into <pjoCur>.
      m_pis->SkipWhitespace();
      tchCur = m_pis->ReadChar("EOF looking for double quote."); // throws on EOF.
      if (_tyCharTraits::s_tcDoubleQuote != tchCur)
        THROWBADJSONSTREAM("Found [%TC] when looking key start double quote.", tchCur);
      _tyStdStr strNextKey;
      _ReadString(strNextKey); // Might throw for any number of reasons. This may be the empty string.
      pjoCur->SwapKey(strNextKey);
      m_pis->SkipWhitespace();
      tchCur = m_pis->ReadChar("EOF looking for colon."); // throws on EOF.
      if (_tyCharTraits::s_tcColon != tchCur)
        THROWBADJSONSTREAM("Found [%TC] when looking for colon.", tchCur);
    }
    else
    {
      _tyJsonArray *pjaCur = m_pjrxCurrent->m_pjrxNext->PGetJsonArray();
      Assert(!pjaCur->FEndOfIteration());
      if (_tyCharTraits::s_tcRightSquareBr == tchCur)
      {
        m_pjrxCurrent->SetEndOfIteration(m_pis->ByPosGet());
        pjaCur->SetEndOfIteration();
        return false; // We reached the end of the iteration.
      }
      if (_tyCharTraits::s_tcComma != tchCur)
        THROWBADJSONSTREAM("Found [%TC] when looking for comma or array end.", tchCur);
    }
    m_pis->SkipWhitespace();
    m_pjrxCurrent->m_posStartValue = m_pis->ByPosGet();
    m_pjrxCurrent->m_posEndValue = 0; // Reset this to zero because we haven't yet read the value for this next element yet.
    // The first non-whitespace character tells us what the value type is:
    m_pjrxCurrent->m_tcFirst = m_pis->ReadChar("EOF looking for next object/array value.");
    m_pjrxCurrent->SetValueType(_tyJsonValue::GetJvtTypeFromChar(m_pjrxCurrent->m_tcFirst));
    if (ejvtJsonValueTypeCount == m_pjrxCurrent->JvtGetValueType())
      THROWBADJSONSTREAM("Found [%TC] when looking for value starting character.", m_pjrxCurrent->m_tcFirst);
    m_pjrxCurrent->m_pjrxNext->m_posEndValue = m_pis->ByPosGet(); // Update this as we iterate though there is no real reason to - might help with debugging.
    return true;
  }

  // Attach to the root of the JSON value tree.
  // We merely figure out the type of the value at this position.
  void AttachRoot(t_tyJsonInputStream &_ris)
  {
    Assert(!FAttached()); // We shouldn't have attached to the stream yet.
    std::unique_ptr<_tyJsonValue> pjvRootVal = std::make_unique<_tyJsonValue>();
    std::unique_ptr<_tyJsonReadContext> pjrxRoot = std::make_unique<_tyJsonReadContext>(&*pjvRootVal, (_tyJsonReadContext *)nullptr);
    _ris.SkipWhitespace();
    pjrxRoot->m_posStartValue = _ris.ByPosGet();
    Assert(!pjrxRoot->m_posEndValue); // We should have a 0 now - unset - must be >0 when set (invariant).

    // The first non-whitespace character tells us what the value type is:
    pjrxRoot->m_tcFirst = _ris.ReadChar("Empty JSON file.");
    pjvRootVal->SetValueType(_tyJsonValue::GetJvtTypeFromChar(pjrxRoot->m_tcFirst));
    if (ejvtJsonValueTypeCount == pjvRootVal->JvtGetValueType())
      THROWBADJSONSTREAM("Found [%TC] when looking for value starting character.", pjrxRoot->m_tcFirst);
    // Set up current state:
    m_pjrxContextStack.swap(pjrxRoot);    // swap with any existing stack.
    m_pjrxCurrent = &*m_pjrxContextStack; // current position is soft reference.
    m_pjrxRootVal.swap(pjvRootVal);       // swap in root value for tree.
    m_pis = &_ris;
  }

  bool FMoveDown() const
  {
    return const_cast<_tyThis *>(this)->FMoveDown(); // a little hacky but also the easiest.
  }
  bool FMoveDown()
  {
    Assert(FAttached()); // We should have been attached to a file by now.

    if (!FAtAggregateValue())
      return false; // Then we are at a leaf value.

    // If we have a parent then that is where we need to go - nothing to do besides that:
    if (m_pjrxCurrent->m_pjrxPrev)
    {
      m_pjrxCurrent = m_pjrxCurrent->m_pjrxPrev;
      return true;
    }

    // If we haven't read the current value then we must read it - this is where some actual work gets done...
    // 1) For objects we read the first string and then we read the first character of the corresponding value.
    //      a) For objects of zero content we still create the subobject, it will just return that there are no (key,value) pairs.
    // 2) For arrays we read the first character of the first value in the array.
    //      a) For empty arrays we still create the subobject, it will just return that there are no values in the array.
    // In fact we must not have read the current value since we would have already pushed the context onto the stack and thus we wouldn't be here.
    Assert(!m_pjrxCurrent->m_posEndValue);

    // We should be at the start of the value plus 1 character - this is important as we will be registered with the input stream throughout the streaming.
    Assert((m_pjrxCurrent->m_posStartValue + sizeof(_tyChar)) == m_pis->ByPosGet());
    m_pis->SkipWhitespace();

    std::unique_ptr<_tyJsonReadContext> pjrxNewRoot;
    if (ejvtObject == m_pjrxCurrent->JvtGetValueType())
    {
      // For valid JSON we may see 2 different things here:
      // 1) '"': Indicates we have a label for the first (key,value) pair of the object.
      // 2) '}': Indicates that we have an empty object.
      _tyChar tchCur = m_pis->ReadChar("EOF looking for first character of an object."); // throws on eof.
      if ((_tyCharTraits::s_tcRightCurlyBr != tchCur) && (_tyCharTraits::s_tcDoubleQuote != tchCur))
        THROWBADJSONSTREAM("Found [%TC] when looking for first character of object.", tchCur);

      // Then first value inside of the object. We must create a JsonObject that will be used to manage the iteration of the set of values within it.
      _tyJsonObject *pjoNew = m_pjrxCurrent->PJvGet()->PCreateJsonObject();
      pjrxNewRoot = std::make_unique<_tyJsonReadContext>(&pjoNew->RJvGet(), nullptr);
      if (_tyCharTraits::s_tcDoubleQuote == tchCur)
      {
        _tyStdStr strFirstKey;
        _ReadString(strFirstKey); // Might throw for any number of reasons. This may be the empty string.
        pjoNew->SwapKey(strFirstKey);
        m_pis->SkipWhitespace();
        tchCur = m_pis->ReadChar("EOF looking for colon on first object pair."); // throws on eof.
        if (_tyCharTraits::s_tcColon != tchCur)
          THROWBADJSONSTREAM("Found [%TC] when looking for colon on first object pair.", tchCur);
        m_pis->SkipWhitespace();
        pjrxNewRoot->m_posStartValue = m_pis->ByPosGet();
        Assert(!pjrxNewRoot->m_posEndValue); // We should have a 0 now - unset - must be >0 when set (invariant).
        // The first non-whitespace character tells us what the value type is:
        pjrxNewRoot->m_tcFirst = m_pis->ReadChar("EOF looking for first object value.");
        pjrxNewRoot->SetValueType(_tyJsonValue::GetJvtTypeFromChar(pjrxNewRoot->m_tcFirst));
        if (ejvtJsonValueTypeCount == pjrxNewRoot->JvtGetValueType())
          THROWBADJSONSTREAM("Found [%TC] when looking for value starting character.", pjrxNewRoot->m_tcFirst);
      }
      else
      {
        // We are an empty object but we need to push ourselves onto the context stack anyway because then things all work the same.
        pjrxNewRoot->m_posEndValue = pjrxNewRoot->m_posStartValue = m_pis->ByPosGet();
        pjrxNewRoot->SetValueType(ejvtEndOfObject); // Use special value type to indicate we are at the "end of the set of objects".
      }
    }
    else // Array.
    {
      // For valid JSON we may see 2 different things here:
      // 1) ']': Indicates that we have an empty object.
      // 2) A valid "value starting" character indicates the start of the first value of the array.
      _tyFilePos posStartValue = m_pis->ByPosGet();
      _tyChar tchCur = m_pis->ReadChar("EOF looking for first character of an array."); // throws on eof.
      EJsonValueType jvtCur = _tyJsonValue::GetJvtTypeFromChar(tchCur);
      if ((_tyCharTraits::s_tcRightSquareBr != tchCur) && (ejvtJsonValueTypeCount == jvtCur))
        THROWBADJSONSTREAM("Found [%TC] when looking for first char of array value.", tchCur);

      // Then first value inside of the object. We must create a JsonObject that will be used to manage the iteration of the set of values within it.
      _tyJsonArray *pjaNew = m_pjrxCurrent->PJvGet()->PCreateJsonArray();
      pjrxNewRoot = std::make_unique<_tyJsonReadContext>(&pjaNew->RJvGet(), nullptr);
      if (ejvtJsonValueTypeCount != jvtCur)
      {
        pjrxNewRoot->m_posStartValue = posStartValue;
        Assert(!pjrxNewRoot->m_posEndValue); // We should have a 0 now - unset - must be >0 when set (invariant).
        // The first non-whitespace character tells us what the value type is:
        pjrxNewRoot->m_tcFirst = tchCur;
        pjrxNewRoot->SetValueType(jvtCur);
      }
      else
      {
        // We are an empty object but we need to push ourselves onto the context stack anyway because then things all work the same.
        pjrxNewRoot->m_posEndValue = pjrxNewRoot->m_posStartValue = m_pis->ByPosGet();
        pjrxNewRoot->SetValueType(ejvtEndOfArray); // Use special value type to indicate we are at the "end of the set of objects".
      }
    }

    // Indicate that we have the end position of the aggregate we moved into:
    m_pjrxCurrent->m_posEndValue = m_pis->ByPosGet();
    // Push the new context onto the context stack:
    _tyJsonReadContext::PushStack(m_pjrxContextStack, pjrxNewRoot);
    m_pjrxCurrent = &*m_pjrxContextStack; // current position is soft reference.
    return true;                          // We did go down.
  }
  // Move up in the context - this is significantly easier to implement than FMoveDown().
  bool FMoveUp() const
  {
    return const_cast<_tyThis *>(this)->FMoveUp(); // a little hacky but also the easiest.
  }
  bool FMoveUp()
  {
    Assert(FAttached()); // We should have been attached to a file by now.
    if (!!m_pjrxCurrent->m_pjrxNext)
    {
      m_pjrxCurrent = &*m_pjrxCurrent->m_pjrxNext;
      return true;
    }
    return false; // ain't nowhere to go.
  }
  void swap(JsonReadCursor &_r)
  {
    AssertValid();
    _r.AssertValid();
    std::swap(m_pis, _r.m_pis);
    std::swap(m_pjrxCurrent, _r.m_pjrxCurrent);
    m_pjrxRootVal.swap(_r.m_pjrxRootVal);
    m_pjrxContextStack.swap(_r.m_pjrxContextStack);
  }
protected:
  _tyJsonInputStream *m_pis{};                            // Soft reference to stream from which we read.
  std::unique_ptr<_tyJsonValue> m_pjrxRootVal;            // Hard reference to the root value of the value tree.
  std::unique_ptr<_tyJsonReadContext> m_pjrxContextStack; // Implement a simple doubly linked list.
  _tyJsonReadContext *m_pjrxCurrent{};                    // The current cursor position within context stack.
};

// Helper/example methods - these are use in jsonpp.cpp:
namespace n_JSONStream
{

  // Input only method - just read the file and don't do anything with the data.
  template <class t_tyJsonInputStream>
  void StreamReadJsonValue(JsonReadCursor<t_tyJsonInputStream> &_jrc)
  {
    if (_jrc.FAtAggregateValue())
    {
      // save state, move down, and recurse.
      typename JsonReadCursor<t_tyJsonInputStream>::_tyJsonRestoreContext jrx(_jrc); // Restore to current context on destruct.
      bool f = _jrc.FMoveDown();
      Assert(f);
      for (; !_jrc.FAtEndOfAggregate(); (void)_jrc.FNextElement())
        StreamReadJsonValue(_jrc);
    }
    else
    {
      // discard value.
      _jrc.SkipTopContext();
    }
  }

  // Input/output method.
  template <class t_tyJsonInputStream, class t_tyJsonOutputStream>
  void StreamReadWriteJsonValue(JsonReadCursor<t_tyJsonInputStream> &_jrc, JsonValueLife<t_tyJsonOutputStream> &_jvl)
  {
    if (_jrc.FAtAggregateValue())
    {
      // save state, move down, and recurse.
      typename JsonReadCursor<t_tyJsonInputStream>::_tyJsonRestoreContext jrx(_jrc); // Restore to current context on destruct.
      bool f = _jrc.FMoveDown();
      Assert(f);
      for (; !_jrc.FAtEndOfAggregate(); (void)_jrc.FNextElement())
      {
        if (ejvtObject == _jvl.JvtGetValueType())
        {
          // For a value inside of an object we have to act specially because there will be a label to this value.
          typename JsonReadCursor<t_tyJsonInputStream>::_tyStdStr strKey;
          EJsonValueType jvt;
          bool fGotKey = _jrc.FGetKeyCurrent(strKey, jvt);
          Assert(fGotKey); // We should get a key here always and really we should throw if not.
          JsonValueLife<t_tyJsonOutputStream> jvlObjectElement(_jvl, strKey.c_str(), _jrc.JvtGetValueType());
          StreamReadWriteJsonValue(_jrc, jvlObjectElement);
        }
        else // array.
        {
          JsonValueLife<t_tyJsonOutputStream> jvlArrayElement(_jvl, _jrc.JvtGetValueType());
          StreamReadWriteJsonValue(_jrc, jvlArrayElement);
        }
      }
    }
    else
    {
      // Copy the value into the output stream.
      typename JsonValueLife<t_tyJsonOutputStream>::_tyJsonValue jvValue; // Make a copy of the current
      _jrc.GetValue(jvValue);
      _jvl.SetValue(std::move(jvValue));
    }
  }

  struct JSONUnitTestContext
  {
    bool m_fSkippedSomething{false};
    bool m_fSkipNextArray{false};
    int m_nArrayIndexSkip{-1};
  };

  // Input/output method.
  template <class t_tyJsonInputStream, class t_tyJsonOutputStream>
  void StreamReadWriteJsonValueUnitTest(JsonReadCursor<t_tyJsonInputStream> &_jrc, JsonValueLife<t_tyJsonOutputStream> &_jvl, JSONUnitTestContext &_rjutx)
  {
    if (_jrc.FAtAggregateValue())
    {
      // save state, move down, and recurse.
      typename JsonReadCursor<t_tyJsonInputStream>::_tyJsonRestoreContext jrx(_jrc); // Restore to current context on destruct.
      bool f = _jrc.FMoveDown();
      Assert(f);
      for (; !_jrc.FAtEndOfAggregate(); (void)_jrc.FNextElement())
      {
        if (ejvtObject == _jvl.JvtGetValueType())
        {
          // For a value inside of an object we have to act specially because there will be a label to this value.
          typename JsonReadCursor<t_tyJsonInputStream>::_tyStdStr strKey;
          EJsonValueType jvt;
          bool fGotKey = _jrc.FGetKeyCurrent(strKey, jvt);
          if (_rjutx.m_fSkipNextArray && (ejvtArray == jvt))
          {
            _rjutx.m_fSkipNextArray = false;
            continue; // Skip the entire array.
          }
          if (strKey == "skip")
          {
            _rjutx.m_fSkippedSomething = true;
            continue; // Skip this potentially complex value to test skipping input.
          }
          else if (strKey == "skipdownup")
          {
            if (_jrc.FMoveDown())
              (void)_jrc.FMoveUp();
            _rjutx.m_fSkippedSomething = true;
            continue; // Skip this potentially complex value to test skipping input.
          }
          else if (strKey == "skipleafup")
          {
            int nMoveDown = 0;
            while (_jrc.FMoveDown())
              ++nMoveDown;
            while (nMoveDown--)
              (void)_jrc.FMoveUp();
            _rjutx.m_fSkippedSomething = true;
            continue; // Skip this potentially complex value to test skipping input.
          }
          else if (strKey == "skiparray")
          {
            // We expect an integer specifying which index in the next array we encounter to skip.
            // We will also skip this key,value pair.
            if (_jrc.JvtGetValueType() == ejvtNumber)
              _jrc.GetValue(_rjutx.m_nArrayIndexSkip);
            else if (_jrc.JvtGetValueType() == ejvtTrue)
              _rjutx.m_fSkipNextArray = true;

            _rjutx.m_fSkippedSomething = true;
            continue; // Skip this potentially complex value to test skipping input.
          }

          Assert(fGotKey); // We should get a key here always and really we should throw if not.
          JsonValueLife<t_tyJsonOutputStream> jvlObjectElement(_jvl, strKey.c_str(), _jrc.JvtGetValueType());
          StreamReadWriteJsonValueUnitTest(_jrc, jvlObjectElement, _rjutx);
        }
        else // array.
        {
          if (_jvl.NSubValuesWritten() == _rjutx.m_nArrayIndexSkip)
          {
            _rjutx.m_nArrayIndexSkip = -1;
            continue;
          }
          JsonValueLife<t_tyJsonOutputStream> jvlArrayElement(_jvl, _jrc.JvtGetValueType());
          StreamReadWriteJsonValueUnitTest(_jrc, jvlArrayElement, _rjutx);
        }
      }
    }
    else
    {
      // Copy the value into the output stream.
      typename JsonValueLife<t_tyJsonOutputStream>::_tyJsonValue jvValue; // Make a copy of the current
      _jrc.GetValue(jvValue);
      _jvl.SetValue(std::move(jvValue));
    }
  }

  template <class t_tyJsonInputStream, class t_tyJsonOutputStream>
  struct StreamJSON
  {
    typedef t_tyJsonInputStream _tyJsonInputStream;
    typedef t_tyJsonOutputStream _tyJsonOutputStream;
    typedef typename _tyJsonInputStream::_tyCharTraits _tyCharTraits;
    static_assert(std::is_same_v<_tyCharTraits, typename _tyJsonOutputStream::_tyCharTraits>);
    typedef JsonFormatSpec<_tyCharTraits> _tyJsonFormatSpec;
    typedef JsonReadCursor<_tyJsonInputStream> _tyJsonReadCursor;
    typedef JsonValueLife<_tyJsonOutputStream> _tyJsonValueLife;
    typedef std::pair<const char*, vtyFileHandle> _tyPrFilenameHandle;

    static void Stream(const char *_pszInputFile, _tyPrFilenameHandle _prfnhOutput, bool _fReadOnly, bool _fCheckSkippedKey, const _tyJsonFormatSpec *_pjfs)
    {
      _tyJsonInputStream jis;
      jis.Open(_pszInputFile);
      _tyJsonReadCursor jrc;
      jis.AttachReadCursor(jrc);

      if (_fReadOnly)
        n_JSONStream::StreamReadJsonValue(jrc); // Read the value at jrc - more specifically stream in the value.
      else
      {
        // Open the write file to which we will be streaming JSON.
        _tyJsonOutputStream jos;
        if (!!_prfnhOutput.first)
          jos.Open(_prfnhOutput.first); // Open by default will truncate the file.
        else
          jos.AttachFd(_prfnhOutput.second);
        _tyJsonValueLife jvl(jos, jrc.JvtGetValueType(), _pjfs);
        if (_fCheckSkippedKey)
        {
          n_JSONStream::JSONUnitTestContext rjutx;
          n_JSONStream::StreamReadWriteJsonValueUnitTest(jrc, jvl, rjutx);
        }
        else
          n_JSONStream::StreamReadWriteJsonValue(jrc, jvl); // Read the value at jrc - more specifically stream in the value.
      }
    }
    static void Stream(vtyFileHandle _hFileInput, _tyPrFilenameHandle _prfnhOutput, bool _fReadOnly, bool _fCheckSkippedKey, const _tyJsonFormatSpec *_pjfs)
    {
      _tyJsonInputStream jis;
      jis.AttachFd(_hFileInput);
      _tyJsonReadCursor jrc;
      jis.AttachReadCursor(jrc);

      if (_fReadOnly)
        n_JSONStream::StreamReadJsonValue(jrc); // Read the value at jrc - more specifically stream in the value.
      else
      {
        // Open the write file to which we will be streaming JSON.
        _tyJsonOutputStream jos;
        if (!!_prfnhOutput.first)
          jos.Open(_prfnhOutput.first); // Open by default will truncate the file.
        else
          jos.AttachFd(_prfnhOutput.second);
        _tyJsonValueLife jvl(jos, jrc.JvtGetValueType(), _pjfs);
        if (_fCheckSkippedKey)
        {
          n_JSONStream::JSONUnitTestContext rjutx;
          n_JSONStream::StreamReadWriteJsonValueUnitTest(jrc, jvl, rjutx);
        }
        else
          n_JSONStream::StreamReadWriteJsonValue(jrc, jvl); // Read the value at jrc - more specifically stream in the value.
      }
    }
    static void Stream(const char *_pszInputFile, const char *_pszOutputFile, bool _fReadOnly, bool _fCheckSkippedKey, const _tyJsonFormatSpec *_pjfs)
    {
      _tyJsonInputStream jis;
      jis.Open(_pszInputFile);
      _tyJsonReadCursor jrc;
      jis.AttachReadCursor(jrc);

      if (_fReadOnly)
        n_JSONStream::StreamReadJsonValue(jrc); // Read the value at jrc - more specifically stream in the value.
      else
      {
        // Open the write file to which we will be streaming JSON.
        _tyJsonOutputStream jos;
        jos.Open(_pszOutputFile); // Open by default will truncate the file.
        _tyJsonValueLife jvl(jos, jrc.JvtGetValueType(), _pjfs);
        if (_fCheckSkippedKey)
        {
          n_JSONStream::JSONUnitTestContext rjutx;
          n_JSONStream::StreamReadWriteJsonValueUnitTest(jrc, jvl, rjutx);
        }
        else
          n_JSONStream::StreamReadWriteJsonValue(jrc, jvl); // Read the value at jrc - more specifically stream in the value.
      }
    }
    static void Stream(vtyFileHandle _hFileInput, const char *_pszOutputFile, bool _fReadOnly, bool _fCheckSkippedKey, const _tyJsonFormatSpec *_pjfs)
    {
      _tyJsonInputStream jis;
      jis.AttachFd(_hFileInput);
      _tyJsonReadCursor jrc;
      jis.AttachReadCursor(jrc);

      if (_fReadOnly)
        n_JSONStream::StreamReadJsonValue(jrc); // Read the value at jrc - more specifically stream in the value.
      else
      {
        // Open the write file to which we will be streaming JSON.
        _tyJsonOutputStream jos;
        jos.Open(_pszOutputFile); // Open by default will truncate the file.
        _tyJsonValueLife jvl(jos, jrc.JvtGetValueType(), _pjfs);
        if (_fCheckSkippedKey)
        {
          n_JSONStream::JSONUnitTestContext rjutx;
          n_JSONStream::StreamReadWriteJsonValueUnitTest(jrc, jvl, rjutx);
        }
        else
          n_JSONStream::StreamReadWriteJsonValue(jrc, jvl); // Read the value at jrc - more specifically stream in the value.
      }
    }
  };

} // namespace n_JSONStream

__BIENUTIL_END_NAMESPACE