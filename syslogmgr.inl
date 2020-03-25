#pragma once

// syslogmgr.inl
// This abstracts the idea of a syslog - while also perhaps using syslog to log.
// dbien: 12MAR2020

#include "syslogmgr.h"

// _SysLogThreadHeader:
template < class t_tyJsonOutputStream >
void _SysLogThreadHeader::ToJSONStream( JsonValueLife< t_tyJsonOutputStream > & _jvl ) const
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
void _SysLogThreadHeader::FromJSONStream( JsonReadCursor< t_tyJsonInputStream > & _jrc )
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

// _SysLogContext:
template < class t_tyJsonOutputStream >
void _SysLogContext::ToJSONStream( JsonValueLife< t_tyJsonOutputStream > & _jvl ) const
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
void _SysLogContext::FromJSONStream( JsonReadCursor< t_tyJsonInputStream > & _jrc )
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

template < const int t_kiInstance >
bool 
_SysLogMgr< t_kiInstance >::_FTryCreateUniqueJSONLogFile( const char * _pszProgramName )
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
    std::unique_ptr< _tyJsonOutputStream > pjosThreadLog;
    pjosThreadLog = std::make_unique< _tyJsonOutputStream >();
    pjosThreadLog->Open( strLogFile.c_str() );
    _tyJsonFormatSpec jfs; // Make sure we pretty print the JSON to make it readable right away.
    jfs.m_nWhitespacePerIndent = 2;
    jfs.m_fEscapePrintableWhitespace = true;
    std::unique_ptr< _tyJsonValueLife > pjvlRoot = std::make_unique< _tyJsonValueLife >( *pjosThreadLog, ejvtObject, &jfs );
    { //B
        // Create the SysLogThreadHeader object as the first object within the log file.
        _tyJsonValueLife jvlSysLogThreadHeader( *pjvlRoot, "SysLogThreadHeader", ejvtObject );
        slth.ToJSONStream( jvlSysLogThreadHeader );
    } //EB
    // Now open up an array to contain the set of log message details.
    std::unique_ptr< _tyJsonValueLife > pjvlSysLogArray = std::make_unique< _tyJsonValueLife >( *pjvlRoot, "SysLog", ejvtArray );
    // Now forward everything into the object itself - we succeeded in creating the logging file.
    m_pjosThreadLog.swap( pjosThreadLog );
    m_pjvlRootThreadLog.swap( pjvlRoot );
    m_pjvlSysLogArray.swap( pjvlSysLogArray );
    n_SysLog::Log( eslmtInfo, "SysLogMgr: Created thread-specific JSON log file at [%s].", strLogFile.c_str() );
    return true;
}
