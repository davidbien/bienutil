#pragma once

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// syslogmgr.inl
// This abstracts the idea of a syslog - while also perhaps using syslog to log.
// dbien: 12MAR2020

#include "syslogmgr.h"

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
    THROWNAMEDEXCEPTION("_SysLogThreadHeader::ToJSONStream(): Not at an object.");
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
    THROWNAMEDEXCEPTION("_SysLogThreadHeader::ToJSONStream(): Not at an object.");
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
    THROWNAMEDEXCEPTION("_SysLogContext::ToJSONStream(): Not at an object.");
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
              _jrc.GetValue(m_nmsSinceStart);
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
    THROWNAMEDEXCEPTION("_SysLogContext::ToJSONStream(): Not at an object.");
}

template <const int t_kiInstance>
bool _SysLogMgr<t_kiInstance>::_FTryCreateUniqueJSONLogFile(const char *_pszProgramName, const n_SysLog::vtyJsoValueSysLog *_pjvThreadSpecificJson)
{
  // Move _pszProgramName past any directory:
  const char *pszProgNameNoPath = strrchr(_pszProgramName, '/');
  if (!!pszProgNameNoPath)
    _pszProgramName = pszProgNameNoPath + 1;
  // Now get the executable path:
  std::string strExePath;
  GetCurrentExecutablePath(strExePath);
  if (!strExePath.length())
    strExePath = "./"; // Just use current working directory if we can't find exe directory.
  Assert('/' == strExePath[strExePath.length() - 1]);
  _SysLogThreadHeader slth;
  slth.m_szProgramName = strExePath;
  slth.m_szProgramName += _pszProgramName; // Put full path to EXE here for disambiguation when working with multiple versions.
  slth.m_timeStart = time(0);
  slth.m_nmsSinceProgramStart = _GetMsSinceProgramStart();
  uuid_generate(slth.m_uuid);
  slth.m_tidThreadId = s_tls_tidThreadId;

  std::string strLogFile = slth.m_szProgramName;
  uuid_string_t ustUuid;
  uuid_unparse_lower(slth.m_uuid, ustUuid);
  strLogFile += ".";
  strLogFile += ustUuid;
  strLogFile += ".log.json";

  // We must make sure we can initialize the file before we declare that it is opened.
  std::unique_ptr<_tyJsonOutputStream> pjosThreadLog;
  pjosThreadLog = std::make_unique<_tyJsonOutputStream>();
  pjosThreadLog->Open(strLogFile.c_str());
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
AssertVerify_LogMessage(  bool _fAbort, bool _fAssert, const char * _szAssertVerify, const char * _szAssertion, const char * _szFile, 
                          unsigned int _nLine, const char * _szFunction, const char * _szMesg, ... )
{
  if ( !n_SysLog::FSetInAssertOrVerify( true ) )
  {
    try
    {
      std::string strMesg;
      if ( !!_szMesg && !!*_szMesg )
      {
        va_list ap;
        va_start(ap, _szMesg);
        char tc;
        int nRequired = vsnprintf( &tc, 1, _szMesg, ap );
        va_end(ap);
        if ( nRequired < 0 )
          (void)FPrintfStdStrNoThrow( strMesg, "AssertVerify_LogMessage(): vsnprintf() returned nRequired[%d].", nRequired );
        else
        {
          va_start(ap, _szMesg);
          int nRet = NPrintfStdStr( strMesg, nRequired, _szMesg, ap );
          va_end(ap);
          if (nRet < 0)
            (void)FPrintfStdStrNoThrow( strMesg, "AssertVerify_LogMessage(): 2nd call to vsnprintf() returned nRequired[%d].", nRequired );
        }
      }

      // We log both the full string - which is the only thing that will end up in the syslog - and each field individually in JSON to allow searching for specific criteria easily.
      std::string strFmt;
      (void)FPrintfStdStrNoThrow( strFmt, !strMesg.length() ? "%s:[%s:%d],%s(): %s." : "%s:[%s:%d],%s(): %s. %s", _szAssertVerify, _szFile, _nLine, _szFunction, _szAssertion, strMesg.c_str() );

      n_SysLog::vtyJsoValueSysLog jvLog( ejvtObject );
      jvLog("szAssertion").SetStringValue( _szAssertion );
      if ( strMesg.length() )
        jvLog("Mesg").SetStringValue( std::move( strMesg ) );
      jvLog("szFunction").SetStringValue( _szFunction );
      jvLog("szFile").SetStringValue( _szFile );
      jvLog("nLine").SetValue( _nLine );
      jvLog("fAssert").SetBoolValue( _fAssert );

      n_SysLog::Log( eslmtError, jvLog, strFmt.c_str() );
      if ( _fAbort )
      {
        n_SysLog::CloseThreadSysLog(); // flush and close syslog for this thread only before we abort.
        abort();
      }
    }
    catch( ... )
    {
      (void)n_SysLog::FSetInAssertOrVerify( false );
      throw;
    }
  }

}
