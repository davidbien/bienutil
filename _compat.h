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

// Define some types that are standard in Linux in the global namespace:
#ifdef WIN32
typedef std::make_signed<size_t>::type ssize_t;
#endif

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

// Close the file and set the handle to vkhInvalidFileHandle.
inline int FileClose( vtyFileHandle & _rhFile )
{
#ifdef WIN32
    int iResult = ::CloseHandle( _rhFile ) ? 0 : -1;
#elif defined( __APPLE__ ) || defined( __linux__ )
    int iResult = ::close( _RhFile );
#endif
    _rhFile = vkhInvalidFileHandle;
    return iResult;
}

inline vtyFileHandle OpenReadOnlyFile( const char * _pszFileName )
{
#ifdef WIN32
    vtyFileHandle hFile = ::CreateFile( _pszFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
#elif defined( __APPLE__ ) || defined( __linux__ )
    vtyFileHandle hFile = open( _pszFileName, O_RDONLY );
#endif
    return hFile;
}
// This will create or truncate an existing the file specified by _pszFileName.
inline vtyFileHandle CreateWriteOnlyFile( const char * _pszFileName )
{
#ifdef WIN32
    vtyFileHandle hFile = ::CreateFile( _pszFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
#elif defined( __APPLE__ ) || defined( __linux__ )
    vtyFileHandle hFile = open( _pszFileName, O_WRONLY | O_CREAT | O_TRUNC, 0666 );
#endif
    return hFile;
}
// This will create or truncate an existing the file specified by _pszFileName.
inline vtyFileHandle CreateReadWriteFile( const char * _pszFileName )
{
#ifdef WIN32
    vtyFileHandle hFile = ::CreateFile( _pszFileName, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
#elif defined( __APPLE__ ) || defined( __linux__ )
    vtyFileHandle hFile = open( _pszFileName, O_RDWR | O_CREAT | O_TRUNC, 0666 );
#endif
    return hFile;
}

// Define a "mapped memory handle" which contains all that we need to perform the unmapping. Under Windows this is just a pv, under Linux this is a (pv,size).
class MappedMemoryHandle
{
    typedef MappedMemoryHandle _tyThis;
public:
    MappedMemoryHandle() = default;
    MappedMemoryHandle( MappedMemoryHandle const & ) = default;
    MappedMemoryHandle & operator=(MappedMemoryHandle const &) = default;
#ifdef WIN32
    MappedMemoryHandle( void * _pv, size_t _st )
        : m_pv( _pv )
    {
    }
    void * Pv() const
    {
        return m_pv;
    }
    void Clear()
    {
        m_pv = vkpvNullMapping;
    }
    void swap( MappedMemoryHandle & _r )
    {
        std::swap( m_pv );
    }
    bool operator == ( _tyThis const & _r ) const
    {
        return m_pv == _r.m_pv;
    }
protected:
    void * m_pv{vkpvNullMapping};
#elif defined( __APPLE__ ) || defined( __linux__ )
    MappedMemoryHandle( void * _pv, size_t _st )
        : m_prpv( _pv, _st )
    {
    }
    void Clear()
    {
        m_prpv.first = vkpvNullMapping;
        m_prpv.second = 0;
    }
    void * Pv() const
    {
        return m_prpv.first;
    }
    size_t length() const
    {
        return m_prpv.second;
    }
    void swap( MappedMemoryHandle & _r )
    {
        m_prpv.swap( _r.m_prpv );
    }
    bool operator == ( _tyThis const & _r ) const
    {
        return m_prpv == _r.m_prpv;
    }
protected:
    std::pair< void *, size_t > m_prpv{vkpvNullMapping,0};
#endif
public:
    operator void *() const
    {
        return Pv();
    }
    bool FFailedMapping() const
    {
        return Pv() == vkpvNullMapping;
    }
    bool FIsNull() const
    {
        return FFailedMapping();
    }
};
typedef MappedMemoryHandle vtyMappedMemoryHandle;

inline vtyMappedMemoryHandle MapReadOnlyHandle( vtyFileHandle _hFile, size_t * _pstSizeMapping = 0 ) noexcept(true)
{
#ifdef WIN32
    static_assert( sizeof(sizeFile) == sizeof(*_pstSizeMapping) );
    if ( !!_pstSizeMapping && ( !GetFileSizeEx( _hFile, _pstSizeMapping ) || !*_pstSizeMapping ) )
        return vtyMappedMemoryHandle(); // Can't map a zero size file.
     // This will also fail if the backing file happens to be of zero size. No reason to call GetFileSizeEx() under Windows unless the caller wants the size.
    vtyFileHandle hMapping = ::CreateFileMappingA( _hFile, NULL, PAGE_READONLY, 0, 0, NULL );
    if ( !hMapping ) // CreateFileMappingA return 0 on failure - annoyingly enough.
        return vtyMappedMemoryHandle();
    void* pvFile = ::MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
    ::CloseHandle( hMapping ); // Close the mapping because MapViewOfFile maintains an internal handle on it.
    if ( !pvFile )
        return vtyMappedMemoryHandle();
    return vtyMappedMemoryHandle(pvFile);
#elif defined( __APPLE__ ) || defined( __linux__ )
    // We must specify a size to mmap under linux:
    struct stat bufStat;
    static_assert( sizeof(bufStat.st_stat) == sizeof(*_pstSizeMapping) );
    int iStat = ::fstat( fd, &bufStat );
    if ( -1 == iStat )
        return vtyMappedMemoryHandle();
    if ( !!_pstSizeMapping )
        *_pstSizeMapping = bufStat.st_size;
    void * pvFile = mmap( 0, bufStat.st_size, PROT_READ, MAP_NORESERVE | MAP_SHARED, _hFile );
    if ( MAP_FAILED == pvFile )
        return vtyMappedMemoryHandle();
    return vtyMappedMemoryHandle( pvFile, bufStat.st_size );
#endif
}
// To map writeable you must map read/write - it seems on both Linux and Windows.
inline vtyMappedMemoryHandle MapReadWriteHandle( vtyFileHandle _hFile, size_t * _pstSizeMapping = 0 ) noexcept(true)
{
#ifdef WIN32
    if ( !!_pstSizeMapping && ( !GetFileSizeEx( _hFile, _pstSizeMapping ) || !*_pstSizeMapping ) )
        return vtyMappedMemoryHandle(); // Can't map a zero size file.
     // This will also fail if the backing file happens to be of zero size. No reason to call GetFileSizeEx() under Windows unless the caller wants the size.
    vtyFileHandle hMapping = ::CreateFileMappingA( _hFile, NULL, PAGE_READWRITE, 0, 0, NULL );
    if ( !hMapping ) // CreateFileMappingA return 0 on failure - annoyingly enough.
        return vtyMappedMemoryHandle();
    void* pvFile = ::MapViewOfFile(hMapping, FILE_MAP_WRITE, 0, 0, 0); // FILE_MAP_WRITE means read/write according to docs.
    ::CloseHandle( hMapping ); // Close the mapping because MapViewOfFile maintains an internal handle on it.
    if ( !pvFile )
        return vtyMappedMemoryHandle();
    return vtyMappedMemoryHandle(pvFile);
#elif defined( __APPLE__ ) || defined( __linux__ )
    // We must specify a size to mmap under linux:
    struct stat bufStat;
    static_assert( sizeof(bufStat.st_stat) == sizeof(*_pstSizeMapping) );
    int iStat = ::fstat( fd, &bufStat );
    if ( -1 == iStat )
        return vtyMappedMemoryHandle();
    if ( !!_pstSizeMapping )
        *_pstSizeMapping = bufStat.st_size;
    void * pvFile = mmap( 0, bufStat.st_size,  PROT_READ | PROT_WRITE, MAP_NORESERVE | MAP_SHARED, _hFile );
    if ( MAP_FAILED == pvFile )
        return vtyMappedMemoryHandle();
    return vtyMappedMemoryHandle( pvFile, bufStat.st_size );
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

// This provides a general usage method that returns a void* and closes all other files and handles, etc. Clean and is a major usage scenario.
// Return the size of the mapping in _rstSizeMapping.
// We don't throw here and we don't log - this is meant as a utility method and a black box.
inline vtyMappedMemoryHandle MapReadOnlyFilename( const char * _pszFileName, size_t * _pstSizeMapping = 0 ) noexcept(true)
{
#ifdef WIN32
    vtyFileHandle hFile = ::CreateFile( _pszFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
    if ( INVALID_HANDLE_VALUE == hFile )
        return vtyMappedMemoryHandle();
    vtyMappedMemoryHandle hmm = MapReadOnlyHandle( hFile, _pstSizeMapping );
    ::CloseHandle( hFile );
    return hmm;
#elif defined( __APPLE__ ) || defined( __linux__ )
    int fd = ::open( _pszFileName, O_RDONLY );
    if ( -1 == fd )
        return nullptr;
    vtyMappedMemoryHandle hmm = MapReadOnlyHandle( fd, _pstSizeMapping );
    ::close( fd )
    return hmm;
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
    return SetFilePointerEx( _hFile, _off, _poffResult, _whence ) ? 0 : -1;
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
NFileSeekAndThrow( vtyFileHandle _hFile, vtySeekOffset _off, vtySeekWhence _whence )
{
    vtySeekOffset offResult;
    int iSeek = FileSeek( _hFile, _off, _whence, &offResult );
    if ( !!iSeek )
        THROWNAMEDEXCEPTIONERRNO( GetLastErrNo(), "FileSeek() failed, _hFile[0x%lx].", (uint64_t)_hFile );
    return offResult;
}

// Reading and writing files:
inline int FileRead( vtyFileHandle _hFile, void * _pvBuffer, size_t _stNBytesToRead, size_t * _pstNBytesRead = 0 )
{
#ifdef WIN32
    VerifyThrowSz( _stNBytesToRead <= std::numeric_limits<DWORD>::max(), "Windows is limited to 4GB reads. This read was for [%lu] bytes.", _stNBytesToRead );
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
    VerifyThrowSz( _stNBytesToWrite <= std::numeric_limits<DWORD>::max(), "Windows is limited to 4GB writes. This write was for [%lu] bytes.", _stNBytesToWrite );
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

// Set the size of the file bigger or smaller. I avoid using truncate to set the file larger in linux due to performance concerns.
// There is no expectation that this results in zeros in the new portion of a larger file.
inline int FileSetSize( vtyFileHandle _hFile, size_t _stSize )
{
#ifdef WIN32
    if ( !SetFilePointerEx( _hFile, _stSize, NULL, FILE_BEGIN ) )
        return -1;
    return SetEndOfFile( _hFile ) ? 0 : -1;
#elif defined( __APPLE__ ) || defined( __linux__ )
    struct stat bufStat;
    int iResult = ::fstat( _hFile, &bufStat );
    if ( -1 == iResult )
        return -1;
    if ( _stSize < bufStat.st_size )
        return ::ftruncate( _hFile, _stSize );
    else
    if ( _stSize > bufStat.st_size )
    { // ftruncate writes zeros and it is unclear if that cause perf issues. We don't care about zeros here.
        ssize_t posEnd = ::lseek(_hFile, s_knGrowFileByBytes - 1, SEEK_SET);
        if (-1 == posEnd)
            return -1;
        errno = 0;
        return ::write(m_hFile, "", 1); // write a single byte to grow the file to s_knGrowFileByBytes.
    }
    return 0; // no-op.
#endif
}

__BIENUTIL_END_NAMESPACE
