#pragma once

// _timeutl.h
// Normalize dealing with time.
// dbien: 16MAR2020

#include <string>
#include <_strutil.h>

namespace n_TimeUtil
{
    // Choose a simple representation for now.
    void TimeToString( time_t const & _rtt, std::string & _rstr )
    {
        struct tm tmLocal;
        struct tm * ptm = localtime_r( &_rtt, &tmLocal );
        if ( !ptm )
        {
            _rstr = "error";
            return;
        }
        assert( ptm == &tmLocal );

        const int knBuf = 128;
        char rgcBuf[ knBuf ];
        size_t stWritten = strftime( rgcBuf, knBuf-1, "%Y%m%d-%H%M%S", &tmLocal );
        assert( !!stWritten ); // not much we can do about this if it fires.
        rgcBuf[ knBuf-1 ] = 0;
        _rstr = rgcBuf;
    }
    int ITimeFromString( const char * _pszTimeStr, time_t & _rtt )
    {
        // We are looking for YYYYMMDD-HHMMSS and this is in local time.
        struct tm tmLocal;
        size_t nLen = strlen( _pszTimeStr );
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