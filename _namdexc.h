/*
 * Copyright (c) 1997
 * Silicon Graphics Computer Systems, Inc.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Silicon Graphics makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 */

// dbien: This modified from SGI STL's v3.22 "stdexcept" header.

#ifndef ___NAMDEXC_H___
#define ___NAMDEXC_H___

#include <stdarg.h>
#include <string>
#include "bienutil.h"

namespace std // REVIEW:<dbien>: Not sure why I extended std here - should rid that.
{

#define NAMEDEXC_BUFSIZE 4096

// My version of SGI STL's named exception that accepts an allocator
//  ( I just like to see a no leaks after the debug run :-)
// Templatize by base to reuse the code and to allow expected throw types to be returned.
template < class t_TyAllocator = allocator< char >, class t_TyBaseClass = exception >
class _t__Named_exception : public t_TyBaseClass 
{
  typedef t_TyBaseClass _TyBase;
public:
  typedef basic_string< char, char_traits<char>, t_TyAllocator > string_type;
  typedef t_TyBaseClass _tyExceptionBase;

  _t__Named_exception()
  {
    strncpy( m_rgcExceptionName, "_t__Named_exception", s_stBufSize );
    m_rgcExceptionName[s_stBufSize - 1] = '\0';
  }
  _t__Named_exception(const char * _pc);
  {
    if ( _pc )
    {
      strncpy(m_rgcExceptionName, _pc, s_stBufSize);
      m_rgcExceptionName[s_stBufSize - 1] = '\0';
    }
  }
  _t__Named_exception(const string_type& __str) 
  {
    strncpy(m_rgcExceptionName, __str.c_str(), s_stBufSize);
    m_rgcExceptionName[s_stBufSize - 1] = '\0';
  }
  _t__Named_exception( const char * _pcFmt, va_list args ) 
  {
      RenderVA( _pcFmt, args );
  }
  virtual const char* what() const _BIEN_NOTHROW { return m_rgcExceptionName; }

  template < class t__TyTraits, class t__TyAllocator >
  void SetWhat(basic_string<char, t__TyTraits, t__TyAllocator> const & _str)
  {
    SetWhat(_str.c_str(), _str.length());
  }

  void SetWhat(const char * _pszWhat, size_t _stLen)
  {
    size_t stMinLen = (std::min)((size_t)s_stBufSize,_stLen);
    strncpy(m_rgcExceptionName, _pszWhat, stMinLen);
    m_rgcExceptionName[stMinLen - 1] = '\0';
  }

  void RenderVA( const char * _pcFmt, va_list args )
  {
    m_rgcExceptionName[0] = 0;
    (void)vsnprintf( m_rgcExceptionName, s_stBufSize, _pcFmt, args );
    m_rgcExceptionName[s_stBufSize - 1] = 0;
  }
protected:
  enum : size_t // clang didn't like the static const size_t declaration somehow - this works but also required a cast above.
  {
    s_stBufSize = NAMEDEXC_BUFSIZE
  };
  char m_rgcExceptionName[s_stBufSize];
};

#define THROWNAMEDEXCEPTION( MESG... ) ExceptionUsage< std::_t__Named_exception<> >::ThrowFileLineFunc( __FILE__, __LINE__, __PRETTY_FUNCTION__, MESG )

template < class t_TyAllocator = allocator< char >, class t_TyBaseClass = exception >
class _t__Named_exception_errno : public _t__Named_exception< t_TyAllocator, t_TyBaseClass >
{
  typedef _t__Named_exception_errno _TyThis;
  typedef _t__Named_exception< t_TyAllocator, t_TyBaseClass > _TyBase;
public:
  typedef basic_string< char, char_traits<char>, t_TyAllocator > string_type;
  typedef _t__Named_exception< t_TyAllocator, t_TyBaseClass > _tyExceptionBase;

  _t__Named_exception_errno( int _errno = 0 )
    : _TyBase(),
      m_errno( _errno )
  {
  }
  _t__Named_exception_errno( const string_type& __str, int _errno = 0 ) 
    : _TyBase( __str ),
      m_errno( _errno )
  {
  }
  _t__Named_exception_errno( int _errno, const char * _pcFmt, va_list args )
    : m_errno( _errno )
  {
      RenderVA( _pcFmt, args );
  }
  _t__Named_exception_errno( const char * _pcFmt, va_list args ) 
  {
      RenderVA( _pcFmt, args );
  }

  using _TyBase::what;
  using _TyBase::SetWhat;
  void RenderVA( const char * _pcFmt, va_list args )
  {
    _TyBase::RenderVA( _pcFmt, args );
    if ( m_errno )
    {
      // Add the errno description onto the end of the error string.
      const int knErrorMesg = 256;
      char rgcErrorMesg[ knErrorMesg ];
      char rgcErrorMesg2[ knErrorMesg ];
      if ( !!strerror_r( m_errno, rgcErrorMesg2, knErrorMesg ) )
        (void)snprintf( rgcErrorMesg, knErrorMesg, "errno:[%d]", m_errno );
      else
      {
        rgcErrorMesg2[knErrorMesg-1] = 0;
#ifdef GCC_COMPILER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation="
#endif //GCC_COMPILER
        (void)snprintf( rgcErrorMesg, knErrorMesg, "errno:[%d]: %s", m_errno, rgcErrorMesg2 );
#ifdef GCC_COMPILER
#pragma GCC diagnostic pop
#endif //GCC_COMPILER
      }
      rgcErrorMesg[knErrorMesg-1] = 0;

      const int knBuf = NAMEDEXC_BUFSIZE;
      char rgcBuf[knBuf];
#ifdef GCC_COMPILER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation="
#endif //GCC_COMPILER
      (void)snprintf( rgcBuf, knBuf, "%s, %s", m_rgcExceptionName, rgcErrorMesg );
#ifdef GCC_COMPILER
#pragma GCC diagnostic pop
#endif //GCC_COMPILER
      rgcBuf[knBuf-1] = 0;
      memcpy( m_rgcExceptionName, rgcBuf, NAMEDEXC_BUFSIZE ); // Update the description of the exception with the description of the errno.
    }
  }

  int Errno() const { return m_errno; }
  void SetErrno( int _errno )
  {
    m_errno = _errno;
  }
protected:
  using _TyBase::m_rgcExceptionName;
  int m_errno{0};
};

#define THROWNAMEDEXCEPTIONERRNO( _errno, MESG... ) ExceptionUsage< std::_t__Named_exception_errno<> >::ThrowFileLineFuncErrno( __FILE__, __LINE__, __PRETTY_FUNCTION__, _errno, MESG )

// Override the bad_variant_access exception to return a bit more info about what went wrong. Put it here for general usage.
template < class t_TyAllocator = allocator< char > >
class named_bad_variant_access : public std::_t__Named_exception< t_TyAllocator, bad_variant_access >
{
  typedef VerifyFailedException _TyThis;
  typedef std::_t__Named_exception<> _TyBase;
public:
  using _TyBase::string_type;
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
#define THROWNAMEDBADVARIANTACCESSEXCEPTION( MESG... ) ExceptionUsage<named_bad_variant_access>::ThrowFileLineFunc( __FILE__, __LINE__, __PRETTY_FUNCTION__, MESG )

} // namespace std

// ExceptionUsage: Provide some useful template methods for throw exception with variable number of arguments.
template < class t_tyException >
struct ExceptionUsage
{
  typedef t_tyException _TyException;

  static void ThrowFileLine( const char * _pcFile, int _nLine, const char * _pcFmt, ... )
  {
    // We add _psFile:[_nLine]: to the start of the format string.
    const int knBuf = NAMEDEXC_BUFSIZE;
    char rgcBuf[knBuf+1];
    (void)snprintf( rgcBuf, knBuf, "[%s:%d]: %s", _pcFile, _nLine, _pcFmt );
    rgcBuf[knBuf] = 0;

    va_list ap;
    va_start( ap, _pcFmt );
    _TyException exc( rgcBuf, ap ); // Don't throw in between va_start and va_end.
    va_end( ap );
    throw std::move( exc );
  }
  static void ThrowFileLineFunc( const char * _pcFile, int _nLine, const char * _pcFunc, const char * _pcFmt, ... )
  {
    // We add _psFile:[_nLine]: to the start of the format string.
    const int knBuf = NAMEDEXC_BUFSIZE;
    char rgcBuf[knBuf+1];
    (void)snprintf( rgcBuf, knBuf, "[%s:%d],%s: %s", _pcFile, _nLine, _pcFunc, _pcFmt );
    rgcBuf[knBuf] = 0;

    va_list ap;
    va_start( ap, _pcFmt );
    _TyException exc( rgcBuf, ap ); // Don't throw in between va_start and va_end.
    va_end( ap );
    throw std::move( exc );
  }
  static void ThrowFileLineErrno( const char * _pcFile, int _nLine, int _errno, const char * _pcFmt, ... )
  {
    // We add _psFile:[_nLine]: to the start of the format string.
    const int knBuf = NAMEDEXC_BUFSIZE;
    char rgcBuf[knBuf+1];
    (void)snprintf( rgcBuf, knBuf, "[%s:%d]: %s", _pcFile, _nLine, _pcFmt );
    rgcBuf[knBuf] = 0;

    va_list ap;
    va_start( ap, _pcFmt );
    _TyException exc( _errno, rgcBuf, ap ); // Don't throw in between va_start and va_end.
    va_end( ap );
    throw std::move( exc );
  }
  static void ThrowFileLineFuncErrno( const char * _pcFile, int _nLine, const char * _pcFunc, int _errno, const char * _pcFmt, ... )
  {
    // We add _psFile:[_nLine]: to the start of the format string.
    const int knBuf = NAMEDEXC_BUFSIZE;
    char rgcBuf[knBuf+1];
    (void)snprintf( rgcBuf, knBuf, "[%s:%d],%s: %s", _pcFile, _nLine,  _pcFunc, _pcFmt );
    rgcBuf[knBuf] = 0;

    va_list ap;
    va_start( ap, _pcFmt );
    _TyException exc( _errno, rgcBuf, ap ); // Don't throw in between va_start and va_end.
    va_end( ap );
    throw std::move( exc );
  }
  static void Throw( const char * _pcFmt, ... )
  {
    va_list ap;
    va_start( ap, _pcFmt );
    _TyException exc( _pcFmt, ap ); // Don't throw in between va_start and va_end.
    va_end( ap );
    throw std::move( exc );
  }
  static void ThrowErrno( int _errno, const char * _pcFmt, ... )
  {
    va_list ap;
    va_start( ap, _pcFmt );
    _TyException exc( _errno, _pcFmt, ap ); // Don't throw in between va_start and va_end.
    va_end( ap );
    throw std::move( exc );
  }
};

#endif //___NAMDEXC_H___
