#pragma once

#include <string>
#include <memory>
#include <unistd.h>
#include <stdexcept>
#include <fcntl.h>

#include "bienutil.h"
#include "_namdexc.h"
#include "_debug.h"
#include "_util.h"
#include "_dbgthrw.h"

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
        for ( const char * pcFmtCur = _pcFmt; !!*_pcFmt && ( pcBufCur != pcBufTail ); ++_pcFmt )
        {
            if ( ( pcFmtCur[0] == '%' ) && ( pcFmtCur[1] == 'T' ) && ( pcFmtCur[0] == 'C' ) )
            {
                const char * pcCharFmtCur = _tyCharTraits::s_szFormatChar;
                for ( ; !!*pcCharFmtCur && ( pcBufCur != pcBufTail ); ++pcCharFmtCur )
                    *pcBufCur++ = *pcCharFmtCur;
            }
            else
                *pcBufCur++ = *_pcFmt;
        }
        *pcBufTail = 0;

        _TyBase::RenderVA( rgcBuf, args ); // Render into the exception description buffer.
    }
};
// By default we will always add the __FILE__, __LINE__ even in retail for debugging purposes.
#define THROWBADJSONSTREAM( MESG, ARGS... ) ExceptionUsage<bad_json_stream_exception<_tyCharTraits>>::ThrowFileLine( __FILE__, __LINE__, MESG, ARGS )
#define THROWBADJSONSTREAMERRNO( ERRNO, MESG, ARGS... ) ExceptionUsage<bad_json_stream_exception<_tyCharTraits>>::ThrowFileLineErrno( __FILE__, __LINE__, ERRNO, MESG, ARGS )

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
        for ( const char * pcFmtCur = _pcFmt; !!*_pcFmt && ( pcBufCur != pcBufTail ); ++_pcFmt )
        {
            if ( ( pcFmtCur[0] == '%' ) && ( pcFmtCur[1] == 'T' ) && ( pcFmtCur[0] == 'C' ) )
            {
                const char * pcCharFmtCur = _tyCharTraits::s_szFormatChar;
                for ( ; !!*pcCharFmtCur && ( pcBufCur != pcBufTail ); ++pcCharFmtCur )
                    *pcBufCur++ = *pcCharFmtCur;
            }
            else
                *pcBufCur++ = *_pcFmt;
        }
        *pcBufTail = 0;

        RenderVA( rgcBuf, args ); // Render into the exception description buffer.
    }
};
// By default we will always add the __FILE__, __LINE__ even in retail for debugging purposes.
#define THROWBADJSONSEMANTICUSE( MESG... ) ExceptionUsage<json_stream_bad_semantic_usage_exception<_tyCharTraits>>::ThrowFileLine( __FILE__, __LINE__, MESG )

#ifdef __NAMDDEXC_STDBASE
#pragma pop_macro("std")
#endif //__NAMDDEXC_STDBASE

// JsonCharTraits< char > : Traits for 8bit char representation.
template <>
struct JsonCharTraits< char >
{
    typedef char _tyChar;
    typedef _tyChar * _tyLPSTR;
    typedef const _tyChar * _tyLPCSTR;
    typedef std::basic_string< _tyChar > _tyStdStr;

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
    static _tyLPCSTR s_szWhiteSpace; // = " \t\n\r";
    static bool FIsWhitespace( _tyChar _tc )
    {
        return ( ( ' ' == _tc ) || ( '\t' == _tc ) || ( '\n' == _tc ) || ( '\r' == _tc ) );
    }

    // The formatting command to format a single character using printf style.
    static const char * s_szFormatChar; // = "%c";
};

JsonCharTraits< char >::_tyLPCSTR JsonCharTraits< char >::s_szWhiteSpace = " \t\n\r";
const char * JsonCharTraits< char >::s_szFormatChar = "%c";

// JsonCharTraits< wchar_t > : Traits for 16bit char representation.
template <>
struct JsonCharTraits< wchar_t >
{
    typedef wchar_t _tyChar;
    typedef _tyChar * _tyLPSTR;
    typedef const _tyChar * _tyLPCSTR;
    using _tyStdStr = std::basic_string< _tyChar >;

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
    static _tyLPCSTR s_szWhiteSpace; // = L" \t\n\r";
    static bool FIsWhitespace( _tyChar _tc )
    {
        return ( ( L' ' == _tc ) || ( L'\t' == _tc ) || ( L'\n' == _tc ) || ( L'\r' == _tc ) );
    }

    // The formatting command to format a single character using printf style.
    static const char * s_szFormatChar; // = "%lc";
};

JsonCharTraits< wchar_t >::_tyLPCSTR JsonCharTraits< wchar_t >::s_szWhiteSpace = L" \t\n\r";
const char * JsonCharTraits< wchar_t >::s_szFormatChar = "%lc";

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
template < class t_tyCharTraits >
class JsonLinuxInputStream : public JsonInputStreamBase< t_tyCharTraits, ssize_t >
{
    typedef JsonLinuxInputStream _tyThis;
public:
    typedef t_tyCharTraits _tyCharTraits;
    typedef typename _tyCharTraits::_tyChar _tyChar;
    typedef ssize_t _tyFilePos;
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
        assert( !FOpened() );
        m_fd = open( _szFilename, O_RDONLY );
        if ( !FOpened() )
            THROWBADJSONSTREAMERRNO( errno, "JsonLinuxInputStream::Open(): Unable to open() file [%s]", _szFilename );
        m_fHasLookahead = false; // ensure that if we had previously been opened that we don't think we still have a lookahead.
        m_fOwnFdLifetime = true; // This object owns the lifetime of m_fd - ie. we need to close upon destruction.
        m_szFilename = _szFilename; // For error reporting and general debugging. Of course we don't need to store this.
    }

    // Attach to an FD whose lifetime we do not own. This can be used, for instance, to attach to stdin which is usually at FD 0 unless reopen()ed.
    void AttachFd( int _fd )
    {
        assert( !FOpened() );
        assert( _fd != -1 );
        m_fd = _fd;
        m_fHasLookahead = false; // ensure that if we had previously been opened that we don't think we still have a lookahead.
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

    // Attach to this FOpened() JsonLinuxInputStream.
    void AttachReadCursor( _tyJsonReadCursor & _rjrc )
    {
        assert( !_rjrc.FAttached() );
        assert( FOpened() );
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
            sstRead = read( m_fd, &m_tcLookahead, sizeof m_tcLookahead );
            if ( -1 == sstRead )
                THROWBADJSONSTREAMERRNO( errno, "JsonLinuxInputStream::SkipWhitespace(): read() failed for file [%s]", m_szFilename.c_str() );
            if ( sstRead != sizeof m_tcLookahead )
                THROWBADJSONSTREAMERRNO( errno, "JsonLinuxInputStream::SkipWhitespace(): read() for file [%s] had [%d] leftover bytes.", m_szFilename.c_str(), sstRead );
            
            if ( !sstRead )
            {
                m_fHasLookahead = false;
                return; // We have skipped the whitespace until we hit EOF.
            }
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
        assert( FOpened() );
        _tyFilePos pos = lseek( m_fd, 0, SEEK_CUR );
        if ( -1 == pos )
            THROWBADJSONSTREAMERRNO( errno, "JsonLinuxInputStream::PosGet(): lseek() failed for file [%s]", m_szFilename.c_str() );
        if ( m_fHasLookahead )
            pos -= sizeof m_tcLookahead; // Since we have a lookahead we are actually one character before.
        return pos;
    }
    // Read a single character from the file - always throw on EOF.
    _tyChar ReadChar( const char * _pcEOFMessage )
    {
        assert( FOpened() );
        if ( m_fHasLookahead )
        {
            m_fHasLookahead = false;
            return m_tcLookahead;
        }
        ssize_t sstRead = read( m_fd, &m_tcLookahead, sizeof m_tcLookahead );
        if ( sstRead != sizeof m_tcLookahead )
        {
            if ( -1 == sstRead )
                THROWBADJSONSTREAMERRNO( errno, "JsonLinuxInputStream::ReadChar(): read() failed for file [%s]", m_szFilename.c_str() );
            else
            {
                assert( !sstRead ); // For multibyte characters this could fail but then we would have a bogus file anyway and would want to throw.
                THROWBADJSONSTREAM( "[%s]: %s", m_szFilename.c_str(), _pcEOFMessage );
            }
        }
        assert( !m_fHasLookahead );
        return m_tcLookahead;
    }
    
    bool FReadChar( _tyChar & _rtch, bool _fThrowOnEOF, const char * _pcEOFMessage )
    {
        assert( FOpened() );
        if ( m_fHasLookahead )
        {
            m_fHasLookahead = false;
            return m_tcLookahead;
        }
        ssize_t sstRead = read( m_fd, &m_tcLookahead, sizeof m_tcLookahead );
        if ( sstRead != sizeof m_tcLookahead )
        {
            if ( -1 == sstRead )
                THROWBADJSONSTREAMERRNO( errno, "JsonLinuxInputStream::FReadChar(): read() failed for file [%s]", m_szFilename.c_str() );
            else
            if ( !!sstRead )
                THROWBADJSONSTREAMERRNO( errno, "JsonLinuxInputStream::FReadChar(): read() for file [%s] had [%d] leftover bytes.", m_szFilename.c_str(), sstRead );
            else
            {
                if ( _fThrowOnEOF )
                    THROWBADJSONSTREAM( "[%s]: %s", m_szFilename.c_str(), _pcEOFMessage );
                return false;
            }
        }
        return true;
    }

    void PushBackLastChar()
    {
        assert( !m_fHasLookahead ); // support single character lookahead only.
        m_fHasLookahead = true;
    }

protected:
    std::basic_string<char> m_szFilename;
    int m_fd{-1}; // file descriptor.
    _tyChar m_tcLookahead{0}; // Everytime we read a character we put it in the m_tcLookahead and clear that we have a lookahead.
    bool m_fHasLookahead{false};
    bool m_fOwnFdLifetime{false}; // Should we close the FD upon destruction?
};

// _EJsonValueType: The types of values in a JsonValue object.
enum _EJsonValueType : char
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

    // Note that this JsonValue very well may be referred to by a parent JsonValue.
    // JsonValues must be destructed carefully.

    JsonValue( const JsonValue& ) = delete;
    JsonValue& operator=( const JsonValue& ) = delete;

    JsonValue( JsonValue * _pjvParent = nullptr, EJsonValueType _jvtType = ejvtJsonValueTypeCount )
        :   m_pjvParent( _pjvParent ),
            m_jvtType( _jvtType )
    {
        assert( !m_pvValue ); // Should have been zeroed by default member initialization on site.
    }
    JsonValue( JsonValue && _rr )
    {
        assert( FIsNull() );
        swap( _rr );
    }
    ~JsonValue()
    {
        if ( m_pvValue )
            _DestroyValue();
    }

    JsonValue & operator = ( JsonValue && _rr )
    {
        Destroy();
        swap( _rr );
    }
    void swap( JsonValue && _rr )
    {
        std::swap( _rr.m_pjvParent, m_pjvParent );
        std::swap( _rr.m_pvValue, m_pvValue );
        std::swap( _rr.m_jvtType, m_jvtType );
    }
    void Destroy() // Destroy and make null.
    {
        if ( m_pvValue )
        {
            _DestroyValue();
            m_pjvParent = 0;
            m_jvtType = ejvtJsonValueTypeCount;
        }
        assert( FIsNull() );
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

    // We assume that we have sole use of this input stream - as any such sharing would need complex guarding otherwise.
    // We should never need to backtrack when reading from the stream - we may need to fast forward for sure but not backtrack.
    // As such we should always be right at the point we want to be within the file and should never need to seek backward to reach the point we need to.
    // This means that we shouldn't need to actually store any position context within the read cursor since the presumption is that we are always where we
    //  should be. We will store this info in debug and use this to ensure that we are where we think we should be.
    template < class t_tyInputStream, class t_tyJsonReadCursor >
    void AttachInputStream( t_tyInputStream & _ris, t_tyJsonReadCursor & _rjrc )
    {
        assert( FIsNull() ); // We should be null here - we should be at the root and have no parent.
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

    _tyJsonObject * PCreateJsonObject()
    {
        assert( ejvtObject == m_jvtType );
        _CreateValue();
        return ( ejvtObject == m_jvtType ) ? (_tyJsonObject*)m_pvValue : 0;
    }
    _tyJsonObject * PGetJsonObject()
    {
        assert( ejvtObject == m_jvtType );
        if ( !m_pvValue )
            _CreateValue();
        return ( ejvtObject == m_jvtType ) ? (_tyJsonObject*)m_pvValue : 0;
    }
    _tyJsonArray * PCreateJsonArray()
    {
        assert( ejvtArray == m_jvtType );
        _CreateValue();
        return ( ejvtArray == m_jvtType ) ? (_tyJsonArray*)m_pvValue : 0;
    }
    _tyJsonArray * PGetJsonArray()
    {
        assert( ejvtArray == m_jvtType );
        if ( !m_pvValue )
            _CreateValue();
        return ( ejvtArray == m_jvtType ) ? (_tyJsonArray*)m_pvValue : 0;
    }
    _tyStdStr * PCreateStringValue()
    {
        assert( ( ejvtNumber == m_jvtType ) || ( ejvtString == m_jvtType ) );
        _CreateValue();
        return ( ( ejvtNumber == m_jvtType ) || ( ejvtString == m_jvtType ) ) ? (_tyStdStr*)m_pvValue : 0;
    }
    _tyStdStr * PGetStringValue()
    {
        assert( ( ejvtNumber == m_jvtType ) || ( ejvtString == m_jvtType ) );
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
            assert( 0 );
            break;
        }
    }
    void _CreateValue()
    {
        assert( !m_pvValue );
        // We need to create a connected value object depending on type:
        switch( m_jvtType )
        {
        case ejvtObject:
            assert( !!m_pjvParent );
            m_pvValue = new _tyJsonObject( m_pjvParent );
            break;
        case ejvtArray:
            assert( !!m_pjvParent );
            m_pvValue = new _tyJsonArray( m_pjvParent );
            break;
        case ejvtNumber:
        case ejvtString:
            m_pvValue = new _tyStdStr();
            break;
        case ejvtTrue:
        case ejvtFalse:
        case ejvtNull:
        default:
            assert( 0 ); // Shouldn't be calling for cases for which there is no value.
            break;
        }
    }

    const JsonValue * m_pjvParent{}; // We make this const and then const_cast<> as needed.
    void * m_pvValue{}; // Just use void* since all our values are pointers to objects.
    // m_pvValue is one of:
    // for ejvtTrue, ejvtFalse, ejvtNull: nullptr
    // for ejvtObject: JsonObject *
    // for ejvtArray: JsonArray *
    // for ejvtNumber, ejvtString: _tyStdStr *
    EJsonValueType m_jvtType{ejvtJsonValueTypeCount}; // Type of the JsonValue.
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
    using _tyBase::RJvGet;
    using _tyBase::NElement;
    using _tyBase::SetNElement;
    using _tyBase::IncElement;

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
        assert( !!m_pjvCur );
        return m_pjvCur->PGetJsonObject();
    }
    _tyJsonArray * PGetJsonArray() const
    {
        assert( !!m_pjvCur );
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
        assert( !!m_pjvCur );
        if ( !m_pjvCur )
            return ejvtJsonValueTypeCount;
        return m_pjvCur->JvtGetValueType();
    }
    void SetValueType( EJsonValueType _jvt )
    {
        assert( !!m_pjvCur );
        if ( !!m_pjvCur )
            m_pjvCur->SetValueType( _jvt );
    }

    _tyStdStr * PGetStringValue() const
    {
        assert( !!m_pjvCur );
        return !m_pjvCur ? 0 : m_pjvCur->PGetStringValue();
    }

    void SetEndOfIteration( _tyFilePos _pos )
    {
        m_tcFirst = 0;
        m_posStartValue = m_posEndValue = _pos;
        // The value is set to EndOfIteration separately.
    }

    // Push _pjrxNewHead as the new head of stack before _pjrxHead.
    static void PushStack( std::unique_ptr< JsonReadContext > & _pjrxHead, std::unique_ptr< JsonReadContext > & _pjrxNewHead )
    {
        if ( !!_pjrxHead )
        {
            assert( !_pjrxHead->m_pjrxPrev );
            assert( !_pjrxNewHead->m_pjrxPrev );
            assert( !_pjrxNewHead->m_pjrxNext );
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
    ~JsonRestoreContext()
    {
        if ( !!m_pjrx && !!m_pjrc )
        {
            // We cannot be throwing out of a destructor because that f's things up (for one we might be in the middle of a stack unwinding due to a previous throw).
            // So we handle any thrown exceptions locally and log.
            try
            {
                m_pjrc->MoveToContext( *m_pjrx );
            }
            catch( std::exception & rexc )
            {
                fprintf( stderr, "Exception caught in ~JsonRestoreContext(): %s.", rexc.what() );
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
            m_pjrc->MoveToContext( *m_pjrx );
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

    JsonReadCursor() = default;
    JsonReadCursor( JsonReadCursor const & ) = delete;
    JsonReadCursor& operator = ( JsonReadCursor const & ) = delete;

    void AssertValid() const 
    {
        assert( !m_pjrxCurrent == !m_pis );
        assert( !m_pjrxRootVal == !m_pis );
        assert( !m_pjrxContextStack == !m_pis );
        // Make sure that the current context is in the context stack.
        const _tyJsonReadContext * pjrxCheck = &*m_pjrxContextStack;
        for ( ; !!pjrxCheck && ( pjrxCheck != m_pjrxCurrent ); pjrxCheck = pjrxCheck->PJrcGetNext() )
            ;
        assert( !!pjrxCheck ); // If this fails then the current context is not in the content stack - this is bad.
    }
    bool FAttached() const 
    {
        AssertValid();
        return !!m_pis;
    }

    const _tyJsonReadContext & GetCurrentContext() const
    {
        assert( !!m_pjrxCurrent );
        return *m_pjrxCurrent;
    }

    // Returns true if the current value is an aggregate value - object or array.
    bool FAtAggregateValue() const 
    {
        AssertValid();
        return !!m_pjrxCurrent && 
            ( ( ejvtArray == m_pjrxCurrent->JvtGetValueType() ) || ( ejvtObject == m_pjrxCurrent->JvtGetValueType() ) );
    }
    // Return if we are at the end of the iteration of an aggregate (object or array).
    bool FAtEndOfAggregate() const 
    {
        AssertValid();
        return !!m_pjrxCurrent && 
            ( ( ejvtEndOfArray == m_pjrxCurrent->JvtGetValueType() ) || ( ejvtEndOfObject == m_pjrxCurrent->JvtGetValueType() ) );
    }

    // Get the current key if there is a current key.
    bool FGetKeyCurrent( _tyStdStr & _rstrKey, EJsonValueType & _rjvt ) const
    {
        assert( FAttached() );
        if ( FAtEndOfAggregate() )
            return false;
        // This should only be called when we are inside of an object's value.
        // So, we need to have a parent value and that parent value should correspond to an object.
        // Otherwise we throw an "invalid semantic use" exception.
        if ( !m_pjrxCurrent || !m_pjrxCurrent->m_pjrxNext || ( ejvtObject != m_pjrxCurrent->m_pjrxNext->JvtGetValueType() ) )
            THROWBADJSONSEMANTICUSE( "JsonReadCursor::FGetKeyCurrent(): Not located at an object so no key available." );
        // If the key value in the object is the empty string then this is an empty object and we return false.
        _tyJsonObject * pjoCur = m_pjrxCurrent->m_pjrxNext->PGetJsonObject();
        assert( !pjoCur->FEndOfIteration() );
        pjoCur->GetKey( _rstrKey );
        _rjvt = m_pjrxCurrent->JvtGetValueType(); // This is the value type for this key string. We may not have read the actual value yet.
        return true;
    }
    EJsonValueType JvtGetValueType() const
    {
        assert( FAttached() );
        return m_pjrxCurrent->JvtGetValueType();
    }
    bool FIsValueNull() const
    {
        ejvtNull == JvtGetValueType();
    }

    // Get a string representation of the value.
    // This is a semantic error if this is an aggregate value (object or array).
    void GetValue( _tyStdStr & _strValue ) const
    {
        assert( FAttached() );
        if ( FAtAggregateValue() )
            THROWBADJSONSEMANTICUSE( "JsonReadCursor::GetValue(string): At an aggregate value - object or array." );

        EJsonValueType jvt = JvtGetValueType();
        if ( ( ejvtEndOfObject == jvt ) || ( ejvtEndOfArray == jvt ) )
            THROWBADJSONSEMANTICUSE( "JsonReadCursor::GetValue(string): No value located at end of object or array." );
        
        // Now check if we haven't read the value yet because then we gotta read it.
        if ( !m_pjrxCurrent->m_posEndValue )
            _ReadSimpleValue();
        
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
        }
    }
    void GetValue( bool & _rf ) const
    {
        // Now check if we haven't read the value yet because then we gotta read it.
        if ( !m_pjrxCurrent->m_posEndValue )
            _ReadSimpleValue();
        
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
    void GetValue( int & _rInt ) const
    {
        // Now check if we haven't read the value yet because then we gotta read it.
        if ( !m_pjrxCurrent->m_posEndValue )
            _ReadSimpleValue();

        if ( ejvtNumber != JvtGetValueType() ) 
            THROWBADJSONSEMANTICUSE( "JsonReadCursor::GetValue(int): Not at a numeric value type." );

        // The presumption is that sscanf won't read past any decimal point if scanning a non-floating point number.
        int iRet = sscanf( "%d", &_rInt );
        assert( 1 == iRet ); // Due to the specification of number we expect this to always succeed.
    }
    void GetValue( long & _rLong ) const
    {
        // Now check if we haven't read the value yet because then we gotta read it.
        if ( !m_pjrxCurrent->m_posEndValue )
            _ReadSimpleValue();

        if ( ejvtNumber != JvtGetValueType() ) 
            THROWBADJSONSEMANTICUSE( "JsonReadCursor::GetValue(long): Not at a numeric value type." );

        // The presumption is that sscanf won't read past any decimal point if scanning a non-floating point number.
        int iRet = sscanf( "%ld", &_rLong );
        assert( 1 == iRet ); // Due to the specification of number we expect this to always succeed.
    }
    void GetValue( float & _rFloat ) const
    {
        // Now check if we haven't read the value yet because then we gotta read it.
        if ( !m_pjrxCurrent->m_posEndValue )
            _ReadSimpleValue();

        if ( ejvtNumber != JvtGetValueType() ) 
            THROWBADJSONSEMANTICUSE( "JsonReadCursor::GetValue(float): Not at a numeric value type." );

        int iRet = sscanf( "%e", &_rFloat );
        assert( 1 == iRet ); // Due to the specification of number we expect this to always succeed.
    }
    void GetValue( double & _rDouble ) const
    {
        // Now check if we haven't read the value yet because then we gotta read it.
        if ( !m_pjrxCurrent->m_posEndValue )
            _ReadSimpleValue();

        if ( ejvtNumber != JvtGetValueType() ) 
            THROWBADJSONSEMANTICUSE( "JsonReadCursor::GetValue(double): Not at a numeric value type." );

        int iRet = sscanf( "%le", &_rDouble );
        assert( 1 == iRet ); // Due to the specification of number we expect this to always succeed.
    }
    void GetValue( long double & _rLongDouble ) const
    {
        // Now check if we haven't read the value yet because then we gotta read it.
        if ( !m_pjrxCurrent->m_posEndValue )
            _ReadSimpleValue();

        if ( ejvtNumber != JvtGetValueType() ) 
            THROWBADJSONSEMANTICUSE( "JsonReadCursor::GetValue(long double): Not at a numeric value type." );

        int iRet = sscanf( "%Le", &_rLongDouble );
        assert( 1 == iRet ); // Due to the specification of number we expect this to always succeed.
    }

    // This will read any unread value into the value object
    void _ReadSimpleValue()
    {
        assert( !m_pjrxCurrent->m_posEndValue );
        EJsonValueType jvt = JvtGetValueType();
        assert( ( jvt >= ejvtFirstJsonSimpleValue ) && ( jvt <= ejvtLastJsonSpecifiedValue ) ); // Should be handled by caller.
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
            _SkipSimpleValue(); // We just skip remaining value checking that it is correct.
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
        *ptcCur++ = _rjrx.m_tcFirst;

        auto lambdaAddCharNum = [&]( _tyChar _tch )
        { 
            if ( ptcCur == rgtcBuffer + knMaxLength )
                THROWBADJSONSTREAM( "JsonReadCursor::_ReadNumber(): Overflow of max JSON number length of [%u].", knMaxLength );
            *ptcCur++ = _tch;
        }

        // We will have read the first character of a number and then will need to satisfy the state machine for the number appropriately.
        bool fZeroFirst = ( _tyCharTraits::s_tc0 == _tcFirst );
        TCHAR tchCurrentChar; // maintained as the current character.
        if ( _tyCharTraits::s_tcMinus == _tcFirst )
        {
            tchCurrentChar = m_pis->ReadChar( "JsonReadCursor::_ReadNumber(): Hit EOF looking for a digit after a minus." ); // This may not be EOF.
            *ptcCur++ = tchCurrentChar;
            if ( ( tchCurrentChar < _tyCharTraits::s_tc0 ) || ( tchCurrentChar > _tyCharTraits::s_tc9 ) )
                THROWBADJSONSTREAM( "JsonReadCursor::_ReadNumber(): Found [%TC] when looking for digit after a minus.", tchCurrentChar );
            fZeroFirst = ( _tyCharTraits::s_tc0 == tchCurrentChar );
        }
        // If we are the root element then we may encounter EOF and have that not be an error when reading some of the values.
        bool _fAtRootElement = !_rjrx->m_pjrxNext;
        if ( fZeroFirst )
        {
            bool fFoundChar = m_pis->FReadChar( tchCurrentChar, !_fAtRootElement, "JsonReadCursor::_ReadNumber(): Hit EOF looking for something after a leading zero." ); // Throw on EOF if we aren't at the root element.
            if ( !fFoundChar )
                return; // Then we read until the end of file for a JSON file containing a single number as its only element.
            *ptcCur++ = tchCurrentChar;
        }
        else // ( !fZeroFirst )
        {
            // Then we expect more digits or a period or an 'e' or an 'E' or EOF if we are the root element.
            bool fFoundChar;
            do
            {            
                bool fFoundChar = m_pis->FReadChar( tchCurrentChar, !_fAtRootElement, "JsonReadCursor::_ReadNumber(): Hit EOF looking for a non-number." ); // Throw on EOF if we aren't at the root element.
                if ( !fFoundChar )
                    return; // Then we read until the end of file for a JSON file containing a single number as its only element.
                lambdaAddCharNum( tchCurrentChar );
            } 
            while ( ( tchCurrentChar >= _tyCharTraits::s_tc0 ) && ( tchCurrentChar <= _tyCharTraits::s_tc9 ) );
        }
    
        if ( tchCurrentChar == _tyCharTraits::s_tcPeriod )
        {
            // Then according to the JSON spec we must have at least one digit here.
            tchCurrentChar = m_pis->ReadChar(); // throw on EOF.
            if ( ( tchCurrentChar < _tyCharTraits::s_tc0 ) || ( tchCurrentChar > _tyCharTraits::s_tc9 ) )
                THROWBADJSONSTREAM( "JsonReadCursor::_ReadNumber(): Found [%TC] when looking for digit after a decimal point.", tchCurrentChar );
            // Now we expect a digit, 'e', 'E', EOF or something else that our parent can check out.
            lambdaAddCharNum( tchCurrentChar );
            do
            {            
                bool fFoundChar = m_pis->FReadChar( tchCurrentChar, !_fAtRootElement, "JsonReadCursor::_ReadNumber(): Hit EOF looking for a non-number after the period." ); // Throw on EOF if we aren't at the root element.
                if ( !fFoundChar )
                    return; // Then we read until the end of file for a JSON file containing a single number as its only element.            
                lambdaAddCharNum( tchCurrentChar );
            }
            while ( ( tchCurrentChar >= _tyCharTraits::s_tc0 ) && ( tchCurrentChar <= _tyCharTraits::s_tc9 ) );
        }
        if (    ( tchCurrentChar == _tyCharTraits::s_tcE ) ||
                ( tchCurrentChar == _tyCharTraits::s_tce ) )
        {
            // Then we might see a plus or a minus or a number - but we cannot see EOF here correctly:
            tchCurrentChar = m_pis->ReadChar(); // Throws on EOF.
            lambdaAddCharNum( tchCurrentChar );
            if (    ( tchCurrentChar == _tyCharTraits::s_tcMinus ) ||
                   ( tchCurrentChar == _tyCharTraits::s_tcPlus ) )
            {
                tchCurrentChar = m_pis->ReadChar(); // Then we must still find a number after - throws on EOF.
                lambdaAddCharNum( tchCurrentChar );
            }
            // Now we must see at least one digit so for the first read we throw.
            if ( ( tchCurrentChar < _tyCharTraits::s_tc0 ) || ( tchCurrentChar > _tyCharTraits::s_tc9 ) )
                THROWBADJSONSTREAM( "JsonReadCursor::_ReadNumber(): Found [%TC] when looking for digit after a exponent indicator (e/E).", tchCurrentChar );
            // We have satisfied "at least a digit after an exponent indicator" - now we might just hit EOF or anything else after this...
            do
            {
                bool fFoundChar = m_pis->FReadChar( tchCurrentChar, !_fAtRootElement, "JsonReadCursor::_ReadNumber(): Hit EOF looking for a non-number after the exponent indicator." ); // Throw on EOF if we aren't at the root element.
                if ( !fFoundChar )
                    return; // Then we read until the end of file for a JSON file containing a single number as its only element.            
                lambdaAddCharNum( tchCurrentChar );
            } 
            while ( ( tchCurrentChar >= _tyCharTraits::s_tc0 ) && ( tchCurrentChar <= _tyCharTraits::s_tc9 ) );
        }
        m_pis->PushBackLastChar(); // Let caller read this and decide what to do depending on context - we don't expect a specific character.
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
                bool fFoundChar = m_pis->FReadChar( tchCurrentChar, !_fAtRootElement, "JsonReadCursor::_ReadNumber(): Hit EOF looking for a non-number after the exponent indicator." ); // Throw on EOF if we aren't at the root element.
                if ( !fFoundChar )
                    return; // Then we read until the end of file for a JSON file containing a single number as its only element.            
            } 
            while ( ( tchCurrentChar >= _tyCharTraits::s_tc0 ) && ( tchCurrentChar <= _tyCharTraits::s_tc9 ) );
        }
        m_pis->PushBackLastChar(); // Let caller read this and decide what to do depending on context - we don't expect a specific character here.
    }

    // Read the string starting at the current position. The '"' has already been read.
    void _ReadString( _tyStdStr & _rstrRead ) const
    {
        assert( !_rstrRead.length() ); // We will append to the string, so if there is content it will be appended to.
        const int knLenBuffer = 1023;
        _tyChar rgtcBuffer[ knLenBuffer+1 ]; // We will append a zero when writing to the string.
        rgtcBuffer[0] = 0; // preterminate
        _tyChar * ptchCur;
        int knLenUsed = 0;

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
                        unsigned int uCurrentMultiplier = ( 1 << 24 );
                        // Must find 4 hex digits:
                        for ( int n=0; n < 4; ++n, ( uCurrentMultiplier >>= 8 ) )
                        {
                            tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipRemainingString(): EOF found looking for 4 hex digits following \\u." ); // throws on EOF.
                            if ( ( tchCur >= _tyCharTraits::s_tc0 ) && ( tchCur <= _tyCharTraits::s_tc9 ) )
                                uHex += uCurrentMultiplier * ( tchCur - _tyCharTraits::s_tc0 );
                            else
                            if ( ( tchCur >= _tyCharTraits::s_tca ) && ( tchCur <= _tyCharTraits::s_tcf ) )
                                uHex += uCurrentMultiplier * ( 10 + ( tchCur - _tyCharTraits::s_tca ) );
                            else
                            if ( ( tchCur >= _tyCharTraits::s_tcA ) && ( tchCur <= _tyCharTraits::s_tcF ) )
                                uHex += uCurrentMultiplier * ( 10 + ( tchCur - _tyCharTraits::s_tcA ) );
                            else
                                THROWBADJSONSTREAM( "JsonReadCursor::_SkipRemainingString(): Found [%TC] when looking for digit following \\u.", tchCur );
                        }
                        // If we are supposed to throw on overflow then check for it, otherwise just truncate to the character type silently.
                        if ( _tyCharTraits::s_fThrowOnUnicodeOverflow && ( sizeof(_tyChar) < sizeof(uHex) ) )
                        {
                            if ( uHex >= ( 1u << ( CHAR_BIT * sizeof(_tyChar) ) ) )
                                THROWBADJSONSTREAM( "JsonReadCursor::_SkipRemainingString(): Unicode hex overflow [%u].", uHex );
                        }
                        tchCur = (_tyChar)uHex;
                    }
                    default:
                        THROWBADJSONSTREAM( "JsonReadCursor::_SkipRemainingString(): Found [%TC] when looking for competetion of backslash when reading string.", tchCur );
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
            assert(false);
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
                THROWBADJSONSTREAM( "JsonReadCursor::_SkipWholeObject(): Found [%TC] when looking for comma or array end.", tchCur );
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
            _rjrx.SetValueType( ejvtEndOfObject );
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
            tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipObject(): EOF looking for end object } or comma." ); // throws on EOF.
        }
        _rjrx.m_posEndValue = m_pis->PosGet(); // Update the end to the current end as usual.
        _rjrx.SetValueType( ejvtEndOfObject ); // Indicate that we have iterated to the end of this object.
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
            _rjrx.SetValueType( ejvtEndOfArray );
            return;
        }

        // Then we have partially iterated the object. All contexts above us are closed which means we should find
        //  either a comma or end square bracket.
        m_pis->SkipWhitespace();
        _tyChar tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipObject(): EOF looking for end array ] or comma." ); // throws on EOF.
        while ( _tyCharTraits::s_tcRightSquareBr !=tchCur )
        {
            if ( _tyCharTraits::s_tcComma != tchCur )
                THROWBADJSONSTREAM( "JsonReadCursor::_SkipObject(): Found [%TC] when looking for comma or array end.", tchCur );
            m_pis->SkipWhitespace();
            _SkipValue();
            tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipObject(): EOF looking for end array ] or comma." ); // throws on EOF.
        }
        _rjrx.m_posEndValue = m_pis->PosGet(); // Update the end to the current end as usual.
        _rjrx.SetValueType( ejvtEndOfObject ); // Indicate that we have iterated to the end of this object.
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
        assert( FAttached() );
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
            assert( ( m_pjrxCurrent->JvtGetValueType() == ejvtEndOfObject ) == ( ejvtObject != m_pjrxCurrent->m_pjrxNext->JvtGetValueType() ) ); // Would be weird if it didn't match.
            return false; // We are already at the end of the iteration.

        }
        // Perform common tasks:
        // We may be at the leaf of the current pathway when calling this or we may be in the middle of some pathway.
        // We must close any contexts above this object first.
        SkipAllContextsAboveCurrent(); // Skip and close all the contexts above the current context.
        // Now just skip the value at the top of the stack but don't close it:
        _SkipContext( *m_pjrxCurrent );
        // Now we are going to look for a comma or an right curly/square bracket:
        m_pis->SkipWhitespace();
        _tyChar tchCur = m_pis->ReadChar( "JsonReadCursor::FNextElement(): EOF looking for end object/array }/] or comma." ); // throws on EOF.

        if ( ejvtObject == m_pjrxCurrent->m_pjrxNext->JvtGetValueType() )
        {
            _tyJsonObject * pjoCur = m_pjrxCurrent->m_pjrxNext->PGetJsonObject();            
            assert( !pjoCur->FEndOfIteration() );
            if ( _tyCharTraits::s_tcRightCurlyBr == tchCur )
            {
                m_pjrxCurrent->SetEndOfIteration( m_pis->PosGet() );
                pjoCur->SetEndOfIteration();
                return false; // We reached the end of the iteration.
            }
            m_pis->SkipWhitespace();
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
            assert( !pjaCur->FEndOfIteration() );
            if ( _tyCharTraits::s_tcRightSquareBr == tchCur )
            {
                m_pjrxCurrent->SetEndOfIteration( m_pis->PosGet() );
                pjaCur->SetEndOfIteration();
                return false; // We reached the end of the iteration.
            }
            m_pis->SkipWhitespace();
            if ( _tyCharTraits::s_tcComma != tchCur )
                THROWBADJSONSTREAM( "JsonReadCursor::FNextElement(): Found [%TC] when looking for comma or array end.", tchCur );
        }
        m_pis->SkipWhitespace();
        m_pjrxCurrent->m_posStartValue = m_pis->PosGet();
        assert( !m_pjrxCurrent->m_posEndValue ); // We should have a 0 now - unset - must be >0 when set (invariant) - this should have been cleared above.
        // The first non-whitespace character tells us what the value type is:
        m_pjrxCurrent->m_tcFirst = m_pis->ReadChar( "JsonReadCursor::FNextElement(): EOF looking for next object/array value." );
        m_pjrxCurrent->SetValueType( _tyJsonValue::GetJvtTypeFromChar( m_pjrxCurrent->m_tcFirst ) );
        if ( ejvtJsonValueTypeCount == m_pjrxCurrent->JvtGetValueType() )
            THROWBADJSONSTREAM( "JsonReadCursor::FNextElement(): Found [%TC] when looking for value starting character.", m_pjrxCurrent->m_tcFirst );
        return true;
    }
        
    // Attach to the root of the JSON value tree.
    // We merely figure out the type of the value at this position.
    void AttachRoot( t_tyJsonInputStream & _ris )
    {
        assert( !FAttached() ); // We shouldn't have attached to the stream yet.
        std::unique_ptr< _tyJsonValue > pjvRootVal = std::make_unique<_tyJsonValue>();
        std::unique_ptr< _tyJsonReadContext > pjrxRoot = std::make_unique<_tyJsonReadContext>( &*pjvRootVal, (_tyJsonReadContext*)nullptr );
        _ris.SkipWhitespace();
        pjrxRoot->m_posStartValue = _ris.PosGet();
        assert( !pjrxRoot->m_posEndValue ); // We should have a 0 now - unset - must be >0 when set (invariant).
    
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

    bool FMoveDown()
    {
        assert( FAttached() ); // We should have been attached to a file by now.

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
        assert( !m_pjrxCurrent->m_posEndValue );

        // We should be at the start of the value plus 1 character - this is important as we will be registered with the input stream throughout the streaming.
        assert( ( m_pjrxCurrent->m_posStartValue + sizeof(_tyChar) ) == m_pis->PosGet() );
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
                assert( !pjrxNewRoot->m_posEndValue ); // We should have a 0 now - unset - must be >0 when set (invariant).
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
                assert( !pjrxNewRoot->m_posEndValue ); // We should have a 0 now - unset - must be >0 when set (invariant).
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
    
        // Push the new context onto the context stack:
        _tyJsonReadContext::PushStack( m_pjrxContextStack, pjrxNewRoot );
        m_pjrxCurrent = &*m_pjrxContextStack; // current position is soft reference.
        return true; // We did go down.
    }

    // Move up in the context - this is significantly easier to implement than FMoveDown().
    bool FMoveUp()
    {
        assert( FAttached() ); // We should have been attached to a file by now.
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
