#pragma once

//          Copyright David Lawrence Bien 1997 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _compat.inl
// Inlines for multiplatform compatibility.
// dbien
// Want to include the _compat.h at the top with the only dependencies being OS files and I have some other dependencies that need to go here.

#include "bienutil.h"
#include "_namdexc.h"
#include "_compat.h"

__BIENUTIL_BEGIN_NAMESPACE

inline int
GetErrorString( vtyErrNo _errno, char * _rgchBuffer, size_t _stLen ) noexcept
{
#ifdef WIN32
  DWORD dwLen = ::FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, _errno, 0, _rgchBuffer, (DWORD)_stLen, NULL );
  Assert( dwLen < _stLen );
  _rgchBuffer[ _stLen - 1 ] = 0;
  return !dwLen ? -1 : 0;
#elif defined( __linux__ ) || defined( __APPLE__ )
  return ::strerror_r( _errno, _rgchBuffer, _stLen );
#endif
}

// Unmap the mapping given the handle and set the handle to a null handle.
inline int
UnmapHandle( vtyMappedMemoryHandle const & _rhmm ) noexcept
{
  if ( !_rhmm.FFailedMapping() )
  {
#ifdef WIN32
    BOOL f = UnmapViewOfFile( _rhmm.Pv() );
    Assert( f );
    return f ? 0 : -1;
#elif defined( __APPLE__ ) || defined( __linux__ )
    int iUnmap = ::munmap( _rhmm.Pv(), _rhmm.length() );
    Assert( !iUnmap );
    return iUnmap;
#endif
  }
  return 0;
}

inline int
FileSeek( vtyFileHandle _hFile, vtySeekOffset _off, vtySeekWhence _whence, vtySeekOffset * _poffResult ) noexcept
{
#ifdef WIN32
  return SetFilePointerEx( _hFile, *( (LARGE_INTEGER *)&_off ), (PLARGE_INTEGER)_poffResult, _whence ) ? 0 : -1;
#elif defined( __APPLE__ ) || defined( __linux__ )
  PrepareErrNo();
#if INTPTR_MAX == INT32_MAX
  vtySeekOffset off = lseek64( _hFile, _off, _whence );
#else
  vtySeekOffset off = lseek( _hFile, _off, _whence );
#endif
  if ( -1ll != off )
  {
    Assert( off >= 0 );
    if ( _poffResult )
      *_poffResult = off;
    return 0;
  }
  return -1;
#endif
}

// Seek and return the offset, throw on seek failure.
inline vtySeekOffset
NFileSeekAndThrow( vtyFileHandle _hFile, vtySeekOffset _off, vtySeekWhence _whence )
{
  vtySeekOffset offResult;
  int iSeek = FileSeek( _hFile, _off, _whence, &offResult );
  if ( !!iSeek )
    THROWNAMEDEXCEPTIONERRNO( GetLastErrNo(), "FileSeek() failed, _hFile[0x%zx].", (size_t)_hFile );
  return offResult;
}

inline int
FileRead( vtyFileHandle _hFile, void * _pvBuffer, uint64_t _u64NBytesToRead, uint64_t * _pu64NBytesRead ) noexcept
{
  // First check for overflow of read on 32bit platforms:
#ifdef WIN32
// We need only loop on 64bit platforms:
#if INTPTR_MAX == INT32_MAX
  if ( _u64NBytesToRead >= ( std::numeric_limits< DWORD >::max )() )
  {
    SetLastErrNo( vkerrOverflow );
    return -1;
  }
  DWORD nBytesRead;
  if ( !::ReadFile( _hFile, _pvBuffer, (DWORD)_u64NBytesToRead, &nBytesRead, NULL ) )
    return -1;
  if ( _pu64NBytesRead )
    *_pu64NBytesRead = nBytesRead;
  return 0;
#else  // INTPTR_MAX == INT32_MAX
  static_assert( sizeof( size_t ) == 8 );
  uint64_t u64BytesRead = 0;
  const DWORD kdwMaxPerRead = ( std::numeric_limits< DWORD >::max )(); // as we understand - couldn't find otherwise.
  for ( ; _u64NBytesToRead > 0; )
  {
    DWORD dwBytesToRead = ( _u64NBytesToRead > kdwMaxPerRead ) ? kdwMaxPerRead : (DWORD)_u64NBytesToRead;
    DWORD nBytesRead;
    if ( !::ReadFile( _hFile, _pvBuffer, dwBytesToRead, &nBytesRead, NULL ) )
      return -1;
    u64BytesRead += nBytesRead;
    if ( nBytesRead != dwBytesToRead ) // we hit EOF.
      break;                           // can't read anymore - up to caller if wants to treat as error.
    _u64NBytesToRead -= nBytesRead;
    (uint8_t *&)_pvBuffer += nBytesRead;
  }
  if ( _pu64NBytesRead )
    *_pu64NBytesRead = u64BytesRead;
  return 0;
#endif // INTPTR_MAX != INT32_MAX
#elif defined( __APPLE__ ) || defined( __linux__ )
  PrepareErrNo();
  // According to the online documentation "On Linux, read() (and similar system calls) will transfer at most
  //  0x7ffff000 (2,147,479,552) bytes, returning the number of bytes actually transferred.  (This is true on
  //  both 32-bit and 64-bit systems.)"
  const uint64_t ku64MaxPerRead = 0x7ffff000ull;
// We need only loop on 64bit platforms:
#if INTPTR_MAX == INT32_MAX
  if ( _u64NBytesToRead > ku64MaxPerRead )
  {
    SetLastErrNo( vkerrOverflow );
    return -1;
  }
  ssize_t sstRead = ::read( _hFile, _pvBuffer, (size_t)_u64NBytesToRead );
  if ( -1 == sstRead )
    return -1;
  if ( _pu64NBytesRead )
    *_pu64NBytesRead = (size_t)sstRead;
  return 0;
#else  // INTPTR_MAX == INT32_MAX
  static_assert( sizeof( size_t ) == 8 );
  uint64_t u64BytesRead = 0;
  for ( ; _u64NBytesToRead > 0; )
  {
    uint64_t u64BytesToRead = ( _u64NBytesToRead > ku64MaxPerRead ) ? ku64MaxPerRead : _u64NBytesToRead;
    ssize_t sstRead = ::read( _hFile, _pvBuffer, u64BytesToRead );
    if ( -1ll == sstRead )
      return -1;
    u64BytesRead += (size_t)sstRead;
    if ( (size_t)sstRead != u64BytesToRead ) // we hit EOF.
      break;                                 // can't read anymore - up to caller if wants to treat as error.
    _u64NBytesToRead -= u64BytesToRead;
    (uint8_t *&)_pvBuffer += u64BytesToRead;
  }
  if ( _pu64NBytesRead )
    *_pu64NBytesRead = u64BytesRead;
  return 0;
#endif // INTPTR_MAX != INT32_MAX
#endif
}

inline int
FileWrite( vtyFileHandle _hFile, const void * _pvBuffer, uint64_t _u64NBytesToWrite, uint64_t * _pu64NBytesWritten ) noexcept
{
#ifdef WIN32
// We need only loop on 64bit platforms:
#if INTPTR_MAX == INT32_MAX
  if ( _u64NBytesToWrite > ( std::numeric_limits< DWORD >::max )() )
  {
    SetLastErrNo( vkerrOverflow );
    return -1;
  }
  DWORD nBytesWritten;
  if ( !WriteFile( _hFile, _pvBuffer, (DWORD)_u64NBytesToWrite, &nBytesWritten, NULL ) )
    return -1;
  if ( _pu64NBytesWritten )
    *_pu64NBytesWritten = nBytesWritten;
  return 0;
#else  // INTPTR_MAX == INT32_MAX
  static_assert( sizeof( size_t ) == 8 );
  uint64_t u64BytesWritten = 0;
  const DWORD kdwMaxPerWrite = ( std::numeric_limits< DWORD >::max )(); // as we understand - couldn't find otherwise.
  for ( ; _u64NBytesToWrite > 0; )
  {
    DWORD dwBytesToWrite = ( _u64NBytesToWrite > kdwMaxPerWrite ) ? kdwMaxPerWrite : (DWORD)_u64NBytesToWrite;
    DWORD nBytesWritten;
    if ( !::WriteFile( _hFile, _pvBuffer, dwBytesToWrite, &nBytesWritten, NULL ) )
      return -1;
    u64BytesWritten += nBytesWritten;
    if ( nBytesWritten != dwBytesToWrite ) // we may not have transferred all bytes in some cases - caller needs to check for this on success.
      break;                               // can't write anymore - up to caller if wants to treat as error.
    _u64NBytesToWrite -= nBytesWritten;
    (uint8_t *&)_pvBuffer += nBytesWritten;
  }
  if ( _pu64NBytesWritten )
    *_pu64NBytesWritten = u64BytesWritten;
  return 0;
#endif // INTPTR_MAX != INT32_MAX
#elif defined( __APPLE__ ) || defined( __linux__ )
  PrepareErrNo();
  // According to the online documentation "On Linux, write() (and similar system calls) will transfer at most
  //  0x7ffff000 (2,147,479,552) bytes, returning the number of bytes actually transferred.  (This is true on
  //  both 32-bit and 64-bit systems.)"
  const uint64_t ku64MaxPerWrite = 0x7ffff000ull;
// We need only loop on 64bit platforms:
#if INTPTR_MAX == INT32_MAX
  if ( _u64NBytesToWrite > ku64MaxPerWrite )
  {
    SetLastErrNo( vkerrOverflow );
    return -1;
  }
  ssize_t sstWritten = ::write( _hFile, _pvBuffer, _u64NBytesToWrite );
  if ( -1 == sstWritten )
    return -1;
  if ( _pu64NBytesWritten )
    *_pu64NBytesWritten = (size_t)sstWritten;
  return 0;
#else  // INTPTR_MAX == INT32_MAX
  static_assert( sizeof( size_t ) == 8 );
  uint64_t u64BytesWritten = 0;
  for ( ; _u64NBytesToWrite > 0; )
  {
    uint64_t u64BytesToWrite = ( _u64NBytesToWrite > ku64MaxPerWrite ) ? ku64MaxPerWrite : _u64NBytesToWrite;
    ssize_t sstWrite = ::write( _hFile, _pvBuffer, u64BytesToWrite );
    if ( -1ll == sstWrite )
      return -1;
    u64BytesWritten += (size_t)sstWrite;
    if ( (size_t)sstWrite != u64BytesToWrite ) // we hit EOF.
      break;                                   // can't write anymore - up to caller if wants to treat as error.
    _u64NBytesToWrite -= u64BytesToWrite;
    (uint8_t *&)_pvBuffer += u64BytesToWrite;
  }
  if ( _pu64NBytesWritten )
    *_pu64NBytesWritten = u64BytesWritten;
  return 0;
#endif // INTPTR_MAX != INT32_MAX
#endif
}
inline void
FileWriteOrThrow( vtyFileHandle _hFile, const void * _pvBuffer, uint64_t _u64NBytesToWrite )
{
  uint64_t nbyWritten;
  int iResult = FileWrite( _hFile, _pvBuffer, _u64NBytesToWrite, &nbyWritten );
  if ( !!iResult )
    THROWNAMEDEXCEPTIONERRNO( GetLastErrNo(), "FileWrite() failed." );
  Assert( nbyWritten == _u64NBytesToWrite );
  VerifyThrowSz( nbyWritten == _u64NBytesToWrite, "Only wrote [%llu] bytes of [%llu].", nbyWritten, _u64NBytesToWrite );
}

// A file copy utility that uses the compatibility layer.
inline int
FileCopy( const char * _pszDest, const char * _pszSrc, bool _fDeleteOnFail, uint64_t _u64BlockSize )
{
  vtyFileHandle hSrc = OpenReadOnlyFile( _pszSrc );
  if ( vkhInvalidFileHandle == hSrc )
  {
    LOGSYSLOG( eslmtError, "Failed to open source file '%s' for copy", _pszSrc );
    return -1;
  }

  vtyFileHandle hDest = CreateWriteOnlyFile( _pszDest );
  if ( vkhInvalidFileHandle == hDest )
  {
    LOGSYSLOG( eslmtError, "Failed to create destination file '%s' for copy", _pszDest );
    FileClose( hSrc );
    return -2;
  }

  vector< uint8_t > vBuffer( _u64BlockSize );
  try
  {
    uint64_t u64BytesRead;
    while ( 0 == FileRead( hSrc, vBuffer.data(), _u64BlockSize, &u64BytesRead ) && u64BytesRead > 0 )
    {
      FileWriteOrThrow( hDest, vBuffer.data(), u64BytesRead );
    }
  }
  catch ( ... )
  {
    FileClose( hSrc );
    FileClose( hDest );
    if ( _fDeleteOnFail )
      (void)FileDelete( _pszDest );
    LOGSYSLOG( eslmtError, "FileCopy(): Exception copying from '%s' to '%s'", _pszSrc, _pszDest );
    throw;
  }

  FileClose( hSrc );
  FileClose( hDest );
  return 0;
}

inline int
FileCopyNoThrow( const char * _pszDest, const char * _pszSrc, bool _fDeleteOnFail, uint64_t _u64BlockSize ) noexcept
{
  try
  {
    return FileCopy(_pszDest, _pszSrc, _fDeleteOnFail, _u64BlockSize);
  }
  catch ( const std::exception & e )
  {
    LOGSYSLOG( eslmtError, "FileCopyNoThrow(): std::exception copying from '%s' to '%s': %s", _pszSrc, _pszDest, e.what() );
    return -3;
  }
  catch ( ... )
  {
    // LOGSYSLOG( eslmtError, "FileCopyNoThrow(): Unknown exception copying from '%s' to '%s'", _pszSrc, _pszDest ); - already logged the same thing above.
    return -4;
  }
}

// Time methods:
inline int
LocalTimeFromTime( const time_t * _ptt, struct tm * _ptmDest ) noexcept
{
#ifdef WIN32
  errno_t e = localtime_s( _ptmDest, _ptt );
  return !e ? 0 : -1;
#elif defined( __APPLE__ ) || defined( __linux__ )
  struct tm * ptm = localtime_r( _ptt, _ptmDest );
  return !ptm ? -1 : 0;
#endif
}

inline void
UUIDCreate( vtyUUID & _ruuid ) noexcept
{
#ifdef WIN32
  (void)UuidCreate( &_ruuid ); // Not much to do on failure...
#elif defined( __ANDROID__ )
  memset( &_ruuid, 0, sizeof( _ruuid ) ); // We need to get this working for Android eventually.
#elif defined( __APPLE__ ) || defined( __linux__ )
  uuid_generate( _ruuid );
#endif
}

inline int
UUIDToString( const vtyUUID & _ruuid, char * _rgcBuffer, const size_t _knBuf ) noexcept
{
  Assert( _knBuf >= vkstUUIDNCharsWithNull );
#ifdef WIN32
  if ( _knBuf < vkstUUIDNCharsWithNull )
  {
    ::SetLastError( ERROR_BAD_ARGUMENTS );
    return -1;
  }
  RPC_CSTR cstrResult;
  RPC_STATUS s = UuidToStringA( &_ruuid, &cstrResult );
  if ( ( RPC_S_OK == s ) && !!cstrResult )
  {
    strncpy_s( _rgcBuffer, _knBuf, (char *)cstrResult, _TRUNCATE );
    RpcStringFreeA( &cstrResult );
    return 0;
  }
  ::SetLastError( s );
  return -1;
#elif defined( __ANDROID__ )
  const char rgcNullGuid[] = "00000000-0000-0000-0000-000000000000";
  memcpy( _rgcBuffer, rgcNullGuid, ( std::min )( sizeof( rgcNullGuid ), _knBuf ) );
  _rgcBuffer[ _knBuf - 1 ] = 0;
  return 0;
#elif defined( __APPLE__ ) || defined( __linux__ )
  if ( _knBuf < vkstUUIDNCharsWithNull )
  {
    errno = EINVAL;
    return -1;
  }
  uuid_string_t ustOut;
  uuid_unparse_lower( _ruuid, ustOut );
  memcpy( _rgcBuffer, ustOut, ( std::min )( sizeof( ustOut ), _knBuf ) );
  _rgcBuffer[ _knBuf - 1 ] = 0;
  return 0;
#endif
}

inline int
UUIDFromString( const char * _rgcBuffer, vtyUUID & _ruuid ) noexcept
{
  size_t nLenBuf = StrNLen( _rgcBuffer, vkstUUIDNChars );
  Assert( nLenBuf == vkstUUIDNChars );
#ifdef WIN32
  if ( nLenBuf < vkstUUIDNChars )
  {
    ::SetLastError( ERROR_BAD_ARGUMENTS );
    return -1;
  }
  // Copy to string buf before sending in for safety - ensures null termination, etc.
  vtyUUIDString uusBuf;
  memcpy( uusBuf, _rgcBuffer, vkstUUIDNChars );
  uusBuf[ vkstUUIDNChars ] = 0;
  RPC_STATUS s = ::UuidFromStringA( (RPC_CSTR)uusBuf, &_ruuid );
  if ( RPC_S_OK != s )
  {
    ::SetLastError( s );
    return -1;
  }
  return 0;
#elif defined( __ANDROID__ )
  // TODO: Get a uuid.lib for ANDROID.
  memset( &_ruuid, 0, sizeof _ruuid );
  return 0;
#elif defined( __APPLE__ ) || defined( __linux__ )
  if ( nLenBuf < vkstUUIDNChars )
  {
    errno = EINVAL;
    return -1;
  }
  // Copy to string buf before sending in for safety - ensures null termination, etc.
  vtyUUIDString uusBuf;
  memcpy( uusBuf, _rgcBuffer, vkstUUIDNChars );
  uusBuf[ vkstUUIDNChars ] = 0;
  int iParseUuid = ::uuid_parse( uusBuf, _ruuid );
  if ( !!iParseUuid )
  {
    errno = EINVAL;
    return -1;
  }
  return 0;
#endif
}

__BIENUTIL_END_NAMESPACE
