#pragma once

// syslogmgr.h
// This abstracts the idea of a syslog - while also perhaps using syslog to log.
// dbien: 12MAR2020

// Architecture:
// 1) We want to allow both single and multithreaded logging architectures.
// 2) Want to output a specific log file in JSON format.
//      a) Perhaps merged.
//      b) Perhaps also/or by thread.
// 3) No blocking on logging so we will use per-thread-singletons which also help with above architecture.
// 4) These per-thread singletons are fully capable of all functionality, but they may also forward some
//      functionality to the overriding "syslog manager thread" which can produce a "merged log" and may even
//      be the thing that calls the syslog method to avoid a context switch for the calling thread.
// 5) No apparent globals - the impl hides the presence of singleton-per-thread globals.

#include <mutex>
#include <thread>
#include <syslog.h>
#include <_strutil.h>

enum _ESysLogMessageType : char
{
    eslmtInfo,
    eslmtWarning,
    eslmtError,
    eslmtSysLogMessageTypeCount
};
typedef _ESysLogMessageType ESysLogMessageType;

// Templatize this merely so we don't need a cpp module.
template < const int t_kiInstance = 0 >
class _SysLogMgr
{
    typedef _SysLogMgr _tyThis;
protected:  // These methods aren't for general consumption. Use the s_SysLog namespace methods.

    void Log( ESysLogMessageType _eslmt, std::string && _rrStrLog )
    {
        int iPriority;
        switch( _eslmt )
        {
        case eslmtInfo:
            iPriority = LOG_INFO;
            break;
        case eslmtWarning:
            iPriority = LOG_WARNING;
            break;
        default:
            assert(0);
        case eslmtError:
            iPriority = LOG_ERR;
            break;
        }
        iPriority |= LOG_USER;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-security"
        syslog( iPriority, _rrStrLog.c_str() );
 #pragma clang diagnostic pop
    }

    static _SysLogMgr & RGetThreadSysLogMgr()
    {
        // If we haven't created the SysLogMgr() on this thread then do so now.
        if ( !s_tls_pThis )
        {
            s_tls_tidThreadId = std::__libcpp_thread_id();
            s_tls_upThis = std::make_unique< _SysLogMgr >( s_pslmOverlord ); // This ensures we destroy this before the app goes away.
            s_tls_pThis = &*s_tls_upThis; // Get the non-object pointer because thread_local objects are function calls to obtain pointer.
        }
        return *s_tls_pThis;
    }
public:
    static void InitSysLog( const char * _pszProgramName, int _grfOption, int _grfFacility )
    {
        openlog( _pszProgramName, _grfOption, _grfFacility );
    }
    static void StaticLog( ESysLogMessageType _eslmt, std::string && _rrStrLog )
    {
        // So the caller has given us some stuff to log and even gave us a string that we get to own.
        _SysLogMgr & rslm = RGetThreadSysLogMgr();
        rslm.Log( _eslmt, std::move( _rrStrLog ) );
    }
    static const char * SzMessageType( ESysLogMessageType _eslmt )
    {
        switch ( _eslmt )
        {
        case eslmtInfo:
            return "Info";
        case eslmtWarning:
            return "Warning";
        case eslmtError:
            return "Error";
        default:
            assert(0);
            return "UknownMesgType";
        }
    }

    _SysLogMgr( _SysLogMgr * _pslmOverlord )
        : m_pslmOverlord( _pslmOverlord ) // if !m_pslmOverlord then we are the overlord!!! - or there is no overlord.
    {
    }

protected:
// non-static members:
    _SysLogMgr * m_pslmOverlord; // If this is zero then we are the overlord or there is no overlord.
    bool m_fCallSysLogEachThread;
// static members:
    static _SysLogMgr * s_pslmOverlord; // A pointer to the "overlord" _SysLogMgr that is running on its own thread. The lifetime for this is managed by s_tls_pThis for the overlord thread.
    static std::mutex s_mtxOverlord; // This used by overlord to guard access.
    static thread_local std::unique_ptr< _SysLogMgr > s_tls_upThis; // This object will be created in all threads the first time something logs in that thread.
                                                                  // However the "overlord thread" will create this on purpose when it is created.
    static __thread _SysLogMgr * s_tls_pThis; // The result of the call to gettid() for this thread.
    static __thread std::__libcpp_thread_id s_tls_tidThreadId; // The result of the call to gettid() for this thread.
};

template < const int t_kiInstance >
_SysLogMgr< t_kiInstance > * _SysLogMgr< t_kiInstance >::s_pslmOverlord = nullptr;
template < const int t_kiInstance >
std::mutex _SysLogMgr< t_kiInstance >::s_mtxOverlord;
template < const int t_kiInstance > thread_local
std::unique_ptr< _SysLogMgr< t_kiInstance > > _SysLogMgr< t_kiInstance >::s_tls_upThis;
template < const int t_kiInstance > __thread
_SysLogMgr< t_kiInstance > * _SysLogMgr< t_kiInstance >::s_tls_pThis = 0;
template < const int t_kiInstance > __thread
std::__libcpp_thread_id _SysLogMgr< t_kiInstance >::s_tls_tidThreadId = 0;

// Access all syslog functionality through this namespace.
namespace n_SysLog
{
    using SysLogMgr = _SysLogMgr<0>;
    inline void InitSysLog( const char * _pszProgramName, int _grfOption, int _grfFacility )
    {
        SysLogMgr::InitSysLog( _pszProgramName, _grfOption, _grfFacility );
    }
    inline void Log( ESysLogMessageType _eslmtType, const char * _pcFmt, ... )
    {
        va_list ap;
        va_start( ap, _pcFmt );
        char tc;
        int nRequired = vsnprintf( &tc, 1, _pcFmt, ap );
        va_end( ap );
        if ( nRequired < 0 )
            THROWNAMEDEXCEPTION( "n_SysLog::Log(): vsnprintf() returned nRequired[%d].", nRequired );
        va_start( ap, _pcFmt );
        std::string strLog;
        int nRet = NPrintfStdStr( strLog, nRequired, _pcFmt, ap );
        va_end( ap );
        if ( nRet < 0 )
            THROWNAMEDEXCEPTION( "n_SysLog::Log(): NPrintfStdStr() returned nRet[%d].", nRequired );
        SysLogMgr::StaticLog( _eslmtType, std::move(strLog) );
    }
    void Log( ESysLogMessageType _eslmtType, const char * _pcFile, unsigned int _nLine, const char * _pcFmt, ... )
    {
        // We add [type]:_psFile:_nLine: to the start of the format string.
        std::string strFmtAnnotated;
        PrintfStdStr( strFmtAnnotated, "%s:%d: %s", _pcFile, _nLine, _pcFmt );

        va_list ap;
        va_start( ap, _pcFmt );
        char tc;
        int nRequired = vsnprintf( &tc, 1, strFmtAnnotated.c_str(), ap );
        va_end( ap );
        if ( nRequired < 0 )
            THROWNAMEDEXCEPTION( "n_SysLog::Log(): vsnprintf() returned nRequired[%d].", nRequired );
        va_start( ap, _pcFmt );
        std::string strLog;
        int nRet = NPrintfStdStr( strLog, nRequired, strFmtAnnotated.c_str(), ap );
        va_end( ap );
        if ( nRet < 0 )
            THROWNAMEDEXCEPTION( "n_SysLog::Log(): NPrintfStdStr() returned nRet[%d].", nRequired );
        SysLogMgr::StaticLog( _eslmtType, std::move(strLog) );
    }
} // namespace n_SysLog
