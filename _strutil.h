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