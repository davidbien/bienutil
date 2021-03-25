#pragma once

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// syslogmgr.inl
// This abstracts the idea of a syslog - while also perhaps using syslog to log.
// dbien: 12MAR2020

#include "syslogmgr.h"
#include "_strutil.h"
#include "jsonobjs.h"
#include "_heapchk.h"

__BIENUTIL_BEGIN_NAMESPACE

namespace n_SysLog
{
  inline void Log(ESysLogMessageType _eslmtType, const char *_pcFmt, ...)
  {
    // We add <type>: to the start of the format string.
    std::string strFmtAnnotated;
    PrintfStdStr(strFmtAnnotated, "<%s>: %s", SysLogMgr::SzMessageType(_eslmtType), _pcFmt);

    va_list ap2;
    int nRequired;
    {//B
      va_list ap;
      va_start(ap, _pcFmt);
      va_copy(ap2,ap);
      nRequired = vsnprintf(NULL, 0, strFmtAnnotated.c_str(), ap);
      va_end(ap);
      if (nRequired < 0)
        THROWNAMEDEXCEPTION("vsnprintf() returned nRequired[%d].", nRequired);
    }//EB
    std::string strLog;
    int nRet = NPrintfStdStr(strLog, nRequired, strFmtAnnotated.c_str(), ap2);
    va_end(ap2);
    if (nRet < 0)
      THROWNAMEDEXCEPTION("NPrintfStdStr() returned nRet[%d].", nRet);

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
  inline void Log(ESysLogMessageType _eslmtType, const char *_pcFile, unsigned int _nLine, const char *_pcFmt, ...)
  {
    // We add [type]:_psFile:_nLine: to the start of the format string.
    std::string strFmtAnnotated;
    PrintfStdStr(strFmtAnnotated, "<%s>:%s:%d: %s", SysLogMgr::SzMessageType(_eslmtType), _pcFile, _nLine, _pcFmt);

    va_list ap2;
    int nRequired;
    {//B
      va_list ap;
      va_start(ap, _pcFmt);
      va_copy(ap2, ap);
      nRequired = vsnprintf(NULL, 0, strFmtAnnotated.c_str(), ap);
      va_end(ap);
      if (nRequired < 0)
        THROWNAMEDEXCEPTION("vsnprintf() returned nRequired[%d].", nRequired);
    }//EB
    std::string strLog;
    int nRet = NPrintfStdStr(strLog, nRequired, strFmtAnnotated.c_str(), ap2);
    va_end(ap2);
    if (nRet < 0)
      THROWNAMEDEXCEPTION("NPrintfStdStr() returned nRet[%d].", nRet);
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

    va_list ap2;
    int nRequired;
    {//B
      va_list ap;
      va_start(ap, _pcFmt);
      va_copy(ap2, ap);
      nRequired = vsnprintf(NULL, 0, strFmtAnnotated.c_str(), ap);
      va_end(ap);
      if (nRequired < 0)
        THROWNAMEDEXCEPTION("vsnprintf() returned nRequired[%d].", nRequired);
    }//EB
    std::string strLog;
    int nRet = NPrintfStdStr(strLog, nRequired, strFmtAnnotated.c_str(), ap2);
    va_end(ap2);
    if (nRet < 0)
      THROWNAMEDEXCEPTION("NPrintfStdStr() returned nRet[%d].", nRet);
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
  inline void Log(ESysLogMessageType _eslmtType, int _errno, const char *_pcFile, unsigned int _nLine, const char *_pcFmt, ...)
  {
    // We add [type]:_psFile:_nLine: to the start of the format string.
    std::string strFmtAnnotated;
    { //B
      // Add the errno description onto the end of the error string.
      std::string strErrno;
      GetErrnoStdStr(_errno, strErrno);
      PrintfStdStr(strFmtAnnotated, "<%s>:%s:%d: %s, %s", SysLogMgr::SzMessageType(_eslmtType), _pcFile, _nLine, _pcFmt, strErrno.c_str());
    }

    va_list ap2;
    int nRequired;
    {//B
      va_list ap;
      va_start(ap, _pcFmt);
      va_copy(ap2, ap);
      nRequired = vsnprintf(NULL, 0, strFmtAnnotated.c_str(), ap);
      va_end(ap);
      if (nRequired < 0)
        THROWNAMEDEXCEPTION("vsnprintf() returned nRequired[%d].", nRequired);
    }//EB
    std::string strLog;
    int nRet = NPrintfStdStr(strLog, nRequired, strFmtAnnotated.c_str(), ap2);
    va_end(ap2);
    if (nRet < 0)
      THROWNAMEDEXCEPTION("NPrintfStdStr() returned nRet[%d].", nRet);
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

    va_list ap2;
    int nRequired;
    {//B
      va_list ap;
      va_start(ap, _pcFmt);
      va_copy(ap2, ap);
      nRequired = vsnprintf(NULL, 0, strFmtAnnotated.c_str(), ap);
      va_end(ap);
      if (nRequired < 0)
        THROWNAMEDEXCEPTION("vsnprintf() returned nRequired[%d].", nRequired);
    }//EB
    std::string strLog;
    int nRet = NPrintfStdStr(strLog, nRequired, strFmtAnnotated.c_str(), ap2);
    va_end(ap2);
    if (nRet < 0)
      THROWNAMEDEXCEPTION("NPrintfStdStr() returned nRet[%d].", nRet);

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
  inline void Log(ESysLogMessageType _eslmtType, vtyJsoValueSysLog const &_rjvLog, const char *_pcFile, unsigned int _nLine, const char *_pcFmt, ...)
  {
    // We add [type]:_psFile:_nLine: to the start of the format string.
    std::string strFmtAnnotated;
    PrintfStdStr(strFmtAnnotated, "<%s>:%s:%d: %s", SysLogMgr::SzMessageType(_eslmtType), _pcFile, _nLine, _pcFmt);

    va_list ap2;
    int nRequired;
    {//B
      va_list ap;
      va_start(ap, _pcFmt);
      va_copy(ap2, ap);
      nRequired = vsnprintf(NULL, 0, strFmtAnnotated.c_str(), ap);
      va_end(ap);
      if (nRequired < 0)
        THROWNAMEDEXCEPTION("vsnprintf() returned nRequired[%d].", nRequired);
    }//EB
    std::string strLog;
    int nRet = NPrintfStdStr(strLog, nRequired, strFmtAnnotated.c_str(), ap2);
    va_end(ap2);
    if (nRet < 0)
      THROWNAMEDEXCEPTION("NPrintfStdStr() returned nRet[%d].", nRet);
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

    va_list ap2;
    int nRequired;
    {//B
      va_list ap;
      va_start(ap, _pcFmt);
      va_copy(ap2, ap);
      nRequired = vsnprintf(NULL, 0, strFmtAnnotated.c_str(), ap);
      va_end(ap);
      if (nRequired < 0)
        THROWNAMEDEXCEPTION("vsnprintf() returned nRequired[%d].", nRequired);
    }//EB
    std::string strLog;
    int nRet = NPrintfStdStr(strLog, nRequired, strFmtAnnotated.c_str(), ap2);
    va_end(ap2);
    if (nRet < 0)
      THROWNAMEDEXCEPTION("NPrintfStdStr() returned nRet[%d].", nRet);
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
  inline void Log(ESysLogMessageType _eslmtType, vtyJsoValueSysLog const &_rjvLog, int _errno, const char *_pcFile, unsigned int _nLine, const char *_pcFmt, ...)
  {
    // We add [type]:_psFile:_nLine: to the start of the format string.
    std::string strFmtAnnotated;
    { //B
      // Add the errno description onto the end of the error string.
      std::string strErrno;
      GetErrnoStdStr(_errno, strErrno);
      PrintfStdStr(strFmtAnnotated, "<%s>:%s:%d: %s, %s", SysLogMgr::SzMessageType(_eslmtType), _pcFile, _nLine, _pcFmt, strErrno.c_str());
    }

    va_list ap2;
    int nRequired;
    {//B
      va_list ap;
      va_start(ap, _pcFmt);
      va_copy(ap2, ap);
      nRequired = vsnprintf(NULL, 0, strFmtAnnotated.c_str(), ap);
      va_end(ap);
      if (nRequired < 0)
        THROWNAMEDEXCEPTION("vsnprintf() returned nRequired[%d].", nRequired);
    }//EB
    std::string strLog;
    int nRet = NPrintfStdStr(strLog, nRequired, strFmtAnnotated.c_str(), ap2);
    va_end(ap2);
    if (nRet < 0)
      THROWNAMEDEXCEPTION("NPrintfStdStr() returned nRet[%d].", nRet);
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

// _SysLogThreadHeader:
template <class t_tyJsonOutputStream>
void _SysLogThreadHeader::ToJSONStream(JsonValueLife<t_tyJsonOutputStream> &_jvl) const
{
  // We should be in an object in the JSONStream and we will add our (key,values) to this object.
  Assert(_jvl.FAtObjectValue());
  if (_jvl.FAtObjectValue())
  {
    _jvl.WriteStringValue("ProgName", m_szProgramName);
    _jvl.WriteUuidStringValue("uuid", m_uuid);
    _jvl.WriteTimeStringValue("TimeStarted", m_timeStart);
    _jvl.WriteValue("msSinceProgramStart", m_nmsSinceProgramStart);
    _jvl.WriteValue("ThreadId", m_tidThreadId);
  }
  else
    THROWNAMEDEXCEPTION("Not at an object.");
}

template <class t_tyJsonInputStream>
void _SysLogThreadHeader::FromJSONStream(JsonReadCursor<t_tyJsonInputStream> &_jrc)
{
  Clear();
  // We should be in an object in the JSONStream and we will add our (key,values) to this object.
  Assert(_jrc.FAtObjectValue());
  if (_jrc.FAtObjectValue())
  {
    JsonRestoreContext<t_tyJsonInputStream> jrx(_jrc);
    if (_jrc.FMoveDown())
    {
      for (; !_jrc.FAtEndOfAggregate(); (void)_jrc.FNextElement())
      {
        // Ignore unknown stuff:
        typedef typename t_tyJsonInputStream::_tyCharTraits _tyCharTraits;
        typename _tyCharTraits::_tyStdStr strKey;
        EJsonValueType jvtValue;
        bool fGetKey = _jrc.FGetKeyCurrent(strKey, jvtValue);
        Assert(fGetKey);
        if (fGetKey)
        {
          if (ejvtString == jvtValue)
          {
            if (strKey == "ProgName")
            {
              _jrc.GetValue(m_szProgramName);
              continue;
            }
            if (strKey == "TimeStarted")
            {
              _jrc.GetTimeStringValue(m_timeStart);
              continue;
            }
            if (strKey == "msSinceProgramStart")
            {
              _jrc.GetValue(m_nmsSinceProgramStart);
              continue;
            }
            if (strKey == "uuid")
            {
              _jrc.GetUuidStringValue(m_uuid);
              continue;
            }
            continue;
          }
          if (ejvtNumber == jvtValue)
          {
            if (strKey == "ThreadId")
            {
              _jrc.GetValue(m_tidThreadId);
              continue;
            }
            continue;
          }
        }
      }
    }
  }
  else
    THROWNAMEDEXCEPTION("Not at an object.");
}

// _SysLogContext:
template <class t_tyJsonOutputStream>
void _SysLogContext::ToJSONStream(JsonValueLife<t_tyJsonOutputStream> &_jvl) const
{
  // We should be in an object in the JSONStream and we will add our (key,values) to this object.
  Assert(_jvl.FAtObjectValue());
  if (_jvl.FAtObjectValue())
  {
    _jvl.WriteValue("msec", m_nmsSinceProgramStart);
    _jvl.WriteTimeStringValue("Time", m_time);
    _jvl.WriteValue("Type", (uint8_t)m_eslmtType);
    _jvl.WriteStringValue("Mesg", m_szFullMesg);
    if (!m_szFile.empty())
    {
      _jvl.WriteStringValue("File", m_szFile);
      _jvl.WriteValue("Line", m_nLine);
    }
    if (!!m_errno)
    {
      _jvl.WriteValue("errno", m_errno);
      std::string strErrDesc;
      GetErrnoDescStdStr(m_errno, strErrDesc);
      if (!!strErrDesc.length())
        _jvl.WriteStringValue("ErrnoDesc", strErrDesc);
    }
    if (!!m_pjvLog)
    {
      JsonValueLife<t_tyJsonOutputStream> jvlDetail(_jvl, "Detail", m_pjvLog->JvtGetValueType());
      m_pjvLog->ToJSONStream(jvlDetail);
    }
  }
  else
    THROWNAMEDEXCEPTION("Not at an object.");
}

template <class t_tyJsonInputStream>
void _SysLogContext::FromJSONStream(JsonReadCursor<t_tyJsonInputStream> &_jrc)
{
  Clear();
  // We should be in an object in the JSONStream and we will add our (key,values) to this object.
  Assert(_jrc.FAtObjectValue());
  if (_jrc.FAtObjectValue())
  {
    JsonRestoreContext<t_tyJsonInputStream> jrx(_jrc);
    if (_jrc.FMoveDown())
    {
      for (; !_jrc.FAtEndOfAggregate(); (void)_jrc.FNextElement())
      {
        // Ignore unknown stuff:
        typedef typename t_tyJsonInputStream::_tyCharTraits _tyCharTraits;
        typename _tyCharTraits::_tyStdStr strKey;
        EJsonValueType jvtValue;
        bool fGetKey = _jrc.FGetKeyCurrent(strKey, jvtValue);
        Assert(fGetKey);
        if (fGetKey)
        {
          if (ejvtString == jvtValue)
          {
            if (strKey == "msec")
            {
              _jrc.GetValue(m_nmsSinceProgramStart);
              continue;
            }
            if (strKey == "Time")
            {
              _jrc.GetValue(m_time);
              continue;
            }
            if (strKey == "Mesg")
            {
              _jrc.GetValue(m_szFullMesg);
              continue;
            }
            if (strKey == "File")
            {
              _jrc.GetValue(m_szFile);
              continue;
            }
            continue;
          }
          if (ejvtNumber == jvtValue)
          {
            if (strKey == "Type")
            {
              _jrc.GetValue(m_eslmtType);
              continue;
            }
            if (strKey == "Line")
            {
              _jrc.GetValue(m_nLine);
              continue;
            }
            if (strKey == "errno")
            {
              _jrc.GetValue(m_errno);
              continue;
            }
            continue;
          }
        }
      }
    }
  }
  else
    THROWNAMEDEXCEPTION("Not at an object.");
}

template <const int t_kiInstance>
_SysLogMgr<t_kiInstance>::_SysLogMgr(_SysLogMgr *_pslmOverlord)
    : m_pslmOverlord(_pslmOverlord) // if !m_pslmOverlord then we are the overlord!!! - or there is no overlord.
{
}
template <const int t_kiInstance>
_SysLogMgr<t_kiInstance>::~_SysLogMgr()
{
  CloseSysLogFile();
}

template <const int t_kiInstance>
bool _SysLogMgr<t_kiInstance>::_FTryCreateUniqueJSONLogFile(const char *_pszProgramName, const n_SysLog::vtyJsoValueSysLog *_pjvThreadSpecificJson)
{
  // Move _pszProgramName past any directory:
  const char *pszProgNameNoPath = strrchr(_pszProgramName, TChGetFileSeparator<char>() );
  if (!!pszProgNameNoPath)
  {
    _pszProgramName = pszProgNameNoPath + 1;
  }
  // Now get the executable path:
  std::string strExePath;
  GetCurrentExecutablePath(strExePath);
  if (!strExePath.length())
  {
    strExePath = "."; // Just use current working directory if we can't find exe directory.
    strExePath += TChGetFileSeparator<char>();
  }
  Assert(TChGetFileSeparator<char>() == strExePath[strExePath.length() - 1]);
  _SysLogThreadHeader slth;
  slth.m_szProgramName = strExePath;
  slth.m_szProgramName += _pszProgramName; // Put full path to EXE here for disambiguation when working with multiple versions.
  slth.m_timeStart = time(0);
  slth.m_nmsSinceProgramStart = _GetMsSinceProgramStart();
  UUIDCreate(slth.m_uuid);
  slth.m_tidThreadId = s_tls_tidThreadId;

  std::string strLogFile = slth.m_szProgramName;
  vtyUUIDString uusUuid;
  UUIDToString(slth.m_uuid, uusUuid, sizeof uusUuid);
  strLogFile += ".";
  strLogFile += uusUuid;
  strLogFile += ".log.json";

  // We must make sure we can initialize the file before we declare that it is opened.
  std::unique_ptr<_tyJsonOutputStream> pjosThreadLog;
  pjosThreadLog = std::make_unique<_tyJsonOutputStream>();
  pjosThreadLog->Open(strLogFile.c_str(), FileSharing::ShareRead);
  _tyJsonFormatSpec jfs; // Make sure we pretty print the JSON to make it readable right away.
  jfs.m_nWhitespacePerIndent = 2;
  jfs.m_fEscapePrintableWhitespace = true;
  std::unique_ptr<_tyJsonValueLife> pjvlRoot = std::make_unique<_tyJsonValueLife>(*pjosThreadLog, ejvtObject, &jfs);
  { //B
    // Create the SysLogThreadHeader object as the first object within the log file.
    _tyJsonValueLife jvlSysLogThreadHeader(*pjvlRoot, "SysLogThreadHeader", ejvtObject);
    slth.ToJSONStream(jvlSysLogThreadHeader);
    if (_pjvThreadSpecificJson) // write any thread-specific JSON data to the log header.
    {
      _tyJsonValueLife jvlThreadSpec(jvlSysLogThreadHeader, "ThreadSpecificData", _pjvThreadSpecificJson->JvtGetValueType());
      _pjvThreadSpecificJson->ToJSONStream(jvlThreadSpec);
    }
  } //EB
  // Now open up an array to contain the set of log message details.
  std::unique_ptr<_tyJsonValueLife> pjvlSysLogArray = std::make_unique<_tyJsonValueLife>(*pjvlRoot, "SysLog", ejvtArray);
  // Now forward everything into the object itself - we succeeded in creating the logging file.
  m_pjosThreadLog.swap(pjosThreadLog);
  m_pjvlRootThreadLog.swap(pjvlRoot);
  m_pjvlSysLogArray.swap(pjvlSysLogArray);
  n_SysLog::Log(eslmtInfo, "SysLogMgr: Created thread-specific JSON log file at [%s].", strLogFile.c_str());
  return true;
}

template <const int t_kiInstance>
void _SysLogMgr<t_kiInstance>::Log(ESysLogMessageType _eslmt, std::string &&_rrStrLog, const _SysLogContext *_pslc)
{
#ifndef WIN32
  int iPriority;
  switch (_eslmt)
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
    Assert(0);
  case eslmtError:
    iPriority = LOG_ERR;
    break;
  }
  iPriority |= LOG_USER;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
  // First log.
  syslog(iPriority, _rrStrLog.c_str());
#pragma GCC diagnostic pop
#else //WIN32
  if ( m_grfOption & LOG_PERROR )
  {
    fprintf( stderr, "%s\n", _rrStrLog.c_str() );
  }
#endif //WIN32

  // Then log the context to thread logging file.
  if (!!_pslc && !!m_pjosThreadLog && m_pjosThreadLog->FOpened() && !!m_pjvlRootThreadLog && !!m_pjvlSysLogArray)
  {
    // Create an object for this log message:
    _tyJsonValueLife jvlSysLogContext(*m_pjvlSysLogArray, ejvtObject);
    _pslc->ToJSONStream(jvlSysLogContext);
  }
}

template <const int t_kiInstance>
bool _SysLogMgr<t_kiInstance>::FHasJSONLogFile() const
{
  Assert((!!m_pjosThreadLog && m_pjosThreadLog->FOpened()) == !!m_pjvlRootThreadLog);
  Assert((!!m_pjosThreadLog && m_pjosThreadLog->FOpened()) == !!m_pjvlSysLogArray);
  return !!m_pjosThreadLog && m_pjosThreadLog->FOpened();
}

template <const int t_kiInstance>
void _SysLogMgr<t_kiInstance>::CloseSysLogFile() noexcept(true)
{
  // To close merely release each unique_ptr in the same order the object would be destructed:
  try
  {
    m_pjvlSysLogArray.reset();
  }
  catch(...)
  {
  }
  try
  {
    m_pjvlRootThreadLog.reset();
  }
  catch(...)
  {
  }
  try
  {
    m_pjosThreadLog.reset();
  }
  catch (...)
  {
  }
}

// This failure mimicks the ANSI standard failure: Print a message (in our case to the syslog and potentially as well to the screen) and then flush the log file and abort() (if _fAbort).
inline void 
AssertVerify_LogMessage(  EAbortBreakIgnore _eabi, bool _fAssert, const char * _szAssertVerify, const char * _szAssertion, const char * _szFile, 
                          unsigned int _nLine, const char * _szFunction, const char * _szMesg, ... )
{
  if ( !n_SysLog::FSetInAssertOrVerify( true ) )
  {
    try
    {
      std::string strMesg;
      if ( !!_szMesg && !!*_szMesg )
      {
        va_list ap2;
        int nRequired;
        {//B
          va_list ap;
          va_start(ap, _szMesg);
          va_copy(ap2, ap);
          nRequired = vsnprintf( NULL, 0, _szMesg, ap );
          va_end(ap);
        }//EB
        if ( nRequired < 0 )
          (void)FPrintfStdStrNoThrow( strMesg, "AssertVerify_LogMessage(): vsnprintf() returned nRequired[%d].", nRequired );
        else
        {
          int nRet = NPrintfStdStr( strMesg, nRequired, _szMesg, ap2 );
          va_end(ap2);
          if (nRet < 0)
            (void)FPrintfStdStrNoThrow( strMesg, "AssertVerify_LogMessage(): 2nd call to vsnprintf() returned nRequired[%d].", nRequired );
        }
      }
      
      // We must check if there are any % signs in _szAssertion and substitute %% for each - otherwise these can cause the program to ABORT - which we do not prefer.
      string strAssertion( _szAssertion );
      for ( size_t posPercent = strAssertion.find('%'); posPercent != string::npos; posPercent = strAssertion.find( '%', posPercent + 2 ) )
        strAssertion.insert( strAssertion.begin() + posPercent, '%' );

      // We log both the full string - which is the only thing that will end up in the syslog - and each field individually in JSON to allow searching for specific criteria easily.
      std::string strFmt;
      (void)FPrintfStdStrNoThrow( strFmt, !strMesg.length() ? "%s:[%s:%d],%s(): %s." : "%s:[%s:%d],%s: %s. %s", _szAssertVerify, _szFile, _nLine, _szFunction, strAssertion.c_str(), strMesg.c_str() );

      n_SysLog::vtyJsoValueSysLog jvLog( ejvtObject );
      jvLog("szAssertion").SetStringValue( _szAssertion );
      if ( strMesg.length() )
        jvLog("Mesg").SetStringValue( std::move( strMesg ) );
      jvLog("szFunction").SetStringValue( _szFunction );
      jvLog("szFile").SetStringValue( _szFile );
      jvLog("nLine").SetValue( _nLine );
      jvLog("fAssert").SetBoolValue( _fAssert );

      n_SysLog::Log( eslmtError, jvLog, strFmt.c_str() );
      if ( eabiAbort == _eabi )
      {
        n_SysLog::CloseThreadSysLog(); // flush and close syslog for this thread only before we abort.
        (void)n_SysLog::FSetInAssertOrVerify( false );
        abort();
      }
      else
      if ( eabiThrowException == _eabi )
      {
        (void)n_SysLog::FSetInAssertOrVerify( false );
        THROWVERIFYFAILEDEXCEPTION( strFmt.c_str() );
      }
      else
      if ( eabiBreak == _eabi )
      {
        (void)n_SysLog::FSetInAssertOrVerify( false ); // Change this here since we may be able to change lines in a method above us in the debugger in VC for instance.
        DEBUG_BREAK;
      }
    }
    catch( ... )
    {
      (void)n_SysLog::FSetInAssertOrVerify( false );
      fprintf( stderr, "AssertVerify_LogMessage(): Caught exception - rethrowing.\n" );
      throw;
    }
    (void)n_SysLog::FSetInAssertOrVerify( false );
  }
}

inline void 
Trace_LogMessageVArg( EAbortBreakIgnore _eabi, const char * _szFile, unsigned int _nLine, const char * _szFunction, const n_SysLog::vtyJsoValueSysLog * _pjvTrace, const char * _szMesg, va_list _ap )
{
  try
  {
    std::string strMesg;
    if ( !!_szMesg && !!*_szMesg )
    {
      va_list ap2;
      int nRequired;
      {//B
        va_copy(ap2, _ap);
        nRequired = vsnprintf( NULL, 0, _szMesg, _ap );
        va_end(_ap);
      }//EB
      if ( nRequired < 0 )
        (void)FPrintfStdStrNoThrow( strMesg, "Trace_LogMessageVArg(): vsnprintf() returned nRequired[%d].", nRequired );
      else
      {
        int nRet = NPrintfStdStr( strMesg, nRequired, _szMesg, ap2 );
        va_end(ap2);
        if (nRet < 0)
          (void)FPrintfStdStrNoThrow( strMesg, "Trace_LogMessageVArg(): 2nd call to vsnprintf() returned nRequired[%d].", nRequired );
      }
    }

    // We log both the full string - which is the only thing that will end up in the syslog - and each field individually in JSON to allow searching for specific criteria easily.
    std::string strFmt;
    (void)FPrintfStdStrNoThrow( strFmt, !strMesg.length() ? "Trace:[%s:%d],%s()" : "Trace:[%s:%d],%s: %s", _szFile, _nLine, _szFunction, strMesg.c_str() );

    n_SysLog::vtyJsoValueSysLog jvLog( ejvtObject );
    if ( strMesg.length() )
      jvLog("Mesg").SetStringValue( std::move( strMesg ) );
    jvLog("szFunction").SetStringValue( _szFunction );
    jvLog("szFile").SetStringValue( _szFile );
    jvLog("nLine").SetValue( _nLine );
    jvLog("fTrace").SetBoolValue( true );
    if ( !!_pjvTrace )
      jvLog("Detail") = *_pjvTrace;

    n_SysLog::Log( eslmtInfo, jvLog, strFmt.c_str() );
    if ( eabiAbort == _eabi )
    {
      n_SysLog::CloseThreadSysLog(); // flush and close syslog for this thread only before we abort.
      abort();
    }
    else
    if ( eabiBreak == _eabi )
    {
      DEBUG_BREAK;
    }
  }
  catch( std::exception const & _rexc )
  {
    fprintf( stderr, "Trace_LogMessageVArg(): Caught exception: [%s].\n", _rexc.what() );
    return;
  }
  catch( ... )
  {
    fprintf( stderr, "Trace_LogMessageVArg(): Caught unknown exception.\n" );
    throw;
  }
}
inline void 
Trace_LogMessage( EAbortBreakIgnore _eabi, const char * _szFile, unsigned int _nLine, const char * _szFunction, const n_SysLog::vtyJsoValueSysLog * _pjvTrace, const char * _szMesg, ... )
{
  va_list ap;
  if ( !!_szMesg && !!*_szMesg )
    va_start(ap, _szMesg);
  Trace_LogMessageVArg( _eabi, _szFile, _nLine, _szFunction, _pjvTrace, _szMesg, ap );
}

__BIENUTIL_END_NAMESPACE
