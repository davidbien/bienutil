#pragma once

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// jsonstrm.h
// Templates to implement JSON input/output streaming. STAX design allows streaming of very large JSON files.
// dbien: 17FEB2020

#include <string>
#include <memory>
#include <unistd.h>
#include <stdexcept>
#include <fcntl.h>
#include <limits.h>
#include <sys/mman.h>
#include <uuid/uuid.h>

#include "bienutil.h"
#include "bientypes.h"
#include "_namdexc.h"
#include "_debug.h"
#include "_util.h"
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

// struct JsonCharTraits: Only specializations of this.
// This describes the type of file that the JSON files is persisted within.
template < class t_tyChar >
struct JsonCharTraits;
template < class t_tyCharTraits >
class JsonValue;
template < class t_tyCharTraits >
class JsonObject;
template < class t_tyCharTraits >
class JsonArray;
template < class t_tyJsonInputStream >
class JsonReadCursor;
template < class t_tyCharTraits >
class JsonFormatSpec;

#ifndef __JSONSTRM_DEFAULT_ALLOCATOR
#if defined(_USE_STLPORT) && !defined(NDEBUG)
#define __JSONSTRM_DEFAULT_ALLOCATOR __STD::_stlallocator< char, __STD::__debug_alloc< __STD::__malloc_alloc > >
#define __JSONSTRM_GET_ALLOCATOR(t) __STD::_stlallocator< t, __STD::__debug_alloc< __STD::__malloc_alloc > >
#else // defined(_USE_STLPORT) && !defined(NDEBUG)
#define __JSONSTRM_DEFAULT_ALLOCATOR __STD::allocator< char >
#define __JSONSTRM_GET_ALLOCATOR(t) __STD::allocator< t >
#endif // defined(_USE_STLPORT) && !defined(NDEBUG)
#endif //__JSONSTRM_DEFAULT_ALLOCATOR

#ifdef __NAMDDEXC_STDBASE
#pragma push_macro("std")
#undef std
#endif //__NAMDDEXC_STDBASE

// This exception will get thrown when there is an issue with the JSON stream.
template < class t_tyCharTraits >
class bad_json_stream_exception : public std::_t__Named_exception_errno< __JSONSTRM_DEFAULT_ALLOCATOR >
{
    typedef std::_t__Named_exception_errno< __JSONSTRM_DEFAULT_ALLOCATOR > _TyBase;
public:
    typedef t_tyCharTraits _tyCharTraits;

    bad_json_stream_exception( const string_type & __s, int _nErrno = 0 ) 
        : _TyBase( __s, _nErrno ) 
    {
    }
    bad_json_stream_exception( const char * _pcFmt, va_list args ) 
    {
        RenderVA( _pcFmt, args );
    }
    bad_json_stream_exception( int _errno, const char * _pcFmt, va_list args ) 
        : _TyBase( _errno )
    {
        RenderVA( _pcFmt, args );
    }
    void RenderVA( const char * _pcFmt, va_list args )
    {
        // When we see %TC we should substitute the formating for the type of characters in t_tyCharTraits.
        const int knBuf = NAMEDEXC_BUFSIZE;
        char rgcBuf[knBuf+1];
        char * pcBufCur = rgcBuf;
        char * const pcBufTail = rgcBuf + NAMEDEXC_BUFSIZE;
        for ( const char * pcFmtCur = _pcFmt; !!*pcFmtCur && ( pcBufCur != pcBufTail ); ++pcFmtCur )
        {
            if ( ( pcFmtCur[0] == '%' ) && ( pcFmtCur[1] == 'T' ) && ( pcFmtCur[2] == 'C' ) )
            {
                pcFmtCur += 2;
                const char * pcCharFmtCur = _tyCharTraits::s_szFormatChar;
                for ( ; !!*pcCharFmtCur && ( pcBufCur != pcBufTail ); ++pcCharFmtCur )
                    *pcBufCur++ = *pcCharFmtCur;
            }
            else
                *pcBufCur++ = *pcFmtCur;
        }
        *pcBufCur = 0;

        _TyBase::RenderVA( rgcBuf, args ); // Render into the exception description buffer.
    }
};
// By default we will always add the __FILE__, __LINE__ even in retail for debugging purposes.
#define THROWBADJSONSTREAM( MESG... ) ExceptionUsage<bad_json_stream_exception<_tyCharTraits>>::ThrowFileLine( __FILE__, __LINE__, MESG )
#define THROWBADJSONSTREAMERRNO( ERRNO, MESG... ) ExceptionUsage<bad_json_stream_exception<_tyCharTraits>>::ThrowFileLineErrno( __FILE__, __LINE__, ERRNO, MESG )

// This exception will get thrown if the user of the read cursor does something inappropriate given the current context.
template < class t_tyCharTraits >
class json_stream_bad_semantic_usage_exception : public std::_t__Named_exception< __JSONSTRM_DEFAULT_ALLOCATOR >
{
    typedef std::_t__Named_exception< __JSONSTRM_DEFAULT_ALLOCATOR > _TyBase;
public:
    typedef t_tyCharTraits _tyCharTraits;

    json_stream_bad_semantic_usage_exception( const string_type & __s ) 
        : _TyBase( __s ) 
    {
    }
    json_stream_bad_semantic_usage_exception( const char * _pcFmt, va_list args ) 
    {
        // When we see %TC we should substitute the formating for the type of characters in t_tyCharTraits.
        const int knBuf = NAMEDEXC_BUFSIZE;
        char rgcBuf[knBuf+1];
        char * pcBufCur = rgcBuf;
        char * const pcBufTail = rgcBuf + NAMEDEXC_BUFSIZE;
        for ( const char * pcFmtCur = _pcFmt; !!*pcFmtCur && ( pcBufCur != pcBufTail ); ++pcFmtCur )
        {
            if ( ( pcFmtCur[0] == '%' ) && ( pcFmtCur[1] == 'T' ) && ( pcFmtCur[2] == 'C' ) )
            {
                pcFmtCur += 2;
                const char * pcCharFmtCur = _tyCharTraits::s_szFormatChar;
                for ( ; !!*pcCharFmtCur && ( pcBufCur != pcBufTail ); ++pcCharFmtCur )
                    *pcBufCur++ = *pcCharFmtCur;
            }
            else
                *pcBufCur++ = *pcFmtCur;
        }
        *pcBufCur = 0;

        RenderVA( rgcBuf, args ); // Render into the exception description buffer.
    }
};
// By default we will always add the __FILE__, __LINE__ even in retail for debugging purposes.
#define THROWBADJSONSEMANTICUSE( MESG... ) ExceptionUsage<json_stream_bad_semantic_usage_exception<_tyCharTraits>>::ThrowFileLine( __FILE__, __LINE__, MESG )

#ifdef __NAMDDEXC_STDBASE
#pragma pop_macro("std")
#endif //__NAMDDEXC_STDBASE

// For string constants we use #define macros because we do compile-time translation to the character of interest which precludes them being present in the specializations of JsonCharTraits.
#define JSONSTRM_PrintableWhitespace "\t\n\r"
#define JSONSTRM_EscapeStringChars "\\\"\b\f\t\n\r"

// JsonCharTraits< char > : Traits for 8bit char representation.
template <>
struct JsonCharTraits< char >
{
    typedef char _tyChar;
    typedef char _tyPersistAsChar;
    typedef _tyChar * _tyLPSTR;
    typedef const _tyChar * _tyLPCSTR;
    typedef StrWRsv< std::basic_string< _tyChar > > _tyStdStr;

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
    // The formatting command to format a single character using printf style.
    static constexpr const char * s_szFormatChar = "%c";

    static const int s_knMaxNumberLength = 512; // Yeah this is absurd but why not?

    static bool FIsWhitespace( _tyChar _tc )
    {
        return ( ( ' ' == _tc ) || ( '\t' == _tc ) || ( '\n' == _tc ) || ( '\r' == _tc ) );
    }
    static bool FIsIllegalChar( _tyChar _tc )
    {
        return !_tc; // we could expand this...
    }
    static size_t StrLen( _tyLPCSTR _psz )
    {
        return strlen( _psz );
    }
    static size_t StrNLen( _tyLPCSTR _psz, size_t _stLen = std::numeric_limits< size_t >::max() )
    {
        return ::StrNLen( _psz, _stLen );
    }
    static size_t StrCSpn( _tyLPCSTR _psz, _tyLPCSTR _pszCharSet )
    { 
        return strcspn( _psz, _pszCharSet );
    }
    static size_t StrCSpn( _tyLPCSTR _psz, _tyChar _tcBegin, _tyChar _tcEnd, _tyLPCSTR _pszCharSet )
    {
        _tyLPCSTR pszCur = _psz;
        for ( ; !!*pszCur && ( ( *pszCur < _tcBegin ) || ( *pszCur >= _tcEnd ) ); ++pszCur )
        {
            // Passed the first test, now ensure it isn't one of the _pszCharSet chars:
            _tyLPCSTR pszCharSetCur = _pszCharSet;
            for ( ; !!*pszCharSetCur && ( *pszCharSetCur != *pszCur ); ++pszCharSetCur )
                ;
            if ( !!*pszCharSetCur )
                break;
        }
        return pszCur - _psz;
    }
    static size_t StrRSpn( _tyLPCSTR _pszBegin, _tyLPCSTR _pszEnd, _tyLPCSTR _pszSet )
    { 
        return ::StrRSpn( _pszBegin, _pszEnd, _pszSet );
    }
    static void MemSet( _tyLPSTR _psz, _tyChar _tc, size_t _n )
    { 
        (void)memset( _psz, _tc, _n );
    }
    static int Snprintf( _tyLPSTR _psz, size_t _n, _tyLPCSTR _pszFmt, ... )
    {
        va_list ap;
        va_start( ap, _pszFmt );
        int iRet = ::vsnprintf( _psz, _n, _pszFmt, ap );
        va_end( ap );
        return iRet;
    }
};

// JsonCharTraits< wchar_t > : Traits for 16bit char representation.
template <>
struct JsonCharTraits< wchar_t >
{
    typedef wchar_t _tyChar;
    typedef char16_t _tyPersistAsChar; // This is a hint and each file object can persist as it likes.
    typedef _tyChar * _tyLPSTR;
    typedef const _tyChar * _tyLPCSTR;
    typedef StrWRsv< std::basic_string< _tyChar > > _tyStdStr;

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
    // The formatting command to format a single character using printf style.
    static constexpr const char * s_szFormatChar = "%lc";

    static const int s_knMaxNumberLength = 512; // Yeah this is absurd but why not?

    static bool FIsWhitespace( _tyChar _tc )
    {
        return ( ( L' ' == _tc ) || ( L'\t' == _tc ) || ( L'\n' == _tc ) || ( L'\r' == _tc ) );
    }
    static bool FIsIllegalChar( _tyChar _tc )
    {
        return !_tc;
    }
    static size_t StrLen( _tyLPCSTR _psz )
    {
        return wcslen( _psz );
    }
    static size_t StrNLen( _tyLPCSTR _psz, size_t _stLen = std::numeric_limits< size_t >::max() )
    {
        return ::StrNLen( _psz, _stLen );
    }
    static size_t StrCSpn( _tyLPCSTR _psz, _tyLPCSTR _pszCharSet )
    { 
        return wcscspn( _psz, _pszCharSet );
    }
    static size_t StrCSpn( _tyLPCSTR _psz, _tyChar _tcBegin, _tyChar _tcEnd, _tyLPCSTR _pszCharSet )
    {
        _tyLPCSTR pszCur = _psz;
        for ( ; !!*pszCur && ( ( *pszCur < _tcBegin ) || ( *pszCur >= _tcEnd ) ); ++pszCur )
        {
            // Passed the first test, now ensure it isn't one of the _pszCharSet chars:
            _tyLPCSTR pszCharSetCur = _pszCharSet;
            for ( ; !!*pszCharSetCur && ( *pszCharSetCur != *pszCur ); ++pszCharSetCur )
                ;
            if ( !!*pszCharSetCur )
                break;
        }
        return pszCur - _psz;
    }
    static size_t StrRSpn( _tyLPCSTR _pszBegin, _tyLPCSTR _pszEnd, _tyLPCSTR _pszSet )
    { 
        return ::StrRSpn( _pszBegin, _pszEnd, _pszSet );
    }
    static void MemSet( _tyLPSTR _psz, _tyChar _tc, size_t _n )
    { 
        (void)wmemset( _psz, _tc, _n );
    }
    static int Snprintf( _tyLPSTR _psz, size_t _n, _tyLPCSTR _pszFmt, ... )
    {
        va_list ap;
        va_start( ap, _pszFmt );
        int iRet = ::vswprintf( _psz, _n, _pszFmt, ap );
        va_end( ap );
        return iRet;
    }
};

// JsonInputStream: Base class for input streams.
template < class t_tyCharTraits, class t_tyFilePos >
class JsonInputStreamBase
{
public:
    typedef t_tyCharTraits _tyCharTraits;
    typedef t_tyFilePos _tyFilePos;
};

// JsonOutputStream: Base class for output streams.
template < class t_tyCharTraits, class t_tyFilePos >
class JsonOutputStreamBase
{
public:
    typedef t_tyCharTraits _tyCharTraits;
    typedef t_tyFilePos _tyFilePos;
};

// JsonLinuxInputStream: A class using open(), read(), etc.
template < class t_tyCharTraits, class t_tyPersistAsChar = typename t_tyCharTraits::_tyPersistAsChar >
class JsonLinuxInputStream : public JsonInputStreamBase< t_tyCharTraits, size_t >
{
    typedef JsonLinuxInputStream _tyThis;
public:
    typedef t_tyCharTraits _tyCharTraits;
    typedef typename _tyCharTraits::_tyChar _tyChar;
    typedef t_tyPersistAsChar _tyPersistAsChar;
    typedef size_t _tyFilePos;
    typedef JsonReadCursor< _tyThis > _tyJsonReadCursor;

    JsonLinuxInputStream() = default;
    ~JsonLinuxInputStream()
    {
        if ( m_fOwnFdLifetime && FOpened() )
        {
            m_fOwnFdLifetime = false; // prevent reentry though we know it should never happen.
            _Close( m_fd );
        }
    }

    bool FOpened() const
    {
        return -1 != m_fd;
    }

    // Throws on open failure. This object owns the lifetime of the file descriptor.
    void Open( const char * _szFilename )
    {
        Assert( !FOpened() );
        m_fd = open( _szFilename, O_RDONLY );
        if ( !FOpened() )
            THROWBADJSONSTREAMERRNO( errno, "JsonLinuxInputStream::Open(): Unable to open() file [%s]", _szFilename );
        m_fHasLookahead = false; // ensure that if we had previously been opened that we don't think we still have a lookahead.
        m_fOwnFdLifetime = true; // This object owns the lifetime of m_fd - ie. we need to close upon destruction.
        m_szFilename = _szFilename; // For error reporting and general debugging. Of course we don't need to store this.
        m_pos = 0;
    }

    // Attach to an FD whose lifetime we do not own. This can be used, for instance, to attach to stdin which is usually at FD 0 unless reopen()ed.
    void AttachFd( int _fd, bool _fUseSeek = false )
    {
        Assert( !FOpened() );
        Assert( _fd != -1 );
        m_fd = _fd;
        m_fHasLookahead = false; // ensure that if we had previously been opened that we don't think we still have a lookahead.
        m_fOwnFdLifetime = false; // This object owns the lifetime of m_fd - ie. we need to close upon destruction.
        m_szFilename.clear(); // No filename indicates we are attached to "some fd".
        m_fUseSeek = _fUseSeek;
        m_pos = 0;
    }

    int Close()
    {
        if ( FOpened() )
        {
            int fd = m_fd;
            m_fd = 0;
            if ( m_fOwnFdLifetime )
                return _Close( fd );
        }
        return 0;
    }
    static int _Close( int _fd )
    {
        return close( _fd );
    }

    // Attach to this FOpened() JsonLinuxInputStream.
    void AttachReadCursor( _tyJsonReadCursor & _rjrc )
    {
        Assert( !_rjrc.FAttached() );
        Assert( FOpened() );
        _rjrc.AttachRoot( *this );
    }

    void SkipWhitespace() 
    {
        // We will keep reading characters until we find non-whitespace:
        if ( m_fHasLookahead && !_tyCharTraits::FIsWhitespace( m_tcLookahead ) )
            return;
        ssize_t sstRead;
        for ( ; ; )
        {
            _tyPersistAsChar cpxRead;
            sstRead = read( m_fd, &cpxRead, sizeof cpxRead );
            m_tcLookahead = cpxRead;
            if ( -1 == sstRead )
                THROWBADJSONSTREAMERRNO( errno, "JsonLinuxInputStream::SkipWhitespace(): read() failed for file [%s]", m_szFilename.c_str() );
            m_pos += sstRead;
            if ( sstRead != sizeof cpxRead )
                THROWBADJSONSTREAMERRNO( errno, "JsonLinuxInputStream::SkipWhitespace(): read() for file [%s] had [%d] leftover bytes.", m_szFilename.c_str(), sstRead );
            if ( !sstRead )
            {
                m_fHasLookahead = false;
                return; // We have skipped the whitespace until we hit EOF.
            }
            if ( _tyCharTraits::FIsIllegalChar( m_tcLookahead ) )
                THROWBADJSONSTREAM( "JsonLinuxInputStream::SkipWhitespace(): Found illegal char [%TC] in file [%s]", m_tcLookahead ? m_tcLookahead : '?', m_szFilename.c_str() );
            if ( !_tyCharTraits::FIsWhitespace( m_tcLookahead ) )
            {
                m_fHasLookahead = true;
                return;
            }
        }
    }

    // Throws if lseek() fails.
    _tyFilePos PosGet() const
    {
        Assert( FOpened() );
        if ( m_fUseSeek )
        {
            _tyFilePos pos = lseek( m_fd, 0, SEEK_CUR );
            if ( -1 == pos )
                THROWBADJSONSTREAMERRNO( errno, "JsonLinuxInputStream::PosGet(): lseek() failed for file [%s]", m_szFilename.c_str() );
            Assert( pos == m_pos ); // These should always match.
            if ( m_fHasLookahead )
                pos -= sizeof m_tcLookahead; // Since we have a lookahead we are actually one character before.
            return pos;
        }
        else
            return m_pos - ( m_fHasLookahead ? sizeof m_tcLookahead : 0 );
    }
    // Read a single character from the file - always throw on EOF.
    _tyChar ReadChar( const char * _pcEOFMessage )
    {
        Assert( FOpened() );
        if ( m_fHasLookahead )
        {
            m_fHasLookahead = false;
            return m_tcLookahead;
        }
        _tyPersistAsChar cpxRead;
        ssize_t sstRead = read( m_fd, &cpxRead, sizeof cpxRead );
        m_tcLookahead = cpxRead;
        if ( sstRead != sizeof cpxRead )
        {
            if ( -1 == sstRead )
                THROWBADJSONSTREAMERRNO( errno, "JsonLinuxInputStream::ReadChar(): read() failed for file [%s]", m_szFilename.c_str() );
            else
            {
                Assert( !sstRead ); // For multibyte characters this could fail but then we would have a bogus file anyway and would want to throw.
                m_pos += sstRead;
                THROWBADJSONSTREAM( "[%s]: %s", m_szFilename.c_str(), _pcEOFMessage );
            }
        }
        if ( _tyCharTraits::FIsIllegalChar( m_tcLookahead ) )
            THROWBADJSONSTREAM( "JsonLinuxInputStream::ReadChar(): Found illegal char [%TC] in file [%s]", m_tcLookahead ? m_tcLookahead : '?', m_szFilename.c_str() );

        m_pos += sstRead;
        Assert( !m_fHasLookahead );
        return m_tcLookahead;
    }
    
    bool FReadChar( _tyChar & _rtch, bool _fThrowOnEOF, const char * _pcEOFMessage )
    {
        Assert( FOpened() );
        if ( m_fHasLookahead )
        {
            _rtch = m_tcLookahead;
            m_fHasLookahead = false;
            return true;
        }
        _tyPersistAsChar cpxRead;
        ssize_t sstRead = read( m_fd, &cpxRead, sizeof cpxRead );
        m_tcLookahead = cpxRead;
        if ( sstRead != sizeof cpxRead )
        {
            if ( -1 == sstRead )
                THROWBADJSONSTREAMERRNO( errno, "JsonLinuxInputStream::FReadChar(): read() failed for file [%s]", m_szFilename.c_str() );
            else
            if ( !!sstRead )
            {
                m_pos += sstRead;
                THROWBADJSONSTREAMERRNO( errno, "JsonLinuxInputStream::FReadChar(): read() for file [%s] had [%d] leftover bytes.", m_szFilename.c_str(), sstRead );
            }
            else
            {
                if ( _fThrowOnEOF )
                    THROWBADJSONSTREAM( "[%s]: %s", m_szFilename.c_str(), _pcEOFMessage );
                return false;
            }
        }
        if ( _tyCharTraits::FIsIllegalChar( m_tcLookahead ) )
            THROWBADJSONSTREAM( "JsonLinuxInputStream::FReadChar(): Found illegal char [%TC] in file [%s]", m_tcLookahead ? m_tcLookahead : '?', m_szFilename.c_str() );
        m_pos += sstRead;
        _rtch = m_tcLookahead;
        return true;
    }

    void PushBackLastChar( bool _fMightBeWhitespace = false )
    {
        Assert( !m_fHasLookahead ); // support single character lookahead only.
        Assert( _fMightBeWhitespace || !_tyCharTraits::FIsWhitespace( m_tcLookahead ) );
        if ( !_tyCharTraits::FIsWhitespace( m_tcLookahead ) )
            m_fHasLookahead = true;
    }

protected:
    std::string m_szFilename;
    _tyFilePos m_pos{0}; // We use this if !m_fUseSeek.
    int m_fd{-1}; // file descriptor.
    _tyChar m_tcLookahead{0}; // Everytime we read a character we put it in the m_tcLookahead and clear that we have a lookahead.
    bool m_fHasLookahead{false};
    bool m_fOwnFdLifetime{false}; // Should we close the FD upon destruction?
    bool m_fUseSeek{true}; // For STDIN we cannot use seek so we just maintain our "current" position.
};

// JsonInputFixedMemStream: Stream a fixed piece o' mem'ry at the JSON parser.
template < class t_tyCharTraits, class t_tyPersistAsChar = typename t_tyCharTraits::_tyPersistAsChar >
class JsonInputFixedMemStream : public JsonInputStreamBase< t_tyCharTraits, size_t >
{
    typedef JsonInputFixedMemStream _tyThis;
public:
    typedef t_tyCharTraits _tyCharTraits;
    typedef typename _tyCharTraits::_tyChar _tyChar;
    typedef t_tyPersistAsChar _tyPersistAsChar;
    typedef size_t _tyFilePos;
    typedef JsonReadCursor< _tyThis > _tyJsonReadCursor;

    JsonInputFixedMemStream() = default;
    JsonInputFixedMemStream( const _tyPersistAsChar * _pcpxBegin, _tyFilePos _stLen )
    {
        Open( _pcpxBegin, _stLen );
    }
    ~JsonInputFixedMemStream()
    {
    }

    bool FOpened() const
    {
        return MAP_FAILED != m_pcpxBegin;
    }
    bool FEOF() const
    {
        Assert( FOpened() );
        return ( m_pcpxEnd == m_pcpxCur ) && !m_fHasLookahead;
    }
    _tyFilePos StLenBytes() const
    {
        Assert( FOpened() );
        return ( m_pcpxEnd - m_pcpxBegin ) * sizeof(_tyPersistAsChar);
    }

    // Throws on open failure. This object owns the lifetime of the file descriptor.
    void Open( const _tyPersistAsChar * _pcpxBegin, _tyFilePos _stLen )
    {
        Close();
        m_pcpxCur = m_pcpxBegin = _pcpxBegin;
        m_pcpxEnd = m_pcpxCur + _stLen;
    }

    void Close()
    {
        m_pcpxBegin = (_tyPersistAsChar *)MAP_FAILED;
        m_pcpxCur = (_tyPersistAsChar *)MAP_FAILED;
        m_pcpxEnd = (_tyPersistAsChar *)MAP_FAILED;
        m_fHasLookahead = false;
        m_tcLookahead = 0;
    }

    // Attach to this FOpened() JsonLinuxInputStream.
    void AttachReadCursor( _tyJsonReadCursor & _rjrc )
    {
        Assert( !_rjrc.FAttached() );
        Assert( FOpened() );
        _rjrc.AttachRoot( *this );
    }

    void SkipWhitespace( const char * _pcFilename = 0 ) 
    {
        // We will keep reading characters until we find non-whitespace:
        if ( m_fHasLookahead && !_tyCharTraits::FIsWhitespace( m_tcLookahead ) )
            return;
        m_fHasLookahead = false;
        for ( ; ; )
        {
            if ( FEOF() )
                return; // We have skipped the whitespace until we hit EOF.
            m_tcLookahead = *m_pcpxCur++;
            if ( _tyCharTraits::FIsIllegalChar( m_tcLookahead ) )
                THROWBADJSONSTREAM( "JsonInputFixedMemStream::SkipWhitespace(): Found illegal char [%TC] in file [%s]", m_tcLookahead ? m_tcLookahead : '?', !_pcFilename ? "(no file)" : _pcFilename );
            if ( !_tyCharTraits::FIsWhitespace( m_tcLookahead ) )
            {
                m_fHasLookahead = true;
                return;
            }
        }
    }

    _tyFilePos PosGet() const
    {
        Assert( FOpened() );
        return ( ( m_pcpxCur - m_pcpxBegin ) - (size_t)m_fHasLookahead ) * sizeof(_tyPersistAsChar);
    }
    // Read a single character from the file - always throw on EOF.
    _tyChar ReadChar( const char * _pcEOFMessage, const char * _pcFilename = 0 )
    {
        Assert( FOpened() );
        if ( m_fHasLookahead )
        {
            Assert( !!m_tcLookahead );
            m_fHasLookahead = false;
            return m_tcLookahead;
        }
        if ( FEOF() )
            THROWBADJSONSTREAM( "[%s]: %s", !_pcFilename ? "(no file)" : _pcFilename, _pcEOFMessage );
        Assert( !!*m_pcpxCur );
        m_tcLookahead = *m_pcpxCur++;
        if ( _tyCharTraits::FIsIllegalChar( m_tcLookahead ) )
            THROWBADJSONSTREAM( "JsonInputFixedMemStream::ReadChar(): Found illegal char [%TC] in file [%s]", m_tcLookahead ? m_tcLookahead : '?', !_pcFilename ? "(no file)" : _pcFilename );
        return m_tcLookahead;
    }
    
    bool FReadChar( _tyChar & _rtch, bool _fThrowOnEOF, const char * _pcEOFMessage, const char * _pcFilename = 0 )
    {
        Assert( FOpened() );
        if ( m_fHasLookahead )
        {
            _rtch = m_tcLookahead;
            m_fHasLookahead = false;
            return true;
        }
        if ( FEOF() )
        {
            if ( _fThrowOnEOF )
                THROWBADJSONSTREAM( "[%s]: %s", !_pcFilename ? "(no file)" : _pcFilename, _pcEOFMessage );
            return false;
        }
        _rtch = m_tcLookahead = *m_pcpxCur++;
        if ( _tyCharTraits::FIsIllegalChar( m_tcLookahead ) )
            THROWBADJSONSTREAM( "JsonInputFixedMemStream::FReadChar(): Found illegal char [%TC] in file [%s]", m_tcLookahead ? m_tcLookahead : '?', !_pcFilename ? "(no file)" : _pcFilename );
        return true;
    }

    void PushBackLastChar( bool _fMightBeWhitespace = false )
    {
        Assert( !m_fHasLookahead ); // support single character lookahead only.
        Assert( _fMightBeWhitespace || !_tyCharTraits::FIsWhitespace( m_tcLookahead ) );
        if ( !_tyCharTraits::FIsWhitespace( m_tcLookahead ) )
            m_fHasLookahead = true;
    }

    std::strong_ordering ICompare( _tyThis const & _rOther ) const
    {
        Assert( FOpened() && _rOther.FOpened() );
        if ( StLenBytes() < _rOther.StLenBytes() )
            return std::strong_ordering::less;
        else
        if ( StLenBytes() > _rOther.StLenBytes() )
            return std::strong_ordering::greater;
        int iComp = memcmp( m_pcpxBegin, _rOther.m_pcpxBegin, StLenBytes() );
        return ( iComp < 0 ) ? std::strong_ordering::less : ( ( iComp > 0 ) ? std::strong_ordering::greater : std::strong_ordering::equal );
    }

protected:
    const _tyPersistAsChar * m_pcpxBegin{(_tyPersistAsChar*)MAP_FAILED};
    const _tyPersistAsChar * m_pcpxCur{(_tyPersistAsChar*)MAP_FAILED};
    const _tyPersistAsChar * m_pcpxEnd{(_tyPersistAsChar*)MAP_FAILED};
    _tyChar m_tcLookahead{0}; // Everytime we read a character we put it in the m_tcLookahead and clear that we have a lookahead.
    bool m_fHasLookahead{false};
};

// JsonLinuxInputMemMappedStream: A class using open(), read(), etc.
template < class t_tyCharTraits, class t_tyPersistAsChar = typename t_tyCharTraits::_tyPersistAsChar >
class JsonLinuxInputMemMappedStream : protected JsonInputFixedMemStream< t_tyCharTraits >
{
    typedef JsonInputFixedMemStream< t_tyCharTraits > _tyBase;
    typedef JsonLinuxInputMemMappedStream _tyThis;
public:
    typedef t_tyCharTraits _tyCharTraits;
    typedef typename _tyCharTraits::_tyChar _tyChar;
    typedef t_tyPersistAsChar _tyPersistAsChar;
    typedef size_t _tyFilePos;
    typedef JsonReadCursor< _tyThis > _tyJsonReadCursor;

    JsonLinuxInputMemMappedStream() = default;
    ~JsonLinuxInputMemMappedStream()
    {
        (void)Close();
    }

    bool FOpened() const
    {
        return FFileOpened() && FFileMapped();
    }
    bool FFileOpened() const
    {
        return -1 != m_fd;
    }
    bool FFileMapped() const
    {
        return (t_tyPersistAsChar*)MAP_FAILED != m_pcpxBegin;
    }
    using _tyBase::FEOF;
    using _tyBase::StLenBytes;

    // Throws on open failure. This object owns the lifetime of the file descriptor.
    void Open( const char * _szFilename )
    {
        Assert( !FOpened() );
        Close();
        m_fd = open( _szFilename, O_RDONLY );
        if ( !FFileOpened() )
            THROWBADJSONSTREAMERRNO( errno, "JsonLinuxInputMemMappedStream::Open(): Unable to open() file [%s]", _szFilename );
        // Now get the size of the file and then map it.
        _tyFilePos posEnd = lseek( m_fd, 0, SEEK_END );
        if ( -1 == posEnd )
            THROWBADJSONSTREAMERRNO( errno, "JsonLinuxInputMemMappedStream::Open(): Attempting to seek to EOF failed for [%s]", _szFilename );
        if ( 0 == posEnd )
            THROWBADJSONSTREAM( "JsonLinuxInputMemMappedStream::Open(): File [%s] is empty - it contains no JSON value.", _szFilename );
        if ( !!( posEnd % sizeof(t_tyPersistAsChar) ) )
            THROWBADJSONSTREAM( "JsonLinuxInputMemMappedStream::Open(): File [%s]'s size not multiple of char size [%d].", _szFilename, posEnd );
        // No need to reset the file pointer to the beginning - and in fact we like it at the end in case someone were to actually try to read from it.
        m_pcpxBegin = (t_tyPersistAsChar*)mmap( 0, posEnd, PROT_READ, MAP_SHARED, m_fd, 0 );
        if ( m_pcpxBegin == (t_tyPersistAsChar*)MAP_FAILED )
            THROWBADJSONSTREAMERRNO( errno, "JsonLinuxInputMemMappedStream::Open(): mmap() failed for [%s]", _szFilename );
        m_pcpxCur = m_pcpxBegin;
        m_pcpxEnd = m_pcpxCur + ( posEnd / sizeof(t_tyPersistAsChar) );
        m_fHasLookahead = false; // ensure that if we had previously been opened that we don't think we still have a lookahead.
        m_szFilename = _szFilename; // For error reporting and general debugging. Of course we don't need to store this.
    }

    int Close()
    {
        if ( FFileMapped() )
        {
            Assert( FFileOpened() );
            const void * pvMapped = m_pcpxBegin;
            size_t stMapped = StLenBytes();
            _tyBase::Close();
            _Unmap( pvMapped, stMapped );
        }
        if ( FFileOpened() )
        {
            int fd = m_fd;
            m_fd = 0;
            return _Close( fd );
        }
        Assert(!FOpened());
        return 0;
    }
    static int _Unmap( const void * _pvMapped, size_t _stMapped )
    {
        return munmap( const_cast< void * >( _pvMapped ), _stMapped );
    }
    static int _Close( int _fd )
    {
        return close( _fd );
    }

    // Attach to this FOpened() JsonLinuxInputStream.
    // We *could* use the base's impl for this but then the read cursor only see's the base's type and not the full type.
    // In this case that works fine but in the general case it might not. Anyway, it adds very little code to do it this way so no worries.
    void AttachReadCursor( _tyJsonReadCursor & _rjrc )
    {
        Assert( !_rjrc.FAttached() );
        Assert( FOpened() );
        _rjrc.AttachRoot( *this );
    }

    void SkipWhitespace()
    {
        return _tyBase::SkipWhitespace( m_szFilename.c_str() );
    }
    using _tyBase::PosGet;

    // Read a single character from the file - always throw on EOF.
    _tyChar ReadChar( const char * _pcEOFMessage )
    {
        return _tyBase::ReadChar( _pcEOFMessage, m_szFilename.c_str() );
    }
    
    bool FReadChar( _tyChar & _rtch, bool _fThrowOnEOF, const char * _pcEOFMessage )
    {
        return _tyBase::FReadChar( _rtch, _fThrowOnEOF, _pcEOFMessage, m_szFilename.c_str() );
    }

    using _tyBase::PushBackLastChar;
    std::strong_ordering ICompare( _tyThis const & _rOther ) const
    {
        return _tyBase::ICompare( _rOther );
    }

protected:
    using _tyBase::m_pcpxBegin;
    using _tyBase::m_pcpxCur;
    using _tyBase::m_pcpxEnd;
    using _tyBase::m_tcLookahead;
    using _tyBase::m_fHasLookahead;
    std::string m_szFilename;
    int m_fd{-1}; // file descriptor.
};

// JsonLinuxOutputStream: A class using open(), read(), etc.
template < class t_tyCharTraits, class t_tyPersistAsChar = typename t_tyCharTraits::_tyPersistAsChar >
class JsonLinuxOutputStream : public JsonOutputStreamBase< t_tyCharTraits, size_t >
{
    typedef JsonLinuxOutputStream _tyThis;
public:
    typedef t_tyCharTraits _tyCharTraits;
    typedef typename _tyCharTraits::_tyChar _tyChar;
    typedef t_tyPersistAsChar _tyPersistAsChar;
    typedef typename _tyCharTraits::_tyLPCSTR _tyLPCSTR;
    typedef size_t _tyFilePos;
    typedef JsonReadCursor< _tyThis > _tyJsonReadCursor;
    typedef JsonFormatSpec< _tyCharTraits > _tyJsonFormatSpec;

    JsonLinuxOutputStream() = default;
    ~JsonLinuxOutputStream()
    {
        if ( m_fOwnFdLifetime && FOpened() )
        {
            m_fOwnFdLifetime = false; // prevent reentry though we know it should never happen.
            (void)_Close( m_fd );
        }
    }

    void swap( JsonLinuxOutputStream & _r )
    {
        _r.m_szFilename.swap( m_szFilename );
        _r.m_szExceptionString.swap( m_szExceptionString );
        std::swap( _r.m_fd, m_fd );
        std::swap( _r.m_fOwnFdLifetime, m_fOwnFdLifetime );        
    }

    // This is a manner of indicating that something happened during streaming.
    // Since we use object destruction to finalize writes to a file and cannot throw out of a destructor.
    void SetExceptionString( const char * _szWhat )
    {
        m_szExceptionString = _szWhat;
    }
    void GetExceptionString( std::string & _rstrExceptionString )
    {
        _rstrExceptionString = m_szExceptionString;
    }

    bool FOpened() const
    {
        return -1 != m_fd;
    }

    // Throws on open failure. This object owns the lifetime of the file descriptor.
    void Open( const char * _szFilename )
    {
        Assert( !FOpened() );
        m_fd = open( _szFilename, O_WRONLY | O_CREAT | O_TRUNC, 0666 );
        if ( !FOpened() )
            THROWBADJSONSTREAMERRNO( errno, "JsonLinuxOutputStream::Open(): Unable to open() file [%s]", _szFilename );
        m_fOwnFdLifetime = true; // This object owns the lifetime of m_fd - ie. we need to close upon destruction.
        m_szFilename = _szFilename; // For error reporting and general debugging. Of course we don't need to store this.
    }

    // Attach to an FD whose lifetime we do not own. This can be used, for instance, to attach to stdout which is usually at FD 1 (unless reopen()ed).
    void AttachFd( int _fd )
    {
        Assert( !FOpened() );
        Assert( _fd != -1 );
        m_fd = _fd;
        m_fOwnFdLifetime = false; // This object owns the lifetime of m_fd - ie. we need to close upon destruction.
        m_szFilename.clear(); // No filename indicates we are attached to "some fd".
    }

    int Close()
    {
        if ( FOpened() )
        {
            int fd = m_fd;
            m_fd = 0;
            if ( m_fOwnFdLifetime )
                return _Close( fd );
        }
        return 0;
    }
    static int _Close( int _fd )
    {
        return close( _fd );
    }

    void WriteByteOrderMark() const
    {
        Assert( 0 == ::lseek( m_fd, 0, SEEK_CUR ) );
        uint8_t rgBOM[] = { 0xFF, 0xFE };
        ssize_t sstWrote = write( m_fd, &rgBOM, sizeof rgBOM );
        if ( sstWrote != sizeof rgBOM )
        {
            if ( -1 == sstWrote )
                THROWBADJSONSTREAMERRNO( errno, "JsonLinuxOutputStream::WriteChar(): write() failed for file [%s]", m_szFilename.c_str() );
            else
                THROWBADJSONSTREAM( "JsonLinuxOutputStream::WriteChar(): write() only wrote [%ld] bytes of [%ld] for file [%s]", sstWrote, sizeof rgBOM, m_szFilename.c_str() );
        }
    }

    void _CheckPersistedChar( _tyChar _tc )
    {
        if ( sizeof(_tyChar) != sizeof(_tyPersistAsChar) )
        {
            if ( !!( _tc & ~( ( 1u << ( sizeof(_tyPersistAsChar) * CHAR_BIT ) ) - 1 ) ) )
                THROWBADJSONSTREAM( "JsonLinuxOutputStream::_CheckPersistedChar(): Would lose information since high data of character gets trunctated, file [%s]", m_szFilename.c_str() );
        }
    }

    // Read a single character from the file - always throw on EOF.
    void WriteChar( _tyChar _tc )
    {
        Assert( FOpened() );
        _CheckPersistedChar( _tc );
        _tyPersistAsChar cpx = (_tyPersistAsChar)_tc;
        ssize_t sstWrote = write( m_fd, &cpx, sizeof cpx );
        if ( sstWrote != sizeof cpx )
        {
            if ( -1 == sstWrote )
                THROWBADJSONSTREAMERRNO( errno, "JsonLinuxOutputStream::WriteChar(): write() failed for file [%s]", m_szFilename.c_str() );
            else
                THROWBADJSONSTREAM( "JsonLinuxOutputStream::WriteChar(): write() only wrote [%ld] bytes of [%ld] for file [%s]", sstWrote, sizeof cpx, m_szFilename.c_str() );
        }
    }
    void WriteRawChars( _tyLPCSTR _psz, ssize_t _sstLen = -1 )
    {
        if ( _sstLen < 0 )
            _sstLen = _tyCharTraits::StrLen( _psz );
        if ( sizeof(_tyChar) != sizeof(_tyPersistAsChar) )
        {
            const size_t kstLenConvertBuf = 4096;
            _tyPersistAsChar rgcpxConvertBuf[ kstLenConvertBuf ];
            _tyLPCSTR const pszEnd = _psz + _sstLen;
            _tyLPCSTR  pszCur = _psz;
            for( ; pszEnd != pszCur; )
            {
                _tyPersistAsChar * pcpxCur = rgcpxConvertBuf;
                _tyPersistAsChar * const pcpxEnd = rgcpxConvertBuf + kstLenConvertBuf;
                for ( ; ( pcpxEnd != pcpxCur ) && ( pszEnd != pszCur ); ++pcpxCur, ++pszCur )
                {
                    if ( !!( *pszCur & ~( ( 1u << ( sizeof(_tyPersistAsChar) * CHAR_BIT ) ) - 1 ) ) )
                    {
                        unsigned int uCur = *pszCur;
                        unsigned int uVal = ( *pszCur & ~( ( 1u << ( sizeof(_tyPersistAsChar) * CHAR_BIT ) ) - 1 ) );
                        THROWBADJSONSTREAM( "JsonLinuxOutputStream::WriteRawChars(): Would lose information since high data of character gets trunctated uCur[%x], uVal[%x], file [%s]", uCur, uVal, m_szFilename.c_str() );
                    }
                    *pcpxCur = (_tyPersistAsChar)*pszCur;
                }
                ssize_t sstWrote = write( m_fd, rgcpxConvertBuf, ( pcpxCur - rgcpxConvertBuf ) * sizeof( _tyPersistAsChar ) );
                if ( sstWrote != ( pcpxCur - rgcpxConvertBuf ) * sizeof( _tyPersistAsChar ) )
                {
                    if ( -1 == sstWrote )
                        THROWBADJSONSTREAMERRNO( errno, "JsonLinuxOutputStream::WriteRawChars(): write() failed for file [%s]", m_szFilename.c_str() );
                    else
                        THROWBADJSONSTREAM( "JsonLinuxOutputStream::WriteRawChars(): write() only wrote [%ld] bytes of [%ld] for file [%s]", sstWrote,
                            ( pcpxCur - rgcpxConvertBuf ) * sizeof( _tyPersistAsChar ), m_szFilename.c_str() );
                }
            }
        }
        else
        {
            ssize_t sstWrote = write( m_fd, _psz, _sstLen );
            if ( sstWrote != _sstLen )
            {
                if ( -1 == sstWrote )
                    THROWBADJSONSTREAMERRNO( errno, "JsonLinuxOutputStream::WriteRawChars(): write() failed for file [%s]", m_szFilename.c_str() );
                else
                    THROWBADJSONSTREAM( "JsonLinuxOutputStream::WriteRawChars(): write() only wrote [%ld] bytes of [%ld] for file [%s]", sstWrote, _sstLen, m_szFilename.c_str() );
            }
        }
    }
    // If <_fEscape> then we escape all special characters when writing.
    void WriteString( bool _fEscape, _tyLPCSTR _psz, ssize_t _sstLen = -1, const _tyJsonFormatSpec * _pjfs = 0 )
    {
        static _tyLPCSTR s_szPrintableWhitespace = str_array_cast< _tyChar >( JSONSTRM_PrintableWhitespace );
        static _tyLPCSTR s_szEscapeStringChars = str_array_cast< _tyChar >( JSONSTRM_EscapeStringChars );
        Assert( FOpened() );
        Assert( !_pjfs || _fEscape ); // only pass formatting when we are escaping the string.
        size_t stLen = (size_t)_sstLen;
        if ( _sstLen < 0 )
            stLen = _tyCharTraits::StrLen( _psz );
        if ( !_fEscape ) // We shouldn't write such characters to strings so we had better know that we don't have any.
            Assert( stLen <= _tyCharTraits::StrCSpn(    _psz, _tyCharTraits::s_tcFirstUnprintableChar, _tyCharTraits::s_tcLastUnprintableChar+1, 
                                                        s_szEscapeStringChars ) );
        _tyLPCSTR pszPrintableWhitespaceAtEnd = _psz + stLen;    
        if ( _fEscape && !!_pjfs && !_pjfs->m_fEscapePrintableWhitespace && _pjfs->m_fEscapePrintableWhitespaceAtEndOfLine )
            pszPrintableWhitespaceAtEnd -= _tyCharTraits::StrRSpn( _psz, pszPrintableWhitespaceAtEnd, s_szPrintableWhitespace );

        // When we are escaping the string we write piecewise.
        _tyLPCSTR pszWrite = _psz;
        while ( !!stLen )
        {
            if ( pszWrite < pszPrintableWhitespaceAtEnd )
            {
                ssize_t sstLenWrite = stLen;
                if ( _fEscape )
                {
                    if ( !_pjfs || _pjfs->m_fEscapePrintableWhitespace ) // escape everything.
                        sstLenWrite = _tyCharTraits::StrCSpn( pszWrite,
                                                            _tyCharTraits::s_tcFirstUnprintableChar, _tyCharTraits::s_tcLastUnprintableChar+1, 
                                                            s_szEscapeStringChars );
                    else
                    {
                        // More complex escaping of whitespace is possible here:
                        _tyLPCSTR pszNoEscape = pszWrite;
                        for ( ; pszPrintableWhitespaceAtEnd != pszNoEscape; ++pszNoEscape )
                        {
                            _tyLPCSTR pszPrintableWS = s_szPrintableWhitespace;
                            for ( ; !!*pszPrintableWS; ++pszPrintableWS )
                            {
                                if ( *pszNoEscape == *pszPrintableWS )
                                    break;
                            }
                            if ( !!*pszPrintableWS )
                                continue; // found printable WS that we want to print.
                            if (    ( *pszNoEscape < _tyCharTraits::s_tcFirstUnprintableChar ) ||
                                    ( *pszNoEscape > _tyCharTraits::s_tcLastUnprintableChar ) )
                            {
                                _tyLPCSTR pszEscapeChar = s_szEscapeStringChars;
                                for ( ; !!*pszEscapeChar && ( *pszNoEscape != *pszEscapeChar ); ++pszEscapeChar )
                                    ;
                                if ( !!*pszEscapeChar )
                                    continue; // found printable WS that we want to print.
                            }
                        }
                        sstLenWrite = pszNoEscape - pszWrite;
                    }                                                                    
                }
                if ( sstLenWrite )
                {
                    WriteRawChars( pszWrite, sstLenWrite );
                    stLen -= sstLenWrite;
                    pszWrite += sstLenWrite;
                    if ( !stLen )
                        return; // the overwhelming case is we return here.
                }
                else
                    Assert( !!stLen ); // invariant.
            }
            // else otherwise we will escape all remaining characters.
            WriteChar( _tyCharTraits::s_tcBackSlash ); // use for error handling.
            // If we haven't written the whole string then we encountered a character we need to escape.
            switch( *pszWrite )
            {
            case _tyCharTraits::s_tcBackSpace:
                WriteChar( _tyCharTraits::s_tcb );
            break;
            case _tyCharTraits::s_tcFormFeed:
                WriteChar( _tyCharTraits::s_tcf );
            break;
            case _tyCharTraits::s_tcNewline:
                WriteChar( _tyCharTraits::s_tcn );
            break;
            case _tyCharTraits::s_tcCarriageReturn:
                WriteChar( _tyCharTraits::s_tcr );
            break;
            case _tyCharTraits::s_tcTab:
                WriteChar( _tyCharTraits::s_tct );
            break;
            case _tyCharTraits::s_tcBackSlash:
            case _tyCharTraits::s_tcDoubleQuote:
                WriteChar( *pszWrite );
            break;
            default:
                // Unprintable character between 0 and 31. We'll use the \uXXXX method for this character.
                WriteChar( _tyCharTraits::s_tcu );
                WriteChar( _tyCharTraits::s_tc0 );
                WriteChar( _tyCharTraits::s_tc0 );
                WriteChar( _tyChar( '0' + ( *pszWrite / 16 ) ) );
                WriteChar( _tyChar( ( *pszWrite % 16 ) < 10 ? ( '0' + ( *pszWrite % 16 ) ) : ( 'A' + ( *pszWrite % 16 ) - 10 ) ) );
            break;
            }
            ++pszWrite;
            --stLen;
        }
    }

protected:
    std::string m_szFilename;
    std::string m_szExceptionString;
    int m_fd{-1}; // file descriptor.
    bool m_fOwnFdLifetime{false}; // Should we close the FD upon destruction?
};

// JsonOutputMemMappedStream: A class using open(), read(), etc.
template < class t_tyCharTraits, class t_tyPersistAsChar = typename t_tyCharTraits::_tyPersistAsChar >
class JsonOutputMemMappedStream : public JsonOutputStreamBase< t_tyCharTraits, size_t >
{
    typedef JsonOutputMemMappedStream _tyThis;
public:
    typedef t_tyCharTraits _tyCharTraits;
    typedef typename _tyCharTraits::_tyChar _tyChar;
    typedef t_tyPersistAsChar _tyPersistAsChar;
    typedef typename _tyCharTraits::_tyLPCSTR _tyLPCSTR;
    typedef size_t _tyFilePos;
    typedef JsonReadCursor< _tyThis > _tyJsonReadCursor;
    typedef JsonFormatSpec< _tyCharTraits > _tyJsonFormatSpec;
    static const _tyFilePos s_knGrowFileByBytes = 65536*4;

    JsonOutputMemMappedStream() = default;
    ~JsonOutputMemMappedStream()
    {
        (void)Close();
    }

    void swap( JsonOutputMemMappedStream & _r )
    {
        _r.m_szFilename.swap( m_szFilename );
        _r.m_szExceptionString.swap( m_szExceptionString );
        std::swap( _r.m_fd, m_fd );
        std::swap( _r.m_pvMapped, m_pvMapped );
        std::swap( _r.m_cpxMappedCur, m_cpxMappedCur );
        std::swap( _r.m_cpxMappedEnd, m_cpxMappedEnd );
        std::swap( _r.m_stMapped, m_stMapped );
    }

    // This is a manner of indicating that something happened during streaming.
    // Since we use object destruction to finalize writes to a file and cannot throw out of a destructor.
    void SetExceptionString( const char * _szWhat )
    {
        m_szExceptionString = _szWhat;
    }
    void GetExceptionString( std::string & _rstrExceptionString )
    {
        _rstrExceptionString = m_szExceptionString;
    }

    bool FOpened() const
    {
        return FFileOpened() && FFileMapped();
    }
    bool FAnyOpened() const
    {
        return FFileOpened() || FFileMapped();
    }
    bool FFileOpened() const
    {
        return -1 != m_fd;
    }
    bool FFileMapped() const
    {
        return MAP_FAILED != m_pvMapped;
    }

    // Throws on open failure. This object owns the lifetime of the file descriptor.
    void Open( const char * _szFilename )
    {
        Assert( !FAnyOpened() );
        Close();
        m_fd = open( _szFilename, O_RDWR | O_CREAT | O_TRUNC, 0666 );
        if ( !FFileOpened() )
            THROWBADJSONSTREAMERRNO( errno, "JsonOutputMemMappedStream::Open(): Unable to open() file [%s]", _szFilename );
        // Set the initial size of the mapping to s_knGrowFileByBytes. Don't want to use ftruncate because that could be slow as it zeros all memory.
        _tyFilePos posEnd = lseek( m_fd, s_knGrowFileByBytes-1, SEEK_SET );
        if ( -1 == posEnd )
            THROWBADJSONSTREAMERRNO( errno, "JsonOutputMemMappedStream::Open(): Attempting to lseek() failed for [%s]", _szFilename );
        ssize_t stRet = write( m_fd, "Z", 1 ); // write a single byte to grow the file to s_knGrowFileByBytes.
        if ( -1 == stRet )
            THROWBADJSONSTREAMERRNO( errno, "JsonOutputMemMappedStream::Open(): Attempting to write() failed for [%s]", _szFilename );
        // No need to reset the file pointer to the beginning - and in fact we like it at the end in case someone were to actually try to write to it.
        m_pvMapped = mmap( 0, s_knGrowFileByBytes, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0 );
        if ( m_pvMapped == MAP_FAILED )
            THROWBADJSONSTREAMERRNO( errno, "JsonOutputMemMappedStream::Open(): mmap() failed for [%s]", _szFilename );
        m_stMapped = s_knGrowFileByBytes;
        m_cpxMappedCur = (_tyPersistAsChar*)m_pvMapped;
        m_cpxMappedEnd = m_cpxMappedCur + ( m_stMapped / sizeof(_tyPersistAsChar) );
        m_szFilename = _szFilename; // For error reporting and general debugging. Of course we don't need to store this.
    }

    int Close()
    {
        int iCloseRet = 0;
        if ( FFileMapped() )
        {
            Assert( FFileOpened() );
            // We need to truncate the file to m_cpxMappedCur - m_pvMapped bytes.
            // We shouldn't throw here so we will just log an error message and set m_szExceptionString.
            iCloseRet = ftruncate( m_fd, ( m_cpxMappedCur - (_tyPersistAsChar*)m_pvMapped ) * sizeof(_tyPersistAsChar) );
            if ( -1 == iCloseRet )
            {
                LOGSYSLOGERRNO( eslmtError, errno, "JsonOutputMemMappedStream::Close(): ftruncate failed." );
                m_szExceptionString = "JsonOutputMemMappedStream::Close(): ftruncate failed.";
            }
            void * pvMapped = m_pvMapped;
            m_pvMapped = MAP_FAILED;
            size_t stMapped = m_stMapped;
            m_stMapped = 0;
            int iUnmap = _Unmap( pvMapped, stMapped ); // We don't care if this fails pretty much.
            Assert( !iUnmap );
        }
        if ( FFileOpened() )
        {
            int fd = m_fd;
            m_fd = 0;
            int iRet = _Close( fd );
            Assert( !iRet );
            if ( -1 == iRet )
                iCloseRet = -1; // it may already be.
        }
        Assert(!FOpened());
        return iCloseRet;
    }
    static int _Unmap( void * _pvMapped, size_t _stMapped )
    {
        return munmap( _pvMapped, _stMapped );
    }
    static int _Close( int _fd )
    {
        return close( _fd );
    }

    void _CheckPersistedChar( _tyChar _tc )
    {
        if ( sizeof(_tyChar) != sizeof(_tyPersistAsChar) )
        {
            if ( !!( _tc & ~( ( 1u << ( sizeof(_tyPersistAsChar) * CHAR_BIT ) ) - 1 ) ) )
                THROWBADJSONSTREAM( "JsonOutputMemMappedStream::_CheckPersistedChar(): Would lose information since high data of character gets trunctated, file [%s]", m_szFilename.c_str() );
        }
    }
    
    // Read a single character from the file - always throw on EOF.
    void WriteChar( _tyChar _tc )
    {
        _CheckPersistedChar( _tc );
        _CheckGrowMap( 1 );
        *m_cpxMappedCur++ = _tc;
    }
    void WriteRawChars( _tyLPCSTR _psz, ssize_t _sstLen = -1 )
    {
        if ( !_sstLen )
            return;
        if ( _sstLen < 0 )
            _sstLen = _tyCharTraits::StrLen( _psz );
        _CheckGrowMap( (size_t)_sstLen );
        if ( sizeof(_tyChar) != sizeof(_tyPersistAsChar) )
        {
            const size_t kstLenConvertBuf = 4096;
            _tyPersistAsChar rgcpxConvertBuf[ kstLenConvertBuf ];
            _tyLPCSTR const pszEnd = _psz + _sstLen;
            _tyLPCSTR  pszCur = _psz;
            for( ; pszEnd != pszCur; )
            {
                _tyPersistAsChar * pcpxCur = rgcpxConvertBuf;
                _tyPersistAsChar * const pcpxEnd = rgcpxConvertBuf + kstLenConvertBuf;
                for ( ; ( pcpxEnd != pcpxCur ) && ( pszEnd != pszCur ); ++pcpxCur, ++pszCur )
                {
                    if ( !!( *pszCur & ~( ( 1u << ( sizeof(_tyPersistAsChar) * CHAR_BIT ) ) - 1 ) ) )
                        THROWBADJSONSTREAM( "JsonOutputMemMappedStream::WriteRawChars(): Would lose information since high data of character gets trunctated, file [%s]", m_szFilename.c_str() );
                    *pcpxCur = (_tyPersistAsChar)*pszCur;
                }
                memcpy( m_cpxMappedCur, rgcpxConvertBuf, ( pcpxCur - rgcpxConvertBuf ) * sizeof( _tyPersistAsChar ) );
                m_cpxMappedCur += ( pcpxCur - rgcpxConvertBuf );
            }
        }
        else
        {        
            memcpy( m_cpxMappedCur, _psz, _sstLen * sizeof(_tyChar) );
            m_cpxMappedCur += _sstLen;
        }
    }
    // If <_fEscape> then we escape all special characters when writing.
    void WriteString( bool _fEscape, _tyLPCSTR _psz, ssize_t _sstLen = -1, const _tyJsonFormatSpec * _pjfs = 0 )
    {
        static _tyLPCSTR s_szPrintableWhitespace = str_array_cast< _tyChar >( JSONSTRM_PrintableWhitespace );
        static _tyLPCSTR s_szEscapeStringChars = str_array_cast< _tyChar >( JSONSTRM_EscapeStringChars );
        Assert( FOpened() );
        Assert( !_pjfs || _fEscape ); // only pass formatting when we are escaping the string.
        if ( !_sstLen )
            return;
        size_t stLen = (size_t)_sstLen;
        if ( _sstLen < 0 )
            stLen = _tyCharTraits::StrLen( _psz );
        if ( !_fEscape ) // We shouldn't write such characters to strings so we had better know that we don't have any.
            Assert( stLen <= _tyCharTraits::StrCSpn(    _psz, _tyCharTraits::s_tcFirstUnprintableChar, _tyCharTraits::s_tcLastUnprintableChar+1, 
                                                        s_szEscapeStringChars ) );
        _tyLPCSTR pszPrintableWhitespaceAtEnd = _psz + stLen;    
        if ( _fEscape && !!_pjfs && !_pjfs->m_fEscapePrintableWhitespace && _pjfs->m_fEscapePrintableWhitespaceAtEndOfLine )
            pszPrintableWhitespaceAtEnd -= _tyCharTraits::StrRSpn( _psz, pszPrintableWhitespaceAtEnd, s_szPrintableWhitespace );

        // When we are escaping the string we write piecewise.
        _tyLPCSTR pszWrite = _psz;
        while ( !!stLen )
        {
            if ( pszWrite < pszPrintableWhitespaceAtEnd )
            {
                ssize_t sstLenWrite = stLen;
                if ( _fEscape )
                {
                    if ( !_pjfs || _pjfs->m_fEscapePrintableWhitespace ) // escape everything.
                        sstLenWrite = _tyCharTraits::StrCSpn( pszWrite,
                                                            _tyCharTraits::s_tcFirstUnprintableChar, _tyCharTraits::s_tcLastUnprintableChar+1, 
                                                            s_szEscapeStringChars );
                    else
                    {
                        // More complex escaping of whitespace is possible here:
                        _tyLPCSTR pszNoEscape = pszWrite;
                        for ( ; pszPrintableWhitespaceAtEnd != pszNoEscape; ++pszNoEscape )
                        {
                            _tyLPCSTR pszPrintableWS = s_szPrintableWhitespace;
                            for ( ; !!*pszPrintableWS; ++pszPrintableWS )
                            {
                                if ( *pszNoEscape == *pszPrintableWS )
                                    break;
                            }
                            if ( !!*pszPrintableWS )
                                continue; // found printable WS that we want to print.
                            if (    ( *pszNoEscape < _tyCharTraits::s_tcFirstUnprintableChar ) ||
                                    ( *pszNoEscape > _tyCharTraits::s_tcLastUnprintableChar ) )
                            {
                                _tyLPCSTR pszEscapeChar = s_szEscapeStringChars;
                                for ( ; !!*pszEscapeChar && ( *pszNoEscape != *pszEscapeChar ); ++pszEscapeChar )
                                    ;
                                if ( !!*pszEscapeChar )
                                    continue; // found printable WS that we want to print.
                            }
                        }
                        sstLenWrite = pszNoEscape - pszWrite;
                    }                                                                    
                }
                if ( sstLenWrite )
                {
                    WriteRawChars( pszWrite, sstLenWrite );
                    stLen -= sstLenWrite;
                    pszWrite += sstLenWrite;
                    if ( !stLen )
                        return; // the overwhelming case is we return here.
                }
                else
                    Assert( !!stLen ); // invariant.
            }
            // else otherwise we will escape all remaining characters.
            WriteChar( _tyCharTraits::s_tcBackSlash ); // use for error handling.
            // If we haven't written the whole string then we encountered a character we need to escape.
            switch( *pszWrite )
            {
            case _tyCharTraits::s_tcBackSpace:
                WriteChar( _tyCharTraits::s_tcb );
            break;
            case _tyCharTraits::s_tcFormFeed:
                WriteChar( _tyCharTraits::s_tcf );
            break;
            case _tyCharTraits::s_tcNewline:
                WriteChar( _tyCharTraits::s_tcn );
            break;
            case _tyCharTraits::s_tcCarriageReturn:
                WriteChar( _tyCharTraits::s_tcr );
            break;
            case _tyCharTraits::s_tcTab:
                WriteChar( _tyCharTraits::s_tct );
            break;
            case _tyCharTraits::s_tcBackSlash:
            case _tyCharTraits::s_tcDoubleQuote:
                WriteChar( *pszWrite );
            break;
            default:
                // Unprintable character between 0 and 31. We'll use the \uXXXX method for this character.
                WriteChar( _tyCharTraits::s_tcu );
                WriteChar( _tyCharTraits::s_tc0 );
                WriteChar( _tyCharTraits::s_tc0 );
                WriteChar( _tyChar( '0' + ( *pszWrite / 16 ) ) );
                WriteChar( _tyChar( ( *pszWrite % 16 ) < 10 ? ( '0' + ( *pszWrite % 16 ) ) : ( 'A' + ( *pszWrite % 16 ) - 10 ) ) );
            break;
            }
            ++pszWrite;
            --stLen;
        }
    }

protected:
    void _CheckGrowMap( size_t _stByAtLeast )
    {
        if ( ( m_cpxMappedEnd -  m_cpxMappedCur ) < _stByAtLeast )
            _GrowMap( _stByAtLeast );
    }
    void _GrowMap( size_t _stByAtLeast )
    {
        size_t stGrowBy = ( ( ( _stByAtLeast - 1 ) / s_knGrowFileByBytes ) + 1 ) * s_knGrowFileByBytes;
        void * pvOldMapping = m_pvMapped;
        m_pvMapped = MAP_FAILED;
        int iRet = munmap( pvOldMapping, m_stMapped );
        Assert( !iRet );
        m_stMapped += stGrowBy;
        iRet = lseek( m_fd, m_stMapped - 1, SEEK_SET );
        if ( -1 == iRet )
            THROWBADJSONSTREAMERRNO( errno, "JsonOutputMemMappedStream::_GrowMap(): lseek() failed for file [%s].", m_szFilename.c_str() );
        iRet = write( m_fd, "Z", 1 ); // just write a single byte to grow the file.
        if ( -1 == iRet )
            THROWBADJSONSTREAMERRNO( errno, "JsonOutputMemMappedStream::_GrowMap(): write() failed for file [%s].", m_szFilename.c_str() );
        m_pvMapped = mmap( 0, m_stMapped, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0 );
        if ( m_pvMapped == MAP_FAILED )
            THROWBADJSONSTREAMERRNO( errno, "JsonOutputMemMappedStream::_GrowMap(): mmap() failed for file [%s].", m_szFilename.c_str() );
        m_cpxMappedEnd = (_tyPersistAsChar*)m_pvMapped + ( m_stMapped / sizeof( _tyPersistAsChar ) );
        m_cpxMappedCur = (_tyPersistAsChar*)m_pvMapped + ( m_cpxMappedCur - (_tyPersistAsChar*)pvOldMapping );
    }

    std::string m_szFilename;
    std::string m_szExceptionString;
    void * m_pvMapped{MAP_FAILED};
    _tyPersistAsChar * m_cpxMappedCur{};
    _tyPersistAsChar * m_cpxMappedEnd{};
    size_t m_stMapped{};
    int m_fd{-1}; // file descriptor.
};

// This just writes to an in-memory stream which then can be done anything with.
template < class t_tyCharTraits, class t_tyPersistAsChar = typename t_tyCharTraits::_tyPersistAsChar, size_t t_kstBlockSize = 65536 >
class JsonOutputMemStream : public JsonOutputStreamBase< t_tyCharTraits, size_t >
{
    typedef JsonOutputMemStream _tyThis;
public:
    typedef t_tyCharTraits _tyCharTraits;
    typedef typename _tyCharTraits::_tyChar _tyChar;
    typedef t_tyPersistAsChar _tyPersistAsChar;
    typedef typename _tyCharTraits::_tyLPCSTR _tyLPCSTR;
    typedef size_t _tyFilePos;
    typedef JsonReadCursor< _tyThis > _tyJsonReadCursor;
    typedef JsonFormatSpec< _tyCharTraits > _tyJsonFormatSpec;
    typedef MemFileContainer< _tyFilePos, false > _tyMemFileContainer;
    typedef MemStream< _tyFilePos, false > _tyMemStream;

    JsonOutputMemStream()
    {
        _tyMemFileContainer mfcMemFile( t_kstBlockSize ); // This creates the MemFile but then it hands it off to the MemStream and is no longer necessary.
        mfcMemFile.OpenStream( m_msMemStream ); // get an empty stream ready for writing.
    }
    ~JsonOutputMemStream()
    {
    }

    void swap( JsonOutputMemStream & _r )
    {
        _r.m_szExceptionString.swap( m_szExceptionString );
        m_msMemStream.swap( _r.m_msMemStream );
    }

    size_t GetLengthChars() const
    {
        return m_msMemStream.GetEndPos() / sizeof(_tyPersistAsChar);
    }
    size_t GetLengthBytes() const
    {
        return m_msMemStream.GetEndPos();
    }

    const _tyMemStream & GetMemStream() const
    {
        return m_msMemStream;   
    }
    _tyMemStream & GetMemStream()
    {
        return m_msMemStream;   
    }

    // This is a manner of indicating that something happened during streaming.
    // Since we use object destruction to finalize writes to a file and cannot throw out of a destructor.
    void SetExceptionString( const char * _szWhat )
    {
        m_szExceptionString = _szWhat;
    }
    void GetExceptionString( std::string & _rstrExceptionString )
    {
        _rstrExceptionString = m_szExceptionString;
    }

    void _CheckPersistedChar( _tyChar _tc )
    {
        if ( sizeof(_tyChar) != sizeof(_tyPersistAsChar) )
        {
            if ( !!( _tc & ~( ( 1u << ( sizeof(_tyPersistAsChar) * CHAR_BIT ) ) - 1 ) ) )
                THROWBADJSONSTREAM( "JsonLinuxOutputStream::_CheckPersistedChar(): Would lose information since high data of character gets trunctated" );
        }
    }
    
    // Read a single character from the file - always throw on EOF.
    void WriteChar( _tyChar _tc )
    {
        _CheckPersistedChar( _tc );
        _tyPersistAsChar cpx = _tc;
        ssize_t sstWrote = m_msMemStream.Write( &cpx, sizeof cpx );
        if ( -1 == sstWrote )
            THROWBADJSONSTREAMERRNO( errno, "JsonOutputMemStream::WriteChar(): write() failed for MemFile." );
        Assert( sstWrote == sizeof cpx );
    }
    void WriteRawChars( _tyLPCSTR _psz, ssize_t _sstLen = -1 )
    {
        if ( _sstLen < 0 )
            _sstLen = _tyCharTraits::StrLen( _psz );
        if ( sizeof(_tyChar) != sizeof(_tyPersistAsChar) )
        {
            const size_t kstLenConvertBuf = 4096;
            _tyPersistAsChar rgcpxConvertBuf[ kstLenConvertBuf ];
            _tyLPCSTR const pszEnd = _psz + _sstLen;
            _tyLPCSTR  pszCur = _psz;
            for( ; pszEnd != pszCur; )
            {
                _tyPersistAsChar * pcpxCur = rgcpxConvertBuf;
                _tyPersistAsChar * const pcpxEnd = rgcpxConvertBuf + kstLenConvertBuf;
                for ( ; ( pcpxEnd != pcpxCur ) && ( pszEnd != pszCur ); ++pcpxCur, ++pszCur )
                {
                    if ( !!( *pszCur & ~( ( 1u << ( sizeof(_tyPersistAsChar) * CHAR_BIT ) ) - 1 ) ) )
                        THROWBADJSONSTREAM( "JsonOutputMemStream::WriteRawChars(): Would lose information since high data of character gets trunctated." );
                    *pcpxCur = (_tyPersistAsChar)*pszCur;
                }
                ssize_t sstWrote = m_msMemStream.Write( rgcpxConvertBuf, ( pcpxCur - rgcpxConvertBuf ) * sizeof( _tyPersistAsChar ) );
                if ( -1 == sstWrote )
                    THROWBADJSONSTREAMERRNO( errno, "JsonOutputMemStream::WriteRawChars(): write() failed for MemFile." );
                Assert( sstWrote == ( pcpxCur - rgcpxConvertBuf ) * sizeof( _tyPersistAsChar ) ); // else we would have thrown.
            }
        }
        else
        {        
            ssize_t sstWrote = m_msMemStream.Write( _psz, _sstLen * sizeof(_tyChar) );
            if ( -1 == sstWrote )
                THROWBADJSONSTREAMERRNO( errno, "JsonOutputMemStream::WriteRawChars(): write() failed for MemFile." );
            Assert( sstWrote == _sstLen * sizeof(_tyChar) ); // else we would have thrown.
        }
    }
    // If <_fEscape> then we escape all special characters when writing.
    void WriteString( bool _fEscape, _tyLPCSTR _psz, ssize_t _sstLen = -1, const _tyJsonFormatSpec * _pjfs = 0 )
    {
        static _tyLPCSTR s_szPrintableWhitespace = str_array_cast< _tyChar >( JSONSTRM_PrintableWhitespace );
        static _tyLPCSTR s_szEscapeStringChars = str_array_cast< _tyChar >( JSONSTRM_EscapeStringChars );
        Assert( !_pjfs || _fEscape ); // only pass formatting when we are escaping the string.
        size_t stLen = (size_t)_sstLen;
        if ( _sstLen < 0 )
            stLen = _tyCharTraits::StrLen( _psz );
        if ( !_fEscape ) // We shouldn't write such characters to strings so we had better know that we don't have any.
            Assert( stLen <= _tyCharTraits::StrCSpn(    _psz, _tyCharTraits::s_tcFirstUnprintableChar, _tyCharTraits::s_tcLastUnprintableChar+1, 
                                                        s_szEscapeStringChars ) );
        _tyLPCSTR pszPrintableWhitespaceAtEnd = _psz + stLen;    
        if ( _fEscape && !!_pjfs && !_pjfs->m_fEscapePrintableWhitespace && _pjfs->m_fEscapePrintableWhitespaceAtEndOfLine )
            pszPrintableWhitespaceAtEnd -= _tyCharTraits::StrRSpn( _psz, pszPrintableWhitespaceAtEnd, s_szPrintableWhitespace );

        // When we are escaping the string we write piecewise.
        _tyLPCSTR pszWrite = _psz;
        while ( !!stLen )
        {
            if ( pszWrite < pszPrintableWhitespaceAtEnd )
            {
                ssize_t sstLenWrite = stLen;
                if ( _fEscape )
                {
                    if ( !_pjfs || _pjfs->m_fEscapePrintableWhitespace ) // escape everything.
                        sstLenWrite = _tyCharTraits::StrCSpn( pszWrite,
                                                            _tyCharTraits::s_tcFirstUnprintableChar, _tyCharTraits::s_tcLastUnprintableChar+1, 
                                                            s_szEscapeStringChars );
                    else
                    {
                        // More complex escaping of whitespace is possible here:
                        _tyLPCSTR pszNoEscape = pszWrite;
                        for ( ; pszPrintableWhitespaceAtEnd != pszNoEscape; ++pszNoEscape )
                        {
                            _tyLPCSTR pszPrintableWS = s_szPrintableWhitespace;
                            for ( ; !!*pszPrintableWS; ++pszPrintableWS )
                            {
                                if ( *pszNoEscape == *pszPrintableWS )
                                    break;
                            }
                            if ( !!*pszPrintableWS )
                                continue; // found printable WS that we want to print.
                            if (    ( *pszNoEscape < _tyCharTraits::s_tcFirstUnprintableChar ) ||
                                    ( *pszNoEscape > _tyCharTraits::s_tcLastUnprintableChar ) )
                            {
                                _tyLPCSTR pszEscapeChar = s_szEscapeStringChars;
                                for ( ; !!*pszEscapeChar && ( *pszNoEscape != *pszEscapeChar ); ++pszEscapeChar )
                                    ;
                                if ( !!*pszEscapeChar )
                                    continue; // found printable WS that we want to print.
                            }
                        }
                        sstLenWrite = pszNoEscape - pszWrite;
                    }                                                                    
                }
                ssize_t sstWrote = 0;
                if ( sstLenWrite )
                {
                    WriteRawChars( pszWrite, sstLenWrite );
                    stLen -= sstLenWrite;
                    pszWrite += sstLenWrite;
                    if ( !stLen )
                        return; // the overwhelming case is we return here.
                }
                else
                    Assert( !!stLen ); // invariant.
            }
            // else otherwise we will escape all remaining characters.
            WriteChar( _tyCharTraits::s_tcBackSlash ); // use for error handling.
            // If we haven't written the whole string then we encountered a character we need to escape.
            switch( *pszWrite )
            {
            case _tyCharTraits::s_tcBackSpace:
                WriteChar( _tyCharTraits::s_tcb );
            break;
            case _tyCharTraits::s_tcFormFeed:
                WriteChar( _tyCharTraits::s_tcf );
            break;
            case _tyCharTraits::s_tcNewline:
                WriteChar( _tyCharTraits::s_tcn );
            break;
            case _tyCharTraits::s_tcCarriageReturn:
                WriteChar( _tyCharTraits::s_tcr );
            break;
            case _tyCharTraits::s_tcTab:
                WriteChar( _tyCharTraits::s_tct );
            break;
            case _tyCharTraits::s_tcBackSlash:
            case _tyCharTraits::s_tcDoubleQuote:
                WriteChar( *pszWrite );
            break;
            default:
                // Unprintable character between 0 and 31. We'll use the \uXXXX method for this character.
                WriteChar( _tyCharTraits::s_tcu );
                WriteChar( _tyCharTraits::s_tc0 );
                WriteChar( _tyCharTraits::s_tc0 );
                WriteChar( _tyChar( '0' + ( *pszWrite / 16 ) ) );
                WriteChar( _tyChar( ( *pszWrite % 16 ) < 10 ? ( '0' + ( *pszWrite % 16 ) ) : ( 'A' + ( *pszWrite % 16 ) - 10 ) ) );
            break;
            }
            ++pszWrite;
            --stLen;
        }
    }
    int IWriteMemStreamToFile( int _fd, bool _fAllowThrows ) noexcept(false)
    {
        try
        {
            m_msMemStream.WriteToFd( _fd, 0 );
            return 0;
        }
        catch( std::exception const & rexc )
        {
            if ( _fAllowThrows )
                throw; // rethrow and let caller handle cuz he wants to.
            LOGSYSLOG( eslmtError, "IWriteMemStreamToFile(): Caught exception [%s].", rexc.what() );
            return -1;
        }
    }
protected:
    std::string m_szExceptionString;
    _tyMemStream m_msMemStream;
};

// This writes to a memstream during the write and then writes the OS file on close.
template < class t_tyCharTraits, class t_tyPersistAsChar = typename t_tyCharTraits::_tyPersistAsChar, size_t t_kstBlockSize = 65536 >
class JsonLinuxOutputMemStream : protected JsonOutputMemStream< t_tyCharTraits, t_tyPersistAsChar, t_kstBlockSize >
{
    typedef JsonLinuxOutputMemStream _tyThis;
    typedef JsonOutputMemStream< t_tyCharTraits, t_tyPersistAsChar, t_kstBlockSize > _tyBase;
public:
    typedef t_tyCharTraits _tyCharTraits;
    typedef typename _tyCharTraits::_tyChar _tyChar;
    typedef t_tyPersistAsChar _tyPersistAsChar;
    typedef typename _tyCharTraits::_tyLPCSTR _tyLPCSTR;
    typedef size_t _tyFilePos;
    typedef JsonReadCursor< _tyThis > _tyJsonReadCursor;
    typedef JsonFormatSpec< _tyCharTraits > _tyJsonFormatSpec;

    JsonLinuxOutputMemStream() = default;
    ~JsonLinuxOutputMemStream()  noexcept(false)
    {
        if ( FOpened() )
            (void)Close( !std::uncaught_exceptions() );
    }

    void swap( JsonLinuxOutputMemStream & _r )
    {
        _tyBase::swap( _r );
        _r.m_szFilename.swap( m_szFilename );
        std::swap( _r.m_fd, m_fd );
        std::swap( _r.m_fOwnFdLifetime, m_fOwnFdLifetime );        
    }

    // This is a manner of indicating that something happened during streaming.
    // Since we use object destruction to finalize writes to a file and cannot throw out of a destructor.
    void SetExceptionString( const char * _szWhat )
    {
        m_szExceptionString = _szWhat;
    }
    void GetExceptionString( std::string & _rstrExceptionString )
    {
        _rstrExceptionString = m_szExceptionString;
    }

    bool FOpened() const
    {
        return -1 != m_fd;
    }

    // Throws on open failure. This object owns the lifetime of the file descriptor.
    void Open( const char * _szFilename )
    {
        Assert( !FOpened() );
        m_fd = open( _szFilename, O_WRONLY | O_CREAT | O_TRUNC, 0666 );
        if ( !FOpened() )
            THROWBADJSONSTREAMERRNO( errno, "JsonLinuxOutputMemStream::Open(): Unable to open() file [%s]", _szFilename );
        m_fOwnFdLifetime = true; // This object owns the lifetime of m_fd - ie. we need to close upon destruction.
        m_szFilename = _szFilename; // For error reporting and general debugging. Of course we don't need to store this.
    }

    // Attach to an FD whose lifetime we do not own. This can be used, for instance, to attach to stdout which is usually at FD 1 (unless reopen()ed).
    void AttachFd( int _fd )
    {
        Assert( !FOpened() );
        Assert( _fd != -1 );
        m_fd = _fd;
        m_fOwnFdLifetime = false; // This object owns the lifetime of m_fd - ie. we need to close upon destruction.
        m_szFilename.clear(); // No filename indicates we are attached to "some fd".
    }

    using _tyBase::IWriteMemStreamToFile;
    int Close( bool _fAllowThrows = true )
    {
        if ( FOpened() )
        {
            int fd = m_fd;
            m_fd = 0;
            int iRetWrite = IWriteMemStreamToFile( fd, _fAllowThrows ); // Catches any exception when !_fAllowThrows otherwise throws through.
            if ( m_fOwnFdLifetime )
            {
                int iRetClose = _Close( fd );
                return !iRetWrite ? iRetClose : iRetWrite; // Return any non-zero as it indicates an error.
            }
        }
        return 0;
    }
    static int _Close( int _fd )
    {
        return close( _fd );
    }

    using _tyBase::WriteChar;
    using _tyBase::WriteRawChars;
    using _tyBase::WriteString;

protected:
    std::string m_szFilename;
    std::string m_szExceptionString;
    int m_fd{-1}; // file descriptor.
    bool m_fOwnFdLifetime{false}; // Should we close the FD upon destruction?
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
    ejvtFalse,
    ejvtNull,
    ejvtLastJsonSpecifiedValue = ejvtNull,
    ejvtEndOfObject, // This pseudo value type indicates that the iteration of the parent object has reached its end.
    ejvtEndOfArray, // This pseudo value type indicates that the iteration of the parent array has reached its end.
    ejvtJsonValueTypeCount
};
typedef _EJsonValueType EJsonValueType;

// class JsonValue:
// The base level object for a Json file is a JsonValue - i.e. a JSON file contains a JsonValue as its root object.
template < class t_tyCharTraits >
class JsonValue
{
    typedef JsonValue _tyThis;
public:
    using _tyCharTraits = t_tyCharTraits;
    using _tyChar = typename _tyCharTraits::_tyChar;
    using _tyStdStr = typename _tyCharTraits::_tyStdStr;
    using _tyJsonObject = JsonObject< t_tyCharTraits >;
    using _tyJsonArray = JsonArray< t_tyCharTraits >;
    using _tyJsonFormatSpec = JsonFormatSpec< t_tyCharTraits >;

    // Note that this JsonValue very well may be referred to by a parent JsonValue.
    // JsonValues must be destructed carefully.

    JsonValue& operator=( const JsonValue& ) = delete;

    JsonValue( JsonValue * _pjvParent = nullptr, EJsonValueType _jvtType = ejvtJsonValueTypeCount )
        :   m_pjvParent( _pjvParent ),
            m_jvtType( _jvtType )
    {
        Assert( !m_pvValue ); // Should have been zeroed by default member initialization on site.
    }
    JsonValue( JsonValue const & _r )
        :   m_pjvParent( _r.m_pjvParent ),
            m_jvtType( _r.m_jvtType )
    {
        if ( !!_r.m_pvValue ) // If the value is populated then copy it.
            _CreateValue( _r );
    }
    JsonValue( JsonValue && _rr )
    {
        swap( _rr );
    }
    ~JsonValue()
    {
        if ( m_pvValue )
            _DestroyValue();
    }

    // Take ownership of the value passed by the caller - just swap it in.
    void SetValue( JsonValue && _rr )
    {
        std::swap( _rr.m_jvtType, m_jvtType );
        std::swap( _rr.m_pvValue, m_pvValue );
    }
    JsonValue & operator = ( JsonValue && _rr )
    {
        if ( this != &_rr )
        {
            Destroy();
            swap( _rr );
        }
        return *this;
    }
    void swap( JsonValue & _r )
    {
        std::swap( _r.m_pjvParent, m_pjvParent );
        std::swap( _r.m_pvValue, m_pvValue );
        std::swap( _r.m_jvtType, m_jvtType );
    }
    void Destroy() // Destroy and make null.
    {
        if ( m_pvValue )
            _DestroyValue();
        m_pjvParent = 0;
        m_jvtType = ejvtJsonValueTypeCount;
        Assert( FIsNull() );
    }
    void DestroyValue() // Just destroy and nullify any associated dynamically allocated value.
    {
        if ( m_pvValue )
            _DestroyValue();
    }

    // Set that this value is at the end of the iteration according to <_fObject>.
    void SetEndOfIteration( bool _fObject )
    {
        DestroyValue();
        m_jvtType = _fObject ? ejvtEndOfObject : ejvtEndOfArray;
    }

    void SetPjvParent( const JsonValue * _pjvParent )
    {
        m_pjvParent = _pjvParent;
    }
    const JsonValue * PjvGetParent() const
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
        return !m_pjvParent && !m_pvValue && ( ejvtJsonValueTypeCount == m_jvtType );
    }
    bool FAtRootValue() const
    {
        return !PjvGetParent();
    }

    EJsonValueType JvtGetValueType() const
    {
        return m_jvtType;
    }
    void SetValueType( EJsonValueType _jvt )
    {
        m_jvtType = _jvt;
    }
    bool FAtAggregateValue() const
    {
        return ( m_jvtType == ejvtObject ) || ( m_jvtType == ejvtArray );
    }
    bool FAtObjectValue() const
    {
        return ( m_jvtType == ejvtObject );
    }
    bool FAtArrayValue() const
    {
        return ( m_jvtType == ejvtArray );
    }

    // We assume that we have sole use of this input stream - as any such sharing would need complex guarding otherwise.
    // We should never need to backtrack when reading from the stream - we may need to fast forward for sure but not backtrack.
    // As such we should always be right at the point we want to be within the file and should never need to seek backward to reach the point we need to.
    // This means that we shouldn't need to actually store any position context within the read cursor since the presumption is that we are always where we
    //  should be. We will store this info in debug and use this to ensure that we are where we think we should be.
    template < class t_tyInputStream, class t_tyJsonReadCursor >
    void AttachInputStream( t_tyInputStream & _ris, t_tyJsonReadCursor & _rjrc )
    {
        Assert( FIsNull() ); // We should be null here - we should be at the root and have no parent.
        // Set the parent to ourselves as a sanity check.
        m_pjvParent = this;
        _rjrc.AttachInputStream( _ris, this );
    }

    // Return the type of the upcoming value from the first character.
    static EJsonValueType GetJvtTypeFromChar( _tyChar _tc )
    {
        switch( _tc )
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

    template < class t_tyJsonOutputStream >
    void WriteSimpleValue( t_tyJsonOutputStream & _rjos, const _tyJsonFormatSpec * _pjfs = 0 ) const 
    {
        switch( m_jvtType )
        {
        case ejvtNumber:
        case ejvtString:
            Assert( !!m_pvValue ); // We expect this to be populated but we will gracefully fail.
            if ( ejvtString == m_jvtType )
                _rjos.WriteChar( _tyCharTraits::s_tcDoubleQuote );
            _rjos.WriteString( ejvtString == m_jvtType, PGetStringValue()->c_str(), PGetStringValue()->length(), ejvtString == m_jvtType ? _pjfs : 0 );
            if ( ejvtString == m_jvtType )
                _rjos.WriteChar( _tyCharTraits::s_tcDoubleQuote );
            break;
        case ejvtTrue:
            _rjos.WriteString( false, str_array_cast< _tyChar >("true"), 4 );
            break;
        case ejvtFalse:
            _rjos.WriteString( false, str_array_cast< _tyChar >("false"), 5 );
            break;
        case ejvtNull:
            _rjos.WriteString( false, str_array_cast< _tyChar >("null"), 4 );
            break;
        default:
            Assert( 0 );
            break;
        }
    }

    _tyJsonObject * PCreateJsonObject()
    {
        Assert( ejvtObject == m_jvtType );
        _CreateValue();
        return ( ejvtObject == m_jvtType ) ? (_tyJsonObject*)m_pvValue : 0;
    }
    _tyJsonObject * PGetJsonObject() const
    {
        Assert( ejvtObject == m_jvtType );
        if ( !m_pvValue )
            _CreateValue();
        return ( ejvtObject == m_jvtType ) ? (_tyJsonObject*)m_pvValue : 0;
    }
    _tyJsonArray * PCreateJsonArray()
    {
        Assert( ejvtArray == m_jvtType );
        _CreateValue();
        return ( ejvtArray == m_jvtType ) ? (_tyJsonArray*)m_pvValue : 0;
    }
    _tyJsonArray * PGetJsonArray() const
    {
        Assert( ejvtArray == m_jvtType );
        if ( !m_pvValue )
            _CreateValue();
        return ( ejvtArray == m_jvtType ) ? (_tyJsonArray*)m_pvValue : 0;
    }
    _tyStdStr * PCreateStringValue()
    {
        Assert( ( ejvtNumber == m_jvtType ) || ( ejvtString == m_jvtType ) );
        _CreateValue();
        return ( ( ejvtNumber == m_jvtType ) || ( ejvtString == m_jvtType ) ) ? (_tyStdStr*)m_pvValue : 0;
    }
    _tyStdStr * PGetStringValue() const
    {
        Assert( ( ejvtNumber == m_jvtType ) || ( ejvtString == m_jvtType ) );
        if ( !m_pvValue )
            _CreateValue();
        return ( ( ejvtNumber == m_jvtType ) || ( ejvtString == m_jvtType ) ) ? (_tyStdStr*)m_pvValue : 0;
    }

protected:
    void _DestroyValue()
    {
        void * pvValue = m_pvValue;
        m_pvValue = 0;
        _DestroyValue( pvValue );
    }
    void _DestroyValue( void * _pvValue )
    {
        // We need to destroy any connected objects:
        switch( m_jvtType )
        {
        case ejvtObject:
            delete (_tyJsonObject*)_pvValue;
            break;
        case ejvtArray:
            delete (_tyJsonArray*)_pvValue;
            break;
        case ejvtNumber:
        case ejvtString:
            delete (_tyStdStr*)_pvValue;
            break;
        case ejvtTrue:
        case ejvtFalse:
        case ejvtNull:
        default:
            Assert( 0 );
            break;
        }
    }
    void _CreateValue() const
    {
        Assert( !m_pvValue );
        // We need to create a connected value object depending on type:
        switch( m_jvtType )
        {
        case ejvtObject:
            m_pvValue = new _tyJsonObject( this );
            break;
        case ejvtArray:
            m_pvValue = new _tyJsonArray( this );
            break;
        case ejvtNumber:
        case ejvtString:
            m_pvValue = new _tyStdStr();
            break;
        case ejvtTrue:
        case ejvtFalse:
        case ejvtNull:
        default:
            Assert( 0 ); // Shouldn't be calling for cases for which there is no value.
            break;
        }
    }
    // Create a new value that is a copy of from the passed JsonValue.
    void _CreateValue( JsonValue const & _r ) const
    {
        Assert( !m_pvValue );
        Assert( m_jvtType == _r.m_jvtType ); // handled by caller.
        Assert( !!_r.m_pvValue );

        // We need to create a connected value object depending on type:
        switch( m_jvtType )
        {
        case ejvtObject:
            m_pvValue = new _tyJsonObject( *_r.PGetJsonObject() );
            break;
        case ejvtArray:
            m_pvValue = new _tyJsonArray( *_r.PGetJsonArray() );
            break;
        case ejvtNumber:
        case ejvtString:
            m_pvValue = new _tyStdStr( *_r.PGetStringValue() );
            break;
        case ejvtTrue:
        case ejvtFalse:
        case ejvtNull:
        default:
            Assert( 0 ); // Shouldn't be calling for cases for which there is no value.
            break;
        }
    }

    const JsonValue * m_pjvParent{}; // We make this const and then const_cast<> as needed.
    union
    {
        mutable void * m_pvValue{};
        mutable _tyStdStr * m_pstrValue;
        mutable _tyJsonObject * m_pjoValue;
        mutable _tyJsonArray * m_pjaValue;
    };
    
    //mutable void * m_pvValue{}; // Just use void* since all our values are pointers to objects.
    // m_pvValue is one of:
    // for ejvtTrue, ejvtFalse, ejvtNull: nullptr
    // for ejvtObject: JsonObject *
    // for ejvtArray: JsonArray *
    // for ejvtNumber, ejvtString: _tyStdStr *
    EJsonValueType m_jvtType{ejvtJsonValueTypeCount}; // Type of the JsonValue.
};

// JsonFormatSpec:
// Formatting specifications for pretty printing JSON files.
template < class t_tyCharTraits >
class JsonFormatSpec
{
    typedef JsonFormatSpec _tyThis;
public:
    typedef t_tyCharTraits _tyCharTraits;
    typedef typename t_tyCharTraits::_tyChar _tyChar;

    template < class t_tyJsonOutputStream >
    void WriteLinefeed( t_tyJsonOutputStream & _rjos ) const
    {
        if ( m_fUseCRLF )
            _rjos.WriteRawChars( _tyCharTraits::s_szCRLF );
        else
            _rjos.WriteChar( _tyCharTraits::s_tcNewline );
    }

    template < class t_tyJsonOutputStream >
    void WriteWhitespaceIndent( t_tyJsonOutputStream & _rjos, unsigned int _nLevel ) const
    {
        Assert( _nLevel );
        if ( _nLevel )
        {
            // Prevent crashing:
            unsigned int nIndent = _nLevel * m_nWhitespacePerIndent;
            if ( nIndent > m_nMaximumIndent )
                nIndent = m_nMaximumIndent;
            _tyChar * pcSpace = (_tyChar*)alloca( nIndent * sizeof(_tyChar) );
            _tyCharTraits::MemSet( pcSpace, m_fUseTabs ? _tyCharTraits::s_tcTab : _tyCharTraits::s_tcSpace, nIndent );
            _rjos.WriteRawChars( pcSpace, nIndent );
        }
    }
    template < class t_tyJsonOutputStream >
    void WriteSpace( t_tyJsonOutputStream & _rjos ) const
    {
        _rjos.WriteChar( _tyCharTraits::s_tcSpace );
    }

    JsonFormatSpec() = default;
    JsonFormatSpec( JsonFormatSpec const & _r ) = default;

    // Use tabs or spaces.
    bool m_fUseTabs{false};
    // The number of tabs or spaces to use for each indentation.
    unsigned int m_nWhitespacePerIndent{4};
    // The maximum indentation to keep things from getting out of view.
    unsigned int m_nMaximumIndent{60};
    // Should be use CR+LF (Windows style) for EOL?
    bool m_fUseCRLF{false};
    // Should we use escape sequences for newlines, carriage returns and tabs:
    bool m_fEscapePrintableWhitespace{true}; // Note that setting this to false violates the JSON standard - but it is useful for reading files sometimes.
    // Should we escape such whitespace when it appears at the end of a value or the whole value is whitespace?
    bool m_fEscapePrintableWhitespaceAtEndOfLine{true};
};

// JsonValueLife:
// This class is used to write JSON files.
template < class t_tyJsonOutputStream >
class JsonValueLife
{
    typedef JsonValueLife _tyThis;
public:
    typedef t_tyJsonOutputStream _tyJsonOutputStream;
    typedef typename t_tyJsonOutputStream::_tyCharTraits _tyCharTraits;
    typedef typename _tyCharTraits::_tyChar _tyChar;
    typedef typename _tyCharTraits::_tyLPCSTR _tyLPCSTR;
    typedef typename _tyCharTraits::_tyStdStr _tyStdStr;
    typedef JsonValue< _tyCharTraits > _tyJsonValue;
    typedef JsonFormatSpec< _tyCharTraits > _tyJsonFormatSpec;

    JsonValueLife() = delete; // Must have a reference to the JsonOutputStream - though we may change this later.
    JsonValueLife( JsonValueLife const & ) = delete;
    JsonValueLife & operator=( JsonValueLife const & ) = delete;

    JsonValueLife( JsonValueLife && _rr, JsonValueLife * _pjvlParent = 0 )
        :   m_rjos( _rr.m_rjos ),
            m_jv( std::move( _rr.m_jv ) ),
            m_pjvlParent( !_pjvlParent ? _rr.m_pjvlParent : _pjvlParent ),
            m_nCurAggrLevel( _rr.m_nCurAggrLevel ),
            m_nSubValuesWritten( _rr.m_nSubValuesWritten ),
            m_optJsonFormatSpec( _rr.m_optJsonFormatSpec )
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

    JsonValueLife( t_tyJsonOutputStream & _rjos, EJsonValueType _jvt, const _tyJsonFormatSpec * _pjfs = 0 )
        :   m_rjos( _rjos ),
            m_jv( nullptr, _jvt )
    {
        if ( !!_pjfs )
            m_optJsonFormatSpec.emplace( *_pjfs );
        _WritePreamble( _jvt );
    }
    JsonValueLife( JsonValueLife & _jvl, EJsonValueType _jvt )
        :   m_rjos( _jvl.m_rjos ),
            m_pjvlParent( &_jvl ),
            m_nCurAggrLevel( _jvl.m_nCurAggrLevel+1 ),
            m_jv( &_jvl.RJvGet(), _jvt ), // Link to parent JsonValue.
            m_optJsonFormatSpec( _jvl.m_optJsonFormatSpec )
    {
        _WritePreamble( _jvt );
    }
    void _WritePreamble( EJsonValueType _jvt )
    {
        if ( m_pjvlParent && !!m_pjvlParent->NSubValuesWritten() ) // Write a comma if we aren't the first sub value.
            m_rjos.WriteChar( _tyCharTraits::s_tcComma );
        if ( !!m_optJsonFormatSpec && !!NCurAggrLevel() )
        {
            m_optJsonFormatSpec->WriteLinefeed( m_rjos );
            m_optJsonFormatSpec->WriteWhitespaceIndent( m_rjos, NCurAggrLevel() );
        }
        // For aggregate types we will need to write something to the output stream right away.
        if ( ejvtObject == _jvt )
            m_rjos.WriteChar( _tyCharTraits::s_tcLeftCurlyBr );
        else
        if ( ejvtArray == _jvt )
            m_rjos.WriteChar( _tyCharTraits::s_tcLeftSquareBr );
    }
    JsonValueLife( JsonValueLife & _jvl, _tyLPCSTR _pszKey, EJsonValueType _jvt )
        :   m_rjos( _jvl.m_rjos ),
            m_pjvlParent( &_jvl ),
            m_nCurAggrLevel( _jvl.m_nCurAggrLevel+1 ),
            m_jv( &_jvl.RJvGet(), _jvt ), // Link to parent JsonValue.
            m_optJsonFormatSpec( _jvl.m_optJsonFormatSpec )
    {
        _WritePreamble( _pszKey, _jvt );
    }
    void _WritePreamble( _tyLPCSTR _pszKey, EJsonValueType _jvt )
    {
        if ( m_pjvlParent && !!m_pjvlParent->NSubValuesWritten() ) // Write a comma if we aren't the first sub value.
            m_rjos.WriteChar( _tyCharTraits::s_tcComma );
        if ( !!m_optJsonFormatSpec )
        {
            if ( NCurAggrLevel() )
            {
                m_optJsonFormatSpec->WriteLinefeed( m_rjos );
                m_optJsonFormatSpec->WriteWhitespaceIndent( m_rjos, NCurAggrLevel() );
            }
        }
        m_rjos.WriteChar( _tyCharTraits::s_tcDoubleQuote );
        m_rjos.WriteString( true, _pszKey );
        m_rjos.WriteChar( _tyCharTraits::s_tcDoubleQuote );
        m_rjos.WriteChar( _tyCharTraits::s_tcColon );
        
        if ( !!m_optJsonFormatSpec )
            m_optJsonFormatSpec->WriteSpace( m_rjos );

        // For aggregate types we will need to write something to the output stream right away.
        if ( ejvtObject == _jvt )
            m_rjos.WriteChar( _tyCharTraits::s_tcLeftCurlyBr );
        else
        if ( ejvtArray == _jvt )
            m_rjos.WriteChar( _tyCharTraits::s_tcLeftSquareBr );
    }
    ~JsonValueLife() noexcept(false) // We might throw from here.
    {
        // If we within a stack unwinding due to another exception then we don't want to throw, otherwise we do.
        bool fInUnwinding = !!std::uncaught_exceptions();
        if ( fInUnwinding )
            return; // We can't be sure of our state and indeed we were seeing this during testing with purposefully corrupt files - which would lead to an AV.
        if ( !FDontWritePostAmble() )
            _WritePostamble(); // The postamble is written here for all objects.
    }
    void _WritePostamble()
    {
        if ( !!m_nSubValuesWritten && !!m_optJsonFormatSpec )
        {
            Assert( ( ejvtObject == m_jv.JvtGetValueType() ) || ( ejvtArray == m_jv.JvtGetValueType() ) );
            m_optJsonFormatSpec->WriteLinefeed( m_rjos );
            if ( NCurAggrLevel() )
                m_optJsonFormatSpec->WriteWhitespaceIndent( m_rjos, NCurAggrLevel() );
        }
        if ( ejvtObject == m_jv.JvtGetValueType() )
        {
            m_rjos.WriteChar( _tyCharTraits::s_tcRightCurlyBr );
        }
        else
        if ( ejvtArray == m_jv.JvtGetValueType() )
        {
            m_rjos.WriteChar( _tyCharTraits::s_tcRightSquareBr );
        }
        else
        {
            // Then we will write the simple value in m_jv. There should be a non-empty value there.
            m_jv.WriteSimpleValue( m_rjos, !m_optJsonFormatSpec ? 0 : &*m_optJsonFormatSpec );
        }
        if ( !!m_pjvlParent )
            m_pjvlParent->IncSubValuesWritten(); // We have successfully written a subobject.
    }

// Accessors:
    _tyJsonValue const & RJvGet() const { return m_jv; }
    _tyJsonValue & RJvGet() { return m_jv; }
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
    void SetValue( _tyJsonValue && _rrjvValue )
    {
        m_jv.SetValue( std::move( _rrjvValue ) );
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
    void WriteNullValue( _tyLPCSTR _pszKey )
    {
        Assert( FAtObjectValue() );
        if ( !FAtObjectValue() )
            THROWBADJSONSEMANTICUSE( "JsonValueLife::WriteNullValue(_pszKey): Writing a (key,value) pair to a non-object." );
        JsonValueLife jvlObjectElement( *this, _pszKey, ejvtNull );
    }
    void WriteBoolValue(  _tyLPCSTR _pszKey, bool _f )
    {
        Assert( FAtObjectValue() );
        if ( !FAtObjectValue() )
            THROWBADJSONSEMANTICUSE( "JsonValueLife::WriteBoolValue(_pszKey): Writing a (key,value) pair to a non-object." );
        JsonValueLife jvlObjectElement( *this, _pszKey, _f ? ejvtTrue : ejvtFalse );
    }
    void WriteValueType( _tyLPCSTR _pszKey, EJsonValueType _jvt )
    {
        switch( _jvt )
        {
            case ejvtNull:
            case ejvtTrue:
            case ejvtFalse:
            {
                Assert( FAtArrayValue() );
                if ( !FAtArrayValue() )
                    THROWBADJSONSEMANTICUSE( "JsonValueLife::WriteValueType(_pszKey): Writing a (key,value) pair to a non-object." );
                JsonValueLife jvlArrayElement( *this, _pszKey, _jvt );
            }
            break;
            default:
                THROWBADJSONSEMANTICUSE( "JsonValueLife::WriteValueType(_pszKey): This method only for null, true, or false." );
            break;
        }
    }

    void WriteStringValue( _tyLPCSTR _pszKey, _tyStdStr const & _rstrVal )
    {
        WriteStringValue( _pszKey, _rstrVal.c_str(), _rstrVal.length() );
    }
    void WriteStringValue( _tyLPCSTR _pszKey, _tyStdStr && _rrstrVal ) // take ownership of passed string.
    {
        _WriteValue( ejvtString, _pszKey, std::move( _rrstrVal ) );
    }
    void WriteStringValue( _tyLPCSTR _pszKey, _tyLPCSTR _pszValue, ssize_t _stLen = -1 )
    {
        if ( _stLen < 0 )
            _stLen = _tyCharTraits::StrLen( _pszValue );
        _WriteValue( ejvtString, _pszKey, _pszValue, (size_t)_stLen );
    }
    
    void WriteValue( _tyLPCSTR _pszKey, uint8_t _by )
    {
        _WriteValue( _pszKey, str_array_cast< _tyChar >( "%hhu" ), _by );
    }
    void WriteValue( _tyLPCSTR _pszKey, int8_t _sby )
    {
        _WriteValue( _pszKey, str_array_cast< _tyChar >( "%hhd" ), _sby );
    }
    void WriteValue( _tyLPCSTR _pszKey, uint16_t _us )
    {
        _WriteValue( _pszKey, str_array_cast< _tyChar >( "%hu" ), _us );
    }
    void WriteValue( _tyLPCSTR _pszKey, int16_t _ss )
    {
        _WriteValue( _pszKey, str_array_cast< _tyChar >( "%hd" ), _ss );
    }
    void WriteValue( _tyLPCSTR _pszKey, uint32_t _ui )
    {
        _WriteValue( _pszKey, str_array_cast< _tyChar >( "%u" ), _ui );
    }
    void WriteValue( _tyLPCSTR _pszKey, int32_t _si )
    {
        _WriteValue( _pszKey, str_array_cast< _tyChar >( "%d" ), _si );
    }
    void WriteValue( _tyLPCSTR _pszKey, uint64_t _ul )
    {
        _WriteValue( _pszKey, str_array_cast< _tyChar >( "%lu" ), _ul );
    }
    void WriteValue( _tyLPCSTR _pszKey, int64_t _sl )
    {
        _WriteValue( _pszKey, str_array_cast< _tyChar >( "%ld" ), _sl );
    }
    void WriteValue( _tyLPCSTR _pszKey, double _dbl )
    {
        _WriteValue( _pszKey, str_array_cast< _tyChar >( "%f" ), _dbl );
    }
    void WriteValue( _tyLPCSTR _pszKey, long double _ldbl )
    {
        _WriteValue( _pszKey, str_array_cast< _tyChar >( "%Lf" ), _ldbl );
    }
    void WriteTimeStringValue( _tyLPCSTR _pszKey, time_t const & _tt )
    {
        std::string strTime;
        n_TimeUtil::TimeToString( _tt, strTime );
        _WriteValue( ejvtString, _pszKey, std::move( strTime ) );
    }
    void WriteUuidStringValue( _tyLPCSTR _pszKey, uuid_t const & _uuidt )
    {
        uuid_string_t ustOut;
        uuid_unparse_lower( _uuidt, ustOut );
        _WriteValue( ejvtString, _pszKey, ustOut );
    }

// Arrray (value) operations:
    void WriteNullValue()
    {
        Assert( FAtArrayValue() );
        if ( !FAtArrayValue() )
            THROWBADJSONSEMANTICUSE( "JsonValueLife::WriteNullValue(): Writing a value to a non-array." );
        JsonValueLife jvlArrayElement( *this, ejvtNull );
    }
    void WriteBoolValue( bool _f )
    {
        Assert( FAtArrayValue() );
        if ( !FAtArrayValue() )
            THROWBADJSONSEMANTICUSE( "JsonValueLife::WriteBoolValue(): Writing a value to a non-array." );
        JsonValueLife jvlArrayElement( *this, _f ? ejvtTrue : ejvtFalse );
    }
    void WriteValueType( EJsonValueType _jvt )
    {
        switch( _jvt )
        {
            case ejvtNull:
            case ejvtTrue:
            case ejvtFalse:
            {
                Assert( FAtArrayValue() );
                if ( !FAtArrayValue() )
                    THROWBADJSONSEMANTICUSE( "JsonValueLife::WriteValueType(): Writing a value to a non-array." );
                JsonValueLife jvlArrayElement( *this, _jvt );
            }
            break;
            default:
                THROWBADJSONSEMANTICUSE( "JsonValueLife::WriteValueType(): This method only for null, true, or false." );
            break;
        }
    }

    void WriteStringValue( _tyLPCSTR _pszValue, ssize_t _stLen = -1 )
    {
        if ( _stLen < 0 )
            _stLen = _tyCharTraits::StrLen( _pszValue );
        _WriteValue( ejvtString, _pszValue, (size_t)_stLen );
    }
    template < class t_tyStr >
    void WriteStringValue( t_tyStr const & _rstrVal )
    {
        WriteStringValue( _rstrVal.c_str(), _rstrVal.length() );
    }
    template < class t_tyStr >
    void WriteStrOrNumValue( EJsonValueType _jvt, t_tyStr const & _rstrVal )
    {
        if ( ( ejvtString != _jvt ) && ( ejvtNumber != _jvt ) )
            THROWBADJSONSEMANTICUSE( "JsonValueLife::WriteStrOrNumValue(): This method only for numbers and strings." );
        _WriteValue( _jvt, _rstrVal.c_str(), _rstrVal.length() );
    }
    void WriteStringValue( _tyStdStr && _rrstrVal ) // take ownership of passed string.
    {
        _WriteValue( ejvtString, std::move( _rrstrVal ) );
    }
    void WriteTimeStringValue( time_t const & _tt )
    {
        std::string strTime;
        n_TimeUtil::TimeToString( _tt, strTime );
        _WriteValue( ejvtString, std::move( strTime ) );
    }
    void WriteUuidStringValue( uuid_t const & _uuidt )
    {
        uuid_string_t ustOut;
        uuid_unparse_lower( _uuidt, ustOut );
        _WriteValue( ejvtString, ustOut );
    }
    
    void WriteValue( uint8_t _by )
    {
        _WriteValue( str_array_cast< _tyChar >( "%hhu" ), _by );
    }
    void WriteValue( int8_t _sby )
    {
        _WriteValue( str_array_cast< _tyChar >( "%hhd" ), _sby );
    }
    void WriteValue( uint16_t _us )
    {
        _WriteValue( str_array_cast< _tyChar >( "%hu" ), _us );
    }
    void WriteValue( int16_t _ss )
    {
        _WriteValue( str_array_cast< _tyChar >( "%hd" ), _ss );
    }
    void WriteValue( uint32_t _ui )
    {
        _WriteValue( str_array_cast< _tyChar >( "%u" ), _ui );
    }
    void WriteValue( int32_t _si )
    {
        _WriteValue( str_array_cast< _tyChar >( "%d" ), _si );
    }
    void WriteValue( uint64_t _ul )
    {
        _WriteValue( str_array_cast< _tyChar >( "%lu" ), _ul );
    }
    void WriteValue( int64_t _sl )
    {
        _WriteValue( str_array_cast< _tyChar >( "%ld" ), _sl );
    }
    void WriteValue( double _dbl )
    {
        _WriteValue( str_array_cast< _tyChar >( "%lf" ), _dbl );
    }
    void WriteValue( long double _ldbl )
    {
        _WriteValue( str_array_cast< _tyChar >( "%Lf" ), _ldbl );
    }

protected:
    template < class t_tyNum >
    void _WriteValue( _tyLPCSTR _pszKey, _tyLPCSTR _pszFmt, t_tyNum _num )
    {
      const int knNum = 512;
      _tyChar rgcNum[ knNum ];
      int nPrinted = _tyCharTraits::Snprintf( rgcNum, knNum, _pszFmt, _num );
      Assert( nPrinted < knNum );
      _WriteValue( ejvtNumber, _pszKey, rgcNum, std::min( nPrinted, knNum-1 ) );
    }
    void _WriteValue( EJsonValueType _ejvt, _tyLPCSTR _pszKey, _tyLPCSTR _pszValue, size_t _stLen )
    {
        Assert( FAtObjectValue() );
        if ( !FAtObjectValue() )
            THROWBADJSONSEMANTICUSE( "JsonValueLife::_WriteValue(): Writing a (key,value) pair to a non-object." );
        Assert( _tyCharTraits::StrNLen( _pszValue, _stLen ) >= _stLen );
        JsonValueLife jvlObjectElement( *this, _pszKey, _ejvt );
        jvlObjectElement.RJvGet().PGetStringValue()->assign( _pszValue, _stLen );
    }
    void _WriteValue( EJsonValueType _ejvt, _tyLPCSTR _pszKey, _tyStdStr && _rrstrVal )
    {
        Assert( FAtObjectValue() );
        if ( !FAtObjectValue() )
            THROWBADJSONSEMANTICUSE( "JsonValueLife::_WriteValue(): Writing a (key,value) pair to a non-object." );
        JsonValueLife jvlObjectElement( *this, _pszKey, _ejvt );
        *jvlObjectElement.RJvGet().PCreateStringValue() = std::move( _rrstrVal );
    }

    template < class t_tyNum >
    void _WriteValue( _tyLPCSTR _pszFmt, t_tyNum _num )
    {
      const int knNum = 512;
      _tyChar rgcNum[ knNum ];
      int nPrinted = _tyCharTraits::Snprintf( rgcNum, knNum, _pszFmt, _num );
      Assert( nPrinted < knNum );
      _WriteValue( ejvtNumber, rgcNum, std::min( nPrinted, knNum-1 ) );
    }
    void _WriteValue( EJsonValueType _ejvt, _tyLPCSTR _pszValue, size_t _stLen )
    {
        Assert( FAtArrayValue() );
        if ( !FAtArrayValue() )
            THROWBADJSONSEMANTICUSE( "JsonValueLife::_WriteValue(): Writing a value to a non-array." );
        Assert( _tyCharTraits::StrNLen( _pszValue, _stLen ) >= _stLen );
        JsonValueLife jvlArrayElement( *this, _ejvt );
        jvlArrayElement.RJvGet().PGetStringValue()->assign( _pszValue, _stLen );
    }
    void _WriteValue( EJsonValueType _ejvt, _tyStdStr && _rrstrVal )
    {
        Assert( FAtArrayValue() );
        if ( !FAtArrayValue() )
            THROWBADJSONSEMANTICUSE( "JsonValueLife::_WriteValue(): Writing a value to a non-array." );
        JsonValueLife jvlArrayElement( *this, _ejvt );
        *jvlArrayElement.RJvGet().PCreateStringValue() = std::move( _rrstrVal );
    }

    _tyJsonOutputStream & m_rjos;
    std::optional< const _tyJsonFormatSpec > m_optJsonFormatSpec;
    JsonValueLife * m_pjvlParent{}; // If we are at the root this will be zero.
    _tyJsonValue m_jv;
    unsigned int m_nSubValuesWritten{}; // When a sub-value finishes its writing it will cause this number to be increment. This allows us to place commas correctly, etc.
    unsigned int m_nCurAggrLevel{};
};

// JsonValueLifeAbstractBase:
// This will contain a polymorphic JsonValueLife<SomeOutputType>.
// Then this object can be used with virtual methods in various scenarios - since templated member methods may not be virtual.
template < class t_tyChar >
class JsonValueLifeAbstractBase
{
    typedef JsonValueLifeAbstractBase _tyThis;
public:
    typedef t_tyChar _tyChar;
    typedef JsonCharTraits< _tyChar > _tyCharTraits;
    typedef JsonValue< _tyCharTraits > _tyJsonValue;
    typedef const _tyChar * _tyLPCSTR;

    JsonValueLifeAbstractBase() = default;
    virtual ~JsonValueLifeAbstractBase() noexcept(false) = default;

    virtual void NewSubValue( EJsonValueType _jvt, std::unique_ptr< _tyThis > & _rNewSubValue ) = 0;
    virtual void NewSubValue( _tyLPCSTR _pszKey, EJsonValueType _jvt, std::unique_ptr< _tyThis > & _rNewSubValue ) = 0;

    virtual _tyJsonValue const & RJvGet() const = 0;
    virtual _tyJsonValue & RJvGet() = 0;
    virtual EJsonValueType JvtGetValueType() const = 0;
    virtual bool FAtAggregateValue() const = 0;
    virtual bool FAtObjectValue() const = 0;
    virtual bool FAtArrayValue() const = 0;
    virtual void SetValue( _tyJsonValue && _rrjvValue ) = 0;
    virtual unsigned int NSubValuesWritten() const = 0;
    virtual unsigned int NCurAggrLevel() const = 0;

    virtual void WriteNullValue( _tyLPCSTR _pszKey ) = 0;
    virtual void WriteBoolValue(  _tyLPCSTR _pszKey, bool _f ) = 0;
    virtual void WriteValueType( _tyLPCSTR _pszKey, EJsonValueType _jvt ) = 0;
    virtual void WriteValue( _tyLPCSTR _pszKey, uint8_t _by ) = 0;
    virtual void WriteValue( _tyLPCSTR _pszKey, int8_t _sby ) = 0;
    virtual void WriteValue( _tyLPCSTR _pszKey, uint16_t _us ) = 0;
    virtual void WriteValue( _tyLPCSTR _pszKey, int16_t _ss ) = 0;
    virtual void WriteValue( _tyLPCSTR _pszKey, uint32_t _ui ) = 0;
    virtual void WriteValue( _tyLPCSTR _pszKey, int32_t _si ) = 0;
    virtual void WriteValue( _tyLPCSTR _pszKey, uint64_t _ul ) = 0;
    virtual void WriteValue( _tyLPCSTR _pszKey, int64_t _sl ) = 0;
    virtual void WriteValue( _tyLPCSTR _pszKey, double _dbl ) = 0;
    virtual void WriteValue( _tyLPCSTR _pszKey, long double _ldbl ) = 0;

    virtual void WriteNullValue() = 0;
    virtual void WriteBoolValue( bool _f ) = 0;
    virtual void WriteValueType( EJsonValueType _jvt ) = 0;
    virtual void WriteStringValue( _tyLPCSTR _pszValue, ssize_t _stLen = -1 ) = 0;
    template < class t_tyStr >
    void WriteStringValue( t_tyStr const & _rstrVal )
    {
        WriteStringValue( _rstrVal.c_str(), _rstrVal.length() );
    }
    virtual void WriteValue( uint8_t _by ) = 0;
    virtual void WriteValue( int8_t _sby ) = 0;
    virtual void WriteValue( uint16_t _us ) = 0;
    virtual void WriteValue( int16_t _ss ) = 0;
    virtual void WriteValue( uint32_t _ui ) = 0;
    virtual void WriteValue( int32_t _si ) = 0;
    virtual void WriteValue( uint64_t _ul ) = 0;
    virtual void WriteValue( int64_t _sl ) = 0;
    virtual void WriteValue( double _dbl ) = 0;
    virtual void WriteValue( long double _ldbl ) = 0;

    // TODO: Add a lot more virtual methods to implement various things.


};

template < class t_tyJsonOutputStream >
class JsonValueLifePoly : 
    public JsonValueLifeAbstractBase< typename t_tyJsonOutputStream::_tyChar >,
    public JsonValueLife< t_tyJsonOutputStream >
{
    typedef JsonValueLifeAbstractBase< typename t_tyJsonOutputStream::_tyChar > _tyAbstractBase;
    typedef JsonValueLife< t_tyJsonOutputStream > _tyBase;
    typedef JsonValueLifePoly _tyThis;
public:
    using typename _tyBase::_tyJsonValue; // Interesting if this inits the vtable correctly... hoping it will of course.
    using typename _tyBase::_tyLPCSTR;
    
    using JsonValueLife< t_tyJsonOutputStream >::JsonValueLife;

    // Construct the same type of JsonValueLife that we are... duh.
    void NewSubValue( EJsonValueType _jvt, std::unique_ptr< _tyAbstractBase > & _rNewSubValue ) override
    {
        _rNewSubValue.reset( new _tyThis( *this, _jvt ) );
    }
    void NewSubValue( _tyLPCSTR _pszKey, EJsonValueType _jvt, std::unique_ptr< _tyAbstractBase > & _rNewSubValue ) override
    {
        _rNewSubValue.reset( new _tyThis( *this, _pszKey, _jvt ) );
    }

    _tyJsonValue const & RJvGet() const override
    { 
        return _tyBase::RJvGet();
    }
    _tyJsonValue & RJvGet() override
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
    void SetValue( _tyJsonValue && _rrjvValue ) override
    {
        _tyBase::SetValue( std::move( _rrjvValue ) );
    }
    unsigned int NSubValuesWritten() const override
    {
        return _tyBase::NSubValuesWritten();
    }
    unsigned int NCurAggrLevel() const override
    {
        return _tyBase::NCurAggrLevel();
    }

    void WriteNullValue( _tyLPCSTR _pszKey ) override
    {
        _tyBase::WriteNullValue( _pszKey );
    }
    void WriteBoolValue(  _tyLPCSTR _pszKey, bool _f ) override
    {
        _tyBase::WriteBoolValue( _pszKey, _f );
    }
    void WriteValueType( _tyLPCSTR _pszKey, EJsonValueType _jvt ) override
    {
        _tyBase::WriteValueType( _pszKey, _jvt );
    }
    void WriteValue( _tyLPCSTR _pszKey, uint8_t _v ) override
    {
        _tyBase::WriteValue( _pszKey, _v );
    }
    void WriteValue( _tyLPCSTR _pszKey, int8_t _v ) override
    {
        _tyBase::WriteValue( _pszKey, _v );
    }
    void WriteValue( _tyLPCSTR _pszKey, uint16_t _v ) override
    {
        _tyBase::WriteValue( _pszKey, _v );
    }
    void WriteValue( _tyLPCSTR _pszKey, int16_t _v ) override
    {
        _tyBase::WriteValue( _pszKey, _v );
    }
    void WriteValue( _tyLPCSTR _pszKey, uint32_t _v ) override
    {
        _tyBase::WriteValue( _pszKey, _v );
    }
    void WriteValue( _tyLPCSTR _pszKey, int32_t _v ) override
    {
        _tyBase::WriteValue( _pszKey, _v );
    }
    void WriteValue( _tyLPCSTR _pszKey, uint64_t _v ) override
    {
        _tyBase::WriteValue( _pszKey, _v );
    }
    void WriteValue( _tyLPCSTR _pszKey, int64_t _v ) override
    {
        _tyBase::WriteValue( _pszKey, _v );
    }
    void WriteValue( _tyLPCSTR _pszKey, double _v ) override
    {
        _tyBase::WriteValue( _pszKey, _v );
    }
    void WriteValue( _tyLPCSTR _pszKey, long double _v ) override
    {
        _tyBase::WriteValue( _pszKey, _v );
    }

    void WriteNullValue() override
    {
        _tyBase::WriteNullValue();
    }
    void WriteBoolValue( bool _f ) override
    {
        _tyBase::WriteBoolValue( _f );
    }
    void WriteValueType( EJsonValueType _jvt ) override
    {
        _tyBase::WriteValueType( _jvt );
    }
    void WriteStringValue( _tyLPCSTR _pszValue, ssize_t _stLen = -1 ) override
    {
        _tyBase::WriteStringValue( _pszValue, _stLen );
    }
    void WriteValue( uint8_t _v ) override
    {
        _tyBase::WriteValue( _v );
    }
    void WriteValue( int8_t _v ) override
    {
        _tyBase::WriteValue( _v );
    }
    void WriteValue( uint16_t _v ) override
    {
        _tyBase::WriteValue( _v );
    }
    void WriteValue( int16_t _v ) override
    {
        _tyBase::WriteValue( _v );
    }
    void WriteValue( uint32_t _v ) override
    {
        _tyBase::WriteValue( _v );
    }
    void WriteValue( int32_t _v ) override
    {
        _tyBase::WriteValue( _v );
    }
    void WriteValue( uint64_t _v ) override
    {
        _tyBase::WriteValue( _v );
    }
    void WriteValue( int64_t _v ) override
    {
        _tyBase::WriteValue( _v );
    }
    void WriteValue( double _v ) override
    {
        _tyBase::WriteValue( _v );
    }
    void WriteValue( long double _v ) override
    {
        _tyBase::WriteValue( _v );
    }


};

// JsonAggregate:
// This may be JsonObject or JsonArray
template < class t_tyCharTraits >
class JsonAggregate
{
    typedef JsonAggregate _tyThis;
public:
    typedef JsonValue< t_tyCharTraits > _tyJsonValue;

    JsonAggregate( const _tyJsonValue * _pjvParent )
    {
        m_jvCur.SetPjvParent( _pjvParent ); // Link the contained value to the parent value. This is a soft reference - we assume any parent will always exist.
    }
    JsonAggregate( const JsonAggregate & _r ) = default; // standard copy constructor works.

    const _tyJsonValue & RJvGet() const
    {
        return m_jvCur;
    }
     _tyJsonValue & RJvGet()
    {
        return m_jvCur;
    }

    int NElement() const
    {
        return m_nObjectOrArrayElement;
    }
    void SetNElement( int _nObjectOrArrayElement ) const
    {
        m_nObjectOrArrayElement = _nObjectOrArrayElement;
    }
    void IncElement()
    {
        ++m_nObjectOrArrayElement;
    }

    void SetEndOfIteration( bool _fObject )
    {
        m_jvCur.SetEndOfIteration( _fObject );
    }
protected:
    _tyJsonValue m_jvCur; // The current JsonValue for this object.
    int m_nObjectOrArrayElement{}; // The index of the object or array that this context's value corresponds to.
};

// class JsonObject:
// This represents an object containing key:value pairs.
template < class t_tyCharTraits >
class JsonObject : protected JsonAggregate< t_tyCharTraits > // use protected because we don't want a virtual destructor.
{
    typedef JsonAggregate< t_tyCharTraits > _tyBase;
    typedef JsonObject _tyThis;
public:
    using _tyCharTraits = t_tyCharTraits;
    typedef JsonValue< t_tyCharTraits > _tyJsonValue;
    using _tyStdStr = typename _tyCharTraits::_tyStdStr;

    JsonObject( const _tyJsonValue * _pjvParent )
        : _tyBase( _pjvParent )
    {
    }
    JsonObject( JsonObject const & _r ) = default;
    using _tyBase::RJvGet;
    using _tyBase::NElement;
    using _tyBase::SetNElement;
    using _tyBase::IncElement;

    const _tyStdStr & RStrKey() const
    {
        return m_strCurKey;
    }
    void GetKey( _tyStdStr & _strCurKey ) const 
    {
        _strCurKey = m_strCurKey;
    }
    void ClearKey()
    {
        m_strCurKey.clear();
    }
    void SwapKey( _tyStdStr & _rstr )
    {
        m_strCurKey.swap( _rstr );
    }

    // Set the JsonObject for the end of iteration.
    void SetEndOfIteration()
    {
        _tyBase::SetEndOfIteration( true );
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
template < class t_tyCharTraits >
class JsonArray : protected JsonAggregate< t_tyCharTraits >
{
    typedef JsonAggregate< t_tyCharTraits > _tyBase;
    typedef JsonArray _tyThis;
public:
    using _tyCharTraits = t_tyCharTraits;
    typedef JsonValue< t_tyCharTraits > _tyJsonValue;
    using _tyStdStr = typename _tyCharTraits::_tyStdStr;

    JsonArray( const _tyJsonValue * _pjvParent )
        : _tyBase( _pjvParent )
    {
    }
    JsonArray( JsonArray const & _r ) = default;
    using _tyBase::RJvGet;
    using _tyBase::NElement;
    using _tyBase::SetNElement;
    using _tyBase::IncElement;

    // Set the JsonArray for the end of iteration.
    void SetEndOfIteration()
    {
        _tyBase::SetEndOfIteration( false );
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
template < class t_tyJsonInputStream >
class JsonReadContext
{
    typedef JsonReadContext _tyThis;
    friend JsonReadCursor< t_tyJsonInputStream >;
public:
    typedef t_tyJsonInputStream _tyJsonInputStream;
    typedef typename t_tyJsonInputStream::_tyCharTraits _tyCharTraits;
    typedef typename _tyCharTraits::_tyChar _tyChar;
    typedef JsonValue< _tyCharTraits > _tyJsonValue;
    typedef JsonObject< _tyCharTraits > _tyJsonObject;
    typedef JsonArray< _tyCharTraits > _tyJsonArray;
    typedef typename t_tyJsonInputStream::_tyFilePos _tyFilePos;
    using _tyStdStr = typename _tyCharTraits::_tyStdStr;

    JsonReadContext( _tyJsonValue * _pjvCur = nullptr, JsonReadContext * _pjrxPrev = nullptr )
        :   m_pjrxPrev( _pjrxPrev ),
            m_pjvCur( _pjvCur )
    {
    }

    _tyJsonValue * PJvGet() const
    {
        return m_pjvCur;
    }
    _tyJsonObject * PGetJsonObject() const
    {
        Assert( !!m_pjvCur );
        return m_pjvCur->PGetJsonObject();
    }
    _tyJsonArray * PGetJsonArray() const
    {
        Assert( !!m_pjvCur );
        return m_pjvCur->PGetJsonArray();
    }

    JsonReadContext * PJrcGetNext()
    {
        return &*m_pjrxNext;
    }
    const JsonReadContext * PJrcGetNext() const
    {
        return &*m_pjrxNext;
    }

    EJsonValueType JvtGetValueType() const
    {
        Assert( !!m_pjvCur );
        if ( !m_pjvCur )
            return ejvtJsonValueTypeCount;
        return m_pjvCur->JvtGetValueType();
    }
    void SetValueType( EJsonValueType _jvt )
    {
        Assert( !!m_pjvCur );
        if ( !!m_pjvCur )
            m_pjvCur->SetValueType( _jvt );
    }

    _tyStdStr * PGetStringValue() const
    {
        Assert( !!m_pjvCur );
        return !m_pjvCur ? 0 : m_pjvCur->PGetStringValue();
    }

    void SetEndOfIteration( _tyFilePos _pos )
    {
        m_tcFirst = 0;
        m_posStartValue = m_posEndValue = _pos;
        // The value is set to EndOfIteration separately.
    }
    bool FEndOfIteration() const
    {
        bool fEnd = (  ( ( JvtGetValueType() == ejvtObject ) && !!m_pjvCur->PGetJsonObject() && m_pjvCur->PGetJsonObject()->FEndOfIteration() )
                    || ( ( JvtGetValueType() == ejvtArray ) && !!m_pjvCur->PGetJsonArray() && m_pjvCur->PGetJsonArray()->FEndOfIteration() ) );
        //Assert( !fEnd || !m_tcFirst ); - dbien: should figure out where I am setting end-of and also reset this to 0.
        return fEnd;
    }

    // Push _pjrxNewHead as the new head of stack before _pjrxHead.
    static void PushStack( std::unique_ptr< JsonReadContext > & _pjrxHead, std::unique_ptr< JsonReadContext > & _pjrxNewHead )
    {
        if ( !!_pjrxHead )
        {
            Assert( !_pjrxHead->m_pjrxPrev );
            Assert( !_pjrxNewHead->m_pjrxPrev );
            Assert( !_pjrxNewHead->m_pjrxNext );
            _pjrxHead->m_pjrxPrev = &*_pjrxNewHead; // soft link.
            _pjrxNewHead->m_pjrxNext.swap( _pjrxHead );
        }
        _pjrxHead.swap( _pjrxNewHead ); // new head is now the head and it points at old head.
    }
    static void PopStack( std::unique_ptr< JsonReadContext > & _pjrxHead )
    {
        std::unique_ptr< JsonReadContext > pjrxOldHead;
        _pjrxHead.swap( pjrxOldHead );
        pjrxOldHead->m_pjrxNext.swap( _pjrxHead );
        _pjrxHead->m_pjrxPrev = 0;
    }

protected:
    _tyJsonValue * m_pjvCur{}; // We maintain a soft reference to the current JsonValue at this level.
    std::unique_ptr< JsonReadContext > m_pjrxNext; // Implement a simple doubly linked list.
    JsonReadContext * m_pjrxPrev{}; // soft reference to parent in list.
    _tyFilePos m_posStartValue{}; // The start of the value for this element - after parsing WS.
    _tyFilePos m_posEndValue{}; // The end of the value for this element - before parsing WS beyond.
    _tyChar m_tcFirst{}; // Only for the number type does this matter but since it does...
};

// JsonRestoreContext:
// Restores the context to formerly current context upon end object lifetime.
template < class t_tyJsonInputStream >
class JsonRestoreContext
{
    typedef JsonRestoreContext _tyThis;
public:
    typedef t_tyJsonInputStream _tyJsonInputStream;
    typedef typename t_tyJsonInputStream::_tyCharTraits _tyCharTraits;
    typedef JsonReadCursor< t_tyJsonInputStream > _tyJsonReadCursor;
    typedef JsonReadContext< t_tyJsonInputStream > _tyJsonReadContext;

    JsonRestoreContext() = default;
    JsonRestoreContext( _tyJsonReadCursor & _jrc )
        :   m_pjrc( &_jrc ),
            m_pjrx( &_jrc.GetCurrentContext() )
    {
    }
    ~JsonRestoreContext() noexcept(false)
    {
        if ( !!m_pjrx && !!m_pjrc )
        {
            bool fInUnwinding = !!std::uncaught_exceptions();
            try
            {
                m_pjrc->MoveToContext( *m_pjrx ); // There really no reason this should throw unless someone does something stupid - which of course happens.
            }
            catch( std::exception & rexc )
            {
                if ( !fInUnwinding )
                    throw;
                LOGSYSLOG( eslmtError, "~JsonRestoreContext(): Caught exception [%s].", rexc.what() );
            }
        }
    }

    // This one does potentially throw since we are not within a destructor.
    void Release()
    {
        if ( !!m_pjrx && !!m_pjrc )
        {
            _tyJsonReadCursor * pjrc = m_pjrc;
            m_pjrc = 0;
            pjrc->MoveToContext( *m_pjrx );
        }
    }

protected:
    _tyJsonReadCursor * m_pjrc{};
    const _tyJsonReadContext * m_pjrx{};
};

// class JsonReadCursor:
template < class t_tyJsonInputStream >
class JsonReadCursor
{
    typedef JsonReadCursor _tyThis;
public:
    typedef t_tyJsonInputStream _tyJsonInputStream;
    typedef typename t_tyJsonInputStream::_tyFilePos _tyFilePos;
    typedef typename t_tyJsonInputStream::_tyCharTraits _tyCharTraits;
    typedef typename _tyCharTraits::_tyChar _tyChar;
    typedef JsonValue< _tyCharTraits > _tyJsonValue;
    typedef JsonObject< _tyCharTraits > _tyJsonObject;
    typedef JsonArray< _tyCharTraits > _tyJsonArray;
    typedef JsonReadContext< t_tyJsonInputStream > _tyJsonReadContext;
    typedef JsonRestoreContext< t_tyJsonInputStream > _tyJsonRestoreContext;
    using _tyStdStr = typename _tyCharTraits::_tyStdStr;
    using _tyLPCSTR = typename _tyCharTraits::_tyLPCSTR;

    JsonReadCursor() = default;
    JsonReadCursor( JsonReadCursor const & ) = delete;
    JsonReadCursor& operator = ( JsonReadCursor const & ) = delete;

     void AssertValid() const 
    {
        Assert( !m_pjrxCurrent == !m_pis );
        Assert( !m_pjrxRootVal == !m_pis );
        Assert( !m_pjrxContextStack == !m_pis );
        // Make sure that the current context is in the context stack.
        if ( !!m_pjrxContextStack )
        {
            const _tyJsonReadContext * pjrxCheck = &*m_pjrxContextStack;
            for ( ; !!pjrxCheck && ( pjrxCheck != m_pjrxCurrent ); pjrxCheck = pjrxCheck->PJrcGetNext() )
                ;
            Assert( !!pjrxCheck ); // If this fails then the current context is not in the content stack - this is bad.
        }
    }
    bool FAttached() const 
    {
        AssertValid();
        return !!m_pis;
    }

    const _tyJsonReadContext & GetCurrentContext() const
    {
        Assert( !!m_pjrxCurrent );
        return *m_pjrxCurrent;
    }

    // Returns true if the current value is an aggregate value - object or array.
    bool FAtAggregateValue() const 
    {
        AssertValid();
        return !!m_pjrxCurrent && 
            ( ( ejvtArray == m_pjrxCurrent->JvtGetValueType() ) || ( ejvtObject == m_pjrxCurrent->JvtGetValueType() ) );
    }
    bool FAtObjectValue() const 
    {
        AssertValid();
        return !!m_pjrxCurrent && ( ejvtObject == m_pjrxCurrent->JvtGetValueType() ) ;
    }
    bool FAtArrayValue() const 
    {
        AssertValid();
        return !!m_pjrxCurrent && ( ejvtArray == m_pjrxCurrent->JvtGetValueType() ) ;
    }
    // Return if we are at the end of the iteration of an aggregate (object or array).
    bool FAtEndOfAggregate() const 
    {
        AssertValid();
        return !!m_pjrxCurrent && 
            ( ( ejvtEndOfArray == m_pjrxCurrent->JvtGetValueType() ) || ( ejvtEndOfObject == m_pjrxCurrent->JvtGetValueType() ) );
    }

    const _tyStdStr & RStrKey( EJsonValueType * _pjvt = 0 ) const
    {
        Assert( FAttached() );
        if ( FAtEndOfAggregate() || !m_pjrxCurrent || !m_pjrxCurrent->m_pjrxNext || ( ejvtObject != m_pjrxCurrent->m_pjrxNext->JvtGetValueType() ) )
            THROWBADJSONSEMANTICUSE( "JsonReadCursor::RStrKey(): Mo key available." );
        _tyJsonObject * pjoCur = m_pjrxCurrent->m_pjrxNext->PGetJsonObject();
        Assert( !pjoCur->FEndOfIteration() ); // sanity
        if ( !!_pjvt )
            *_pjvt = m_pjrxCurrent->JvtGetValueType();
        return pjoCur->RStrKey();
    }
    // Get the current key if there is a current key.
    bool FGetKeyCurrent( _tyStdStr & _rstrKey, EJsonValueType & _rjvt ) const
    {
        Assert( FAttached() );
        if ( FAtEndOfAggregate() )
            return false;
        // This should only be called when we are inside of an object's value.
        // So, we need to have a parent value and that parent value should correspond to an object.
        // Otherwise we throw an "invalid semantic use" exception.
        if ( !m_pjrxCurrent || !m_pjrxCurrent->m_pjrxNext || ( ejvtObject != m_pjrxCurrent->m_pjrxNext->JvtGetValueType() ) )
            THROWBADJSONSEMANTICUSE( "JsonReadCursor::FGetKeyCurrent(): Not located at an object so no key available." );
        // If the key value in the object is the empty string then this is an empty object and we return false.
        _tyJsonObject * pjoCur = m_pjrxCurrent->m_pjrxNext->PGetJsonObject();
        Assert( !pjoCur->FEndOfIteration() );
        pjoCur->GetKey( _rstrKey );
        _rjvt = m_pjrxCurrent->JvtGetValueType(); // This is the value type for this key string. We may not have read the actual value yet.
        return true;
    }
    EJsonValueType JvtGetValueType() const
    {
        Assert( FAttached() );
        return m_pjrxCurrent->JvtGetValueType();
    }
    bool FIsValueNull() const
    {
        return ejvtNull == JvtGetValueType();
    }

    // Get a full copy of the JsonValue. We allow this to work with any type of JsonValue for completeness.
    void GetValue( _tyJsonValue & _rjvValue ) const
    {
        Assert( FAttached() );
        EJsonValueType jvt = JvtGetValueType();
        if ( ( ejvtEndOfObject == jvt ) || ( ejvtEndOfArray == jvt ) )
        {
            _rjvValue.SetEndOfIteration( ejvtEndOfObject == jvt );
            return;
        }
        
        // Now check if we haven't read the simple value yet because then we gotta read it.
        if ( ( ejvtObject != jvt ) && ( ejvtArray != jvt ) && !m_pjrxCurrent->m_posEndValue )
            const_cast< _tyThis * >( this )->_ReadSimpleValue();
        
        _tyJsonValue jvLocal( *m_pjrxCurrent->PJvGet() ); // copy into local then swap values with passed value - solves all sorts of potential issues with initial conditions.
        _rjvValue.swap( jvLocal );
    }

    // Get a string representation of the value.
    // This is a semantic error if this is an aggregate value (object or array).
    void GetValue( _tyStdStr & _strValue ) const
    {
        Assert( FAttached() );
        if ( FAtAggregateValue() )
            THROWBADJSONSEMANTICUSE( "JsonReadCursor::GetValue(string): At an aggregate value - object or array." );

        EJsonValueType jvt = JvtGetValueType();
        if ( ( ejvtEndOfObject == jvt ) || ( ejvtEndOfArray == jvt ) )
            THROWBADJSONSEMANTICUSE( "JsonReadCursor::GetValue(string): No value located at end of object or array." );
        
        // Now check if we haven't read the value yet because then we gotta read it.
        if ( !m_pjrxCurrent->m_posEndValue )
            const_cast< _tyThis * >( this )->_ReadSimpleValue();
        
        switch( JvtGetValueType() )
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
    void GetValue( bool & _rf ) const
    {
        // Now check if we haven't read the value yet because then we gotta read it.
        if ( !m_pjrxCurrent->m_posEndValue )
            const_cast< _tyThis * >( this )->_ReadSimpleValue();
        
        switch( JvtGetValueType() )
        {
        default:
            THROWBADJSONSEMANTICUSE( "JsonReadCursor::GetValue(bool): Not at a boolean value type." );
            break;
        case ejvtTrue:
            _rf = true;
            break;
        case ejvtFalse:
            _rf = false;
            break;
        }
    }
    template < class t_tyNum >
    void _GetValue( _tyLPCSTR _pszFmt, t_tyNum & _rNumber ) const
    {
        if ( ejvtNumber != JvtGetValueType() ) 
            THROWBADJSONSEMANTICUSE( "JsonReadCursor::GetValue(int): Not at a numeric value type." );

        // Now check if we haven't read the value yet because then we gotta read it.
        if ( !m_pjrxCurrent->m_posEndValue )
            const_cast< _tyThis * >( this )->_ReadSimpleValue();

        // The presumption is that sscanf won't read past any decimal point if scanning a non-floating point number.
        int iRet = sscanf( m_pjrxCurrent->PGetStringValue()->c_str(), _pszFmt, &_rNumber );
        Assert( 1 == iRet ); // Due to the specification of number we expect this to always succeed.
    }
    void GetValue( uint8_t & _rby ) const { _GetValue( "%hhu", _rby ); }
    void GetValue( int8_t & _rsby ) const { _GetValue( "%hhd", _rsby ); }
    void GetValue( uint16_t & _rus ) const { _GetValue( "%hu", _rus ); }
    void GetValue( int16_t & _rss ) const { _GetValue( "%hd", _rss ); }
    void GetValue( uint32_t & _rui ) const { _GetValue( "%u", _rui ); }
    void GetValue( int32_t & _rsi ) const { _GetValue( "%d", _rsi ); }
    void GetValue( uint64_t & _rul ) const { _GetValue( "%lu", _rul ); }
    void GetValue( int64_t & _rsl ) const { _GetValue( "%ld", _rsl ); }
    void GetValue( float & _rfl ) const { _GetValue( "%e", _rfl ); }
    void GetValue( double & _rdbl ) const { _GetValue( "%le", _rdbl ); }
    void GetValue( long double & _rldbl ) const { _GetValue( "%Le", _rldbl ); }

    // Speciality values:
    // Human readable date/time - implemented on ejvtString.
    void GetTimeStringValue( time_t & _tt ) const
    {
        if ( ejvtString != JvtGetValueType() )
            THROWBADJSONSEMANTICUSE( "JsonReadCursor::GetTimeStringValue(): Not at a string value type." );
        int iRet = n_TimeUtil::ITimeFromString( m_pjrxCurrent->PGetStringValue()->c_str(), _tt );
        if ( !!iRet )
            THROWBADJSONSEMANTICUSE( "JsonReadCursor::GetTimeStringValue()): Failed to parse a date/time, iRet[%d].", iRet );
    }
    // Human readable date/time - implemented on ejvtString.
    void GetUuidStringValue( uuid_t & _uuidt ) const
    {
        if ( ejvtString != JvtGetValueType() )
            THROWBADJSONSEMANTICUSE( "JsonReadCursor::GetTimeStringValue(): Not at a string value type." );
        if ( m_pjrxCurrent->PGetStringValue()->length() < std::size( std::declval<uuid_string_t>() ) - 1 )
            THROWBADJSONSEMANTICUSE( "JsonReadCursor::GetTimeStringValue(): Not enough characters in the string for uuid_string_t." );
        uuid_string_t ustUuid;
        memcpy( &ustUuid, m_pjrxCurrent->PGetStringValue()->c_str(), sizeof ustUuid );
        ustUuid[ std::size( ustUuid )-1 ] = 0;
        int iRet = uuid_parse( ustUuid, _uuidt );
        if ( !!iRet )
            THROWBADJSONSEMANTICUSE( "JsonReadCursor::GetTimeStringValue()): Failed to parse a uuid from string [%s].", m_pjrxCurrent->PGetStringValue()->c_str() );
    }

    // This will read any unread value into the value object
    void _ReadSimpleValue()
    {
        Assert( !m_pjrxCurrent->m_posEndValue );
        EJsonValueType jvt = JvtGetValueType();
        Assert( ( jvt >= ejvtFirstJsonSimpleValue ) && ( jvt <= ejvtLastJsonSpecifiedValue ) ); // Should be handled by caller.
        switch( jvt )
        {
        case ejvtNumber:
            _ReadNumber( m_pjrxCurrent->m_tcFirst, !m_pjrxCurrent->m_pjrxNext, *m_pjrxCurrent->PGetStringValue() );
            break;
        case ejvtString:
            _ReadString( *m_pjrxCurrent->PGetStringValue() );
            break;
        case ejvtTrue:
        case ejvtFalse:
        case ejvtNull:
            _SkipSimpleValue( jvt, m_pjrxCurrent->m_tcFirst, !m_pjrxCurrent->m_pjrxNext ); // We just skip remaining value checking that it is correct.
            break;
        default:
            Assert(0);
            break;
        }
        m_pjrxCurrent->m_posEndValue = m_pis->PosGet();
    }

    // Read a JSON number according to the specifications of such.
    // The interesting thing about a number is that there is no character indicating the ending 
    //  - whereas for all other value types there is a specific character ending the value.
    // A JSON number ends when there is a character that is not a JSON number - or the EOF in the case of a JSON stream containing a single number value.
    // For all other JSON values you wouldn't have to worry about perhaps encountering EOF in the midst of reading the value.
    void _ReadNumber( _tyChar _tcFirst, bool _fAtRootElement, _tyStdStr & _rstrRead ) const
    {
        // We will only read a number up to _tyCharTraits::s_knMaxNumberLength in string length.
        const unsigned int knMaxLength = _tyCharTraits::s_knMaxNumberLength;
        _tyChar rgtcBuffer[ knMaxLength + 1 ];
        _tyChar * ptcCur = rgtcBuffer;
        *ptcCur++ = _tcFirst;

        auto lambdaAddCharNum = [&]( _tyChar _tch )
        { 
            if ( ptcCur == rgtcBuffer + knMaxLength )
                THROWBADJSONSTREAM( "JsonReadCursor::_ReadNumber(): Overflow of max JSON number length of [%u].", knMaxLength );
            *ptcCur++ = _tch;
        };

        // We will have read the first character of a number and then will need to satisfy the state machine for the number appropriately.
        bool fZeroFirst = ( _tyCharTraits::s_tc0 == _tcFirst );
        _tyChar tchCurrentChar; // maintained as the current character.
        if ( _tyCharTraits::s_tcMinus == _tcFirst )
        {
            tchCurrentChar = m_pis->ReadChar( "JsonReadCursor::_ReadNumber(): Hit EOF looking for a digit after a minus." ); // This may not be EOF.
            *ptcCur++ = tchCurrentChar;
            if ( ( tchCurrentChar < _tyCharTraits::s_tc0 ) || ( tchCurrentChar > _tyCharTraits::s_tc9 ) )
                THROWBADJSONSTREAM( "JsonReadCursor::_ReadNumber(): Found [%TC] when looking for digit after a minus.", tchCurrentChar );
            fZeroFirst = ( _tyCharTraits::s_tc0 == tchCurrentChar );
        }
        // If we are the root element then we may encounter EOF and have that not be an error when reading some of the values.
        if ( fZeroFirst )
        {
            bool fFoundChar = m_pis->FReadChar( tchCurrentChar, !_fAtRootElement, "JsonReadCursor::_ReadNumber(): Hit EOF looking for something after a leading zero." ); // Throw on EOF if we aren't at the root element.
            if ( !fFoundChar )
                goto Label_DreadedLabel; // Then we read until the end of file for a JSON file containing a single number as its only element.
        }
        else // ( !fZeroFirst )
        {
            // Then we expect more digits or a period or an 'e' or an 'E' or EOF if we are the root element.
            bool fFoundChar;
            for(;;)
            {            
                bool fFoundChar = m_pis->FReadChar( tchCurrentChar, !_fAtRootElement, "JsonReadCursor::_ReadNumber(): Hit EOF looking for a non-number." ); // Throw on EOF if we aren't at the root element.
                if ( !fFoundChar )
                    goto Label_DreadedLabel; // Then we read until the end of file for a JSON file containing a single number as its only element.
                if ( ( tchCurrentChar >= _tyCharTraits::s_tc0 ) && ( tchCurrentChar <= _tyCharTraits::s_tc9 ) )
                    lambdaAddCharNum( tchCurrentChar );
                else
                    break;
            } 
        }
    
        if ( tchCurrentChar == _tyCharTraits::s_tcPeriod )
        {
            lambdaAddCharNum( tchCurrentChar ); // Don't miss your period.
            // Then according to the JSON spec we must have at least one digit here.
            tchCurrentChar = m_pis->ReadChar( "JsonReadCursor::_ReadNumber(): Hit EOF looking for a digit after a decimal point." ); // throw on EOF.
            if ( ( tchCurrentChar < _tyCharTraits::s_tc0 ) || ( tchCurrentChar > _tyCharTraits::s_tc9 ) )
                THROWBADJSONSTREAM( "JsonReadCursor::_ReadNumber(): Found [%TC] when looking for digit after a decimal point.", tchCurrentChar );
            // Now we expect a digit, 'e', 'E', EOF or something else that our parent can check out.
            lambdaAddCharNum( tchCurrentChar );
            for(;;)
            {            
                bool fFoundChar = m_pis->FReadChar( tchCurrentChar, !_fAtRootElement, "JsonReadCursor::_ReadNumber(): Hit EOF looking for a non-number after the period." ); // Throw on EOF if we aren't at the root element.
                if ( !fFoundChar )
                    goto Label_DreadedLabel; // Then we read until the end of file for a JSON file containing a single number as its only element.            
                if ( ( tchCurrentChar >= _tyCharTraits::s_tc0 ) && ( tchCurrentChar <= _tyCharTraits::s_tc9 ) )
                    lambdaAddCharNum( tchCurrentChar );
                else
                    break;
            }
        }
        if (    ( tchCurrentChar == _tyCharTraits::s_tcE ) ||
                ( tchCurrentChar == _tyCharTraits::s_tce ) )
        {
            lambdaAddCharNum( tchCurrentChar );
            // Then we might see a plus or a minus or a number - but we cannot see EOF here correctly:
            tchCurrentChar = m_pis->ReadChar( "JsonReadCursor::_ReadNumber(): Hit EOF looking for a digit after an exponent indicator." ); // Throws on EOF.
            lambdaAddCharNum( tchCurrentChar );
            if (    ( tchCurrentChar == _tyCharTraits::s_tcMinus ) ||
                   ( tchCurrentChar == _tyCharTraits::s_tcPlus ) )
            {
                tchCurrentChar = m_pis->ReadChar( "JsonReadCursor::_ReadNumber(): Hit EOF looking for a digit after an exponent indicator with plus/minus." ); // Then we must still find a number after - throws on EOF.
                lambdaAddCharNum( tchCurrentChar );
            }
            // Now we must see at least one digit so for the first read we throw.
            if ( ( tchCurrentChar < _tyCharTraits::s_tc0 ) || ( tchCurrentChar > _tyCharTraits::s_tc9 ) )
                THROWBADJSONSTREAM( "JsonReadCursor::_ReadNumber(): Found [%TC] when looking for digit after a exponent indicator (e/E).", tchCurrentChar );
            // We have satisfied "at least a digit after an exponent indicator" - now we might just hit EOF or anything else after this...
            for(;;)
            {
                bool fFoundChar = m_pis->FReadChar( tchCurrentChar, !_fAtRootElement, "JsonReadCursor::_ReadNumber(): Hit EOF looking for a non-number after the exponent indicator." ); // Throw on EOF if we aren't at the root element.
                if ( !fFoundChar )
                    goto Label_DreadedLabel; // Then we read until the end of file for a JSON file containing a single number as its only element.            
                if ( ( tchCurrentChar >= _tyCharTraits::s_tc0 ) && ( tchCurrentChar <= _tyCharTraits::s_tc9 ) )
                    lambdaAddCharNum( tchCurrentChar );
                else
                    break;
            } 
        }
        m_pis->PushBackLastChar( true ); // Let caller read this and decide what to do depending on context - we don't expect a specific character.
Label_DreadedLabel: // Just way too easy to do it this way.
        *ptcCur++ = _tyChar(0);
        _rstrRead = rgtcBuffer; // Copy the number into the return buffer.
    }

    // Skip a JSON number according to the specifications of such.
    void _SkipNumber( _tyChar _tcFirst, bool _fAtRootElement ) const
    {
        // We will have read the first character of a number and then will need to satisfy the state machine for the number appropriately.
        bool fZeroFirst = ( _tyCharTraits::s_tc0 == _tcFirst );
        _tyChar tchCurrentChar; // maintained as the current character.
        if ( _tyCharTraits::s_tcMinus == _tcFirst )
        {
            tchCurrentChar = m_pis->ReadChar( "JsonReadCursor::_SkipNumber(): Hit EOF looking for a digit after a minus." ); // This may not be EOF.
            if ( ( tchCurrentChar < _tyCharTraits::s_tc0 ) || ( tchCurrentChar > _tyCharTraits::s_tc9 ) )
                THROWBADJSONSTREAM( "JsonReadCursor::_SkipNumber(): Found [%TC] when looking for digit after a minus.", tchCurrentChar );
            fZeroFirst = ( _tyCharTraits::s_tc0 == tchCurrentChar );
        }
        // If we are the root element then we may encounter EOF and have that not be an error when reading some of the values.
        if ( fZeroFirst )
        {
            bool fFoundChar = m_pis->FReadChar( tchCurrentChar, !_fAtRootElement, "JsonReadCursor::_SkipNumber(): Hit EOF looking for something after a leading zero." ); // Throw on EOF if we aren't at the root element.
            if ( !fFoundChar )
                return; // Then we read until the end of file for a JSON file containing a single number as its only element.
        }
        else // ( !fZeroFirst )
        {
            // Then we expect more digits or a period or an 'e' or an 'E' or EOF if we are the root element.
            bool fFoundChar;
            do
            {            
                bool fFoundChar = m_pis->FReadChar( tchCurrentChar, !_fAtRootElement, "JsonReadCursor::_SkipNumber(): Hit EOF looking for a non-number." ); // Throw on EOF if we aren't at the root element.
                if ( !fFoundChar )
                    return; // Then we read until the end of file for a JSON file containing a single number as its only element.
            } 
            while ( ( tchCurrentChar >= _tyCharTraits::s_tc0 ) && ( tchCurrentChar <= _tyCharTraits::s_tc9 ) );
        }
    
        if ( tchCurrentChar == _tyCharTraits::s_tcPeriod )
        {
            // Then according to the JSON spec we must have at least one digit here.
            tchCurrentChar = m_pis->ReadChar( "JsonReadCursor::_SkipNumber(): Hit EOF looking for a digit after a decimal point." ); // throw on EOF.
            if ( ( tchCurrentChar < _tyCharTraits::s_tc0 ) || ( tchCurrentChar > _tyCharTraits::s_tc9 ) )
                THROWBADJSONSTREAM( "JsonReadCursor::_SkipNumber(): Found [%TC] when looking for digit after a decimal point.", tchCurrentChar );
            // Now we expect a digit, 'e', 'E', EOF or something else that our parent can check out.
            do
            {            
                bool fFoundChar = m_pis->FReadChar( tchCurrentChar, !_fAtRootElement, "JsonReadCursor::_SkipNumber(): Hit EOF looking for a non-number after the period." ); // Throw on EOF if we aren't at the root element.
                if ( !fFoundChar )
                    return; // Then we read until the end of file for a JSON file containing a single number as its only element.            
            }
            while ( ( tchCurrentChar >= _tyCharTraits::s_tc0 ) && ( tchCurrentChar <= _tyCharTraits::s_tc9 ) );
        }
        if (    ( tchCurrentChar == _tyCharTraits::s_tcE ) ||
                ( tchCurrentChar == _tyCharTraits::s_tce ) )
        {
            // Then we might see a plus or a minus or a number - but we cannot see EOF here correctly:
            tchCurrentChar = m_pis->ReadChar( "JsonReadCursor::_SkipNumber(): Hit EOF looking for a digit after an exponent indicator." ); // Throws on EOF.
            if (    ( tchCurrentChar == _tyCharTraits::s_tcMinus ) ||
                    ( tchCurrentChar == _tyCharTraits::s_tcPlus ) )
                tchCurrentChar = m_pis->ReadChar( "JsonReadCursor::_SkipNumber(): Hit EOF looking for a digit after an exponent indicator with plus/minus." ); // Then we must still find a number after - throws on EOF.
            // Now we must see at least one digit so for the first read we throw.
            if ( ( tchCurrentChar < _tyCharTraits::s_tc0 ) || ( tchCurrentChar > _tyCharTraits::s_tc9 ) )
                THROWBADJSONSTREAM( "JsonReadCursor::_SkipNumber(): Found [%TC] when looking for digit after an exponent indicator (e/E).", tchCurrentChar );
            // We have satisfied "at least a digit after an exponent indicator" - now we might just hit EOF or anything else after this...
            do
            {
                bool fFoundChar = m_pis->FReadChar( tchCurrentChar, !_fAtRootElement, "JsonReadCursor::_SkipNumber(): Hit EOF looking for a non-number after the exponent indicator." ); // Throw on EOF if we aren't at the root element.
                if ( !fFoundChar )
                    return; // Then we read until the end of file for a JSON file containing a single number as its only element.            
            } 
            while ( ( tchCurrentChar >= _tyCharTraits::s_tc0 ) && ( tchCurrentChar <= _tyCharTraits::s_tc9 ) );
        }
        m_pis->PushBackLastChar( true ); // Let caller read this and decide what to do depending on context - we don't expect a specific character here.
    }

    // Read the string starting at the current position. The '"' has already been read.
    void _ReadString( _tyStdStr & _rstrRead ) const
    {
        const int knLenBuffer = 1023;
        _tyChar rgtcBuffer[ knLenBuffer+1 ]; // We will append a zero when writing to the string.
        rgtcBuffer[knLenBuffer] = 0; // preterminate end.
        _tyChar * ptchCur = rgtcBuffer;
        _rstrRead.clear();

        // We know we will see a '"' at the end. Along the way we may see multiple excape '\' characters.
        // So we move along checking each character, throwing if we hit EOF before the end of the string is found.

        for( ; ; )
        {
            _tyChar tchCur = m_pis->ReadChar( "JsonReadCursor::_ReadString(): EOF found looking for end of string." ); // throws on EOF.
            if ( _tyCharTraits::s_tcDoubleQuote == tchCur )
                break; // We have reached EOS.
            if ( _tyCharTraits::s_tcBackSlash == tchCur )
            {
                tchCur = m_pis->ReadChar( "JsonReadCursor::_ReadString(): EOF finding completion of backslash escape for string." ); // throws on EOF.
                switch( tchCur )
                {
                    case _tyCharTraits::s_tcDoubleQuote:
                    case _tyCharTraits::s_tcBackSlash:
                    case _tyCharTraits::s_tcForwardSlash:
                        break;   // Just use tchCur.
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
                        unsigned int uCurrentMultiplier = ( 1u << 12 );
                        // Must find 4 hex digits:
                        for ( int n=0; n < 4; ++n, ( uCurrentMultiplier >>= 4 ) )
                        {
                            tchCur = m_pis->ReadChar( "JsonReadCursor::_ReadString(): EOF found looking for 4 hex digits following \\u." ); // throws on EOF.
                            if ( ( tchCur >= _tyCharTraits::s_tc0 ) && ( tchCur <= _tyCharTraits::s_tc9 ) )
                                uHex += uCurrentMultiplier * ( tchCur - _tyCharTraits::s_tc0 );
                            else
                            if ( ( tchCur >= _tyCharTraits::s_tca ) && ( tchCur <= _tyCharTraits::s_tcf ) )
                                uHex += uCurrentMultiplier * ( 10 + ( tchCur - _tyCharTraits::s_tca ) );
                            else
                            if ( ( tchCur >= _tyCharTraits::s_tcA ) && ( tchCur <= _tyCharTraits::s_tcF ) )
                                uHex += uCurrentMultiplier * ( 10 + ( tchCur - _tyCharTraits::s_tcA ) );
                            else
                                THROWBADJSONSTREAM( "JsonReadCursor::_ReadString(): Found [%TC] when looking for digit following \\u.", tchCur );
                        }
                        // If we are supposed to throw on overflow then check for it, otherwise just truncate to the character type silently.
                        if ( _tyCharTraits::s_fThrowOnUnicodeOverflow && ( sizeof(_tyChar) < sizeof(uHex) ) )
                        {
                            if ( uHex >= ( 1u << ( CHAR_BIT * sizeof(_tyChar) ) ) )
                                THROWBADJSONSTREAM( "JsonReadCursor::_ReadString(): Unicode hex overflow [%u].", uHex );
                        }
                        tchCur = (_tyChar)uHex;
                        break;
                    }
                    default:
                        THROWBADJSONSTREAM( "JsonReadCursor::_ReadString(): Found [%TC] when looking for competetion of backslash when reading string.", tchCur );
                        break;
                }
            }
            if ( ptchCur == rgtcBuffer + knLenBuffer )
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

        for( ; ; )
        {
            _tyChar tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipRemainingString(): EOF found looking for end of string." ); // throws on EOF.
            if ( _tyCharTraits::s_tcDoubleQuote == tchCur )
                break; // We have reached EOS.
            if ( _tyCharTraits::s_tcBackSlash == tchCur )
            {
                tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipRemainingString(): EOF finding completion of backslash escape for string." ); // throws on EOF.
                switch( tchCur )
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
                        for ( int n=0; n < 4; ++n )
                        {
                            tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipRemainingString(): EOF found looking for 4 hex digits following \\u." ); // throws on EOF.
                            if ( !( ( ( tchCur >= _tyCharTraits::s_tc0 ) && ( tchCur <= _tyCharTraits::s_tc9 ) ) ||
                                    ( ( tchCur >= _tyCharTraits::s_tca ) && ( tchCur <= _tyCharTraits::s_tcf ) ) ||
                                    ( ( tchCur >= _tyCharTraits::s_tcA ) && ( tchCur <= _tyCharTraits::s_tcF ) ) ) ) 
                                THROWBADJSONSTREAM( "JsonReadCursor::_SkipRemainingString(): Found [%TC] when looking for digit following \\u.", tchCur );
                        }
                    }
                    default:
                        THROWBADJSONSTREAM( "JsonReadCursor::_SkipRemainingString(): Found [%TC] when looking for competetion of backslash when reading string.", tchCur );
                        break;
                }
            }
            // Otherwise we just continue reading.
        }
    }

    // Just skip the zero terminated string starting at the current point in the JSON stream.
    void _SkipFixed( const char * _pcSkip ) const
    {
        // We just cast the chars to _tyChar since that is valid for JSON files.
        const char * pcCur = _pcSkip;
        _tyChar tchCur;
        for ( ; !!*pcCur && ( _tyChar( *pcCur ) == ( tchCur = m_pis->ReadChar("JsonReadCursor::_SkipFixed(): Hit EOF trying to skip fixed string.") ) ); ++pcCur )
            ;
        if ( *pcCur )
            THROWBADJSONSTREAM( "JsonReadCursor::_SkipFixed(): Trying to skip[%s], instead of [%c] found [%TC].", _pcSkip, *pcCur, tchCur );
    }

    void _SkipSimpleValue( EJsonValueType _jvtCur, _tyChar _tcFirst, bool _fAtRootElement ) const
    {
        switch( _jvtCur )
        {
        case ejvtNumber:
            _SkipNumber( _tcFirst, _fAtRootElement );
            break;
        case ejvtString:
            _SkipRemainingString();
            break;
        case ejvtTrue:
            _SkipFixed( "rue" );
            break;
        case ejvtFalse:
            _SkipFixed( "alse" );
            break;
        case ejvtNull:
            _SkipFixed( "ull" );
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
        _tyChar tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipValue(): EOF on first value char." ); // throws on EOF.
        EJsonValueType jvtCur = _tyJsonValue::GetJvtTypeFromChar( tchCur );
        if ( ejvtJsonValueTypeCount == jvtCur )
            THROWBADJSONSTREAM( "JsonReadCursor::_SkipValue(): Found [%TC] when looking for value starting character.", tchCur );
        if ( ejvtObject ==  jvtCur )
            _SkipWholeObject();
        else
        if ( ejvtArray ==  jvtCur )
            _SkipWholeArray();
        else
            _SkipSimpleValue( jvtCur, tchCur, false ); // we assume we aren't at the root element here since we shouldn't get here - we would have a context.
    }

    // We will have read the first '{' of the object.
    void _SkipWholeObject()
    {
        m_pis->SkipWhitespace();
        _tyChar tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipWholeObject(): EOF after begin bracket." ); // throws on EOF.
        while ( _tyCharTraits::s_tcDoubleQuote == tchCur )
        {
            _SkipRemainingString(); // Skip the key.
            m_pis->SkipWhitespace();
            tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipWholeObject(): EOF looking for colon." ); // throws on EOF.
            if ( _tyCharTraits::s_tcColon != tchCur )
                THROWBADJSONSTREAM( "JsonReadCursor::_SkipWholeObject(): Found [%TC] when looking for colon.", tchCur );
            _SkipValue(); // Skip the value.
            m_pis->SkipWhitespace();
            tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipWholeObject(): EOF looking comma or end of object." ); // throws on EOF.
            if ( _tyCharTraits::s_tcComma == tchCur )
            {
                m_pis->SkipWhitespace();
                tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipWholeObject(): EOF looking double quote." ); // throws on EOF.
                if ( _tyCharTraits::s_tcDoubleQuote != tchCur )
                    THROWBADJSONSTREAM( "JsonReadCursor::_SkipWholeObject(): Found [%TC] when looking for double quote.", tchCur );
            }
            else
                break;
        }
        if ( _tyCharTraits::s_tcRightCurlyBr != tchCur )
            THROWBADJSONSTREAM( "JsonReadCursor::_SkipWholeObject(): Found [%TC] when looking for double quote or comma or right bracket.", tchCur );
    }
    // We will have read the first '[' of the array.
    void _SkipWholeArray()
    {
        m_pis->SkipWhitespace();
        _tyChar tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipWholeArray(): EOF after begin bracket." ); // throws on EOF.
        if ( _tyCharTraits::s_tcRightSquareBr != tchCur )
            m_pis->PushBackLastChar(); // Don't send an expected character as it is from the set of value-starting characters.
        while ( _tyCharTraits::s_tcRightSquareBr != tchCur )
        {
            _SkipValue();
            m_pis->SkipWhitespace();
            tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipWholeArray(): EOF looking for comma." ); // throws on EOF.
            if ( _tyCharTraits::s_tcComma == tchCur )
                continue;
            if ( _tyCharTraits::s_tcRightSquareBr != tchCur )
                THROWBADJSONSTREAM( "JsonReadCursor::_SkipWholeArray(): Found [%TC] when looking for comma or array end.", tchCur );
        }
    }

    // Skip an object with an associated context. This will potentially recurse into SkipWholeObject(), SkipWholeArray(), etc. which will not have an associated context.
    void _SkipObject( _tyJsonReadContext & _rjrx )
    {
        // We have a couple of possible starting states here:
        // 1) We haven't read the data for this object ( !_rjrx.m_posEndValue ) in this case we expect to see "key":value, ..., "key":value }
        //      Note that there may also be no (key,value) pairs at all - we may have an empty object.
        // 2) We have read the data. In this case we will have also read the value and we expect to see a ',' or a '}'.
        // 3) We have an empty object and we have read the data - in which case we have already read the final '}' of the object
        //      and we have nothing at all to do.

        if ( ejvtEndOfObject == _rjrx.JvtGetValueType() )
            return; // Then we have already read to the end of this object.

        if ( !_rjrx.m_posEndValue )
        {
            _SkipWholeObject(); // this expects that we have read the first '{'.
            // Now indicate that we are at the end of the object.
            _rjrx.m_posEndValue = m_pis->PosGet(); // Indicate that we have read the value.
            return;
        }

        // Then we have partially iterated the object. All contexts above us are closed which means we should find
        //  either a comma or end curly bracket.
        m_pis->SkipWhitespace();
        _tyChar tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipObject(): EOF looking for end object } or comma." ); // throws on EOF.
        while ( _tyCharTraits::s_tcRightCurlyBr !=tchCur )
        {
            if ( _tyCharTraits::s_tcComma != tchCur )
                THROWBADJSONSTREAM( "JsonReadCursor::_SkipObject(): Found [%TC] when looking for comma or object end.", tchCur );
            m_pis->SkipWhitespace();
            tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipObject(): EOF looking for double quote." ); // throws on EOF.
            if ( _tyCharTraits::s_tcDoubleQuote != tchCur )
                THROWBADJSONSTREAM( "JsonReadCursor::_SkipObject(): Found [%TC] when looking key start double quote.", tchCur );
            _SkipRemainingString();
            m_pis->SkipWhitespace();
            tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipObject(): EOF looking for colon." ); // throws on EOF.
            if ( _tyCharTraits::s_tcColon != tchCur )
                THROWBADJSONSTREAM( "JsonReadCursor::_SkipObject(): Found [%TC] when looking for colon.", tchCur );
            _SkipValue();
            m_pis->SkipWhitespace();
            tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipObject(): EOF looking for end object } or comma." ); // throws on EOF.
        }
        _tyJsonObject * pjoCur = _rjrx.PGetJsonObject();            
        _rjrx.SetEndOfIteration( m_pis->PosGet() ); // Indicate that we have iterated to the end of this object.
        pjoCur->SetEndOfIteration();
    }
    // Skip an array with an associated context. This will potentially recurse into SkipWholeObject(), SkipWholeArray(), etc. which will not have an associated context.
    void _SkipArray( _tyJsonReadContext & _rjrx )
    {
        // We have a couple of possible starting states here:
        // 1) We haven't read the data for this array ( !_rjrx.m_posEndValue ) in this case we expect to see value, ..., value }
        //      Note that there may also be no values at all - we may have an empty array.
        // 2) We have read the data. In this case we will have also read the value and we expect to see a ',' or a ']'.
        // 3) We have an empty object and we have read the data - in which case we have already read the final ']' of the object
        //      and we have nothing at all to do.

        if ( ejvtEndOfArray == _rjrx.JvtGetValueType() )
            return; // Then we have already read to the end of this object.

        if ( !_rjrx.m_posEndValue )
        {
            _SkipWholeArray(); // this expects that we have read the first '['.
            // Now indicate that we are at the end of the array.
            _rjrx.m_posEndValue = m_pis->PosGet(); // Indicate that we have read the value.
            //_rjrx.SetValueType( ejvtEndOfArray );
            return;
        }

        // Then we have partially iterated the object. All contexts above us are closed which means we should find
        //  either a comma or end square bracket.
        m_pis->SkipWhitespace();
        _tyChar tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipArray(): EOF looking for end array ] or comma." ); // throws on EOF.
        while ( _tyCharTraits::s_tcRightSquareBr !=tchCur )
        {
            if ( _tyCharTraits::s_tcComma != tchCur )
                THROWBADJSONSTREAM( "JsonReadCursor::_SkipArray(): Found [%TC] when looking for comma or array end.", tchCur );
            m_pis->SkipWhitespace();
            _SkipValue();
            m_pis->SkipWhitespace();
            tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipArray(): EOF looking for end array ] or comma." ); // throws on EOF.
        }
        _tyJsonArray * pjaCur = _rjrx.PGetJsonArray();            
        _rjrx.SetEndOfIteration( m_pis->PosGet() ); // Indicate that we have iterated to the end of this object.
        pjaCur->SetEndOfIteration();
    }

    // Skip this context - must be at the top of the context stack.
    void _SkipContext( _tyJsonReadContext & _rjrx )
    {
        // Skip this context which is at the top of the stack.
        // A couple of cases here:
        // 1) We are a non-hierarchical type: i.e. not array or object. In this case we merely have to read the value.
        // 2) We are an object or array. In this case we have to read until the end of the object or array.
        if ( ejvtObject == _rjrx.JvtGetValueType() )
            _SkipObject( _rjrx );
        else
        if ( ejvtArray == _rjrx.JvtGetValueType() )
            _SkipArray( _rjrx );
        else
        {
            // If we have already the value then we are done with this type.
            if ( !_rjrx.m_posEndValue )
            {
                _SkipSimpleValue( _rjrx.JvtGetValueType(), _rjrx.m_tcFirst, !_rjrx.m_pjrxNext ); // skip to the end of this value without saving the info.
                _rjrx.m_posEndValue = m_pis->PosGet(); // Update this for completeness in all scenarios.
            }
            return; // We have already fully processed this context.
        }
    }

    // Skip and close all contexts above the current context in the context stack.
    void SkipAllContextsAboveCurrent()
    {
       AssertValid(); 
       while ( &*m_pjrxContextStack != m_pjrxCurrent )
       {
           _SkipContext( *m_pjrxContextStack );
           _tyJsonReadContext::PopStack( m_pjrxContextStack );
       }
    }
    void SkipTopContext()
    {
        _SkipContext( *m_pjrxCurrent );
    }
    // Move to the given context which must be in the context stack.
    // If it isn't in the context stack then a json_stream_bad_semantic_usage_exception() is thrown.
    void MoveToContext( const _tyJsonReadContext & _rjrx )
    {
        _tyJsonReadContext * pjrxFound = &*m_pjrxContextStack;
        for ( ; !!pjrxFound && ( pjrxFound != &_rjrx ); pjrxFound = pjrxFound->PJrcGetNext() )
            ;
        if ( !pjrxFound )
            THROWBADJSONSEMANTICUSE( "JsonReadCursor::MoveToContext(): Context not found in context stack." );
        m_pjrxCurrent = pjrxFound;
    }

    // Move the the next element of the object or array. Return false if this brings us to the end of the entity.
    bool FNextElement()
    {
        Assert( FAttached() );
        // This should only be called when we are inside of an object or array.
        // So, we need to have a parent value and that parent value should correspond to an object or array.
        // Otherwise we throw an "invalid semantic use" exception.
        if (    !m_pjrxCurrent || !m_pjrxCurrent->m_pjrxNext || 
                (   ( ejvtObject != m_pjrxCurrent->m_pjrxNext->JvtGetValueType() ) &&
                    ( ejvtArray != m_pjrxCurrent->m_pjrxNext->JvtGetValueType() ) ) )
            THROWBADJSONSEMANTICUSE( "JsonReadCursor::FNextElement(): Not located at an object or array." );
        
        if (    ( m_pjrxCurrent->JvtGetValueType() == ejvtEndOfObject ) ||
                ( m_pjrxCurrent->JvtGetValueType() == ejvtEndOfArray ) )
        {
            Assert( ( m_pjrxCurrent->JvtGetValueType() == ejvtEndOfObject ) == ( ejvtObject != m_pjrxCurrent->m_pjrxNext->JvtGetValueType() ) ); // Would be weird if it didn't match.
            return false; // We are already at the end of the iteration.
        }
        // Perform common tasks:
        // We may be at the leaf of the current pathway when calling this or we may be in the middle of some pathway.
        // We must close any contexts above this object first.
        SkipAllContextsAboveCurrent(); // Skip and close all the contexts above the current context.
        // Close the top context if it is not read already:
        if (    !m_pjrxCurrent->m_posEndValue ||
                ( ( ( ejvtObject == m_pjrxCurrent->JvtGetValueType() ) || 
                  ( ejvtArray == m_pjrxCurrent->JvtGetValueType() ) )
                    && !m_pjrxCurrent->FEndOfIteration() ) )
            SkipTopContext(); // Then skip the value at the current context.
        // Destroy the current object regardless - we are going to the next one.
        m_pjrxCurrent->PJvGet()->Destroy();

        // Now we are going to look for a comma or an right curly/square bracket:
        m_pis->SkipWhitespace();
        _tyChar tchCur = m_pis->ReadChar( "JsonReadCursor::FNextElement(): EOF looking for end object/array }/] or comma." ); // throws on EOF.

        if ( ejvtObject == m_pjrxCurrent->m_pjrxNext->JvtGetValueType() )
        {
            _tyJsonObject * pjoCur = m_pjrxCurrent->m_pjrxNext->PGetJsonObject();            
            Assert( !pjoCur->FEndOfIteration() );
            if ( _tyCharTraits::s_tcRightCurlyBr == tchCur )
            {
                m_pjrxCurrent->SetEndOfIteration( m_pis->PosGet() );
                pjoCur->SetEndOfIteration();
                return false; // We reached the end of the iteration.
            }
            if ( _tyCharTraits::s_tcComma != tchCur )
                THROWBADJSONSTREAM( "JsonReadCursor::FNextElement(): Found [%TC] when looking for comma or object end.", tchCur );
            // Now we will read the key string of the next element into <pjoCur>.
            m_pis->SkipWhitespace();
            tchCur = m_pis->ReadChar( "JsonReadCursor::FNextElement(): EOF looking for double quote." ); // throws on EOF.
            if ( _tyCharTraits::s_tcDoubleQuote != tchCur )
                THROWBADJSONSTREAM( "JsonReadCursor::FNextElement(): Found [%TC] when looking key start double quote.", tchCur );
            _tyStdStr strNextKey;
            _ReadString( strNextKey ); // Might throw for any number of reasons. This may be the empty string.
            pjoCur->SwapKey( strNextKey );
            m_pis->SkipWhitespace();
            tchCur = m_pis->ReadChar( "JsonReadCursor::FNextElement(): EOF looking for colon." ); // throws on EOF.
            if ( _tyCharTraits::s_tcColon != tchCur )
                THROWBADJSONSTREAM( "JsonReadCursor::FNextElement(): Found [%TC] when looking for colon.", tchCur );
        }
        else
        {
            _tyJsonArray * pjaCur = m_pjrxCurrent->m_pjrxNext->PGetJsonArray();            
            Assert( !pjaCur->FEndOfIteration() );
            if ( _tyCharTraits::s_tcRightSquareBr == tchCur )
            {
                m_pjrxCurrent->SetEndOfIteration( m_pis->PosGet() );
                pjaCur->SetEndOfIteration();
                return false; // We reached the end of the iteration.
            }
            if ( _tyCharTraits::s_tcComma != tchCur )
                THROWBADJSONSTREAM( "JsonReadCursor::FNextElement(): Found [%TC] when looking for comma or array end.", tchCur );
        }
        m_pis->SkipWhitespace();
        m_pjrxCurrent->m_posStartValue = m_pis->PosGet();
        m_pjrxCurrent->m_posEndValue = 0; // Reset this to zero because we haven't yet read the value for this next element yet.
        // The first non-whitespace character tells us what the value type is:
        m_pjrxCurrent->m_tcFirst = m_pis->ReadChar( "JsonReadCursor::FNextElement(): EOF looking for next object/array value." );
        m_pjrxCurrent->SetValueType( _tyJsonValue::GetJvtTypeFromChar( m_pjrxCurrent->m_tcFirst ) );
        if ( ejvtJsonValueTypeCount == m_pjrxCurrent->JvtGetValueType() )
            THROWBADJSONSTREAM( "JsonReadCursor::FNextElement(): Found [%TC] when looking for value starting character.", m_pjrxCurrent->m_tcFirst );
        m_pjrxCurrent->m_pjrxNext->m_posEndValue = m_pis->PosGet(); // Update this as we iterate though there is no real reason to - might help with debugging.
        return true;
    }
        
    // Attach to the root of the JSON value tree.
    // We merely figure out the type of the value at this position.
    void AttachRoot( t_tyJsonInputStream & _ris )
    {
        Assert( !FAttached() ); // We shouldn't have attached to the stream yet.
        std::unique_ptr< _tyJsonValue > pjvRootVal = std::make_unique<_tyJsonValue>();
        std::unique_ptr< _tyJsonReadContext > pjrxRoot = std::make_unique<_tyJsonReadContext>( &*pjvRootVal, (_tyJsonReadContext*)nullptr );
        _ris.SkipWhitespace();
        pjrxRoot->m_posStartValue = _ris.PosGet();
        Assert( !pjrxRoot->m_posEndValue ); // We should have a 0 now - unset - must be >0 when set (invariant).
    
        // The first non-whitespace character tells us what the value type is:
        pjrxRoot->m_tcFirst = _ris.ReadChar( "JsonReadCursor::AttachRoot(): Empty JSON file." );
        pjvRootVal->SetValueType( _tyJsonValue::GetJvtTypeFromChar( pjrxRoot->m_tcFirst ) );
        if ( ejvtJsonValueTypeCount == pjvRootVal->JvtGetValueType() )
            THROWBADJSONSTREAM( "JsonReadCursor::AttachRoot(): Found [%TC] when looking for value starting character.", pjrxRoot->m_tcFirst );
        // Set up current state:
        m_pjrxContextStack.swap( pjrxRoot ); // swap with any existing stack.
        m_pjrxCurrent = &*m_pjrxContextStack; // current position is soft reference.
        m_pjrxRootVal.swap( pjvRootVal ); // swap in root value for tree.
        m_pis = &_ris;
    }

    bool FMoveDown() const
    {
        return const_cast< _tyThis * >( this )->FMoveDown(); // a little hacky but also the easiest.
    }
    bool FMoveDown()
    {
        Assert( FAttached() ); // We should have been attached to a file by now.

        if ( !FAtAggregateValue() )
            return false; // Then we are at a leaf value.

        // If we have a parent then that is where we need to go - nothing to do besides that:
        if ( m_pjrxCurrent->m_pjrxPrev )
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
        Assert( !m_pjrxCurrent->m_posEndValue );

        // We should be at the start of the value plus 1 character - this is important as we will be registered with the input stream throughout the streaming.
        Assert( ( m_pjrxCurrent->m_posStartValue + sizeof(_tyChar) ) == m_pis->PosGet() );
        m_pis->SkipWhitespace();

        std::unique_ptr< _tyJsonReadContext > pjrxNewRoot;
        if ( ejvtObject == m_pjrxCurrent->JvtGetValueType() )
        {
            // For valid JSON we may see 2 different things here:
            // 1) '"': Indicates we have a label for the first (key,value) pair of the object.
            // 2) '}': Indicates that we have an empty object.
            _tyChar tchCur = m_pis->ReadChar( "JsonReadCursor::FMoveDown(): EOF looking for first character of an object." ); // throws on eof.
            if ( ( _tyCharTraits::s_tcRightCurlyBr != tchCur ) && ( _tyCharTraits::s_tcDoubleQuote != tchCur ) )
                THROWBADJSONSTREAM( "JsonReadCursor::FMoveDown(): Found [%TC] when looking for first character of object.", tchCur );
            
            // Then first value inside of the object. We must create a JsonObject that will be used to manage the iteration of the set of values within it.
            _tyJsonObject * pjoNew = m_pjrxCurrent->PJvGet()->PCreateJsonObject();
            pjrxNewRoot = std::make_unique<_tyJsonReadContext>( &pjoNew->RJvGet(), nullptr );
            if ( _tyCharTraits::s_tcDoubleQuote == tchCur )
            {
                _tyStdStr strFirstKey;
                _ReadString( strFirstKey ); // Might throw for any number of reasons. This may be the empty string.
                pjoNew->SwapKey( strFirstKey );
                m_pis->SkipWhitespace();
                tchCur = m_pis->ReadChar( "JsonReadCursor::FMoveDown(): EOF looking for colon on first object pair." ); // throws on eof.
                if ( _tyCharTraits::s_tcColon != tchCur )
                    THROWBADJSONSTREAM( "JsonReadCursor::FMoveDown(): Found [%TC] when looking for colon on first object pair.", tchCur );
                m_pis->SkipWhitespace();
                pjrxNewRoot->m_posStartValue = m_pis->PosGet();
                Assert( !pjrxNewRoot->m_posEndValue ); // We should have a 0 now - unset - must be >0 when set (invariant).
                // The first non-whitespace character tells us what the value type is:
                pjrxNewRoot->m_tcFirst = m_pis->ReadChar( "JsonReadCursor::FMoveDown(): EOF looking for first object value." );
                pjrxNewRoot->SetValueType( _tyJsonValue::GetJvtTypeFromChar( pjrxNewRoot->m_tcFirst ) );
                if ( ejvtJsonValueTypeCount == pjrxNewRoot->JvtGetValueType() )
                    THROWBADJSONSTREAM( "JsonReadCursor::FMoveDown(): Found [%TC] when looking for value starting character.", pjrxNewRoot->m_tcFirst );
            }
            else
            {
                // We are an empty object but we need to push ourselves onto the context stack anyway because then things all work the same.
                pjrxNewRoot->m_posEndValue = pjrxNewRoot->m_posStartValue = m_pis->PosGet();
                pjrxNewRoot->SetValueType( ejvtEndOfObject ); // Use special value type to indicate we are at the "end of the set of objects".
            }
        }
        else // Array.
        {
            // For valid JSON we may see 2 different things here:
            // 1) ']': Indicates that we have an empty object.
            // 2) A valid "value starting" character indicates the start of the first value of the array.
            _tyFilePos posStartValue = m_pis->PosGet();
            _tyChar tchCur = m_pis->ReadChar( "JsonReadCursor::FMoveDown(): EOF looking for first character of an array." ); // throws on eof.
            EJsonValueType jvtCur = _tyJsonValue::GetJvtTypeFromChar( tchCur );
            if ( ( _tyCharTraits::s_tcRightSquareBr != tchCur ) && ( ejvtJsonValueTypeCount == jvtCur ) )
                THROWBADJSONSTREAM( "JsonReadCursor::FMoveDown(): Found [%TC] when looking for first char of array value.", tchCur );
            
            // Then first value inside of the object. We must create a JsonObject that will be used to manage the iteration of the set of values within it.
            _tyJsonArray * pjaNew = m_pjrxCurrent->PJvGet()->PCreateJsonArray();
            pjrxNewRoot = std::make_unique<_tyJsonReadContext>( &pjaNew->RJvGet(), nullptr );
            if ( ejvtJsonValueTypeCount != jvtCur )
            {
                pjrxNewRoot->m_posStartValue = posStartValue;
                Assert( !pjrxNewRoot->m_posEndValue ); // We should have a 0 now - unset - must be >0 when set (invariant).
                // The first non-whitespace character tells us what the value type is:
                pjrxNewRoot->m_tcFirst = tchCur;
                pjrxNewRoot->SetValueType( jvtCur );
            }
            else
            {
                // We are an empty object but we need to push ourselves onto the context stack anyway because then things all work the same.
                pjrxNewRoot->m_posEndValue = pjrxNewRoot->m_posStartValue = m_pis->PosGet();
                pjrxNewRoot->SetValueType( ejvtEndOfArray ); // Use special value type to indicate we are at the "end of the set of objects".
            }
        }
    
        // Indicate that we have the end position of the aggregate we moved into:
        m_pjrxCurrent->m_posEndValue = m_pis->PosGet();
        // Push the new context onto the context stack:
        _tyJsonReadContext::PushStack( m_pjrxContextStack, pjrxNewRoot );
        m_pjrxCurrent = &*m_pjrxContextStack; // current position is soft reference.
        return true; // We did go down.
    }

    // Move up in the context - this is significantly easier to implement than FMoveDown().
    bool FMoveUp() const
    {
        return const_cast< _tyThis * >( this )->FMoveUp(); // a little hacky but also the easiest.
    }
    bool FMoveUp()
    {
        Assert( FAttached() ); // We should have been attached to a file by now.
        if ( !!m_pjrxCurrent->m_pjrxNext )
        {
            m_pjrxCurrent = &*m_pjrxCurrent->m_pjrxNext;
            return true;
        }
        return false; // ain't nowhere to go.
    }

    void swap( JsonReadCursor & _r )
    {
        AssertValid();
        _r.AssertValid();
        std::swap( m_pis, _r.m_pis );
        std::swap( m_pjrxCurrent, _r.m_pjrxCurrent );
        m_pjrxRootVal.swap( _r.m_pjrxRootVal );
        m_pjrxContextStack.swap( _r.m_pjrxContextStack );
    }

protected:
    _tyJsonInputStream * m_pis{}; // Soft reference to stream from which we read.
    std::unique_ptr< _tyJsonValue > m_pjrxRootVal; // Hard reference to the root value of the value tree.
    std::unique_ptr< _tyJsonReadContext > m_pjrxContextStack; // Implement a simple doubly linked list.
    _tyJsonReadContext * m_pjrxCurrent{}; // The current cursor position within context stack.
};

// Helper/example methods - these are use in jsonpp.cpp:
namespace n_JSONStream
{

// Input only method - just read the file and don't do anything with the data.
template < class t_tyJsonInputStream >
void StreamReadJsonValue( JsonReadCursor< t_tyJsonInputStream > & _jrc )
{
    if ( _jrc.FAtAggregateValue() )
    {
        // save state, move down, and recurse.
        typename JsonReadCursor< t_tyJsonInputStream >::_tyJsonRestoreContext jrx( _jrc ); // Restore to current context on destruct.
        bool f = _jrc.FMoveDown();
        Assert(f);
        for ( ; !_jrc.FAtEndOfAggregate(); (void)_jrc.FNextElement() )
            StreamReadJsonValue( _jrc );
    }
    else
    {
        // discard value.
        _jrc.SkipTopContext();
    }
}

// Input/output method.
template < class t_tyJsonInputStream, class t_tyJsonOutputStream >
void StreamReadWriteJsonValue( JsonReadCursor< t_tyJsonInputStream > & _jrc, JsonValueLife< t_tyJsonOutputStream > & _jvl )
{
    if ( _jrc.FAtAggregateValue() )
    {
        // save state, move down, and recurse.
        typename JsonReadCursor< t_tyJsonInputStream >::_tyJsonRestoreContext jrx( _jrc ); // Restore to current context on destruct.
        bool f = _jrc.FMoveDown();
        Assert(f);
        for ( ; !_jrc.FAtEndOfAggregate(); (void)_jrc.FNextElement() )
        {
            if ( ejvtObject == _jvl.JvtGetValueType() )
            {
                // For a value inside of an object we have to act specially because there will be a label to this value.
                typename JsonReadCursor< t_tyJsonInputStream >::_tyStdStr strKey;
                EJsonValueType jvt;
                bool fGotKey = _jrc.FGetKeyCurrent( strKey, jvt );
                Assert( fGotKey ); // We should get a key here always and really we should throw if not.
                JsonValueLife< t_tyJsonOutputStream > jvlObjectElement( _jvl, strKey.c_str(), _jrc.JvtGetValueType() );
                StreamReadWriteJsonValue( _jrc, jvlObjectElement );
            }
            else // array.
            {
                JsonValueLife< t_tyJsonOutputStream > jvlArrayElement( _jvl, _jrc.JvtGetValueType() );
                StreamReadWriteJsonValue( _jrc, jvlArrayElement );
            }
        }
    }
    else
    {
        // Copy the value into the output stream.
        typename JsonValueLife< t_tyJsonOutputStream >::_tyJsonValue jvValue; // Make a copy of the current 
        _jrc.GetValue( jvValue );
        _jvl.SetValue( std::move( jvValue ) );
    }
}

struct JSONUnitTestContext
{
    bool m_fSkippedSomething{false};
    bool m_fSkipNextArray{false};
    int m_nArrayIndexSkip{-1};
};

// Input/output method.
template < class t_tyJsonInputStream, class t_tyJsonOutputStream >
void StreamReadWriteJsonValueUnitTest( JsonReadCursor< t_tyJsonInputStream > & _jrc, JsonValueLife< t_tyJsonOutputStream > & _jvl, JSONUnitTestContext & _rjutx )
{
    if ( _jrc.FAtAggregateValue() )
    {
        // save state, move down, and recurse.
        typename JsonReadCursor< t_tyJsonInputStream >::_tyJsonRestoreContext jrx( _jrc ); // Restore to current context on destruct.
        bool f = _jrc.FMoveDown();
        Assert(f);
        for ( ; !_jrc.FAtEndOfAggregate(); (void)_jrc.FNextElement() )
        {
            if ( ejvtObject == _jvl.JvtGetValueType() )
            {
                // For a value inside of an object we have to act specially because there will be a label to this value.
                typename JsonReadCursor< t_tyJsonInputStream >::_tyStdStr strKey;
                EJsonValueType jvt;
                bool fGotKey = _jrc.FGetKeyCurrent( strKey, jvt );
                if ( _rjutx.m_fSkipNextArray && ( ejvtArray == jvt ) )
                {
                    _rjutx.m_fSkipNextArray = false;
                    continue; // Skip the entire array.
                }
                if ( strKey == "skip")
                {   
                    _rjutx.m_fSkippedSomething = true;
                    continue; // Skip this potentially complex value to test skipping input.
                }
                else
                if ( strKey == "skipdownup")
                {   
                    if ( _jrc.FMoveDown() )
                        (void)_jrc.FMoveUp();
                    _rjutx.m_fSkippedSomething = true;
                    continue; // Skip this potentially complex value to test skipping input.
                }
                else
                if ( strKey == "skipleafup")
                {  
                    int nMoveDown = 0;
                    while ( _jrc.FMoveDown() )
                        ++nMoveDown;
                    while ( nMoveDown-- )
                        (void)_jrc.FMoveUp();
                    _rjutx.m_fSkippedSomething = true;
                    continue; // Skip this potentially complex value to test skipping input.
                }
                else
                if ( strKey == "skiparray")
                {
                    // We expect an integer specifying which index in the next array we encounter to skip.
                    // We will also skip this key,value pair.
                    if ( _jrc.JvtGetValueType() == ejvtNumber )
                        _jrc.GetValue( _rjutx.m_nArrayIndexSkip );
                    else
                    if ( _jrc.JvtGetValueType() == ejvtTrue )
                        _rjutx.m_fSkipNextArray = true;
                    
                    _rjutx.m_fSkippedSomething = true;
                    continue; // Skip this potentially complex value to test skipping input.
                }

                Assert( fGotKey ); // We should get a key here always and really we should throw if not.
                JsonValueLife< t_tyJsonOutputStream > jvlObjectElement( _jvl, strKey.c_str(), _jrc.JvtGetValueType() );
                StreamReadWriteJsonValueUnitTest( _jrc, jvlObjectElement, _rjutx );
            }
            else // array.
            {
                if ( _jvl.NSubValuesWritten() == _rjutx.m_nArrayIndexSkip )
                {
                    _rjutx.m_nArrayIndexSkip = -1;
                    continue;
                }                
                JsonValueLife< t_tyJsonOutputStream > jvlArrayElement( _jvl, _jrc.JvtGetValueType() );
                StreamReadWriteJsonValueUnitTest( _jrc, jvlArrayElement, _rjutx );
            }
        }
    }
    else
    {
        // Copy the value into the output stream.
        typename JsonValueLife< t_tyJsonOutputStream >::_tyJsonValue jvValue; // Make a copy of the current 
        _jrc.GetValue( jvValue );
        _jvl.SetValue( std::move( jvValue ) );
    }
}

template < class t_tyJsonInputStream, class t_tyJsonOutputStream >
struct StreamJSON
{
    typedef t_tyJsonInputStream _tyJsonInputStream;
    typedef t_tyJsonOutputStream _tyJsonOutputStream;
    typedef typename _tyJsonInputStream::_tyCharTraits _tyCharTraits;
    static_assert( std::is_same_v< _tyCharTraits, typename _tyJsonOutputStream::_tyCharTraits > );
    typedef JsonFormatSpec< _tyCharTraits > _tyJsonFormatSpec;
    typedef JsonReadCursor< _tyJsonInputStream > _tyJsonReadCursor;
    typedef JsonValueLife< _tyJsonOutputStream > _tyJsonValueLife;
    typedef std::pair< const char *, int > _tyPrFilenameFd;

    static void Stream( const char * _pszInputFile, _tyPrFilenameFd _prfnfdOutput, bool _fReadOnly, bool _fCheckSkippedKey, const _tyJsonFormatSpec * _pjfs )
    {
        _tyJsonInputStream jis;
        jis.Open( _pszInputFile );
        _tyJsonReadCursor jrc;
        jis.AttachReadCursor( jrc );

        if ( _fReadOnly )
            n_JSONStream::StreamReadJsonValue( jrc ); // Read the value at jrc - more specifically stream in the value.
        else
        {
            // Open the write file to which we will be streaming JSON.
            _tyJsonOutputStream jos;
            if ( !!_prfnfdOutput.first )
                jos.Open( _prfnfdOutput.first ); // Open by default will truncate the file.
            else
                jos.AttachFd( _prfnfdOutput.second );
            _tyJsonValueLife jvl( jos, jrc.JvtGetValueType(), _pjfs );
            if ( _fCheckSkippedKey )
            {
                n_JSONStream::JSONUnitTestContext rjutx;
                n_JSONStream::StreamReadWriteJsonValueUnitTest( jrc, jvl, rjutx );
            }
            else
                n_JSONStream::StreamReadWriteJsonValue( jrc, jvl ); // Read the value at jrc - more specifically stream in the value.
        }
    }
    static void Stream( int _fdInput, _tyPrFilenameFd _prfnfdOutput, bool _fReadOnly, bool _fCheckSkippedKey, const _tyJsonFormatSpec * _pjfs )
    {
        _tyJsonInputStream jis;
        jis.AttachFd( _fdInput );
        _tyJsonReadCursor jrc;
        jis.AttachReadCursor( jrc );

        if ( _fReadOnly )
            n_JSONStream::StreamReadJsonValue( jrc ); // Read the value at jrc - more specifically stream in the value.
        else
        {
            // Open the write file to which we will be streaming JSON.
            _tyJsonOutputStream jos;
            if ( !!_prfnfdOutput.first )
                jos.Open( _prfnfdOutput.first ); // Open by default will truncate the file.
            else
                jos.AttachFd( _prfnfdOutput.second );
            _tyJsonValueLife jvl( jos, jrc.JvtGetValueType(), _pjfs );
            if ( _fCheckSkippedKey )
            {
                n_JSONStream::JSONUnitTestContext rjutx;
                n_JSONStream::StreamReadWriteJsonValueUnitTest( jrc, jvl, rjutx );
            }
            else
                n_JSONStream::StreamReadWriteJsonValue( jrc, jvl ); // Read the value at jrc - more specifically stream in the value.
        }
    }
    static void Stream( const char * _pszInputFile, const char * _pszOutputFile, bool _fReadOnly, bool _fCheckSkippedKey, const _tyJsonFormatSpec * _pjfs )
    {
        _tyJsonInputStream jis;
        jis.Open( _pszInputFile );
        _tyJsonReadCursor jrc;
        jis.AttachReadCursor( jrc );

        if ( _fReadOnly )
            n_JSONStream::StreamReadJsonValue( jrc ); // Read the value at jrc - more specifically stream in the value.
        else
        {
            // Open the write file to which we will be streaming JSON.
            _tyJsonOutputStream jos;
            jos.Open( _pszOutputFile ); // Open by default will truncate the file.
            _tyJsonValueLife jvl( jos, jrc.JvtGetValueType(), _pjfs );
            if ( _fCheckSkippedKey )
            {
                n_JSONStream::JSONUnitTestContext rjutx;
                n_JSONStream::StreamReadWriteJsonValueUnitTest( jrc, jvl, rjutx );
            }
            else
                n_JSONStream::StreamReadWriteJsonValue( jrc, jvl ); // Read the value at jrc - more specifically stream in the value.
        }
    }
    static void Stream( int _fdInput, const char * _pszOutputFile, bool _fReadOnly, bool _fCheckSkippedKey, const _tyJsonFormatSpec * _pjfs )
    {
        _tyJsonInputStream jis;
        jis.AttachFd( _fdInput );
        _tyJsonReadCursor jrc;
        jis.AttachReadCursor( jrc );

        if ( _fReadOnly )
            n_JSONStream::StreamReadJsonValue( jrc ); // Read the value at jrc - more specifically stream in the value.
        else
        {
            // Open the write file to which we will be streaming JSON.
            _tyJsonOutputStream jos;
            jos.Open( _pszOutputFile ); // Open by default will truncate the file.
            _tyJsonValueLife jvl( jos, jrc.JvtGetValueType(), _pjfs );
            if ( _fCheckSkippedKey )
            {
                n_JSONStream::JSONUnitTestContext rjutx;
                n_JSONStream::StreamReadWriteJsonValueUnitTest( jrc, jvl, rjutx );
            }
            else
                n_JSONStream::StreamReadWriteJsonValue( jrc, jvl ); // Read the value at jrc - more specifically stream in the value.
        }
    }
};

} // namespace n_JSONStream
