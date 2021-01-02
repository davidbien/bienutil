#pragma once

// _compat.inl
// Inlines for multiplatform compatibility.
// dbien
// Want to include the _compat.h at the top with the only dependencies being OS files and I have some other dependencies that need to go here.

#include "bienutil.h"
#include "_namdexc.h"

__BIENUTIL_BEGIN_NAMESPACE

inline int GetErrorString( vtyErrNo _errno, char * _rgchBuffer, size_t _stLen )
{
#ifdef WIN32
    DWORD dwLen = ::FormatMessageA( FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, _errno, 0, _rgchBuffer, (DWORD)_stLen, NULL );
    Assert( dwLen < _stLen );
    _rgchBuffer[_stLen-1] = 0;
    return !dwLen ? -1 : 0;
#elif defined( __linux__ ) || defined( __APPLE__ )
    return strerror_r( _errno, _pszString, _stLen );
#endif
}

// Unmap the mapping given the handle and set the handle to a null handle.
inline int UnmapHandle( vtyMappedMemoryHandle const & _rhmm ) noexcept(true)
{
    if ( !!_rhmm.FFailedMapping() )
    {
#ifdef WIN32
        BOOL f = UnmapViewOfFile( hmm.Pv() );
        Assert( f );
        return f ? 0 : -1;
#elif defined( __APPLE__ ) || defined( __linux__ )
        int iUnmap = ::munmap( hmm.Pv(), hmm.length() );
        Assert( !iUnmap );
        return iUnmap;
#endif
    }
}

inline int
FileSeek( vtyFileHandle _hFile, vtySeekOffset _off, vtySeekWhence _whence, vtySeekOffset * _poffResult = 0 )
{
#ifdef WIN32
    return SetFilePointerEx( _hFile, *((LARGE_INTEGER*)&_off), _poffResult, _whence ) ? 0 : -1;
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

inline int FileRead( vtyFileHandle _hFile, void * _pvBuffer, size_t _stNBytesToRead, size_t * _pstNBytesRead = 0 )
{
#ifdef WIN32
    VerifyThrowSz( _stNBytesToRead <= (std::numeric_limits<DWORD>::max)(), "Windows is limited to 4GB reads. This read was for [%lu] bytes.", _stNBytesToRead );
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

inline int FileWrite( vtyFileHandle _hFile, const void * _pvBuffer, size_t _stNBytesToWrite, size_t * _pstNBytesWritten = 0 )
{
#ifdef WIN32
    VerifyThrowSz( _stNBytesToWrite <= (std::numeric_limits<DWORD>::max)(), "Windows is limited to 4GB writes. This write was for [%lu] bytes.", _stNBytesToWrite );
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

__BIENUTIL_END_NAMESPACE
