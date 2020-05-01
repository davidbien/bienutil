#pragma once

// _strutil.h
// String utilities. A *very* loose and unorganized collection at this point - on a need basis - not designed.
// dbien: 12MAR2020

#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <string>
#include <compare>
#include <utility>
#include "_namdexc.h"
#include "_smartp.h"
#if __APPLE__
#include <libproc.h>
#include <mach-o/dyld.h>
#elif __linux__
#include <assert.h>
#include <sys/auxv.h>
#include <sys/stat.h>
#else
#endif

// StrRSpn:
// Find the count of _pszSet chars that occur at the end of [_pszBegin,_pszEnd).
template < class t_tyChar >
size_t StrRSpn( const t_tyChar * _pszBegin, const t_tyChar * _pszEnd, const t_tyChar * _pszSet )
{
    const t_tyChar * pszCur = _pszEnd;
    for ( ; pszCur-- != _pszBegin; )
    {
        const t_tyChar * pszCurSet = _pszSet;
        for ( ; !!*pszCurSet && ( *pszCurSet != *pszCur ); ++pszCurSet )
            ;
        if ( !*pszCurSet )
            break;
    }
    ++pszCur;
    return _pszEnd - pszCur;
}

template < class t_tyChar >
size_t StrNLen( const t_tyChar * _psz, size_t _stMaxLen = std::numeric_limits< size_t >::max() )
{
    if ( !_psz || !_stMaxLen )
        return 0;
    const t_tyChar * pszMax = ( std::numeric_limits< size_t >::max() == _stMaxLen ) ? (_psz-1) : (_psz+_stMaxLen);
    const t_tyChar * pszCur = _psz;
    for ( ; ( pszMax != pszCur ) && !!*pszCur; ++pszCur )
        ;
    return pszCur - _psz;
}

template < class t_tyChar >
std::strong_ordering ICompareStr( const t_tyChar * _pszLeft, const t_tyChar * _pszRight )
{
    for ( ; !!*_pszLeft && ( *_pszLeft == *_pszRight ); ++_pszLeft, ++_pszRight )
        ;
    return ( *_pszLeft < *_pszRight ) ? std::strong_ordering::less : ( ( *_pszLeft > *_pszRight ) ? std::strong_ordering::greater : std::strong_ordering::equal );
}

// Return a string formatted like printf. Throws.
template < class t_tyChar >
void PrintfStdStr( std::basic_string< t_tyChar > & _rstr, const t_tyChar * _pcFmt, ... )
{
    va_list ap;
    va_start( ap, _pcFmt );
    // Initially just pass a single character bufffer since we only care about the return value:
    t_tyChar tc;
    int nRequired = vsnprintf( &tc, 1, _pcFmt, ap );
    va_end( ap );
    if ( nRequired < 0 )
        THROWNAMEDEXCEPTION( "PrintfStdStr(): vsnprintf() returned nRequired[%d].", nRequired );
    va_start( ap, _pcFmt );
    _rstr.resize( nRequired ); // this will reserve nRequired+1.
    int nRet = vsnprintf( &_rstr[0], nRequired+1, _pcFmt, ap );
    va_end( ap );
    if ( nRet < 0 )
        THROWNAMEDEXCEPTION( "PrintfStdStr(): vsnprintf() returned nRet[%d].", nRet );
}

// In this overload the caller has already made the first call to obtain the size of the buffer required.
template < class t_tyChar >
int NPrintfStdStr( std::basic_string< t_tyChar > & _rstr, int _nRequired, const t_tyChar * _pcFmt, va_list _ap )
{
    _rstr.resize( _nRequired ); // this will reserver nRequired+1.
    int nRet = vsnprintf( &_rstr[0], _nRequired+1, _pcFmt, _ap );
    return nRet;
}

// Return an error message string to the caller in a standard manner.
inline void GetErrnoStdStr( int _errno, std::string & _rstr )
{
    const int knErrorMesg = 256;
    char rgcErrorMesg[ knErrorMesg ];
    if ( !strerror_r( _errno, rgcErrorMesg, knErrorMesg ) )
    {
        rgcErrorMesg[ knErrorMesg-1 ] = 0;
        PrintfStdStr( _rstr, "errno:[%d]: %s", _errno, rgcErrorMesg );
    }
    else
        PrintfStdStr( _rstr, "errno:[%d]", _errno );
}

// Just return the error description if found.
inline void GetErrnoDescStdStr( int _errno, std::string & _rstr )
{
    const int knErrorMesg = 256;
    char rgcErrorMesg[ knErrorMesg ];
    if ( !strerror_r( _errno, rgcErrorMesg, knErrorMesg ) )
        _rstr = rgcErrorMesg;
    else
        _rstr.clear();
}

// This method will not look for negative numbers but it can read into a signed number.
template < class t_tyNum >
int IReadPositiveNum( const char * _psz, ssize_t _sstLen, t_tyNum & _rNum, bool _fThrowOnError )
{
	_rNum = 0;
	if ( !_psz || !*_psz )
    {
        if ( _fThrowOnError )
            THROWNAMEDEXCEPTION( "ReadPositiveNum(): Null or empty string passed." );
        return -1;
    }
    if ( _sstLen <= 0 )
        _sstLen = strlen( _psz );
    const char * pszCur = _psz;
    const char * const pszEnd = pszCur + _sstLen;
    for ( ; pszEnd != pszCur; ++pszCur )
    {
        int iCur = int(*pszCur) - int('0');
        if ( ( iCur < 0 ) || ( iCur > 9 ) )
        {
            if ( _fThrowOnError )
               THROWNAMEDEXCEPTION( "ReadPositiveNum(): Non-digit passed." );
            return -2;
        }
        t_tyNum numBefore = _rNum;
        _rNum *= 10;
        _rNum += iCur;
        if ( _rNum < numBefore ) // overflow.
        {
            if ( _fThrowOnError )
               THROWNAMEDEXCEPTION( "ReadPositiveNum(): Overflow." );
            return -3;
        }
    }
    return 0;
}

// Get the full executable path - not including the executable but yes including a final '/'.
void
GetCurrentExecutablePath( std::string & _rstrPath )
{
    _rstrPath.clear();
#if __APPLE__
	char rgBuf[PROC_PIDPATHINFO_MAXSIZE];
    pid_t pid = getpid();
    int iRet = proc_pidpath( pid, rgBuf, sizeof( rgBuf ) );
    assert( iRet > 0 );
    if ( iRet > 0 )
        _rstrPath = rgBuf;
    else
    { // Then try a different way.
        __BIENUTIL_USING_NAMESPACE
        char c;
        uint32_t len = 1;
        std::string strInitPath;
        (void)_NSGetExecutablePath( &c, &len );
        strInitPath.resize( len-1 ); // will reserver len.
        int iRet = _NSGetExecutablePath( &strInitPath[0], &len );
        assert( !iRet );
        char * cpMallocedPath = realpath( strInitPath.c_str(), 0 );
        FreeVoid fv( cpMallocedPath ); // Ensure we free.
        assert( !!cpMallocedPath );
        if ( !cpMallocedPath )
            _rstrPath = strInitPath;
        else
            _rstrPath = cpMallocedPath;
    }
#elif __linux__
    char * cpPath = (char *)getauxval(AT_EXECFN);
    if ( !!cpPath )
        _rstrPath = cpPath;
    else
    {
        const int knBuf = PATH_MAX*4;
        char rgcBuf[knBuf];
        ssize_t len = readlink( "/proc/self/exe", rgcBuf, knBuf );
        if ( len > knBuf )
            _rstrPath.clear();
        else
            _rstrPath = rgcBuf;
    }
#else
#endif 
    std::string::size_type stLastSlash = _rstrPath.find_last_of('/');
    if ( std::string::npos != stLastSlash )
        _rstrPath.resize( stLastSlash+1 );
}

template< typename t_tyChar, std::size_t t_knLength >
struct str_array
{
    constexpr t_tyChar const* c_str() const { return data_; }
    constexpr t_tyChar const* data() const { return data_; }
    constexpr t_tyChar operator[]( std::size_t i ) const { return data_[ i ]; }
    constexpr t_tyChar const* begin() const { return data_; }
    constexpr t_tyChar const* end() const { return data_ + t_knLength; }
    constexpr std::size_t size() const { return t_knLength; }
    constexpr operator const t_tyChar * () const { return data_; }
    // TODO: add more members of std::basic_string

    t_tyChar data_[ t_knLength + 1 ];  // +1 for null-terminator
};

namespace n_StrArrayStaticCast
{
    template< typename t_tyCharResult, typename t_tyCharSource >
    constexpr t_tyCharResult static_cast_ascii( t_tyCharSource x )
    {
        if( !( x >= 0 && x <= 127 ) )
            THROWNAMEDEXCEPTION( "n_StrArrayStaticCast::static_cast_ascii(): Character value must be in basic ASCII range (0..127)" );
        return static_cast<t_tyCharResult>( x );
    }

    template< typename t_tyCharResult, typename t_tyCharSource, std::size_t N, std::size_t... I >
    constexpr str_array< t_tyCharResult, N - 1 > do_str_array_cast( const t_tyCharSource(&a)[N], std::index_sequence<I...> )
    {
        return { static_cast_ascii<t_tyCharResult>( a[I] )..., 0 };
    }
} //namespace n_StrArrayStaticCast

template< typename t_tyCharResult, typename t_tyCharSource, std::size_t N, typename Indices = std::make_index_sequence< N - 1 > >
constexpr str_array< t_tyCharResult, N - 1 > str_array_cast( const t_tyCharSource(&a)[N] )
{
    return n_StrArrayStaticCast::do_str_array_cast< t_tyCharResult >( a, Indices{} );
}
