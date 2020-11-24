#pragma once

//          Copyright David Lawrence Bien 1997 - 2020.
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
#include <syslog.h>
#include <_strutil.h>
#include <sys/syscall.h>
#include <uuid/uuid.h>
#include "bienutil.h"
#include "bientypes.h"
#include "_util.h"

template <class t_tyChar>
struct JsonCharTraits;
template <class t_tyJsonInputStream>
class JsonReadCursor;
template <class t_tyJsonOutputStream>
class JsonValueLife;
template <class t_tyCharTraits>
class JsonFormatSpec;
template <class t_tyCharTraits, class t_tyPersistAsChar>
class JsonLinuxOutputStream;
template <class t_tyChar>
class JsoValue;

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
    GetProgramStart()
    {
      (void)clock_gettime(CLOCK_MONOTONIC_COARSE, &m_tsProgramStart);
    }
    timespec m_tsProgramStart;
  };
  typedef JsoValue<char> vtyJsoValueSysLog;
  void InitSysLog(const char *_pszProgramName, int _grfOption, int _grfFacility, const vtyJsoValueSysLog *_pjvThreadSpecificJson = 0);
  // Used for when we are about to abort(), etc. We can only quickly and easily close the current thread's syslog file if there is one.
  void CloseThreadSysLog();
  void Log(ESysLogMessageType _eslmtType, const char *_pcFmt, ...);
  void Log(ESysLogMessageType _eslmtType, const char *_pcFile, unsigned int _nLine, const char *_pcFmt, ...);
  void Log(ESysLogMessageType _eslmtType, vtyJsoValueSysLog const &_rjvLog, const char *_pcFmt, ...);
  void Log(ESysLogMessageType _eslmtType, vtyJsoValueSysLog const &_rjvLog, const char *_pcFile, unsigned int _nLine, const char *_pcFmt, ...);
  // Methods including errno:
  void Log(ESysLogMessageType _eslmtType, int _errno, const char *_pcFmt, ...);
  void Log(ESysLogMessageType _eslmtType, int _errno, const char *_pcFile, unsigned int _nLine, const char *_pcFmt, ...);
  void Log(ESysLogMessageType _eslmtType, vtyJsoValueSysLog const &_rjvLog, int _errno, const char *_pcFmt, ...);
  void Log(ESysLogMessageType _eslmtType, vtyJsoValueSysLog const &_rjvLog, int _errno, const char *_pcFile, unsigned int _nLine, const char *_pcFmt, ...);
} // namespace n_SysLog

#define LOGSYSLOG(TYPE, MESG...) n_SysLog::Log(TYPE, __FILE__, __LINE__, MESG)
#define LOGSYSLOGERRNO(TYPE, ERRNO, MESG...) n_SysLog::Log(TYPE, ERRNO, __FILE__, __LINE__, MESG)
#define LOGSYSLOG_JSON(TYPE, JSONVALUE, MESG...) n_SysLog::Log(TYPE, JSONVALUE, __FILE__, __LINE__, MESG)
#define LOGSYSLOGERRNO_JSON(TYPE, JSONVALUE, ERRNO, MESG...) n_SysLog::Log(TYPE, JSONVALUE, ERRNO, __FILE__, __LINE__, MESG)

// _SysLogThreadHeader:
// This contains thread level info about syslog messages that is constant for all contained messages.
struct _SysLogThreadHeader
{
  size_t m_nmsSinceProgramStart{0}; // easiest way to do this.
  std::string m_szProgramName;
#if __APPLE__
  __uint64_t m_tidThreadId{0}; // The result of the call to gettid() for this thread.
#elif __linux__
  pid_t m_tidThreadId{0}; // The result of the call to gettid() for this thread.
#else
#error Need to know the OS for thread id representation.
#endif
  time_t m_timeStart{0}; // The time that this program was started - or at least when InitSysLog() was called.
  uuid_t m_uuid{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

  _SysLogThreadHeader() = default;

  void Clear()
  {
    m_nmsSinceProgramStart = 0;
    memset(&m_uuid, 0, sizeof m_uuid);
    assert(1 == uuid_is_null(m_uuid));
    m_szProgramName.clear();
    m_tidThreadId = 0;
    m_timeStart = 0;
  }

  template <class t_tyJsonOutputStream>
  void ToJSONStream(JsonValueLife<t_tyJsonOutputStream> &_jvl) const;
  template <class t_tyJsonInputStream>
  void FromJSONStream(JsonReadCursor<t_tyJsonInputStream> &_jrc);
};

// _SysLogContext:
// This breaks out all info from a log message so we can put it into a JSON file where it can then be put in a DB, etc.
struct _SysLogContext
{
  size_t m_nmsSinceProgramStart;           // easiest way to do this.
  const JsoValue<char> *m_pjvLog{nullptr}; // additional JSON to log to the entry.
  time_t m_time{0};
  std::string m_szFullMesg; // The full annotated message.
  std::string m_szFile;
  int m_nLine{0};
  int m_errno{0};
  ESysLogMessageType m_eslmtType{eslmtSysLogMessageTypeCount};

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

  template <class t_tyJsonOutputStream>
  void ToJSONStream(JsonValueLife<t_tyJsonOutputStream> &_jvl) const;
  template <class t_tyJsonInputStream>
  void FromJSONStream(JsonReadCursor<t_tyJsonInputStream> &_jrc);
};

// Templatize this merely so we don't need a cpp module.
template <const int t_kiInstance = 0>
class _SysLogMgr
{
  typedef _SysLogMgr _tyThis;

protected: // These methods aren't for general consumption. Use the s_SysLog namespace methods.
  typedef JsonLinuxOutputStream<JsonCharTraits<char>, char> _tyJsonOutputStream;
  typedef JsonFormatSpec<JsonCharTraits<char>> _tyJsonFormatSpec;
  typedef JsonValueLife<_tyJsonOutputStream> _tyJsonValueLife;

  void Log(ESysLogMessageType _eslmt, std::string &&_rrStrLog, const _SysLogContext *_pslc);

  bool FHasJSONLogFile() const;
  bool FCreateUniqueJSONLogFile(const char *_pszProgramName, const n_SysLog::vtyJsoValueSysLog *_pjvThreadSpecificJson)
  {
    // Don't fail running the program if we cannot create the log file.
    try
    {
      return _FTryCreateUniqueJSONLogFile(_pszProgramName, _pjvThreadSpecificJson);
    }
    catch (std::exception const &exc)
    {
      LOGSYSLOG(eslmtWarning, exc.what());
      return false;
    }
  }
  bool _FTryCreateUniqueJSONLogFile(const char *_pszProgramName, const n_SysLog::vtyJsoValueSysLog *_pjvThreadSpecificJson);

  // Static method. Generally not to be called directly - use the n_SysLog methods.
  static _SysLogMgr &RGetThreadSysLogMgr()
  {
    // If we haven't created the SysLogMgr() on this thread then do so now.
    if (!s_tls_pThis)
    {
#if __APPLE__
      // This gets the thread id that we see in the Console syslog, etc.
      (void)pthread_threadid_np(0, &s_tls_tidThreadId);
#elif __linux__
#ifdef SYS_gettid
      s_tls_tidThreadId = syscall(SYS_gettid);
#else
#error "SYS_gettid unavailable on this system"
#endif
#else
#error Need to know the OS for thread id representation.
#endif
      s_tls_upThis = std::make_unique<_SysLogMgr>(s_pslmOverlord); // This ensures we destroy this before the app goes away.
      s_tls_pThis = &*s_tls_upThis;                                // Get the non-object pointer because thread_local objects are function calls to obtain pointer.
    }
    return *s_tls_pThis;
  }

public:
  static void InitSysLog(const char *_pszProgramName, int _grfOption, int _grfFacility, const n_SysLog::vtyJsoValueSysLog *_pjvThreadSpecificJson)
  {
    openlog(_pszProgramName, _grfOption, _grfFacility);
    assert(!_pjvThreadSpecificJson || s_fGenerateUniqueJSONLogFile);
    if (s_fGenerateUniqueJSONLogFile)
    {
      _SysLogMgr &rslm = _SysLogMgr::RGetThreadSysLogMgr();
      if (!rslm.FCreateUniqueJSONLogFile(_pszProgramName, _pjvThreadSpecificJson))
      {
        fprintf(stderr, "InitSysLog(): FCreateUniqueJSONLogFile() failed.\n");
        // continue on bravely...
      }
    }
  }
  static void CloseThreadSysLogFile()
  {
    // We don't actually close the SysLogMgr for this thread because we may want to log again and then we would just recreate it.
    // Just close any JSON logfile for this thread so that it is complete as of this call.
    // Also need to ensure that another logfile is not created for this thread.
    if ( !!s_tls_pThis && s_fGenerateUniqueJSONLogFile )
    {
      _SysLogMgr &rslm = _SysLogMgr::RGetThreadSysLogMgr();
      rslm.CloseSysLogFile();
    }
  }
  void CloseSysLogFile();
  static void StaticLog(ESysLogMessageType _eslmt, std::string &&_rrStrLog, const _SysLogContext *_pslc)
  {
    // So the caller has given us some stuff to log and even gave us a string that we get to own.
    _SysLogMgr &rslm = RGetThreadSysLogMgr();
    rslm.Log(_eslmt, std::move(_rrStrLog), _pslc);
  }
  static const char *SzMessageType(ESysLogMessageType _eslmt)
  {
    switch (_eslmt)
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

  _SysLogMgr(_SysLogMgr *_pslmOverlord)
      : m_pslmOverlord(_pslmOverlord) // if !m_pslmOverlord then we are the overlord!!! - or there is no overlord.
  {
  }

  static size_t _GetMsSinceProgramStart()
  {
    timespec tsCur;
    if (!clock_gettime(CLOCK_MONOTONIC_COARSE, &tsCur))
    {
      size_t ms = (tsCur.tv_sec - s_psProgramStart.m_tsProgramStart.tv_sec) * 1000;
      ms += (tsCur.tv_nsec - s_psProgramStart.m_tsProgramStart.tv_nsec) / 1000000;
      return ms;
    }
    return 0;
  }

protected:
  // non-static members:
  _SysLogMgr *m_pslmOverlord; // If this is zero then we are the overlord or there is no overlord.
  std::unique_ptr<_tyJsonOutputStream> m_pjosThreadLog;
  std::unique_ptr<_tyJsonValueLife> m_pjvlRootThreadLog;        // The root of the thread log - we may add a footer at end of execution.
  std::unique_ptr<_tyJsonValueLife> m_pjvlSysLogArray;          // The current position within the SysLog diagnostic log message array.
                                                                // static members:
  static _SysLogMgr *s_pslmOverlord;                            // A pointer to the "overlord" _SysLogMgr that is running on its own thread. The lifetime for this is managed by s_tls_pThis for the overlord thread.
  static std::mutex s_mtxOverlord;                              // This used by overlord to guard access.
  static thread_local std::unique_ptr<_SysLogMgr> s_tls_upThis; // This object will be created in all threads the first time something logs in that thread.
                                                                // However the "overlord thread" will create this on purpose when it is created.
  static __thread _SysLogMgr *s_tls_pThis;                      // The result of the call to gettid() for this thread.
#if __APPLE__
  static __thread __uint64_t s_tls_tidThreadId; // The result of the call to gettid() for this thread.
#elif __linux__
  static thread_local pid_t s_tls_tidThreadId; // The result of the call to gettid() for this thread.
#else
#error Need to know the OS for thread id representation.
#endif
  static bool s_fCallSysLogEachThread;
  static bool s_fGenerateUniqueJSONLogFile; // Generate a unique log file in the same directory as the executable and then log that log filename to the syslog.
  static n_SysLog::GetProgramStart s_psProgramStart;
};

template <const int t_kiInstance>
_SysLogMgr<t_kiInstance> *_SysLogMgr<t_kiInstance>::s_pslmOverlord = nullptr;
template <const int t_kiInstance>
std::mutex _SysLogMgr<t_kiInstance>::s_mtxOverlord;
template <const int t_kiInstance>
thread_local std::unique_ptr<_SysLogMgr<t_kiInstance>> _SysLogMgr<t_kiInstance>::s_tls_upThis;
template <const int t_kiInstance>
__thread _SysLogMgr<t_kiInstance> *_SysLogMgr<t_kiInstance>::s_tls_pThis = 0;
#if __APPLE__
template <const int t_kiInstance>
__thread __uint64_t _SysLogMgr<t_kiInstance>::s_tls_tidThreadId;
#elif __linux__
template <const int t_kiInstance>
thread_local pid_t _SysLogMgr<t_kiInstance>::s_tls_tidThreadId;
#else
#error Need to know the OS for thread id representation.
#endif
template <const int t_kiInstance>
bool _SysLogMgr<t_kiInstance>::s_fCallSysLogEachThread = true;
template <const int t_kiInstance>
bool _SysLogMgr<t_kiInstance>::s_fGenerateUniqueJSONLogFile = true;
template <const int t_kiInstance>
n_SysLog::GetProgramStart _SysLogMgr<t_kiInstance>::s_psProgramStart;

// Access all syslog functionality through this namespace.
namespace n_SysLog
{
  using SysLogMgr = _SysLogMgr<0>;
  inline void InitSysLog(const char *_pszProgramName, int _grfOption, int _grfFacility, const vtyJsoValueSysLog *_pjvThreadSpecificJson)
  {
    SysLogMgr::InitSysLog(_pszProgramName, _grfOption, _grfFacility, _pjvThreadSpecificJson);
  }
  inline void CloseThreadSysLog()
  {
    SysLogMgr::CloseThreadSysLog();
  }
  inline void Log(ESysLogMessageType _eslmtType, const char *_pcFmt, ...)
  {
    // We add <type>: to the start of the format string.
    std::string strFmtAnnotated;
    PrintfStdStr(strFmtAnnotated, "<%s>: %s", SysLogMgr::SzMessageType(_eslmtType), _pcFmt);

    va_list ap;
    va_start(ap, _pcFmt);
    char tc;
    int nRequired = vsnprintf(&tc, 1, strFmtAnnotated.c_str(), ap);
    va_end(ap);
    if (nRequired < 0)
      THROWNAMEDEXCEPTION("n_SysLog::Log(): vsnprintf() returned nRequired[%d].", nRequired);
    va_start(ap, _pcFmt);
    std::string strLog;
    int nRet = NPrintfStdStr(strLog, nRequired, strFmtAnnotated.c_str(), ap);
    va_end(ap);
    if (nRet < 0)
      THROWNAMEDEXCEPTION("n_SysLog::Log(): NPrintfStdStr() returned nRet[%d].", nRequired);

    bool fHasLogFile = SysLogMgr::FStaticHasJSONLogFile();
    _SysLogContext slx;
    if (fHasLogFile)
    {
      slx.m_eslmtType = _eslmtType;
      slx.m_szFullMesg = strLog;
      slx.m_time = time(0);
      slx.m_nmsSinceProgramStart = SysLogMgr::_GetMsSinceProgramStart();
    }
    SysLogMgr::StaticLog(_eslmtType, std::move(strLog), fHasLogFile ? &slx : 0);
  }
  void Log(ESysLogMessageType _eslmtType, const char *_pcFile, unsigned int _nLine, const char *_pcFmt, ...)
  {
    // We add [type]:_psFile:_nLine: to the start of the format string.
    std::string strFmtAnnotated;
    PrintfStdStr(strFmtAnnotated, "<%s>:%s:%d: %s", SysLogMgr::SzMessageType(_eslmtType), _pcFile, _nLine, _pcFmt);

    va_list ap;
    va_start(ap, _pcFmt);
    char tc;
    int nRequired = vsnprintf(&tc, 1, strFmtAnnotated.c_str(), ap);
    va_end(ap);
    if (nRequired < 0)
      THROWNAMEDEXCEPTION("n_SysLog::Log(): vsnprintf() returned nRequired[%d].", nRequired);
    va_start(ap, _pcFmt);
    std::string strLog;
    int nRet = NPrintfStdStr(strLog, nRequired, strFmtAnnotated.c_str(), ap);
    va_end(ap);
    if (nRet < 0)
      THROWNAMEDEXCEPTION("n_SysLog::Log(): NPrintfStdStr() returned nRet[%d].", nRequired);
    bool fHasLogFile = SysLogMgr::FStaticHasJSONLogFile();
    _SysLogContext slx;
    if (fHasLogFile)
    {
      slx.m_eslmtType = _eslmtType;
      slx.m_szFullMesg = strLog;
      slx.m_time = time(0);
      slx.m_nmsSinceProgramStart = SysLogMgr::_GetMsSinceProgramStart();
      slx.m_szFile = _pcFile;
      slx.m_nLine = _nLine;
    }
    SysLogMgr::StaticLog(_eslmtType, std::move(strLog), fHasLogFile ? &slx : 0);
  }
  // Methods including errno:
  inline void Log(ESysLogMessageType _eslmtType, int _errno, const char *_pcFmt, ...)
  {
    std::string strFmtAnnotated;
    { //B
      // Add the errno description onto the end of the error string.
      std::string strErrno;
      GetErrnoStdStr(_errno, strErrno);
      // We add <type>: to the start of the format string.
      PrintfStdStr(strFmtAnnotated, "<%s>: %s, %s", SysLogMgr::SzMessageType(_eslmtType), _pcFmt, strErrno.c_str());
    } //EB

    va_list ap;
    va_start(ap, _pcFmt);
    char tc;
    int nRequired = vsnprintf(&tc, 1, strFmtAnnotated.c_str(), ap);
    va_end(ap);
    if (nRequired < 0)
      THROWNAMEDEXCEPTION("n_SysLog::Log(): vsnprintf() returned nRequired[%d].", nRequired);
    va_start(ap, _pcFmt);
    std::string strLog;
    int nRet = NPrintfStdStr(strLog, nRequired, strFmtAnnotated.c_str(), ap);
    va_end(ap);
    if (nRet < 0)
      THROWNAMEDEXCEPTION("n_SysLog::Log(): NPrintfStdStr() returned nRet[%d].", nRequired);
    _SysLogContext slx;
    bool fHasLogFile = SysLogMgr::FStaticHasJSONLogFile();
    if (fHasLogFile)
    {
      slx.m_eslmtType = _eslmtType;
      slx.m_szFullMesg = strLog;
      slx.m_time = time(0);
      slx.m_nmsSinceProgramStart = SysLogMgr::_GetMsSinceProgramStart();
      slx.m_errno = _errno;
    }
    SysLogMgr::StaticLog(_eslmtType, std::move(strLog), fHasLogFile ? &slx : 0);
  }
  void Log(ESysLogMessageType _eslmtType, int _errno, const char *_pcFile, unsigned int _nLine, const char *_pcFmt, ...)
  {
    // We add [type]:_psFile:_nLine: to the start of the format string.
    std::string strFmtAnnotated;
    { //B
      // Add the errno description onto the end of the error string.
      std::string strErrno;
      GetErrnoStdStr(_errno, strErrno);
      PrintfStdStr(strFmtAnnotated, "<%s>:%s:%d: %s, %s", SysLogMgr::SzMessageType(_eslmtType), _pcFile, _nLine, _pcFmt, strErrno.c_str());
    }

    va_list ap;
    va_start(ap, _pcFmt);
    char tc;
    int nRequired = vsnprintf(&tc, 1, strFmtAnnotated.c_str(), ap);
    va_end(ap);
    if (nRequired < 0)
      THROWNAMEDEXCEPTION("n_SysLog::Log(): vsnprintf() returned nRequired[%d].", nRequired);
    va_start(ap, _pcFmt);
    std::string strLog;
    int nRet = NPrintfStdStr(strLog, nRequired, strFmtAnnotated.c_str(), ap);
    va_end(ap);
    if (nRet < 0)
      THROWNAMEDEXCEPTION("n_SysLog::Log(): NPrintfStdStr() returned nRet[%d].", nRequired);
    _SysLogContext slx;
    bool fHasLogFile = SysLogMgr::FStaticHasJSONLogFile();
    if (fHasLogFile)
    {
      slx.m_eslmtType = _eslmtType;
      slx.m_szFullMesg = strLog;
      slx.m_time = time(0);
      slx.m_nmsSinceProgramStart = SysLogMgr::_GetMsSinceProgramStart();
      slx.m_szFile = _pcFile;
      slx.m_nLine = _nLine;
      slx.m_errno = _errno;
    }
    SysLogMgr::StaticLog(_eslmtType, std::move(strLog), fHasLogFile ? &slx : 0);
  }
  // Same as above except adding JsoValue additional JSON logging:
  inline void Log(ESysLogMessageType _eslmtType, vtyJsoValueSysLog const &_rjvLog, const char *_pcFmt, ...)
  {
    // We add <type>: to the start of the format string.
    std::string strFmtAnnotated;
    PrintfStdStr(strFmtAnnotated, "<%s>: %s", SysLogMgr::SzMessageType(_eslmtType), _pcFmt);

    va_list ap;
    va_start(ap, _pcFmt);
    char tc;
    int nRequired = vsnprintf(&tc, 1, strFmtAnnotated.c_str(), ap);
    va_end(ap);
    if (nRequired < 0)
      THROWNAMEDEXCEPTION("n_SysLog::Log(): vsnprintf() returned nRequired[%d].", nRequired);
    va_start(ap, _pcFmt);
    std::string strLog;
    int nRet = NPrintfStdStr(strLog, nRequired, strFmtAnnotated.c_str(), ap);
    va_end(ap);
    if (nRet < 0)
      THROWNAMEDEXCEPTION("n_SysLog::Log(): NPrintfStdStr() returned nRet[%d].", nRequired);

    bool fHasLogFile = SysLogMgr::FStaticHasJSONLogFile();
    _SysLogContext slx;
    if (fHasLogFile)
    {
      slx.m_eslmtType = _eslmtType;
      slx.m_szFullMesg = strLog;
      slx.m_time = time(0);
      slx.m_nmsSinceProgramStart = SysLogMgr::_GetMsSinceProgramStart();
      slx.m_pjvLog = &_rjvLog;
    }
    SysLogMgr::StaticLog(_eslmtType, std::move(strLog), fHasLogFile ? &slx : 0);
  }
  void Log(ESysLogMessageType _eslmtType, vtyJsoValueSysLog const &_rjvLog, const char *_pcFile, unsigned int _nLine, const char *_pcFmt, ...)
  {
    // We add [type]:_psFile:_nLine: to the start of the format string.
    std::string strFmtAnnotated;
    PrintfStdStr(strFmtAnnotated, "<%s>:%s:%d: %s", SysLogMgr::SzMessageType(_eslmtType), _pcFile, _nLine, _pcFmt);

    va_list ap;
    va_start(ap, _pcFmt);
    char tc;
    int nRequired = vsnprintf(&tc, 1, strFmtAnnotated.c_str(), ap);
    va_end(ap);
    if (nRequired < 0)
      THROWNAMEDEXCEPTION("n_SysLog::Log(): vsnprintf() returned nRequired[%d].", nRequired);
    va_start(ap, _pcFmt);
    std::string strLog;
    int nRet = NPrintfStdStr(strLog, nRequired, strFmtAnnotated.c_str(), ap);
    va_end(ap);
    if (nRet < 0)
      THROWNAMEDEXCEPTION("n_SysLog::Log(): NPrintfStdStr() returned nRet[%d].", nRequired);
    bool fHasLogFile = SysLogMgr::FStaticHasJSONLogFile();
    _SysLogContext slx;
    if (fHasLogFile)
    {
      slx.m_eslmtType = _eslmtType;
      slx.m_szFullMesg = strLog;
      slx.m_time = time(0);
      slx.m_nmsSinceProgramStart = SysLogMgr::_GetMsSinceProgramStart();
      slx.m_szFile = _pcFile;
      slx.m_nLine = _nLine;
      slx.m_pjvLog = &_rjvLog;
    }
    SysLogMgr::StaticLog(_eslmtType, std::move(strLog), fHasLogFile ? &slx : 0);
  }
  // Methods including errno:
  inline void Log(ESysLogMessageType _eslmtType, vtyJsoValueSysLog const &_rjvLog, int _errno, const char *_pcFmt, ...)
  {
    std::string strFmtAnnotated;
    { //B
      // Add the errno description onto the end of the error string.
      std::string strErrno;
      GetErrnoStdStr(_errno, strErrno);
      // We add <type>: to the start of the format string.
      PrintfStdStr(strFmtAnnotated, "<%s>: %s, %s", SysLogMgr::SzMessageType(_eslmtType), _pcFmt, strErrno.c_str());
    } //EB

    va_list ap;
    va_start(ap, _pcFmt);
    char tc;
    int nRequired = vsnprintf(&tc, 1, strFmtAnnotated.c_str(), ap);
    va_end(ap);
    if (nRequired < 0)
      THROWNAMEDEXCEPTION("n_SysLog::Log(): vsnprintf() returned nRequired[%d].", nRequired);
    va_start(ap, _pcFmt);
    std::string strLog;
    int nRet = NPrintfStdStr(strLog, nRequired, strFmtAnnotated.c_str(), ap);
    va_end(ap);
    if (nRet < 0)
      THROWNAMEDEXCEPTION("n_SysLog::Log(): NPrintfStdStr() returned nRet[%d].", nRequired);
    _SysLogContext slx;
    bool fHasLogFile = SysLogMgr::FStaticHasJSONLogFile();
    if (fHasLogFile)
    {
      slx.m_eslmtType = _eslmtType;
      slx.m_szFullMesg = strLog;
      slx.m_time = time(0);
      slx.m_nmsSinceProgramStart = SysLogMgr::_GetMsSinceProgramStart();
      slx.m_errno = _errno;
      slx.m_pjvLog = &_rjvLog;
    }
    SysLogMgr::StaticLog(_eslmtType, std::move(strLog), fHasLogFile ? &slx : 0);
  }
  void Log(ESysLogMessageType _eslmtType, vtyJsoValueSysLog const &_rjvLog, int _errno, const char *_pcFile, unsigned int _nLine, const char *_pcFmt, ...)
  {
    // We add [type]:_psFile:_nLine: to the start of the format string.
    std::string strFmtAnnotated;
    { //B
      // Add the errno description onto the end of the error string.
      std::string strErrno;
      GetErrnoStdStr(_errno, strErrno);
      PrintfStdStr(strFmtAnnotated, "<%s>:%s:%d: %s, %s", SysLogMgr::SzMessageType(_eslmtType), _pcFile, _nLine, _pcFmt, strErrno.c_str());
    }

    va_list ap;
    va_start(ap, _pcFmt);
    char tc;
    int nRequired = vsnprintf(&tc, 1, strFmtAnnotated.c_str(), ap);
    va_end(ap);
    if (nRequired < 0)
      THROWNAMEDEXCEPTION("n_SysLog::Log(): vsnprintf() returned nRequired[%d].", nRequired);
    va_start(ap, _pcFmt);
    std::string strLog;
    int nRet = NPrintfStdStr(strLog, nRequired, strFmtAnnotated.c_str(), ap);
    va_end(ap);
    if (nRet < 0)
      THROWNAMEDEXCEPTION("n_SysLog::Log(): NPrintfStdStr() returned nRet[%d].", nRequired);
    _SysLogContext slx;
    bool fHasLogFile = SysLogMgr::FStaticHasJSONLogFile();
    if (fHasLogFile)
    {
      slx.m_eslmtType = _eslmtType;
      slx.m_szFullMesg = strLog;
      slx.m_time = time(0);
      slx.m_nmsSinceProgramStart = SysLogMgr::_GetMsSinceProgramStart();
      slx.m_szFile = _pcFile;
      slx.m_nLine = _nLine;
      slx.m_errno = _errno;
      slx.m_pjvLog = &_rjvLog;
    }
    SysLogMgr::StaticLog(_eslmtType, std::move(strLog), fHasLogFile ? &slx : 0);
  }
} // namespace n_SysLog
