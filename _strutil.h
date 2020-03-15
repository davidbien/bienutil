#pragma once

// _strutil.h
// String utilities.
// dbien: 12MAR2020

#include <string>
#include <_namdexc.h>

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
    _rstr.resize( nRequired ); // this will reserver nRequired+1.
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