#pragma once

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _assert.h

#include <assert.h>
#include <string>
#include "syslogmgr.h"
#include "_namdexc.h"

// This file defines assertion and runtime check stuff.
// We allow assertion when ASSERTSENABLED is set non-zero. Normal in release binaries this is not set.
// Runtime checks are always enabled as the code is expect to *need* to run - i.e. the running
//  of code inside of a runtime check is not optional.

__BIENUTIL_BEGIN_NAMESPACE

#ifdef Assert
#error _assert.h: Assert() macro is already defined.
#endif

enum EAbortBreakIgnore
{
  eabiIgnore,           // Ignore entirely - log and continue.
  eabiBreak,            // Break into the debugger but otherwise same as Ignore.
  eabiThrowException,   // Throw an exception after logging - this is not valid for Assert() because it changes the flow of control - thought I guess it could be used for unit testing.
  eabiAbort,            // Abort after logging.
  eabiEAbortBreakIgnoreCount
};

// This exception will get thrown when eabiThrowException is set.
class VerifyFailedException : public _t__Named_exception< >
{
  typedef VerifyFailedException _TyThis;
  typedef _t__Named_exception<> _TyBase;
public:
  VerifyFailedException( const char * _pc ) 
      : _TyBase( _pc ) 
  {
  }
  VerifyFailedException( const string_type & __s ) 
      : _TyBase( __s ) 
  {
  }
  VerifyFailedException( const char * _pcFmt, va_list _args )
      : _TyBase( _pcFmt, _args )
  {
  }
};
// By default we will always add the __FILE__, __LINE__ even in retail for debugging purposes.
#define THROWVERIFYFAILEDEXCEPTION( MESG, ... ) ExceptionUsage<VerifyFailedException>::ThrowFileLine( __FILE__, __LINE__, MESG, ##__VA_ARGS__ )

#ifndef ASSERTSENABLED
#ifdef	NDEBUG
#define ASSERTSENABLED 0
#else
#define ASSERTSENABLED 1
#endif // !NDEBUG
#endif // !ASSERTSENABLED

// We might have assert-related statements - i.e. when we record some initial state and then assert something about it later.
// In those case we need to have these previous assert-related statements (or subsequent for that matter) present in the build regardless of NDEBUG value.
#if ASSERTSENABLED
#define AssertStatement( s ) s;
#else
#define AssertStatement( s )
#endif
#if ASSERTSENABLED
#define AssertFragment( s ) s
#else
#define AssertFragment( s )
#endif
#if ASSERTSENABLED
#define AssertComma( s )  ,s
#else
#define AssertComma( s )
#endif
#if ASSERTSENABLED
#define AssertComma2( s1, s2 ) s1##,##s2
#else
#define AssertComma2( s )
#endif

#ifndef ACTIONONASSERT
// By default we do not abort on assert. Abort generates a core dump so if you want to examine the state of things you would want to abort.
#define ACTIONONASSERT eabiBreak
#endif // !ACTIONONASSERT

#ifndef ACTIONONVERIFY
// By default we do not abort on VERIFY. Abort generates a core dump so if you want to examine the state of things you would want to abort.
#define ACTIONONVERIFY eabiBreak
#endif // !ACTIONONVERIFY

// This failure mimicks the ANSI standard failure: Print a message (in our case to the syslog and potentially as well to the screen) and then flush the log file and abort() (if _fAbort).
// Need to define this later so we can use Assert() in various objects that we'd like to.
void 
AssertVerify_LogMessage(  EAbortBreakIgnore _eabi, bool _fAssert, const char * _szAssertVerify, const char * _szAssertion, const char * _szFile, 
                          unsigned int _nLine, const char * _szFunction, const char * _szMesg, ... );

// Verify() macros that by default will break into the debugger.
#define Verify(expr)							          \
     ( static_cast <bool> (expr)						\
      ? void (0)							              \
      : AssertVerify_LogMessage( ACTIONONVERIFY, false, "Verify", #expr, __FILE__, __LINE__, FUNCTION_PRETTY_NAME, 0 ) )
#define VerifySz(expr, sz, ...)							  \
     ( static_cast <bool> (expr)					  \
      ? void (0)							              \
      : AssertVerify_LogMessage( ACTIONONVERIFY, false, "Verify", #expr, __FILE__, __LINE__, FUNCTION_PRETTY_NAME, sz, ##__VA_ARGS__ ) )
// FVerifyInline(expr) expects expr to be true and will assert if not but returns the result of the operation to the caller and thus
//  may be used inline (and indeed is intended to be used inline) in a logical statement.
#define FVerifyInline(expr)                 \
     ( static_cast <bool> (expr)					  \
      ? true							                  \
      : ( AssertVerify_LogMessage( ACTIONONVERIFY, false, "FVerifyInline", #expr, __FILE__, __LINE__, FUNCTION_PRETTY_NAME, 0 ), false ) )

// Verify() macros that will throw an exception even in retail.
#define VerifyThrow(expr)							\
     ( static_cast <bool> (expr)						\
      ? void (0)							            \
      : AssertVerify_LogMessage( eabiThrowException, false, "VerifyThrow", #expr, __FILE__, __LINE__, FUNCTION_PRETTY_NAME, 0 ) )
#define VerifyThrowSz(expr, sz, ...)							  \
     ( static_cast <bool> (expr)					  \
      ? void (0)							              \
      : AssertVerify_LogMessage( eabiThrowException, false, "VerifyThrow", #expr, __FILE__, __LINE__, FUNCTION_PRETTY_NAME, sz, ##__VA_ARGS__ ) )
// FVerifyInline(expr) expects expr to be true and will assert if not but returns the result of the operation to the caller and thus
//  may be used inline (and indeed is intended to be used inline) in a logical statement.
#define FVerifyThrowInline(expr)                 \
     ( static_cast <bool> (expr)					  \
      ? true							                  \
      : ( AssertVerify_LogMessage( eabiThrowException, false, "FVerifyThrowInline", #expr, __FILE__, __LINE__, FUNCTION_PRETTY_NAME, 0 ), false ) )

#if !ASSERTSENABLED
#define Assert(expr)		(static_cast<void>(0))
#define AssertSz(expr, sz...)		(static_cast<void>(0))
#else // #if !ASSERTSENABLED
#define Assert(expr)							          \
     ( static_cast <bool> (expr)						\
      ? void (0)							              \
      : AssertVerify_LogMessage( ACTIONONASSERT, true, "Assert", #expr, __FILE__, __LINE__, FUNCTION_PRETTY_NAME, 0 ) )
#define AssertSz(expr, sz, ...)							  \
     ( static_cast <bool> (expr)					  \
      ? void (0)							              \
      : AssertVerify_LogMessage( ACTIONONASSERT, true, "Assert", #expr, __FILE__, __LINE__, FUNCTION_PRETTY_NAME, sz, ##__VA_ARGS__ ) )
#endif // #if !ASSERTSENABLED

__BIENUTIL_END_NAMESPACE
