#pragma once

//          Copyright David Lawrence Bien 1997 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

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
#include <memory>
#include <optional>
#include <chrono>
#include <assert.h>
#ifndef WIN32
#include <syslog.h>
#include <sys/syscall.h>
#ifndef __ANDROID__
#include <uuid/uuid.h>
#endif //!__ANDROID__
#endif //! WIN32
#include "bienutil.h"
#include "bientypes.h"
#include "_util.h"
#include "_compat.h"

__BIENUTIL_BEGIN_NAMESPACE

#ifdef WIN32
// Define these constants that aren't used in WINDOWS to standardize between windows and linux.
#define LOG_USER 0
#define LOG_PERROR 1
#endif // WIN32

template < class t_tyChar > struct JsonCharTraits;
template < class t_tyJsonInputStream > class JsonReadCursor;
template < class t_tyJsonOutputStream > class JsonValueLife;
template < class t_tyCharTraits > class JsonFormatSpec;
template < class t_tyCharTraits, class t_tyPersistAsChar > class JsonFileOutputStream;
template < class t_tyChar > class JsoValue;

enum _ESysLogMessageType : uint8_t
{
  eslmtInfo,
  eslmtWarning,
  eslmtError,
  eslmtSysLogMessageTypeCount
};
typedef _ESysLogMessageType ESysLogMessageType;

// Predeclare:
namespace n_SysLog
{
struct GetProgramStart
{
  typedef chrono::high_resolution_clock _tyClock;
  typedef chrono::time_point< _tyClock > _tyTimePoint;
  GetProgramStart() { m_tpProgramStart = _tyClock::now(); }
  uint64_t NMillisecondsSinceStart() const
  {
    _tyTimePoint tpNow = _tyClock::now();
    _tyClock::duration dur = tpNow - m_tpProgramStart;
    return chrono::duration_cast< chrono::milliseconds >( dur ).count();
  }
  _tyTimePoint m_tpProgramStart;
};
typedef JsoValue< char > vtyJsoValueSysLog;
// If <_fIsMainThread> is true, then we are the main thread - before having created other threads - this allows us to set some
//  globals that will inform the creation of logging objects on other threads.
void
InitSysLog( const char * _pszProgramName, int _grfOption, int _grfFacility, const vtyJsoValueSysLog * _pjvThreadSpecificJson = nullptr,
            bool _fIsMainThread = true );
// Add a method that allows specification of a directory for the log file.
void
InitSysLog( const char * _pszProgramName, int _grfOption, int _grfFacility, const char * _pszLogDir, const vtyJsoValueSysLog * _pjvThreadSpecificJson = nullptr,
            bool _fIsMainThread = true );
// Used for when we are about to abort(), etc. We can only quickly and easily close the current thread's syslog file if there is one.
void
CloseThreadSysLog() noexcept( true );
bool
FSetInAssertOrVerify( bool _fInAssertOrVerify );
void
Log( ESysLogMessageType _eslmtType, const char * _pcFmt, ... );
void
Log( ESysLogMessageType _eslmtType, const char * _pcFile, unsigned int _nLine, const char * _pcFmt, ... );
void
Log( ESysLogMessageType _eslmtType, vtyJsoValueSysLog const & _rjvLog, const char * _pcFmt, ... );
void
Log( ESysLogMessageType _eslmtType, vtyJsoValueSysLog const & _rjvLog, const char * _pcFile, unsigned int _nLine, const char * _pcFmt, ... );
// Methods including errno:
void
Log( ESysLogMessageType _eslmtType, int _errno, const char * _pcFmt, ... );
void
Log( ESysLogMessageType _eslmtType, int _errno, const char * _pcFile, unsigned int _nLine, const char * _pcFmt, ... );
void
Log( ESysLogMessageType _eslmtType, vtyJsoValueSysLog const & _rjvLog, int _errno, const char * _pcFmt, ... );
void
Log( ESysLogMessageType _eslmtType, vtyJsoValueSysLog const & _rjvLog, int _errno, const char * _pcFile, unsigned int _nLine, const char * _pcFmt, ... );
} // namespace n_SysLog

#define LOGSYSLOG( TYPE, MESG, ... ) n_SysLog::Log( TYPE, __FILE__, __LINE__, MESG, ##__VA_ARGS__ )
#define LOGSYSLOGERRNO( TYPE, ERRNO, MESG, ... ) n_SysLog::Log( TYPE, ERRNO, __FILE__, __LINE__, MESG, ##__VA_ARGS__ )
#define LOGSYSLOG_JSON( TYPE, JSONVALUE, MESG, ... ) n_SysLog::Log( TYPE, JSONVALUE, __FILE__, __LINE__, MESG, ##__VA_ARGS__ )
#define LOGSYSLOGERRNO_JSON( TYPE, JSONVALUE, ERRNO, MESG, ... ) n_SysLog::Log( TYPE, JSONVALUE, ERRNO, __FILE__, __LINE__, MESG, ##__VA_ARGS__ )
#define LOGEXCEPTION( EXC, MESG, ... ) n_SysLog::LogException( EXC, __FILE__, __LINE__, MESG, ##__VA_ARGS__ )

// _SysLogThreadHeader:
// This contains thread level info about syslog messages that is constant for all contained messages.
struct _SysLogThreadHeader
{
  uint64_t m_nmsSinceProgramStart{ 0 }; // easiest way to do this.
  std::string m_szProgramName;
  vtyProcThreadId m_tidThreadId{ 0 };
  time_t m_timeStart{ 0 }; // The time that this program was started - or at least when InitSysLog() was called.
#ifdef WIN32
  uuid_t m_uuid{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
#else
  uuid_t m_uuid{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
#endif
  bool m_fIsMainThread = false;

  _SysLogThreadHeader() = default;

  void Clear()
  {
    m_nmsSinceProgramStart = 0;
    memset( &m_uuid, 0, sizeof m_uuid );
    m_szProgramName.clear();
    m_tidThreadId = 0;
    m_timeStart = 0;
    m_fIsMainThread = false;
  }

  template < class t_tyJsonOutputStream > void ToJSONStream( JsonValueLife< t_tyJsonOutputStream > & _jvl ) const;
  template < class t_tyJsonInputStream > void FromJSONStream( JsonReadCursor< t_tyJsonInputStream > & _jrc );
};

// _SysLogContext:
// This breaks out all info from a log message so we can put it into a JSON file where it can then be put in a DB, etc.
struct _SysLogContext
{
  uint64_t m_nmsSinceProgramStart{ 0 };         // easiest way to do this.
  const JsoValue< char > * m_pjvLog{ nullptr }; // additional JSON to log to the entry.
  time_t m_time{ 0 };
  std::string m_szFullMesg; // The full annotated message.
  std::string m_szFile;
  int m_nLine{ 0 };
  int m_errno{ 0 };
  ESysLogMessageType m_eslmtType{ eslmtSysLogMessageTypeCount };

  _SysLogContext() = default;

  inline void Clear()
  {
    m_nmsSinceProgramStart = 0;
    m_pjvLog = nullptr;
    m_time = 0;
    m_szFullMesg.clear();
    m_szFile.clear();
    int m_nLine = 0;
    m_eslmtType = eslmtSysLogMessageTypeCount;
    m_errno = 0;
  }

  template < class t_tyJsonOutputStream > void ToJSONStream( JsonValueLife< t_tyJsonOutputStream > & _jvl ) const;
  template < class t_tyJsonInputStream > void FromJSONStream( JsonReadCursor< t_tyJsonInputStream > & _jrc );
};

// Templatize this merely so we don't need a cpp module.
template < const int t_kiInstance = 0 > class _SysLogMgr
{
  typedef _SysLogMgr _tyThis;

protected: // These methods aren't for general consumption. Use the s_SysLog namespace methods.
  typedef JsonFileOutputStream< JsonCharTraits< char >, char > _tyJsonOutputStream;
  typedef JsonFormatSpec< JsonCharTraits< char > > _tyJsonFormatSpec;
  typedef JsonValueLife< _tyJsonOutputStream > _tyJsonValueLife;

  void Log( ESysLogMessageType _eslmt, std::string && _rrStrLog, const _SysLogContext * _pslc );

  bool FHasJSONLogFile() const;
  bool FCreateUniqueJSONLogFile( const char * _pszProgramName, const n_SysLog::vtyJsoValueSysLog * _pjvThreadSpecificJson, bool _fIsMainThread,
                                 const char * _pszLogDir )
  {
    // Don't fail running the program if we cannot create the log file.
    try
    {
      return _FTryCreateUniqueJSONLogFile( _pszProgramName, _pjvThreadSpecificJson, _fIsMainThread, _pszLogDir );
    }
    catch ( std::exception const & exc )
    {
#ifdef WIN32
      fprintf( stderr, "Exception creating JSON log file [%s].\n", exc.what() );
#endif // WIN32
      LOGSYSLOG( eslmtWarning, exc.what() );
      return false;
    }
  }
  bool _FTryCreateUniqueJSONLogFile( const char * _pszProgramName, const n_SysLog::vtyJsoValueSysLog * _pjvThreadSpecificJson, bool _fIsMainThread,
                                     const char * _pszLogDir );

  // Static method. Generally not to be called directly - use the n_SysLog methods.
  static _SysLogMgr & RGetThreadSysLogMgr( const char * _pszProgramName, int _grfOption, int _grfFacility,
                                           const n_SysLog::vtyJsoValueSysLog * _pjvThreadSpecificJson, bool _fIsMainThread, const char * _pszLogDir )
  {
    // If we haven't created the SysLogMgr() on this thread then do so now.
    if ( !s_tls_pThis )
    {
      (void)ThreadGetId( s_tls_tidThreadId );
      s_tls_upThis = std::make_unique< _SysLogMgr >( s_pslmOverlord ); // This ensures we destroy this before the app goes away.
      s_tls_pThis = &*s_tls_upThis; // Get the non-object pointer because thread_local objects are function calls to obtain pointer.
      s_tls_upThis->_SetOptionFacility( _grfOption, _grfFacility );
      if ( s_fGenerateUniqueJSONLogFile )
      {
        if ( !s_tls_upThis->FCreateUniqueJSONLogFile( _pszProgramName, _pjvThreadSpecificJson, _fIsMainThread, _pszLogDir ) )
        {
          fprintf( stderr, "InitSysLog(): FCreateUniqueJSONLogFile() failed, _fIsMainThread[%d].\n", int( _fIsMainThread ) );
          // continue on bravely...
        }
      }
    }
    return *s_tls_pThis;
  }
  // This is for non-main threads. It will create a log manager consistent with the main thread's configuration.
  static _SysLogMgr & RGetThreadSysLogMgr()
  {
    return RGetThreadSysLogMgr( s_strProgramName.c_str(), s_grfOption, s_grfFacility, nullptr, false, s_strLogDir.length() ? s_strLogDir.c_str() : nullptr );
  }

public:
  // This allows a thread to call InitSysLog() with specific values, or to allow lazy creation of a thread sys log mgr upon first logging (preferred approach)
  static void InitSysLog( const char * _pszProgramName, int _grfOption, int _grfFacility, const n_SysLog::vtyJsoValueSysLog * _pjvThreadSpecificJson,
                          bool _fIsMainThread, const char * _pszLogDir )
  {
#ifndef WIN32 // For windows we just don't do anything at all.
    openlog( _pszProgramName, _grfOption, _grfFacility );
#endif //! WIN32
    if ( _fIsMainThread )
    {
      // Set up values that will be used by automatically created SysLogMgr objects in other threads.
      if ( !_pszProgramName )
        throw std::invalid_argument( "pszProgramName must be non-NULL" );
      s_strProgramName = _pszProgramName;
      s_grfOption = _grfOption;
      s_grfFacility = _grfFacility;
      if ( _pszLogDir )
        s_strLogDir = _pszLogDir;
    }
    else
    {
      if ( !_pszProgramName )
        _pszProgramName = s_strProgramName.c_str();
      if ( !_pszLogDir && !s_strLogDir.empty() )
        _pszLogDir = s_strLogDir.c_str();
    }

    assert( !_pjvThreadSpecificJson || s_fGenerateUniqueJSONLogFile );
    _SysLogMgr & rslm = _SysLogMgr::RGetThreadSysLogMgr( _pszProgramName, _grfOption, _grfFacility, _pjvThreadSpecificJson, _fIsMainThread, _pszLogDir );
  }
  static void CloseThreadSysLogFile() noexcept( true )
  {
    // We don't actually close the SysLogMgr for this thread because we may want to log again and then we would just recreate it.
    // Just close any JSON logfile for this thread so that it is complete as of this call.
    // Also need to ensure that another logfile is not created for this thread.
    if ( !!s_tls_pThis && s_fGenerateUniqueJSONLogFile )
    {
      _SysLogMgr & rslm = _SysLogMgr::RGetThreadSysLogMgr();
      rslm.CloseSysLogFile();
    }
  }
  void CloseSysLogFile() noexcept( true );
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
      return "UknownMesgType";
    }
  }
  static bool FStaticHasJSONLogFile() { return RGetThreadSysLogMgr().FHasJSONLogFile(); }
  static bool FStaticSetInAssertOrVerify( bool _fInAssertOrVerify ) { return RGetThreadSysLogMgr().FSetInAssertOrVerify( _fInAssertOrVerify ); }
  bool FSetInAssertOrVerify( bool _fInAssertOrVerify )
  {
    bool fCur = m_fInAssertOrVerify;
    m_fInAssertOrVerify = _fInAssertOrVerify;
    return fCur;
  }

  _SysLogMgr( _SysLogMgr * _pslmOverlord );
  ~_SysLogMgr();

  static uint64_t _GetMsSinceProgramStart() { return s_psProgramStart.NMillisecondsSinceStart(); }

protected:
  void _SetOptionFacility( int _grfOption, int _grfFacility )
  {
    m_grfOption = _grfOption;
    m_grfFacility = _grfFacility;
  }
  // non-static members:
  _SysLogMgr * m_pslmOverlord; // If this is zero then we are the overlord or there is no overlord.
  std::unique_ptr< _tyJsonOutputStream > m_pjosThreadLog;
  std::unique_ptr< _tyJsonValueLife > m_pjvlRootThreadLog; // The root of the thread log - we may add a footer at end of execution.
  std::unique_ptr< _tyJsonValueLife > m_pjvlSysLogArray;   // The current position within the SysLog diagnostic log message array.
  int m_grfOption{ 0 };                                    // Save these here for Windows.
  int m_grfFacility{ 0 };
  bool m_fInAssertOrVerify{
      false }; // We don't want to re-enter Assert() code while processing an assert. As assertions are intimately tied to the SysLogMgr we implement that here.
  // static members:
  static _SysLogMgr * s_pslmOverlord; // A pointer to the "overlord" _SysLogMgr that is running on its own thread. The lifetime for this is managed by
                                      // s_tls_pThis for the overlord thread.
  static std::mutex s_mtxOverlord;    // This used by overlord to guard access.
  static thread_local std::unique_ptr< _SysLogMgr > s_tls_upThis; // This object will be created in all threads the first time something logs in that thread.
                                                                  // However the "overlord thread" will create this on purpose when it is created.
  static THREAD_DECL _SysLogMgr * s_tls_pThis;
  static THREAD_LOCAL_DECL_APPLE vtyProcThreadId s_tls_tidThreadId; // The result of the call to gettid() for this thread.
  static bool s_fCallSysLogEachThread;
  static bool s_fGenerateUniqueJSONLogFile; // Generate a unique log file in the same directory as the executable and then log that log filename to the syslog.
  static n_SysLog::GetProgramStart s_psProgramStart;
  inline static std::string s_strProgramName;
  inline static std::string s_strLogDir;
  inline static int s_grfOption = 0;
  inline static int s_grfFacility = 0;
};

template < const int t_kiInstance > _SysLogMgr< t_kiInstance > * _SysLogMgr< t_kiInstance >::s_pslmOverlord = nullptr;
template < const int t_kiInstance > std::mutex _SysLogMgr< t_kiInstance >::s_mtxOverlord;
template < const int t_kiInstance > thread_local std::unique_ptr< _SysLogMgr< t_kiInstance > > _SysLogMgr< t_kiInstance >::s_tls_upThis;
template < const int t_kiInstance > THREAD_DECL _SysLogMgr< t_kiInstance > * _SysLogMgr< t_kiInstance >::s_tls_pThis = 0;
template < const int t_kiInstance > THREAD_LOCAL_DECL_APPLE vtyProcThreadId _SysLogMgr< t_kiInstance >::s_tls_tidThreadId;
template < const int t_kiInstance > bool _SysLogMgr< t_kiInstance >::s_fCallSysLogEachThread = true;
template < const int t_kiInstance > bool _SysLogMgr< t_kiInstance >::s_fGenerateUniqueJSONLogFile = true;
template < const int t_kiInstance > n_SysLog::GetProgramStart _SysLogMgr< t_kiInstance >::s_psProgramStart;

// Access all syslog functionality through this namespace.
namespace n_SysLog
{
using SysLogMgr = _SysLogMgr< 0 >;
inline void
InitSysLog( const char * _pszProgramName, int _grfOption, int _grfFacility, const vtyJsoValueSysLog * _pjvThreadSpecificJson, bool _fIsMainThread )
{
  SysLogMgr::InitSysLog( _pszProgramName, _grfOption, _grfFacility, _pjvThreadSpecificJson, _fIsMainThread, nullptr );
}
inline void
InitSysLog( const char * _pszProgramName, int _grfOption, int _grfFacility, const char * _pszLogDir, const vtyJsoValueSysLog * _pjvThreadSpecificJson,
            bool _fIsMainThread )
{
  SysLogMgr::InitSysLog( _pszProgramName, _grfOption, _grfFacility, _pjvThreadSpecificJson, _fIsMainThread, _pszLogDir );
}
inline void
CloseThreadSysLog() noexcept( true )
{
  SysLogMgr::CloseThreadSysLogFile();
}
inline bool
FSetInAssertOrVerify( bool _fInAssertOrVerify )
{
  return SysLogMgr::FStaticSetInAssertOrVerify( _fInAssertOrVerify );
}
void
Log( ESysLogMessageType _eslmtType, const char * _pcFmt, ... );
void
Log( ESysLogMessageType _eslmtType, const char * _pcFile, unsigned int _nLine, const char * _pcFmt, ... );
// Methods including errno:
void
Log( ESysLogMessageType _eslmtType, int _errno, const char * _pcFmt, ... );
void
Log( ESysLogMessageType _eslmtType, int _errno, const char * _pcFile, unsigned int _nLine, const char * _pcFmt, ... );
// Same as above except adding JsoValue additional JSON logging:
void
Log( ESysLogMessageType _eslmtType, vtyJsoValueSysLog const & _rjvLog, const char * _pcFmt, ... );
void
Log( ESysLogMessageType _eslmtType, vtyJsoValueSysLog const & _rjvLog, const char * _pcFile, unsigned int _nLine, const char * _pcFmt, ... );
// Methods including errno:
void
Log( ESysLogMessageType _eslmtType, vtyJsoValueSysLog const & _rjvLog, int _errno, const char * _pcFmt, ... );
void
Log( ESysLogMessageType _eslmtType, vtyJsoValueSysLog const & _rjvLog, int _errno, const char * _pcFile, unsigned int _nLine, const char * _pcFmt, ... );
// Exception logging with catch point:
void
LogException( std::exception const & _rexc, const char * _pcFile, unsigned int _nLine, const char * _pcFmt, ... );
} // namespace n_SysLog

__BIENUTIL_END_NAMESPACE