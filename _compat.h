#pragma once
// _compat.h
// Multiplatform compatibility stuff.
// dbien : 30DEC2020

#ifdef WIN32
#include <windows.h>
#endif

#if !defined(WIN32) && !defined(__APPLE__) && !defined(__linux__)
#error We don't have any compatibility platforms we are familiar with.
#endif

#include <utility>
#include <filesystem>
#ifndef WIN32
#include <errno.h>
#endif //!WIN32
#include "bienutil.h"

__BIENUTIL_BEGIN_NAMESPACE

#ifdef WIN32
typedef DWORD vtyErrNo;
#elif defined( __linux__ ) || defined( __APPLE__ )
typedef int vtyErrNo;
#endif
inline void PrepareErrNo()
{
#if defined( __linux__ ) || defined( __APPLE__ )
    errno = 0;
#endif // Nothing to do under Windows.
}
inline vtyErrNo GetLastErrNo()
{
#ifdef WIN32
    return ::GetLastError();
#elif defined( __linux__ ) || defined( __APPLE__ )
    return errno;
#endif
}
inline void SetLastErrNo( vtyErrNo _errno )
{
#ifdef WIN32
    ::SetLastError( _errno );
#elif defined( __linux__ ) || defined( __APPLE__ )
    errno = _errno;
#endif
}
inline void SetGenericFileError()
{
#ifdef WIN32
    ::SetLastError( ERROR_INVALID_HANDLE );
#elif defined( __linux__ ) || defined( __APPLE__ )
    errno = EBADF;
#endif
}

#ifdef WIN32
typedef HANDLE vtyFileHandle;
static const vtyFileHandle vkhInvalidFileHandle = INVALID_HANDLE_VALUE;
#elif defined( __linux__ ) || defined( __APPLE__ )
typedef int vtyFileHandle; // file descriptor.
static const vtyFileHandle vkhInvalidFileHandle = -1;
#endif

// A null mapping value for pointers.
static const void * vkpvNullMapping = (void*)-1;
#if defined( __linux__ ) || defined( __APPLE__ )
static_assert( vkpvNullMapping == MAP_FAILED );
#endif

inline vtyFileHandle OpenReadOnlyFile( const char * _pszFileName )
{
#ifdef WIN32
    vtyFileHandle hFile = CreateFile( _pszFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
#elif defined( __APPLE__ ) || defined( __linux__ )
    vtyFileHandle hFile = open( _pszFileName, O_RDONLY );
#endif
    return hFile;
}

// This provides a general usage method that returns a void* and closes all other files and handles, etc. Clean and is a major usage scenario.
// Return the size of the mapping in _rstSizeMapping.
// We don't throw here and we don't log - this is meant as a utility method and a black box.
inline const void * PvMapFilenameReadOnly( const char * _pszFileName, size_t & _rstSizeMapping )
{
#ifdef WIN32
    vtyFileHandle hFile = ::CreateFile( _pszFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
    if ( INVALID_HANDLE_VALUE == hFile )
        return nullptr;
    const void * pv = PvMapHandleReadOnly( hFile );
    ::CloseHandle( hFile );
    return pv;
#elif defined( __APPLE__ ) || defined( __linux__ )
    int fd = open( _pszFileName, O_RDONLY );
    struct stat bufStat;
    
#endif
}

inline bool FIsConsoleFileHandle( vtyFileHandle _hFile )
{
#ifdef WIN32
    return FILE_TYPE_CHAR == GetFileType( _hFile );
#elif defined( __APPLE__ ) || defined( __linux__ )
    return !!isatty( _hFile );
#endif
}

// File attributes and related methods:
#ifdef WIN32
typedef WIN32_FILE_ATTRIBUTE_DATA vtyFileAttr;
#elif defined( __APPLE__ ) || defined( __linux__ )
typedef struct stat vtyFileAttr;
#endif

int GetFileAttrs( const char * _pszFileName, vtyFileAttr & _rfa )
{
#ifdef WIN32
    return GetFileAttributesEx( _pszFileName, GetFileExInfoStandard, &_rfa ) ? 0 : -1;
#elif defined( __APPLE__ ) || defined( __linux__ )
    PrepareErrNo();
    return ::stat( _pszFileName, &_rfa );
#endif
}

// Handle attributes and related methods:
#ifdef WIN32
typedef BY_HANDLE_FILE_INFORMATION vtyHandleAttr;
#elif defined( __APPLE__ ) || defined( __linux__ )
typedef struct stat vtyHandleAttr;
#endif

int GetHandleAttrs( vtyFileHandle _hFile, vtyHandleAttr & _rha )
{
#ifdef WIN32
    return GetFileInformationByHandle( _pszFileName, &_rha ) ? 0 : -1;
#elif defined( __APPLE__ ) || defined( __linux__ )
    PrepareErrNo();
    return ::fstat( _hFile, &_rha );
#endif
}
uint64_t GetSize_HandleAttr( vtyHandleAttr const & _rha )
{
#ifdef WIN32
    return ( uint64_t( _rha.nFileSizeHigh ) << 32ull ) + uint64_t( _rha.nFileSizeLow );
#elif defined( __APPLE__ ) || defined( __linux__ )
    return _rha.st_size;
#endif
}
bool FIsRegularFile_HandleAttr( vtyHandleAttr const & _rha )
{
#ifdef WIN32
    // May need to modify this:
    return !( ( FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_DIRECTORY  ) & _rha.dwFileAttributes );
#elif defined( __APPLE__ ) || defined( __linux__ )
    return !!S_ISREG( _rha.st_mode );
#endif
}

// Seeking files:
#ifdef WIN32
typedef DWORD vtySeekWhence;
typedef LARGE_INTEGER vtySeekOffset;
static const vtySeekWhence vkSeekBegin = FILE_BEGIN;
static const vtySeekWhence vkSeekCur = FILE_CURRENT;
static const vtySeekWhence vkSeekEnd = FILE_END;
#elif defined( __APPLE__ ) || defined( __linux__ )
typedef int vtySeekWhence;
typedef off_t vtySeekOffset;
static const vtySeekWhence vkSeekBegin = SEEK_SET;
static const vtySeekWhence vkSeekCur = SEEK_CUR;
static const vtySeekWhence vkSeekEnd = SEEK_END;
#endif

inline int
FileSeek( vtyFileHandle _hFile, vtySeekOffset _off, vtySeekWhence _whence, vtySeekOffset * _poffResult = 0 )
{
#ifdef WIN32
    return SetFilePointerEx( _hFile, _off, _poffResult, _whence );
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

// Reading and writing files:
inline int FileRead( vtyFileHandle _hFile, void * _pvBuffer, size_t _stNBytesToRead, size_t * _pstNBytesRead = 0 )
{
#ifdef WIN32
    VerifyThrowSz( _stNBytesToRead <= std::numeric_limits<DWORD>::max(), "Windows is limited to 4GB reads. This read was for [%lu] bytes.", _stNBytesToRead );
    DWORD nBytesRead;
    if ( !ReadFile( _hFile, _pvBuffer, (DWORD)_stNBytesToRead, &nBytesRead, NULL ) )
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

__BIENUTIL_END_NAMESPACE
