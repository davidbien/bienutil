#pragma once

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _trace.h
// Implement a "trace" mechanism to alert to code entry points.
// Traces in retail are off by default but can be turned on by setting TRACESENABLED to non-zero before including this file.

#include "_assert.h"

#ifdef Trace
#error _trace.h: Trace() macro is already defined.
#endif

#ifndef TRACESSENABLED
#ifdef	NDEBUG
#define TRACESSENABLED 0
#else
#define TRACESSENABLED 1
#endif // !NDEBUG
#endif // !TRACESSENABLED

#ifndef ACTIONONTRACE
// By default we just ignore a trace.
#define ACTIONONTRACE eabiIgnore
#endif // !ACTIONONTRACE

void 
Trace_LogMessage( EAbortBreakIgnore _eabi, const char * _szFile, unsigned int _nLine, const char * _szFunction, const n_SysLog::vtyJsoValueSysLog * _pjvTrace, const char * _szMesg, ... ) noexcept(true);
void 
Trace_LogMessageVArg( EAbortBreakIgnore _eabi, const char * _szFile, unsigned int _nLine, const char * _szFunction, const n_SysLog::vtyJsoValueSysLog * _pjvTrace, const char * _szMesg, va_list _ap ) noexcept(true);

#if !TRACESSENABLED
#define Trace(szMesg,...)		(static_cast<void>(0))
#define TraceAndIgnore(szMesg,...) (static_cast<void>(0))
#define TraceAndBreak(szMesg,...) (static_cast<void>(0))
#define TraceAndAbort(szMesg,...)		(static_cast<void>(0))
#else // #if !TRACESSENABLED
#define Trace(szMesg,...) Trace_LogMessage( ACTIONONTRACE, __FILE__, __LINE__, __PRETTY_FUNCTION__, nullptr, szMesg )
#define TraceJson(JSONVAL,szMesg,...) Trace_LogMessage( ACTIONONTRACE, __FILE__, __LINE__, __PRETTY_FUNCTION__, &JSONVAL, szMesg )
#define TraceAndIgnore(szMesg,...) Trace_LogMessage( eabiIgnore, __FILE__, __LINE__, __PRETTY_FUNCTION__, nullptr, szMesg )
#define TraceAndBreak(szMesg,...) Trace_LogMessage( eabiBreak, __FILE__, __LINE__, __PRETTY_FUNCTION__, nullptr, szMesg )
#define TraceAndAbort(szMesg,...) Trace_LogMessage( eabiAbort, __FILE__, __LINE__, __PRETTY_FUNCTION__, nullptr, szMesg )
#endif // #if !TRACESSENABLED
