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
#include <sys/syscall.h>

enum _ESysLogMessageType : uint8_t
{
    eslmtInfo,
    eslmtWarning,
    eslmtError,
    eslmtSysLogMessageTypeCount
};
typedef _ESysLogMessageType ESysLogMessageType;

// _SysLogContext:
// This breaks out all info from a log message so we can put it into a JSON file where it can then be put in a DB, etc.
struct _SysLogContext
{
    time_t m_time;
    std::string m_szProgramName;
    std::string m_szFullMesg; // The full annotated message.
    std::string m_szFile;
#if __APPLE__
    __uint64_t m_tidThreadId; // The result of the call to gettid() for this thread.
#elif __linux__
    pid_t m_tidThreadId; // The result of the call to gettid() for this thread.
#else
#error Need to know the OS for thread id representation.
#endif 
    int m_nLine;
    int m_errno;
    ESysLogMessageType m_eslmtType;

    void Clear()
    {
        m_szProgramName.clear();
        m_tidThreadId = 0;
        m_szFullMesg.clear();
        m_szFile.clear();
        int m_nLine = 0;
        m_eslmtType = eslmtSysLogMessageTypeCount;
        m_errno = 0;
    }

    template < class t_tyJsonOutputStream >
    void ToJSONStream( JsonValueLife< t_tyJsonOutputStream > & _jvl ) const
    {
        // We should be in an object in the JSONStream and we will add our (key,values) to this object.
        assert( _jvl.FAtObjectValue() );
        if ( _jvl.FAtObjectValue() )
        {
            _jvl.WriteValue( "ProgName", m_szProgramName );
            _jvl.WriteValue( "Time", m_time );
            _jvl.WriteValue( "ThreadId", m_tidThreadId );
            _jvl.WriteValue( "Type", m_eslmtType );
            _jvl.WriteValue( "Mesg", m_szFullMesg );
            if ( !m_szFile.empty() )
            {
                _jvl.WriteValue( "File", m_szFile );
                _jvl.WriteValue( "Line", m_nLine );
            }
            if ( !!m_errno )
            {
                _jvl.WriteValue( "errno", m_errno );
                std::string strErrDesc;
                GetErrnoDescStdStr( m_errno, strErrDesc );
                if ( !!strErrDesc.length() )
                    _jvl.WriteValue( "ErrnoDesc", strErrDesc );
            }
        }
        else
            THROWNAMEDEXCEPTION( "_SysLogContext::ToJSONStream(): Not at an object." );
    }

    template < class t_tyJsonInputStream >
    void FromJSONStream( JsonReadCursor< t_tyJsonInputStream > & _jrc )
    {
        Clear();
        // We should be in an object in the JSONStream and we will add our (key,values) to this object.
        assert( _jrc.FAtObjectValue() );
        if ( FAtObjectValue() )
        {
            JsonRestoreContext< t_tyJsonInputStream > jrx( _jrc );
            if ( _jrc.FMoveDown() )
            {
                for ( ; !_jrc.FAtEndOfAggregate(); (void)_jrc.FNextElement() )
                {
                    // Ignore unknown stuff:
                    typedef typename t_tyJsonInputStream::_tyCharTraits _tyCharTraits;
                    typename _tyCharTraits::_tyStdStr strKey;
                    EJsonValueType jvtValue;
                    bool fGetKey = _jrc.FGetKeyCurrent( strKey, jvtValue );
                    assert( fGetKey );
                    if ( fGetKey )
                    {
                        if ( ejvtString == jvtValue )
                        {
                            if ( strKey == "ProgName" )
                            {
                                _jrc.GetValue( m_szProgramName );
                                continue;
                            }
                            if ( strKey == "Time" )
                            {
                                _jrc.GetValue( m_time );
                                continue;
                            }
                            if ( strKey == "Mesg" )
                            {
                                _jrc.GetValue( m_szFullMesg );
                                continue;
                            }
                            if ( strKey == "File" )
                            {
                                _jrc.GetValue( m_szFile );
                                continue;
                            }
                            continue;
                        }
                        if ( ejvtNumber == jvtValue )
                        {
                            if ( strKey == "ThreadId" )
                            {
                                _jrc.GetValue( m_tidThreadId );
                                continue;
                            }
                            if ( strKey == "Type" )
                            {
                                _jrc.GetValue( m_eslmtType );
                                continue;
                            }
                            if ( strKey == "Line" )
                            {
                                _jrc.GetValue( m_nLine );
                                continue;
                            }
                            if ( strKey == "errno" )
                            {
                                _jrc.GetValue( m_errno );
                                continue;
                            }
                            continue;
                        }
                    }
                }
            }
        }
        else
            THROWNAMEDEXCEPTION( "_SysLogContext::ToJSONStream(): Not at an object." );
    }

};

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
#if __APPLE__
            // This gets the thread id that we see in the Console syslog, etc.
            (void)pthread_threadid_np( 0, &s_tls_tidThreadId);
#elif __linux__
#ifdef SYS_gettid
            s_tls_tidThreadId = syscall(SYS_gettid);
#else
#error "SYS_gettid unavailable on this system"
#endif
#else
#error Need to know the OS for thread id representation.
#endif 
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
    bool m_fGenerateUniqueJSONLogFile{true}; // Generate a unique log file in the same directory as the executable and then log that log filename to the syslog.
    std::string m_strJSONLogFile;
// static members:
    static _SysLogMgr * s_pslmOverlord; // A pointer to the "overlord" _SysLogMgr that is running on its own thread. The lifetime for this is managed by s_tls_pThis for the overlord thread.
    static std::mutex s_mtxOverlord; // This used by overlord to guard access.
    static thread_local std::unique_ptr< _SysLogMgr > s_tls_upThis; // This object will be created in all threads the first time something logs in that thread.
                                                                  // However the "overlord thread" will create this on purpose when it is created.
    static __thread _SysLogMgr * s_tls_pThis; // The result of the call to gettid() for this thread.
#if __APPLE__
    static __thread __uint64_t s_tls_tidThreadId; // The result of the call to gettid() for this thread.
#elif __linux__
    static thread_local pid_t s_tls_tidThreadId; // The result of the call to gettid() for this thread.
#else
#error Need to know the OS for thread id representation.
#endif 
};

template < const int t_kiInstance >
_SysLogMgr< t_kiInstance > * _SysLogMgr< t_kiInstance >::s_pslmOverlord = nullptr;
template < const int t_kiInstance >
std::mutex _SysLogMgr< t_kiInstance >::s_mtxOverlord;
template < const int t_kiInstance > thread_local
std::unique_ptr< _SysLogMgr< t_kiInstance > > _SysLogMgr< t_kiInstance >::s_tls_upThis;
template < const int t_kiInstance > __thread
_SysLogMgr< t_kiInstance > * _SysLogMgr< t_kiInstance >::s_tls_pThis = 0;
#if __APPLE__
template < const int t_kiInstance > __thread
__uint64_t _SysLogMgr< t_kiInstance >::s_tls_tidThreadId;
#elif __linux__
template < const int t_kiInstance > thread_local
pid_t _SysLogMgr< t_kiInstance >::s_tls_tidThreadId;
#else
#error Need to know the OS for thread id representation.
#endif 

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
        // We add <type>: to the start of the format string.
        std::string strFmtAnnotated;
        PrintfStdStr( strFmtAnnotated, "<%s>: %s", SysLogMgr::SzMessageType( _eslmtType ), _pcFmt );

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
    void Log( ESysLogMessageType _eslmtType, const char * _pcFile, unsigned int _nLine, const char * _pcFmt, ... )
    {
        // We add [type]:_psFile:_nLine: to the start of the format string.
        std::string strFmtAnnotated;
        PrintfStdStr( strFmtAnnotated, "<%s>:%s:%d: %s", SysLogMgr::SzMessageType( _eslmtType ), _pcFile, _nLine, _pcFmt );

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
// Methods including errno:
    inline void Log( ESysLogMessageType _eslmtType, int _errno, const char * _pcFmt, ... )
    {
        std::string strFmtAnnotated;
        { //B
            // Add the errno description onto the end of the error string.
            std::string strErrno;
            GetErrnoStdStr( _errno, strErrno );
            // We add <type>: to the start of the format string.
            PrintfStdStr( strFmtAnnotated, "<%s>: %s, %s", SysLogMgr::SzMessageType( _eslmtType ), _pcFmt, strErrno.c_str() );
        } //EB

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
    void Log( ESysLogMessageType _eslmtType, int _errno, const char * _pcFile, unsigned int _nLine, const char * _pcFmt, ... )
    {
        // We add [type]:_psFile:_nLine: to the start of the format string.
        std::string strFmtAnnotated;
        { //B
            // Add the errno description onto the end of the error string.
            std::string strErrno;
            GetErrnoStdStr( _errno, strErrno );
            PrintfStdStr( strFmtAnnotated, "<%s>:%s:%d: %s, %s", SysLogMgr::SzMessageType( _eslmtType ), _pcFile, _nLine, _pcFmt, strErrno.c_str() );
        }

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
