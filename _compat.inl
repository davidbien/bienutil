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

inline int GetErrorString( vtyErrNo _errno, char * _rgchBuffer, size_t _stLen ) noexcept
{
#ifdef WIN32
    DWORD dwLen = ::FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, _errno, 0, _rgchBuffer, (DWORD)_stLen, NULL );
    Assert( dwLen < _stLen );
    _rgchBuffer[_stLen-1] = 0;
    return !dwLen ? -1 : 0;
#elif defined( __linux__ ) || defined( __APPLE__ )
    return ::strerror_r( _errno, _rgchBuffer, _stLen ) ? 0 : -1;
#endif
}

// Unmap the mapping given the handle and set the handle to a null handle.
inline int UnmapHandle( vtyMappedMemoryHandle const & _rhmm ) noexcept
{
    if ( !_rhmm.FFailedMapping() )
    {
#ifdef WIN32
      BOOL f = UnmapViewOfFile(_rhmm.Pv());
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
    return SetFilePointerEx( _hFile, *((LARGE_INTEGER*)&_off), (PLARGE_INTEGER)_poffResult, _whence ) ? 0 : -1;
#elif defined( __APPLE__ ) || defined( __linux__ )
    PrepareErrNo();
    vtySeekOffset off = lseek( _hFile, _off, _whence );
    if ( -1 != off )
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
NFileSeekAndThrow(vtyFileHandle _hFile, vtySeekOffset _off, vtySeekWhence _whence)
{
  vtySeekOffset offResult;
  int iSeek = FileSeek(_hFile, _off, _whence, &offResult);
  if (!!iSeek)
    THROWNAMEDEXCEPTIONERRNO(GetLastErrNo(), "FileSeek() failed, _hFile[0x%lx].", (uint64_t)_hFile);
  return offResult;
}

inline int FileRead( vtyFileHandle _hFile, void * _pvBuffer, size_t _stNBytesToRead, size_t * _pstNBytesRead ) noexcept
{
#ifdef WIN32
  if ( _stNBytesToRead > (std::numeric_limits<DWORD>::max)() )
  {
    SetLastErrNo( vkerrOverflow );
    return -1;
  }
  DWORD nBytesRead;
  if ( !::ReadFile( _hFile, _pvBuffer, (DWORD)_stNBytesToRead, &nBytesRead, NULL ) )
      return -1;
  if ( _pstNBytesRead )
      *_pstNBytesRead = nBytesRead;
  return 0;
#elif defined( __APPLE__ ) || defined( __linux__ )
  PrepareErrNo();
  ssize_t sstRead = ::read( _hFile, _pvBuffer, _stNBytesToRead );
  if ( -1 == sstRead )
      return -1;
  if ( _pstNBytesRead )
      *_pstNBytesRead = sstRead;
  return 0;
#endif
}

inline int FileWrite( vtyFileHandle _hFile, const void * _pvBuffer, size_t _stNBytesToWrite, size_t * _pstNBytesWritten ) noexcept
{
#ifdef WIN32  
  if ( _stNBytesToWrite > (std::numeric_limits<DWORD>::max)() )
  {
    SetLastErrNo( vkerrOverflow );
    return -1;
  }
  DWORD nBytesWritten;
  if ( !WriteFile( _hFile, _pvBuffer, (DWORD)_stNBytesToWrite, &nBytesWritten, NULL ) )
      return -1;
  if ( _pstNBytesWritten )
      *_pstNBytesWritten = nBytesWritten;
  return 0;
#elif defined( __APPLE__ ) || defined( __linux__ )
  PrepareErrNo();
  ssize_t sstWritten = ::write( _hFile, _pvBuffer, _stNBytesToWrite );
  if ( -1 == sstWritten )
      return -1;
  if ( _pstNBytesWritten )
      *_pstNBytesWritten = sstWritten;
  return 0;
#endif
}
inline void FileWriteOrThrow( vtyFileHandle _hFile, const void * _pvBuffer, size_t _stNBytesToWrite )
{
  size_t nbyWritten;
  int iResult = FileWrite( _hFile,  _pvBuffer, _stNBytesToWrite, &nbyWritten );
  if ( !!iResult )
    THROWNAMEDEXCEPTIONERRNO(GetLastErrNo(), "FileWrite() failed." );
  Assert( nbyWritten == _stNBytesToWrite );
  VerifyThrowSz( nbyWritten == _stNBytesToWrite, "Only wrote [%lu] bytes of [%lu].", nbyWritten, _stNBytesToWrite );
}


// Time methods:
inline int LocalTimeFromTime(const time_t* _ptt, struct tm* _ptmDest) noexcept
{
#ifdef WIN32
  errno_t e = localtime_s(_ptmDest, _ptt);
  return !e ? 0 : -1;
#elif defined( __APPLE__ ) || defined( __linux__ )
  struct tm* ptm = localtime_r(_ptt, _ptmDest);
  return !ptm ? -1 : 0;
#endif
}

inline void UUIDCreate(vtyUUID& _ruuid) noexcept
{
#ifdef WIN32
  (void)UuidCreate(&_ruuid); // Not much to do on failure...
#elif defined( __APPLE__ ) || defined( __linux__ )
  uuid_generate(_ruuid);
#endif
}

inline int UUIDToString(const vtyUUID& _ruuid, char* _rgcBuffer, const size_t _knBuf) noexcept
{
  Assert(_knBuf >= vkstUUIDNCharsWithNull);
#ifdef WIN32
  if (_knBuf < vkstUUIDNCharsWithNull)
  {
    ::SetLastError(ERROR_BAD_ARGUMENTS);
    return -1;
  }
  RPC_CSTR cstrResult;
  RPC_STATUS s = UuidToStringA(&_ruuid, &cstrResult);
  if ((RPC_S_OK == s) && !!cstrResult)
  {
    strncpy_s(_rgcBuffer, _knBuf, (char*)cstrResult, _TRUNCATE);
    RpcStringFreeA(&cstrResult);
    return 0;
  }
  ::SetLastError(s);
  return -1;
#elif defined( __APPLE__ ) || defined( __linux__ )
  if (_knBuf < vkstUUIDNCharsWithNull)
  {
    errno = EINVAL;
    return -1;
  }
  uuid_string_t ustOut;
  uuid_unparse_lower(_ruuid, ustOut);
  memcpy(_rgcBuffer, ustOut, (std::min)(sizeof(ustOut), _knBuf));
  _rgcBuffer[_knBuf - 1] = 0;
  return 0;
#endif
}

inline int UUIDFromString(const char* _rgcBuffer, vtyUUID& _ruuid) noexcept
{
  size_t nLenBuf = StrNLen(_rgcBuffer, vkstUUIDNChars);
  Assert(nLenBuf == vkstUUIDNChars);
#ifdef WIN32
  if (nLenBuf < vkstUUIDNChars)
  {
    ::SetLastError(ERROR_BAD_ARGUMENTS);
    return -1;
  }
  // Copy to string buf before sending in for safety - ensures null termination, etc.
  vtyUUIDString uusBuf;
  memcpy(uusBuf, _rgcBuffer, vkstUUIDNChars);
  uusBuf[vkstUUIDNChars] = 0;
  RPC_STATUS s = ::UuidFromStringA((RPC_CSTR)uusBuf, &_ruuid);
  if (RPC_S_OK != s)
  {
    ::SetLastError(s);
    return -1;
  }
  return 0;
#elif defined( __APPLE__ ) || defined( __linux__ )
  if (nLenBuf < vkstUUIDNChars)
  {
    errno = EINVAL;
    return -1;
  }
  // Copy to string buf before sending in for safety - ensures null termination, etc.
  vtyUUIDString uusBuf;
  memcpy(uusBuf, _rgcBuffer, vkstUUIDNChars);
  uusBuf[vkstUUIDNChars] = 0;
  int iParseUuid = ::uuid_parse(uusBuf, _ruuid);
  if (!!iParseUuid)
  {
    errno = EINVAL;
    return -1;
  }
  return 0;
#endif
}

__BIENUTIL_END_NAMESPACE
