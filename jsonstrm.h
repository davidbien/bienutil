#pragma once

#include <string>
#include <memory>
#include <unistd.h>

#include "_dbgthrw.h"
#include "_namdexc.h"

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
template < class t_tyCharTraits >
class bad_json_stream : public std::_t__Named_exception_errno< __JSONSTRM_DEFAULT_ALLOCATOR >
{
    typedef std::_t__Named_exception_errno< __JSONSTRM_DEFAULT_ALLOCATOR > _TyBase;
public:
    typedef t_tyCharTraits _tyCharTraits;

    bad_json_stream( const string_type & __s, int _nErrno = 0 ) 
        : _TyBase( __s, _nErrno ) 
    {
    }
    bad_json_stream( int _errno, const char * _pcFmt, va_list args ) 
        : _TyBase( _errno )
    {
        // When we see %TC we should substitute the formating for the type of characters in t_tyCharTraits.
        const int knBuf = 4096;
        char rgcBuf[knBuf+1];
        char * pcBufCur = rgcBuf;
        char * const pcBufTail = rgcBuf + 4096;
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

    static void ThrowBadJsonStream( int _nLine, const char * _pcFile, const char * _pcFmt, ... )
    {
        // We add _psFile:[_nLine]: to the start of the format string.
        const int knBuf = 4096;
        char rgcBuf[knBuf+1];
        snprintf( rgcBuf, knBuf, "%s[%d]: %s", _pcFile, _nLine, _pcFmt );
        rgcBuf[knBuf] = 0;

        va_list ap;
        va_start( ap, _pcFmt );
        bad_json_stream bjs( 0, rgcBuf, ap ); // Don't throw in between va_start and va_end.
        va_end( ap );
        throw bjs;
    }
    static void ThrowBadJsonStreamErrno( int _nLine, const char * _pcFile, int _errno, const char * _pcFmt, ... )
    {
        // We add _psFile:[_nLine]: to the start of the format string.
        const int knBuf = 4096;
        char rgcBuf[knBuf+1];
        snprintf( rgcBuf, knBuf, "%s[%d]: %s", _pcFile, _nLine, _pcFmt );
        rgcBuf[knBuf] = 0;

        va_list ap;
        va_start( ap, _pcFmt );
        bad_json_stream bjs( _errno, rgcBuf, ap ); // Don't throw in between va_start and va_end.
        va_end( ap );
        throw bjs;
    }
    static void ThrowBadJsonStream( const char * _pcFmt, ... )
    {
        va_list ap;
        va_start( ap, _pcFmt );
        bad_json_stream bjs( 0, rgcBuf, ap ); // Don't throw in between va_start and va_end.
        va_end( ap );
        throw bjs;
    }
    static void ThrowBadJsonStreamErrno( int _errno, const char * _pcFmt, ... )
    {
        va_list ap;
        va_start( ap, _pcFmt );
        bad_json_stream bjs( _errno, rgcBuf, ap ); // Don't throw in between va_start and va_end.
        va_end( ap );
        throw bjs;
    }
protected:
};

// By default we will always add the __FILE__, __LINE__ even in retail for debugging purposes.
#define THROWBADJSONSTREAM( MESG, ARGS... ) bad_json_stream::ThrowBadJsonStream( __LINE__, __FILE__, MESG, ARGS )
#define THROWBADJSONSTREAMERRNO( ERRNO, MESG, ARGS... ) bad_json_stream::ThrowBadJsonStream( __LINE__, __FILE__, ERRNO, MESG, ARGS )

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
    static const _tyChar s_tcDoubleQuotes = '"';
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
    static const _tyChar s_tcu = 'u';    

    // The formatting command to format a single character using printf style.
    static const char * s_szFormatChar; // = "%c";
};

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
    static const _tyChar s_tcDoubleQuotes = L'"';
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
    static const _tyChar s_tcu = L'u';

    // The formatting command to format a single character using printf style.
    static const char * s_szFormatChar; // = L"%c";
};

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
public:
    typedef t_tyCharTraits _tyCharTraits;
    typedef typename _tyCharTraits::_tyChar _tyChar;
    typedef ssize_t _tyFilePos;

    JsonLinuxInputStream() = default;

    bool FOpened() const
    {
        return -1 != m_fd;
    }

    // Throws on open failure.
    void Open( const char * _szFilename )
    {
        assert( !FOpened() );
        m_fd = open( _szFilename, O_RDONLY );
        if ( -1 == m_fd )
            THROWBADJSONSTREAMERRNO( errno, "JsonLinuxInputStream::Open(): Unable to open() file [%s]", _szFilename );
        m_fHasLookahead = false; // ensure that if we had previously been opened that we don't think we still have a lookahead.
        m_szFilename = _szFilename; // For error reporting and general debugging. Of course we don't need to store this.
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
    }
    
    bool FReadChar( _tyChar & _rtch, bool _fThrowOnEOF, const char * _pcEOFMessage );
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
        assert( !m_fHasLookahead );
        m_fHasLookahead = true;
    }

protected:
    std::basic_string<char> m_szFilename;
    int m_fd{-1}; // file descriptor.
    _tyChar m_tcLookahead{0}; // Everytime we read a character we put it in the m_tcLookahead and clear that we have a lookahead.
    bool m_fHasLookahead{false};
};

// _EJsonValueType: The types of values in a JsonValue object.
enum _EJsonValueType : char
{
    ejvtObject,
    ejvtArray,
    ejvtNumber,
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
    using _tyStdStr = _tyCharTraits::_tyStdStr;
    using _tyJsonObject = JsonObject< t_tyCharTraits >;
    using _tyJsonArray = JsonArray< t_tyCharTraits >;

    // Note that this JsonValue very well may be referred to by a parent JsonValue.
    // JsonValues must be destructed carefully.

    JsonValue() = default;
    JsonValue( const JsonValue& ) = delete;
    JsonValue& operator=( const JsonValue& ) = delete;

    JsonValue( JsonValue * _pjvParent = nullptr_t, EJsonValueType _jvtType = ejvtJsonValueTypeCount )
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
            case _tyCharTraits::s_tcDoubleQuotes:
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

    // Create the JsonObject associated with this JsonValue.
    _tyJsonObject * PCreateJsonObject()
    {
        assert( ejvtObject == m_jvtType );
        _CreateValue();
        return ( ejvtObject == m_jvtType ) ? (_tyJsonObject*)m_pvValue : 0;
    }
    // Create the JsonObject associated with this JsonValue.
    _tyJsonArray * PCreateJsonArray()
    {
        assert( ejvtArray == m_jvtType );
        _CreateValue();
        return ( ejvtArray == m_jvtType ) ? (_tyJsonArray*)m_pvValue : 0;
    }
    _tyStdStr * PCreateStringValue()
    {
        assert( ( ejvtNumber == m_jvtType ) || ( ejvtString == m_jvtType ) );
        _CreateValue();
        return ( ( ejvtNumber == m_jvtType ) || ( ejvtString == m_jvtType ) ) ? (_tyStdStr*)m_pvValue : 0;
    }

protected:
    ~JsonValue() // Ensure that this JsonValue is destroyed by a trusted entity.
    {
        if ( m_pvValue )
            _DestroyValue();
    }
    void _DestroyValue()
    {
        void * pvValue = m_pvValue;
        m_pvValue = 0;
        _DestroyValue( pvValue )
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
        assert( !!m_pjvParent );
        assert( !m_pvValue );
        // We need to create a connected value object depending on type:
        switch( m_jvtType )
        {
        case ejvtObject:
            m_pvValue = new _tyJsonObject( m_pjvParent );
            break;
        case ejvtArray:
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
    // for ejvtTrue, ejvtFalse, ejvtNull: nullptr_t
    // for ejvtObject: JsonObject *
    // for ejvtArray: JsonArray *
    // for ejvtNumber, ejvtString: _tyStdStr *
    EJsonValueType m_jvtType{ejvtJsonValueTypeCount}; // Type of the JsonValue.
};

// class JsonObject:
// This represents an object containing key:value pairs.
template < class t_tyCharTraits >
class JsonObject
{
    typedef JsonObject _tyThis;
public:
    using _tyCharTraits = t_tyCharTraits;
    using _tyStdStr = _tyCharTraits::_tyStdStr;

    JsonObject( const JsonValue * _pjvParent )
    {
        m_jsvCur.SetPjvParent( _pjvParent ); // Link the contained value to the parent value. This is a soft reference - we assume any parent will always exist.
    }
    void GetKey( _tyStdStr & _strCurKey ) const 
    {
        _strCurKey = m_strCurKey;
    }

    // Set the JsonObject for the end of iteration.
    void SetEndOfIteration( bool _fObject )
    {
        m_strCurKey.clear();
        m_jsvCur.SetEndOfIteration( _fObject );
    }

protected:
    _tyStdStr m_strCurKey; // The current label for this object.
    JsonValue m_jsvCur; // The current JsonValue for this object.
};

// JsonArray:
// This represents an JSON array containing multiple values.
template < class t_tyCharTraits >
class JsonArray
{
    typedef JsonArray _tyThis;
public:
    using _tyCharTraits = t_tyCharTraits;
    using _tyStdStr = _tyCharTraits::_tyStdStr;

    JsonArray( const JsonValue * _pjvParent )
    {
        m_jsvCur.SetPjvParent( _pjvParent ); // Link the contained value to the parent value. This is a soft reference - we assume any parent will always exist.
    }

protected:
    JsonValue m_jsvCur; // The current JsonValue for this object.
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
    typedef typename t_tyJsonInputStream::_tyFilePos _tyFilePos;

    JsonReadContext( _tyJsonValue * _pjvCur = nullptr_t, JsonReadContext * _pjrcPrev = nullptr_t )
        :   m_pjrcPrev( _pjrcPrev ),
            m_pjvCur( _pjvCur )
    {
    }

    _tyJsonValue * PJvGet() const
    {
        return m_pjvCur;
    }

    JsonReadContext * PJrcGetNext()
    {
        return &*m_pjrcNext;
    }
    const JsonReadContext * PJrcGetNext() const
    {
        return &*m_pjrcNext;
    }

    EJsonValueType JvtGetValueType() const
    {
        assert( !!m_pjvCur );
        if ( !m_pjvCur )
            return ejvtJsonValueTypeCount;
        return m_pjvCur->JvtGetValueType();
    }

    // Push _pjrcNewHead as the new head of stack before _pjrcHead.
    static void PushStack( std::unique_ptr< JsonReadContext > & _pjrcHead, std::unique_ptr< JsonReadContext > & _pjrcNewHead )
    {
        assert( !_pjrcHead->m_pjrcPrev );
        assert( !_pjrcNewHead->m_pjrcPrev );
        assert( !_pjrcNewHead->m_pjrcNext );
        _pjrcHead->m_pjrcPrev = this; // soft link.
        _pjrcNewHead->m_pjrcNext.swap( _pjrcHead );
        _pjrcHead.swap( _pjrcNewHead ); // new head is now the head and it points at old head.
    }
    static void PopStack( std::unique_ptr< JsonReadContext > & _pjrcHead )
    {
        std::unique_ptr< JsonReadContext > pjrcOldHead;
        _pjrcHead.swap( pjrcOldHead );
        pjrcOldHead->m_pjrcNext.swap( _pjrcHead );
    }

protected:
    _tyJsonValue * m_pjvCur{}; // We maintain a soft reference to the current JsonValue at this level.
    std::unique_ptr< JsonReadContext > m_pjrcNext; // Implement a simple doubly linked list.
    JsonReadContext * m_pjrcPrev{}; // soft reference to parent in list.
    _tyFilePos m_posStartValue{}; // The start of the value for this element - after parsing WS.
    _tyFilePos m_posEndValue{}; // The end of the value for this element - before parsing WS beyond.
    _tyChar m_tcFirst{}; // Only for the number type does this matter but since it does...
};

// class JsonReadCursor:
template < class t_tyJsonInputStream >
class JsonReadCursor
{
    typedef JsonReadCursor _tyThis;
public:
    typedef t_tyJsonInputStream _tyJsonInputStream;
    typedef typename t_tyJsonInputStream::_tyCharTraits _tyCharTraits;
    typedef typename _tyCharTraits::_tyChar _tyChar;
    typedef JsonValue< _tyCharTraits > _tyJsonValue;
    typedef JsonObject< _tyCharTraits > _tyJsonObject;
    typedef JsonReadContext< _tyCharTraits > _tyJsonReadContext;
    using _tyStdStr = _tyCharTraits::_tyStdStr;

    JsonReadCursor() = default;
    JsonReadCursor( JsonReadCursor const & ) = delete;
    JsonReadCursor& operator = ( JsonReadCursor const & ) = delete;

    void AssertValid() const 
    {
        assert( !m_pjrcCurrent == !m_pis );
        assert( !m_pjrcRootVal == !m_pis );
        assert( !m_pjrcContextStack == !m_pis );
        // Make sure that the current context is in the current context stack.
        const _tyJsonReadContext * pjrcCheck = &*m_pjrcContextStack;
        for ( ; !!pjrcCheck && ( pjrcCheck != m_pjrcCurrent ); pjrcCheck = pjrcCheck->PJrcGetNext() )
            ;
        assert( !!pjrcCheck ); // If this fails then the current context is not in the content stack - this is bad.
    }
    bool FAttached() const 
    {
        AssertValid();
        return !!m_pis;
    }

    // Returns true if the current value is an aggregate value - object or array.
    bool FAtAggregateValue() const 
    {
        AssertValid();
        return !!m_pjrcCurrent && 
            ( ( ejvtArray == m_pjrcCurrent->JvtGetValueType() ) || ( ejvtObject == m_pjrcCurrent->JvtGetValueType() ) );
    }

    // Get the current key if there is a current key.
    bool FGetKeyCurrent( _tyStdStr & _rstrKey, EJsonValueType & _rjvt ) const
    {
        assert( FAttached() );
        // This should only be called when we are inside of an object's value.
        // So, we need to have a parent value and that parent value should correspond to an object.
        // Otherwise we throw an "invalid semantic use" exception.
        if ( !FAttached() )
            ThrowBadSemanticUse( "JsonReadCursor::FGetKeyCurrent(): Not attached to any JSON file." );
        if ( !m_pjrcCurrent || !m_pjrcCurrent->m_pjrcNext || ( ejvtObject != m_pjrcCurrent->m_pjrcNext->JvtGetValueType() ) )
            ThrowBadSemanticUse( "JsonReadCursor::FGetKeyCurrent(): Not located at an object so no key available." );
        // If the key value in the object is the empty string then this is an empty object and we return false.
        _tyJsonObject * pjoCur = m_pjrcCurrent->m_pjrcNext->PGetJsonObject();
        if ( pjoCur->FEmptyObject() ) // This returns true when the object is either initially empty or we are at the end of the set of (key,value) pairs.
            return false;
        pjoCur->GetKey( _rstrKey );
        _rjvt = m_pjrcCurrent->JvtGetValueType(); // This is the value type for this key string. We may not have read the actual value yet.
        return true;
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
        *ptcCur++ = _rjrc.m_tcFirst;

        auto lambdaAddCharNum = [&]( _tyChar _tch )
        { 
            if ( ptcCur == rgtcBuffer + knMaxLength )
                ThrowBadJSONFormatException( "JsonReadCursor::_ReadNumber(): Overflow of max JSON number length of [%u].", knMaxLength );
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
                ThrowBadJSONFormatException( "JsonReadCursor::_ReadNumber(): Found [%TC] when looking for digit after a minus.", tchCurrentChar );
            fZeroFirst = ( _tyCharTraits::s_tc0 == tchCurrentChar );
        }
        // If we are the root element then we may encounter EOF and have that not be an error when reading some of the values.
        bool _fAtRootElement = !_rjrc->m_pjrcNext;
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
                ThrowBadJSONFormatException( "JsonReadCursor::_ReadNumber(): Found [%TC] when looking for digit after a decimal point.", tchCurrentChar );
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
                ThrowBadJSONFormatException( "JsonReadCursor::_ReadNumber(): Found [%TC] when looking for digit after a exponent indicator (e/E).", tchCurrentChar );
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
        TCHAR tchCurrentChar; // maintained as the current character.
        if ( _tyCharTraits::s_tcMinus == _tcFirst )
        {
            tchCurrentChar = m_pis->ReadChar( "JsonReadCursor::_SkipNumber(): Hit EOF looking for a digit after a minus." ); // This may not be EOF.
            if ( ( tchCurrentChar < _tyCharTraits::s_tc0 ) || ( tchCurrentChar > _tyCharTraits::s_tc9 ) )
                ThrowBadJSONFormatException( "JsonReadCursor::_SkipNumber(): Found [%TC] when looking for digit after a minus.", tchCurrentChar );
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
            tchCurrentChar = m_pis->ReadChar(); // throw on EOF.
            if ( ( tchCurrentChar < _tyCharTraits::s_tc0 ) || ( tchCurrentChar > _tyCharTraits::s_tc9 ) )
                ThrowBadJSONFormatException( "JsonReadCursor::_SkipNumber(): Found [%TC] when looking for digit after a decimal point.", tchCurrentChar );
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
            tchCurrentChar = m_pis->ReadChar(); // Throws on EOF.
            if (    ( tchCurrentChar == _tyCharTraits::s_tcMinus ) ||
                    ( tchCurrentChar == _tyCharTraits::s_tcPlus ) )
                tchCurrentChar = m_pis->ReadChar(); // Then we must still find a number after - throws on EOF.
            // Now we must see at least one digit so for the first read we throw.
            if ( ( tchCurrentChar < _tyCharTraits::s_tc0 ) || ( tchCurrentChar > _tyCharTraits::s_tc9 ) )
                ThrowBadJSONFormatException( "JsonReadCursor::_SkipNumber(): Found [%TC] when looking for digit after a exponent indicator (e/E).", tchCurrentChar );
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
            if ( tyCharTraits::s_tcDoubleQuote == tchCur )
                break; // We have reached EOS.
            if ( tyCharTraits::s_tcBackSlash == tchCur )
            {
                tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipRemainingString(): EOF finding completion of backslash escape for string." ); // throws on EOF.
                switch( tchCur )
                {
                    case tyCharTraits::s_tcDoubleQuote:
                    case tyCharTraits::s_tcBackSlash:
                    case tyCharTraits::s_tcForwardSlash:
                        break;   // Just use tchCur.
                    case tyCharTraits::s_tcb:
                        tchCur = tyCharTraits::s_tcBackSpace;
                        break;
                    case tyCharTraits::s_tcf:
                        tchCur = tyCharTraits::s_tcBackSpace;
                        break;
                    case tyCharTraits::s_tcn:
                        tchCur = tyCharTraits::s_tcNewline;
                        break;
                    case tyCharTraits::s_tcr:
                        tchCur = tyCharTraits::s_tcCarriageReturn;
                        break;
                    case tyCharTraits::s_tct:
                        tchCur = tyCharTraits::s_tcTab;
                        break;
                    case tyCharTraits::s_tcu:
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
                                ThrowBadJSONFormatException( "JsonReadCursor::_SkipRemainingString(): Found [%TC] when looking for digit following \\u.", tchCur );
                        }
                        // If we are supposed to throw on overflow then check for it, otherwise just truncate to the character type silently.
                        if ( _tyCharTraits::s_fThrowOnUnicodeOverflow && ( sizeof(_tyChar) < sizeof(uHex) ) )
                        {
                            if ( uHex >= ( 1u << ( CHAR_BIT * sizeof(_tyChar) ) ) )
                                ThrowBadJSONFormatException( "JsonReadCursor::_SkipRemainingString(): Unicode hex overflow [%u].", uHex );
                        }
                        tchCur = (_tyChar)uHex;
                    }
                    default:
                        ThrowBadJSONFormatException( "JsonReadCursor::_SkipRemainingString(): Found [%TC] when looking for competetion of backslash when reading string.", tchCur );
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
            if ( tyCharTraits::s_tcDoubleQuote == tchCur )
                break; // We have reached EOS.
            if ( tyCharTraits::s_tcBackSlash == tchCur )
            {
                tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipRemainingString(): EOF finding completion of backslash escape for string." ); // throws on EOF.
                switch( tchCur )
                {
                    case tyCharTraits::s_tcDoubleQuote:
                    case tyCharTraits::s_tcBackSlash:
                    case tyCharTraits::s_tcForwardSlash:
                    case tyCharTraits::s_tcb:
                    case tyCharTraits::s_tcDoubleQuote:
                    case tyCharTraits::s_tcn:
                    case tyCharTraits::s_tcr:
                    case tyCharTraits::s_tct:
                        break;
                    case tyCharTraits::s_tcu:
                    {
                        // Must find 4 hex digits:
                        for ( int n=0; n < 4; ++n )
                        {
                            tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipRemainingString(): EOF found looking for 4 hex digits following \\u." ); // throws on EOF.
                            if ( !( ( ( tchCur >= _tyCharTraits::s_tc0 ) && ( tchCur <= _tyCharTraits::s_tc9 ) ) ||
                                    ( ( tchCur >= _tyCharTraits::s_tca ) && ( tchCur <= _tyCharTraits::s_tcf ) ) ||
                                    ( ( tchCur >= _tyCharTraits::s_tcA ) && ( tchCur <= _tyCharTraits::s_tcF ) ) ) ) 
                                ThrowBadJSONFormatException( "JsonReadCursor::_SkipRemainingString(): Found [%TC] when looking for digit following \\u.", tchCur );
                        }
                    }
                    default:
                        ThrowBadJSONFormatException( "JsonReadCursor::_SkipRemainingString(): Found [%TC] when looking for competetion of backslash when reading string.", tchCur );
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
        for ( ; !!*pcCur && ( _tyChar( *pcCur ) == ( tchCur = m_pis->ReadChar() ) ); ++pcCur )
            ;
        if ( *pcCur )
            ThrowBadJSONFormatException( "JsonReadCursor::_SkipFixed(): Trying to skip[%s], instead of [%c] found [%TC].", _pcSkip, *pcCur, tchCur );
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
            ThrowBadJSONFormatException( "JsonReadCursor::_SkipValue(): Found [%TC] when looking for value starting character.", tchCur );
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
        while ( tyCharTraits::s_tcDoubleQuote == tchCur )
        {
            _SkipRemainingString(); // Skip the key.
            m_pis->SkipWhitespace();
            tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipWholeObject(): EOF looking for colon." ); // throws on EOF.
            if ( tyCharTraits::s_tcColon != tchCur )
                ThrowBadJSONFormatException( "JsonReadCursor::_SkipWholeObject(): Found [%TC] when looking for colon.", tchCur );
            _SkipValue(); // Skip the value.
            m_pis->SkipWhitespace();
            tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipWholeObject(): EOF looking comma or end of object." ); // throws on EOF.
            if ( tyCharTraits::s_tcComma == tchCur )
            {
                m_pis->SkipWhitespace();
                tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipWholeObject(): EOF looking double quote." ); // throws on EOF.
                if ( tyCharTraits::s_tcDoubleQuote != tchCur )
                    ThrowBadJSONFormatException( "JsonReadCursor::_SkipWholeObject(): Found [%TC] when looking for double quote.", tchCur );
            }
            else
                break;
        }
        if ( tyCharTraits::s_tcRightCurlyBr != tchCur )
            ThrowBadJSONFormatException( "JsonReadCursor::_SkipWholeObject(): Found [%TC] when looking for double quote or comma or right bracket.", tchCur );
    }
    // We will have read the first '[' of the array.
    void _SkipWholeArray()
    {
        m_pis->SkipWhitespace();
        _tyChar tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipWholeArray(): EOF after begin bracket." ); // throws on EOF.
        if ( tyCharTraits::s_tcRightSquareBr != tchCur )
            m_pis->PushBackLastChar(); // Don't send an expected character as it is from the set of value-starting characters.
        while ( tyCharTraits::s_tcRightSquareBr != tchCur )
        {
            _SkipValue();
            m_pis->SkipWhitespace();
            tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipWholeArray(): EOF looking for comma." ); // throws on EOF.
            if ( tyCharTraits::s_tcComma == tchCur )
                continue;
            if ( tyCharTraits::s_tcRightSquareBr != tchCur )
                ThrowBadJSONFormatException( "JsonReadCursor::_SkipWholeObject(): Found [%TC] when looking for comma or array end.", tchCur );
        }
    }

    // Skip an object with an associated context. This will potentially recurse into SkipWholeObject(), SkipWholeArray(), etc. which will not have an associated context.
    void _SkipObject( _tyJsonReadContext & _rjrc )
    {
        // We have a couple of possible starting states here:
        // 1) We haven't read the data for this object ( !_rjrc.m_posEndValue ) in this case we expect to see "key":value, ..., "key":value }
        //      Note that there may also be no (key,value) pairs at all - we may have an empty object.
        // 2) We have read the data. In this case we will have also read the value and we expect to see a ',' or a '}'.
        // 3) We have an empty object and we have read the data - in which case we have already read the final '}' of the object
        //      and we have nothing at all to do.

        if ( ejvtEndOfObject == _rjrc.JvtGetValueType() )
            return; // Then we have already read to the end of this object.

        if ( !_rjrc.m_posEndValue )
        {
            _SkipWholeObject(); // this expects that we have read the first '{'.
            // Now indicate that we are at the end of the object.
            _rjrc.m_posEndValue = m_pis->PosGet(); // Indicate that we have read the value.
            _rjrc.SetValueType( ejvtEndOfObject );
            return;
        }

        // Then we have partially iterated the object. All contexts above us are closed which means we should find
        //  either a comma or end curly bracket.
        m_pis->SkipWhitespace();
        _tyChar tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipObject(): EOF looking for end object } or comma." ); // throws on EOF.
        while ( tyCharTraits::s_tcRightCurlyBr !=tchCur )
        {
            if ( tyCharTraits::s_tcComma != tchCur )
                ThrowBadJSONFormatException( "JsonReadCursor::_SkipObject(): Found [%TC] when looking for comma or object end.", tchCur );
            m_pis->SkipWhitespace();
            tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipObject(): EOF looking for double quote." ); // throws on EOF.
            if ( tyCharTraits::s_tcDoubleQuote != tchCur )
                ThrowBadJSONFormatException( "JsonReadCursor::_SkipObject(): Found [%TC] when looking key start double quote.", tchCur );
            _SkipRemainingString();
            m_pis->SkipWhitespace();
            tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipObject(): EOF looking for colon." ); // throws on EOF.
            if ( tyCharTraits::s_tcColon != tchCur )
                ThrowBadJSONFormatException( "JsonReadCursor::_SkipObject(): Found [%TC] when looking for colon.", tchCur );
            _SkipValue();
            tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipObject(): EOF looking for end object } or comma." ); // throws on EOF.
        }
        _rjrc.m_posEndValue = m_pis->PosGet(); // Update the end to the current end as usual.
        _rjrc.SetValueType( ejvtEndOfObject ); // Indicate that we have iterated to the end of this object.
    }
    // Skip an array with an associated context. This will potentially recurse into SkipWholeObject(), SkipWholeArray(), etc. which will not have an associated context.
    void _SkipArray( _tyJsonReadContext & _rjrc )
    {
        // We have a couple of possible starting states here:
        // 1) We haven't read the data for this array ( !_rjrc.m_posEndValue ) in this case we expect to see value, ..., value }
        //      Note that there may also be no values at all - we may have an empty array.
        // 2) We have read the data. In this case we will have also read the value and we expect to see a ',' or a ']'.
        // 3) We have an empty object and we have read the data - in which case we have already read the final ']' of the object
        //      and we have nothing at all to do.

        if ( ejvtEndOfArray == _rjrc.JvtGetValueType() )
            return; // Then we have already read to the end of this object.

        if ( !_rjrc.m_posEndValue )
        {
            _SkipWholeArray(); // this expects that we have read the first '['.
            // Now indicate that we are at the end of the array.
            _rjrc.m_posEndValue = m_pis->PosGet(); // Indicate that we have read the value.
            _rjrc.SetValueType( ejvtEndOfArray );
            return;
        }

        // Then we have partially iterated the object. All contexts above us are closed which means we should find
        //  either a comma or end square bracket.
        m_pis->SkipWhitespace();
        _tyChar tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipObject(): EOF looking for end array ] or comma." ); // throws on EOF.
        while ( tyCharTraits::s_tcRightSquareBr !=tchCur )
        {
            if ( tyCharTraits::s_tcComma != tchCur )
                ThrowBadJSONFormatException( "JsonReadCursor::_SkipObject(): Found [%TC] when looking for comma or array end.", tchCur );
            m_pis->SkipWhitespace();
            _SkipValue();
            tchCur = m_pis->ReadChar( "JsonReadCursor::_SkipObject(): EOF looking for end array ] or comma." ); // throws on EOF.
        }
        _rjrc.m_posEndValue = m_pis->PosGet(); // Update the end to the current end as usual.
        _rjrc.SetValueType( ejvtEndOfObject ); // Indicate that we have iterated to the end of this object.
    }

    // Skip this context - must be at the top of the context stack.
    void _SkipContext( _tyJsonReadContext & _rjrc )
    {
        // Skip this context which is at the top of the stack.
        // A couple of cases here:
        // 1) We are a non-hierarchical type: i.e. not array or object. In this case we merely have to read the value.
        // 2) We are an object or array. In this case we have to read until the end of the object or array.
        if ( ejvtObject == _rjrc.JvtGetValueType() )
            _SkipObject( _rjrc );
        else
        if ( ejvtArray == _rjrc.JvtGetValueType() )
            _SkipArray( _rjrc );
        else
        {
            // If we have already the value then we are done with this type.
            if ( !_rjrc.m_posEndValue )
            {
                _SkipSimpleValue( _rjrc.JvtGetValueType(), _rjrc.m_tcFirst, !_rjrc.m_pjrcNext ); // skip to the end of this value without saving the info.
                _rjrc.m_posEndValue = m_pis->PosGet(); // Update this for completeness in all scenarios.
            }
            return; // We have already fully processed this context.
        }
    }

    // Skip and close all contexts above the current context in the context stack.
    void SkipAllContextsAboveCurrent()
    {
       AssertValid(); 
       while ( &*m_pjrcContextStack != m_pjrcCurrent )
       {
           _SkipContext( *m_pjrcContextStack );
           _tyJsonReadContext::PopStack( m_pjrcContextStack );
       }
    }

    // Move the the next element of the object or array. Return false if this brings us to the end of the entity.
    bool FNextElement()
    {
        assert( FAttached() );
        // This should only be called when we are inside of an object or array.
        // So, we need to have a parent value and that parent value should correspond to an object or array.
        // Otherwise we throw an "invalid semantic use" exception.
        if ( !FAttached() )
            ThrowBadSemanticUse( "JsonReadCursor::FNextElement(): Not attached to any JSON file." );
        if (    !m_pjrcCurrent || !m_pjrcCurrent->m_pjrcNext || 
                (   ( ejvtObject != m_pjrcCurrent->m_pjrcNext->JvtGetValueType() ) &&
                    ( ejvtArray != m_pjrcCurrent->m_pjrcNext->JvtGetValueType() ) ) )
            ThrowBadSemanticUse( "JsonReadCursor::FNextElement(): Not located at an object or array." );
        
        if (    ( m_pjrcCurrent->JvtGetValueType() == ejvtEndOfObject ) ||
                ( m_pjrcCurrent->JvtGetValueType() == ejvtEndOfArray ) )
        {
            assert( ( m_pjrcCurrent->JvtGetValueType() == ejvtEndOfObject ) == ( ejvtObject != m_pjrcCurrent->m_pjrcNext->JvtGetValueType() ) ); // Would be weird if it didn't match.
            return false; // We are already at the end of the iteration.

        }
        // Perform common tasks:
        // We may be at the leaf of the current pathway when calling this or we may be in the middle of some pathway.
        // We must close any contexts above this object first.
        SkipAllContextsAboveCurrent(); // Skip and close all the contexts above the current context.
        // Now just skip the value at the top of the stack but don't close it:
        _SkipContext( *m_pjrcCurrent );
        // Now we are going to look for a comma or an right curly/square bracket:
        m_pis->SkipWhitespace();
        _tyChar tchCur = m_pis->ReadChar( "JsonReadCursor::FNextElement(): EOF looking for end object/array }/] or comma." ); // throws on EOF.

        if ( ejvtObject == m_pjrcCurrent->m_pjrcNext->JvtGetValueType() )
        {
            _tyJsonObject * pjoCur = m_pjrcCurrent->m_pjrcNext->PGetJsonObject();            
            assert( !pjoCur->FEmptyObject() );
            if ( tyCharTraits::s_tcRightCurlyBr == tchCur )
            {
                m_pjrcCurrent->SetEndOfIteration();
                pjoCur->SetEndOfIteration();
                return false; // We reached the end of the iteration.
            }
            m_pis->SkipWhitespace();
            if ( tyCharTraits::s_tcComma != tchCur )
                ThrowBadJSONFormatException( "JsonReadCursor::FNextElement(): Found [%TC] when looking for comma or object end.", tchCur );
            // Now we will read the key string of the next element into <pjoCur>.
            m_pis->SkipWhitespace();
            tchCur = m_pis->ReadChar( "JsonReadCursor::FNextElement(): EOF looking for double quote." ); // throws on EOF.
            if ( tyCharTraits::s_tcDoubleQuote != tchCur )
                ThrowBadJSONFormatException( "JsonReadCursor::FNextElement(): Found [%TC] when looking key start double quote.", tchCur );
            pjoCur->m_strCurKey.clear();
            _ReadString( pjoCur->m_strCurKey ); // Might throw for any number of reasons. This may be the empty string.
            m_pis->SkipWhitespace();
            tchCur = m_pis->ReadChar( "JsonReadCursor::FNextElement(): EOF looking for colon." ); // throws on EOF.
            if ( tyCharTraits::s_tcColon != tchCur )
                ThrowBadJSONFormatException( "JsonReadCursor::FNextElement(): Found [%TC] when looking for colon.", tchCur );
        }
        else
        {
            _tyJsonArray * pjaCur = m_pjrcCurrent->m_pjrcNext->PGetJsonArray();            
            assert( !pjaCur->FEmptyArray() );
            if ( tyCharTraits::s_tcRightSquareBr == tchCur )
            {
                m_pjrcCurrent->SetEndOfIteration();
                pjaCur->SetEndOfIteration();
                return false; // We reached the end of the iteration.
            }
            m_pis->SkipWhitespace();
            if ( tyCharTraits::s_tcComma != tchCur )
                ThrowBadJSONFormatException( "JsonReadCursor::FNextElement(): Found [%TC] when looking for comma or array end.", tchCur );
        }
        m_pis->SkipWhitespace();
        m_pjrcCurrent->m_posStartValue = m_pis->PosGet();
        assert( !m_pjrcCurrent->m_posEndValue ); // We should have a 0 now - unset - must be >0 when set (invariant) - this should have been cleared above.
        // The first non-whitespace character tells us what the value type is:
        m_pjrcCurrent->m_tcFirst = m_pis->ReadChar( "JsonReadCursor::FNextElement(): EOF looking for next object/array value." );
        m_pjrcCurrent->SetValueType( _tyJsonValue::GetJvtTypeFromChar( m_pjrcCurrent->m_tcFirst ) );
        if ( ejvtJsonValueTypeCount == m_pjrcCurrent->JvtGetValueType() )
            ThrowBadJSONFormatException( "JsonReadCursor::FNextElement(): Found [%TC] when looking for value starting character.", m_pjrcCurrent->m_tcFirst );
    }
        
    // Attach to the root of the JSON value tree.
    // We merely figure out the type of the value at this position.
    void AttachRoot( t_tyJsonInputStream & _ris )
    {
        assert( !FAttached() ); // We shouldn't have attached to the stream yet.
        std::unique_ptr< _tyJsonValue > pjvRootVal = std::make_unique<_tyJsonValue>();
        std::unique_ptr< _tyJsonReadContext > pjrcRoot = std::make_unique<_tyJsonReadContext>( &*pjvRootVal, (_tyJsonReadContext*)nullptr_t );
        _ris.SkipWhitespace();
        pjrcRoot->m_posStartValue = _ris.PosGet();
        assert( !pjrcRoot->m_posEndValue ); // We should have a 0 now - unset - must be >0 when set (invariant).
    
        // The first non-whitespace character tells us what the value type is:
        pjrcRoot->m_tcFirst = _ris.ReadChar( "JsonReadCursor::AttachRoot(): Empty JSON file." );
        pjvRootVal->SetValueType( _tyJsonValue::GetJvtTypeFromChar( pjrcRoot->m_tcFirst ) );
        if ( ejvtJsonValueTypeCount == pjvRootVal->JvtGetValueType() )
            ThrowBadJSONFormatException( "JsonReadCursor::AttachRoot(): Found [%TC] when looking for value starting character.", pjrcRoot->m_tcFirst );
        // Set up current state:
        m_pjrcContextStack.swap( pjrcRoot ); // swap with any existing stack.
        m_pjrcCurrent = &*m_pjrcContextStack; // current position is soft reference.
        m_pjrcRootVal.swap( pjvRootVal ); // swap in root value for tree.
        m_pis = &_ris;
    }

    bool FMoveDown()
    {
        assert( FAttached() ); // We should have been attached to a file by now.

        if ( !FAtAggregateValue() )
            return false; // Then we are at a leaf value.

        // If we have a parent then that is where we need to go - nothing to do besides that:
        if ( m_pjrcCurrent->m_pjrcPrev )
        {
            m_pjrcCurrent = m_pjrcCurrent->m_pjrcPrev;
            return true;
        }

        // If we haven't read the current value then we must read it - this is where some actual work gets done...
        // 1) For objects we read the first string and then we read the first character of the corresponding value.
        //      a) For objects of zero content we still create the subobject, it will just return that there are no (key,value) pairs.
        // 2) For arrays we read the first character of the first value in the array.
        //      a) For empty arrays we still create the subobject, it will just return that there are no values in the array.
        // In fact we must not have read the current value since we would have already pushed the context onto the stack and thus we wouldn't be here.
        assert( !m_pjrcCurrent->m_posEndValue );

        // We should be at the start of the value plus 1 character - this is important as we will be registered with the input stream throughout the streaming.
        assert( ( pjrcRoot->m_posStartValue + sizeof(_tyChar) ) == m_pis->PosGet() );
        m_pis->SkipWhitespace();

        std::unique_ptr< _tyJsonReadContext > pjrcNewRoot;
        if ( ejvtObject == m_pjrcCurrent->JvtGetValueType() )
        {
            // For valid JSON we may see 2 different things here:
            // 1) '"': Indicates we have a label for the first (key,value) pair of the object.
            // 2) '}': Indicates that we have an empty object.
            _tyChar tchCur = m_pis->ReadChar( "JsonReadCursor::FMoveDown(): EOF looking for first character of an object." ); // throws on eof.
            if ( ( _tyCharTraits::s_tcRightCurlyBr != tchCur ) && ( _tyCharTraits::s_tcDoubleQuote != tchCur ) )
                ThrowBadJSONFormatException( "JsonReadCursor::FMoveDown(): Found [%TC] when looking for first character of object.", tchCur );
            
            // Then first value inside of the object. We must create a JsonObject that will be used to manage the iteration of the set of values within it.
            _tyJsonObject * pjoNew = m_pjrcCurrent->PJvGet()->PCreateJsonObject();
            pjrcNewRoot = std::make_unique<_tyJsonReadContext>( pjoNew->PJvGet(), nullptr_t );
            if ( _tyCharTraits::s_tcDoubleQuote == tchCur )
            {
                _ReadString( pjoNew->m_strCurKey ); // Might throw for any number of reasons. This may be the empty string.
                m_pis->SkipWhitespace();
                tchCur = m_pis->ReadChar( "JsonReadCursor::FMoveDown(): EOF looking for colon on first object pair." ); // throws on eof.
                if ( _tyCharTraits::s_tcColon != tchCur )
                    ThrowBadJSONFormatException( "JsonReadCursor::FMoveDown(): Found [%TC] when looking for colon on first object pair.", tchCur );
                m_pis->SkipWhitespace();
                pjrcNewRoot->m_posStartValue = m_pis->PosGet();
                assert( !pjrcNewRoot->m_posEndValue ); // We should have a 0 now - unset - must be >0 when set (invariant).
                // The first non-whitespace character tells us what the value type is:
                pjrcNewRoot->m_tcFirst = m_pis->ReadChar( "JsonReadCursor::FMoveDown(): EOF looking for first object value." );
                pjrcNewRoot->SetValueType( _tyJsonValue::GetJvtTypeFromChar( pjrcRoot->m_tcFirst ) );
                if ( ejvtJsonValueTypeCount == pjrcNewRoot->JvtGetValueType() )
                    ThrowBadJSONFormatException( "JsonReadCursor::FMoveDown(): Found [%TC] when looking for value starting character.", pjrcNewRoot->m_tcFirst );
            }
            else
            {
                // We are an empty object but we need to push ourselves onto the context stack anyway because then things all work the same.
                pjrcNewRoot->m_posEndValue = pjrcNewRoot->m_posStartValue = m_pis->PosGet();
                pjrcNewRoot->SetValueType( ejvtEndOfObject ); // Use special value type to indicate we are at the "end of the set of objects".
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
                ThrowBadJSONFormatException( "JsonReadCursor::FMoveDown(): Found [%TC] when looking for first char of array value.", tchCur );
            
            // Then first value inside of the object. We must create a JsonObject that will be used to manage the iteration of the set of values within it.
            _tyJsonArray * pjaNew = m_pjrcCurrent->PJvGet()->PCreateJsonArray();
            pjrcNewRoot = std::make_unique<_tyJsonReadContext>( pjaNew->PJvGet(), nullptr_t );
            if ( ejvtJsonValueTypeCount != jvtCur )
            {
                pjrcNewRoot->m_posStartValue = posStartValue;
                assert( !pjrcNewRoot->m_posEndValue ); // We should have a 0 now - unset - must be >0 when set (invariant).
                // The first non-whitespace character tells us what the value type is:
                pjrcNewRoot->m_tcFirst = tchCur;
                pjrcNewRoot->SetValueType( jvtCur );
            }
            else
            {
                // We are an empty object but we need to push ourselves onto the context stack anyway because then things all work the same.
                pjrcNewRoot->m_posEndValue = pjrcNewRoot->m_posStartValue = m_pis->PosGet();
                pjrcNewRoot->SetValueType( ejvtEndOfArray ); // Use special value type to indicate we are at the "end of the set of objects".
            }
        }
    
        // Push the new context onto the context stack:
        _tyJsonReadContext::PushStack( m_pjrcContextStack, pjrcNewRoot );
        m_pjrcCurrent = &*m_pjrcContextStack; // current position is soft reference.
        return true; // We did go down.
    }

    // Move up in the context - this is significantly easier to implement than FMoveDown().
    bool FMoveUp()
    {
        assert( FAttached() ); // We should have been attached to a file by now.
        if ( !!m_pjrcCurrent->m_pjrcNext )
        {
            m_pjrcCurrent = &*m_pjrcCurrent->m_pjrcNext;
            return true;
        }
        return false; // ain't nowhere to go.
    }

    void swap( JsonReadCursor & _r )
    {
        AssertValid();
        _r.AssertValid();
        std::swap( m_pis, _r.m_pis );
        std::swap( m_pjrcCurrent, _r.m_pjrcCurrent );
        m_pjrcRootVal.swap( _r.m_pjrcRootVal );
        m_pjrcContextStack.swap( _r.m_pjrcContextStack );
    }

protected:
    _tyJsonInputStream m_pis{}; // Soft reference to stream from which we read.
    std::unique_ptr< _tyJsonValue > m_pjrcRootVal; // Hard reference to the root value of the value tree.
    std::unique_ptr< _tyJsonReadContext > m_pjrcContextStack; // Implement a simple doubly linked list.
    _tyJsonReadContext * m_pjrcCurrent{}; // The current cursor position within context stack.
};
