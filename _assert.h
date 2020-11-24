#pragma once

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _assert.h

#include <assert.h>
#include <string>
#include "_strutil.h"
#include "syslogmgr.h"
#include "jsonobjs.h"

// This file defines assertion and runtime check stuff.
// We allow assertion when ASSERTSENABLED is set non-zero. Normal in release binaries this is not set.
// Runtime checks are always enabled as the code is expect to *need* to run - i.e. the running
//  of code inside of a runtime check is not optional.

#ifdef Assert
#error _assert.h: Assert() macro is already defined.
#endif

#ifndef ASSERTSENABLED
#ifdef	NDEBUG
#define ASSERTSENABLED 0
#else
#define ASSERTSENABLED 1
#endif // !NDEBUG
#endif // !ASSERTSENABLED

#ifndef ABORTONASSERT
// By default we do not abort on assert. Abort generates a core dump so if you want to examine the state of things you would want to abort.
#define ABORTONASSERT 0
#endif // !ABORTONASSERT

#ifndef ABORTONVERIFY
// By default we do not abort on VERIFY. Abort generates a core dump so if you want to examine the state of things you would want to abort.
#define ABORTONVERIFY 0
#endif // !ABORTONVERIFY

// This failure mimicks the ANSI standard failure: Print a message (in our case to the syslog and potentially as well to the screen) and then flush the log file and abort() (if _fAbort).
inline void 
AssertVerify_LogMessage(  bool _fAbort, bool _fAssert, const char * _szAssertVerify, const char * _szAssertion, const char * _szFile, 
                          unsigned int _nLine, const char * _szFunction, const char * _szMesg, ... )
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

#define Verify(expr)							          \
     ( static_cast <bool> (expr)						\
      ? void (0)							              \
      : AssertVerify_LogMessage( !!ABORTONVERIFY, false, "Verify", #expr, __FILE__, __LINE__, __ASSERT_FUNCTION, 0 ) )
#define VerifySz(expr, sz...)							  \
     ( static_cast <bool> (expr)					  \
      ? void (0)							              \
      : AssertVerify_LogMessage( !!ABORTONVERIFY, false, "Verify", #expr, __FILE__, __LINE__, __ASSERT_FUNCTION, sz ) )

#if !ASSERTSENABLED
#define Assert(expr)		(static_cast<void>(0))
#define AssertSz(expr, sz...)		(static_cast<void>(0))
#else // #if !ASSERTSENABLED
#define Assert(expr)							          \
     ( static_cast <bool> (expr)						\
      ? void (0)							              \
      : AssertVerify_LogMessage( !!ABORTONASSERT, true, "Assert", #expr, __FILE__, __LINE__, __ASSERT_FUNCTION, 0 ) )
#define AssertSz(expr, sz...)							  \
     ( static_cast <bool> (expr)					  \
      ? void (0)							              \
      : AssertVerify_LogMessage( !!ABORTONASSERT, true, "Assert", #expr, __FILE__, __LINE__, __ASSERT_FUNCTION, sz ) )
#endif // #if !ASSERTSENABLED

