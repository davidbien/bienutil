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
#include <uuid/uuid.h>
#include "bientypes.h"

enum _ESysLogMessageType : uint8_t
{
    eslmtInfo,
    eslmtWarning,
    eslmtError,
    eslmtSysLogMessageTypeCount
};
typedef _ESysLogMessageType ESysLogMessageType;

// _SysLogThreadHeader:
// This contains thread level info about syslog messages that is constant for all contained messages.
struct _SysLogThreadHeader
{
    std::string m_szProgramName;
#if __APPLE__
    __uint64_t m_tidThreadId{0}; // The result of the call to gettid() for this thread.
#elif __linux__
    pid_t m_tidThreadId{0}; // The result of the call to gettid() for this thread.
#else
#error Need to know the OS for thread id representation.
#endif
    time_t m_timeStart{0}; // The time that this program was started - or at least when InitSysLog() was called.
    uuid_t m_uuid{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };

    _SysLogThreadHeader() = default;

    void Clear()
    {
        memset( &m_uuid, 0, sizeof m_uuid );
        assert( 1 == uuid_is_null( m_uuid ) );
        m_szProgramName.clear();
        m_tidThreadId = 0;
        m_timeStart = 0;
    }

    template < class t_tyJsonOutputStream >
    void ToJSONStream( JsonValueLife< t_tyJsonOutputStream > & _jvl ) const
    {
        // We should be in an object in the JSONStream and we will add our (key,values) to this object.
        assert( _jvl.FAtObjectValue() );
        if ( _jvl.FAtObjectValue() )
        {
            _jvl.WriteValue( "ProgName", m_szProgramName );
            _jvl.WriteUuidStringValue( "uuid", m_uuid );
            _jvl.WriteTimeStringValue( "TimeStarted", m_timeStart );
            _jvl.WriteValue( "ThreadId", m_tidThreadId );
        }
        else
            THROWNAMEDEXCEPTION( "_SysLogThreadHeader::ToJSONStream(): Not at an object." );
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
                            if ( strKey == "TimeStarted" )
                            {
                                _jrc.GetTimeStringValue( m_timeStart );
                                continue;
                            }
                            if ( strKey == "uuid" )
                            {
                                _jrc.GetUuidStringValue( m_uuid );
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
                            continue;
                        }
                    }
                }
            }
        }
        else
            THROWNAMEDEXCEPTION( "_SysLogThreadHeader::ToJSONStream(): Not at an object." );
    }
};

// _SysLogContext:
// This breaks out all info from a log message so we can put it into a JSON file where it can then be put in a DB, etc.
struct _SysLogContext
{
    time_t m_time{0};
    std::string m_szFullMesg; // The full annotated message.
    std::string m_szFile;
    int m_nLine{0};
    int m_errno{0};
    ESysLogMessageType m_eslmtType{eslmtSysLogMessageTypeCount};

    _SysLogContext() = default;

    inline void Clear()
    {
        m_time = 0;
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
            _jvl.WriteTimeStringValue( "Time", m_time );
            _jvl.WriteValue( "Type", (uint8_t)m_eslmtType );
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
    typedef JsonLinuxOutputStream< JsonCharTraits< char > > _tyJsonOutputStream;
    typedef JsonFormatSpec< JsonCharTraits< char > > _tyJsonFormatSpec;
    typedef JsonValueLife< _tyJsonOutputStream > _tyJsonValueLife;

    void Log( ESysLogMessageType _eslmt, std::string && _rrStrLog, const _SysLogContext * _pslc )
    {
        int iPriority;
        switch( _eslmt )
        {
        case eslmtInfo:
#if __APPLE__
            // Under Mac INFO messages won't show up in the Console which is kinda bogus, so mark them as WARNING.
            iPriority = LOG_WARNING;
#else
            iPriority = LOG_INFO;
#endif
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
        // First log.
        syslog( iPriority, _rrStrLog.c_str() );
 #pragma clang diagnostic pop
        // Then log the context to thread logging file.
        if ( !!_pslc && m_josThreadLog.FOpened() && !!m_optjvlRootThreadLog && !!m_optjvlSysLogArray )
        {
            // Create an object for this log message:
             _tyJsonValueLife jvlSysLogContext( *m_optjvlSysLogArray, ejvtObject );
            _pslc->ToJSONStream( jvlSysLogContext );
        }
    }

    bool FHasJSONLogFile() const
    {
        assert( m_josThreadLog.FOpened() == !!m_optjvlRootThreadLog );
        assert( m_josThreadLog.FOpened() == !!m_optjvlSysLogArray );
        return m_josThreadLog.FOpened();
    }
    bool FCreateUniqueJSONLogFile( const char * _pszProgramName )
    {
        // Don't fail running the program if we cannot create the log file.
        try
        {
            return _FTryCreateUniqueJSONLogFile( _pszProgramName );
        }
        catch( std::exception const & exc )
        {
            Log( eslmtWarning, exc.what(), 0 );
            return false;
        }
    }
    bool _FTryCreateUniqueJSONLogFile( const char * _pszProgramName )
    {
        // Move _pszProgramName past any directory:
        const char * pszProgNameNoPath = strrchr( _pszProgramName, '/' );
        if ( !!pszProgNameNoPath )
            _pszProgramName = pszProgNameNoPath + 1;
        // Now get the executable path:
        std::string strExePath;
        GetCurrentExecutablePath( strExePath );
        if ( !strExePath.length() )
            strExePath = "./"; // Just use current working directory if we can't find exe directory.
        assert( '/' == strExePath[ strExePath.length()-1 ] );
        _SysLogThreadHeader slth;
        slth.m_szProgramName = strExePath;
        slth.m_szProgramName += _pszProgramName; // Put full path to EXE here for disambiguation when working with multiple versions.
        slth.m_timeStart = time(0);
        uuid_generate( slth.m_uuid );
        slth.m_tidThreadId = s_tls_tidThreadId;

        std::string strLogFile = slth.m_szProgramName;
        uuid_string_t ustUuid;
        uuid_unparse_lower( slth.m_uuid, ustUuid );
        strLogFile += ".";
        strLogFile += ustUuid;
        strLogFile += ".log.json";

        // We must make sure we can initialize the file before we declare that it is opened.
        m_josThreadLog.Open( strLogFile.c_str() );
        _tyJsonFormatSpec jfs; // Make sure we pretty print the JSON to make it readable right away.
        jfs.m_nWhitespacePerIndent = 2;
        jfs.m_fEscapePrintableWhitespace = true;
        _tyJsonValueLife jvlRoot( m_josThreadLog, ejvtObject, &jfs );
        { //B
            // Create the SysLogThreadHeader object as the first object within the log file.
            _tyJsonValueLife jvlSysLogThreadHeader( jvlRoot, "SysLogThreadHeader", ejvtObject );
            slth.ToJSONStream( jvlSysLogThreadHeader );
        } //EB
        // Now open up an array to contain the set of log message details.
        _tyJsonValueLife jvlSysLogArray( jvlRoot, "SysLog", ejvtArray );
        // Now forward everything into the object itself - we succeeded in creating the logging file.
        m_optjvlRootThreadLog.emplace( std::move( jvlRoot ) );
        m_optjvlSysLogArray.emplace( std::move( jvlSysLogArray ), &*m_optjvlRootThreadLog );
        n_SysLog::Log( eslmtInfo, "SysLogMgr: Created thread-specific JSON log file at [%s].", strLogFile.c_str() );
        return true;
    }

// Static method. Generally not to be called directly - use the n_SysLog methods.
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
        if ( s_fGenerateUniqueJSONLogFile )
        {
            _SysLogMgr & rslm = _SysLogMgr::RGetThreadSysLogMgr();
            if ( !rslm.FCreateUniqueJSONLogFile( _pszProgramName ) )
            {

            }
        }
    }
    static void StaticLog( ESysLogMessageType _eslmt, std::string && _rrStrLog, const _SysLogContext * _pslc )
    {
        // So the caller has given us some stuff to log and even gave us a string that we get to own.
        _SysLogMgr & rslm = RGetThreadSysLogMgr();
        rslm.Log( _eslmt, std::move( _rrStrLog ), _pslc );
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
    static bool FStaticHasJSONLogFile()
    {
        return RGetThreadSysLogMgr().FHasJSONLogFile();
    }

    _SysLogMgr( _SysLogMgr * _pslmOverlord )
        : m_pslmOverlord( _pslmOverlord ) // if !m_pslmOverlord then we are the overlord!!! - or there is no overlord.
    {
    }

protected:
// non-static members:
    _SysLogMgr * m_pslmOverlord; // If this is zero then we are the overlord or there is no overlord.
    _tyJsonOutputStream m_josThreadLog; // JSON log of details of all messages for this execution.
    std::optional< _tyJsonValueLife > m_optjvlRootThreadLog; // The root of the thread log - we may add a footer at end of execution.
    std::optional< _tyJsonValueLife > m_optjvlSysLogArray; // The current position within the SysLog diagnostic log message array.
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
    static bool s_fCallSysLogEachThread;
    static bool s_fGenerateUniqueJSONLogFile; // Generate a unique log file in the same directory as the executable and then log that log filename to the syslog.
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
template < const int t_kiInstance >
bool _SysLogMgr< t_kiInstance >::s_fCallSysLogEachThread = true;
template < const int t_kiInstance >
bool _SysLogMgr< t_kiInstance >::s_fGenerateUniqueJSONLogFile = true;

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
        
        bool fHasLogFile = SysLogMgr::FStaticHasJSONLogFile();
        _SysLogContext slx;
        if ( fHasLogFile )
        {
            slx.m_eslmtType = _eslmtType;
            slx.m_szFullMesg = strLog;
            slx.m_time = time(0);
        }
        SysLogMgr::StaticLog( _eslmtType, std::move(strLog), fHasLogFile ? &slx : 0 );
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
        bool fHasLogFile = SysLogMgr::FStaticHasJSONLogFile();
        _SysLogContext slx;
        if ( fHasLogFile )
        {
            slx.m_eslmtType = _eslmtType;
            slx.m_szFullMesg = strLog;
            slx.m_time = time(0);
            slx.m_szFile = _pcFile;
            slx.m_nLine = _nLine;
        }
        SysLogMgr::StaticLog( _eslmtType, std::move(strLog), fHasLogFile ? &slx : 0 );
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
        _SysLogContext slx;
        bool fHasLogFile = SysLogMgr::FStaticHasJSONLogFile();
        if ( fHasLogFile )
        {
            slx.m_eslmtType = _eslmtType;
            slx.m_szFullMesg = strLog;
            slx.m_time = time(0);
            slx.m_errno = _errno;
        }
        SysLogMgr::StaticLog( _eslmtType, std::move(strLog), fHasLogFile ? &slx : 0 );
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
        _SysLogContext slx;
        bool fHasLogFile = SysLogMgr::FStaticHasJSONLogFile();
        if ( fHasLogFile )
        {
            slx.m_eslmtType = _eslmtType;
            slx.m_szFullMesg = strLog;
            slx.m_time = time(0);
            slx.m_szFile = _pcFile;
            slx.m_nLine = _nLine;
            slx.m_errno = _errno;
        }
        SysLogMgr::StaticLog( _eslmtType, std::move(strLog), fHasLogFile ? &slx : 0 );
    }
} // namespace n_SysLog
