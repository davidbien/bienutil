#pragma once

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _timeutl.h
// Normalize dealing with time.
// dbien: 16MAR2020

#include <string>
#include <_strutil.h>

__BIENUTIL_BEGIN_NAMESPACE

namespace n_TimeUtil
{
    template < class t_tyChar >
    size_t StrFTime( t_tyChar * _rgcBufOut, size_t _nMaxBufOut, const t_tyChar * _szFmt, const struct tm * _ptm );
    template <> inline
    size_t StrFTime( char * _rgcBufOut, size_t _nMaxBufOut, const char * _szFmt, const struct tm * _ptm )
    {
        return strftime( _rgcBufOut, _nMaxBufOut, _szFmt, _ptm );
    }
    template <> inline
    size_t StrFTime( wchar_t * _rgcBufOut, size_t _nMaxBufOut, const wchar_t * _szFmt, const struct tm * _ptm )
    {
        return wcsftime( _rgcBufOut, _nMaxBufOut, _szFmt, _ptm );
    }
    // Choose a simple representation for now.
    template < class t_tyString >
    inline void TimeToString( time_t const & _rtt, t_tyString & _rstr )
    {
        typedef typename t_tyString::value_type _tyChar;
        struct tm tmLocal;
        int iLocalTime = LocalTimeFromTime(&_rtt, &tmLocal);
        if (!!iLocalTime)
        {
          _rstr = "error";
          return;
        }
        const size_t knBuf = 128;
        _tyChar rgcBuf[ knBuf ];
        // REVIEW:<dbien>: For some reason I had to insert an explicit cast here to get things to compile.
        size_t stWritten = StrFTime( rgcBuf, knBuf-1, static_cast< _tyChar const * >( str_array_cast< _tyChar >( "%Y%m%d-%H%M%S" ) ), &tmLocal );
        Assert( !!stWritten ); // not much we can do about this if it fires.
        rgcBuf[ knBuf-1 ] = 0;
        _rstr = rgcBuf;
    }
    template < class t_tyChar >
    inline int ITimeFromString( const t_tyChar * _pszTimeStr, time_t & _rtt )
    {
        // We are looking for YYYYMMDD-HHMMSS and this is in local time.
        struct tm tmLocal;
        size_t nLen = StrNLen( _pszTimeStr );
        if ( nLen < 15 )
            return -1;
        
        int iRet = IReadPositiveNum( _pszTimeStr, 4, tmLocal.tm_year, false );
        if ( !!iRet )
            return -2;
        iRet = IReadPositiveNum( _pszTimeStr+4, 2, tmLocal.tm_mon, false );
        if ( !!iRet )
            return -3;
        iRet = IReadPositiveNum( _pszTimeStr+6, 2, tmLocal.tm_mday, false );
        if ( !!iRet )
            return -4;
        if ( '-' != _pszTimeStr[8] )
            return -5;
        iRet = IReadPositiveNum( _pszTimeStr+9, 2, tmLocal.tm_hour, false );
        if ( !!iRet )
            return -6;
        iRet = IReadPositiveNum( _pszTimeStr+11, 2, tmLocal.tm_min, false );
        if ( !!iRet )
            return -7;
        iRet = IReadPositiveNum( _pszTimeStr+13, 2, tmLocal.tm_min, false );
        if ( !!iRet )
            return -8;
        _rtt = mktime( &tmLocal );
        if ( _rtt == -1 )
            return -9;
        return 0;
    }
};

__BIENUTIL_END_NAMESPACE
