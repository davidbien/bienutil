
#pragma once

//          Copyright David Lawrence Bien 1997 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _compat.h
// Multiplatform compatibility stuff.
// dbien : 30DEC2020

#ifdef WIN32
#include <Rpc.h>
#endif
#include <assert.h>

#if !defined( WIN32 ) && !defined( __APPLE__ ) && !defined( __linux__ )
#error We dont have any compatibility platforms we are familiar with.
#endif

#if ( INTPTR_MAX != INT32_MAX ) && ( INTPTR_MAX != INT64_MAX )
#error We only support 32bit and 64bit compilation.
#endif

#include <bit>
#include <utility>
#include <filesystem>
#ifndef WIN32
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#ifdef __ANDROID__
#include <linux/uuid.h>
typedef guid_t uuid_t;
#else
#include <uuid/uuid.h>
#endif // !__ANDROID__
#endif //! WIN32

// Test for compilation with multi-threading support under various platforms:
#ifndef IS_MULTITHREADED_BUILD
#ifdef _MSC_VER
#ifdef _MT
#define IS_MULTITHREADED_BUILD 1
#else
#define IS_MULTITHREADED_BUILD 0
#endif //_MT
#else  //!_MSC_VER
// Not sure how else to do this except for this - which may not be correct for all compilers/platforms:
#ifdef _REENTRANT
#define IS_MULTITHREADED_BUILD 1
#else
#define IS_MULTITHREADED_BUILD 0
#endif //_REENTRANT
#endif //!_MSC_VER
#endif //! IS_MULTITHREADED_BUILD

#if ( WCHAR_MAX == 0xffff )
#define BIEN_WCHAR_16BIT
#else
#define BIEN_WCHAR_32BIT
#endif

#include "bienutil.h" // This should be the only non-OS file included here.

// Define some types that are standard in Linux in the global namespace:
#ifdef WIN32
typedef std::make_signed< size_t >::type ssize_t;
#endif

#ifdef _MSC_VER
#define FUNCTION_PRETTY_NAME __FUNCSIG__
#else
// We are assuming gcc or clang here.
#define FUNCTION_PRETTY_NAME __PRETTY_FUNCTION__
#endif

__BIENUTIL_BEGIN_NAMESPACE

// Thead stuff:
#ifdef WIN32
#define THREAD_DECL __declspec( thread )
#elif defined( __linux__ ) || defined( __APPLE__ )
#define THREAD_DECL __thread
#endif

// This is for cases where I special cased code building on the Mac - need to review.
#if defined( WIN32 ) || defined( __linux__ )
#define THREAD_LOCAL_DECL_APPLE thread_local
#elif defined( __APPLE__ )
// This may have changed but when it was working on the Mac this is what I used.
#define THREAD_LOCAL_DECL_APPLE __thread
#endif

#ifdef WIN32
typedef DWORD vtyProcThreadId;
#elif defined( __linux__ )
typedef pid_t vtyProcThreadId;
#elif defined( __APPLE__ )
typedef __uint64_t vtyProcThreadId;
#endif

inline void
ThreadGetId( vtyProcThreadId & _rtid ) noexcept
{
#ifdef WIN32
  _rtid = GetCurrentThreadId();
#elif defined( __linux__ )
  _rtid = syscall( SYS_gettid );
#elif defined( __APPLE__ )
  (void)pthread_threadid_np( 0, &_rtid );
#endif
}

// Error number stuff:
#ifdef WIN32
typedef DWORD vtyErrNo;
#elif defined( __linux__ ) || defined( __APPLE__ )
typedef int vtyErrNo;
#endif

static const vtyErrNo vkerrNullErrNo = 0; // This is standard for unix variations and Windows.
#ifdef WIN32
static const vtyErrNo vkerrInvalidArgument = ERROR_INVALID_PARAMETER;
static const vtyErrNo vkerrOverflow = ERROR_ARITHMETIC_OVERFLOW;
static const vtyErrNo vkerrOOM = ERROR_NOT_ENOUGH_MEMORY;
#elif defined( __linux__ ) || defined( __APPLE__ )
static const vtyErrNo vkerrInvalidArgument = EINVAL;
static const vtyErrNo vkerrOverflow = EOVERFLOW;
static const vtyErrNo vkerrOOM = ENOMEM;
#endif

// REVIEW:<dbien>: This "preparation" is only necessary for methods that:
// 1) Don't set an errno on failure. Hence we could have a garbage value in the errno.
// 2) Might set an errno on success.
// Also I don't think it is necessary for Windows for those scenarios where they use errno on success - and there are a few.
inline void
PrepareErrNo() noexcept
{
#if defined( __linux__ ) || defined( __APPLE__ )
  errno = vkerrNullErrNo;
#endif // Nothing to do under Windows.
}
inline vtyErrNo
GetLastErrNo() noexcept
{
#ifdef WIN32
  return ::GetLastError();
#elif defined( __linux__ ) || defined( __APPLE__ )
  return errno;
#endif
}
inline void
SetLastErrNo( vtyErrNo _errno ) noexcept
{
#ifdef WIN32
  ::SetLastError( _errno );
#elif defined( __linux__ ) || defined( __APPLE__ )
  errno = _errno;
#endif
}
inline void
SetGenericFileError() noexcept
{
#ifdef WIN32
  ::SetLastError( ERROR_INVALID_HANDLE );
#elif defined( __linux__ ) || defined( __APPLE__ )
  errno = EBADF;
#endif
}
int
GetErrorString( vtyErrNo _errno, char * _rgchBuffer, size_t _stLen ) noexcept;

#ifdef WIN32
typedef HANDLE vtyFileHandle;
static const vtyFileHandle vkhInvalidFileHandle = INVALID_HANDLE_VALUE;
#elif defined( __linux__ ) || defined( __APPLE__ )
typedef int vtyFileHandle; // file descriptor.
static const vtyFileHandle vkhInvalidFileHandle = -1;
#endif

// Sharing:
enum class FileSharing : uint8_t
{
  NoSharing = 0x00,
  ShareRead = 0x01,
  ShareWrite = 0x02,
  ShareReadWrite = 0x03
};
inline FileSharing
operator&( FileSharing _l, FileSharing _r ) noexcept
{
  return (FileSharing)( uint8_t( _l ) & uint8_t( _r ) );
}
inline FileSharing
operator|( FileSharing _l, FileSharing _r ) noexcept
{
  return (FileSharing)( uint8_t( _l ) | uint8_t( _r ) );
}

// A null mapping value for pointers.
static void * vkpvNullMapping = (void *)-1;

// File separator in any character type:
template < class t_tyChar >
constexpr t_tyChar
TChGetFileSeparator() noexcept
{
#ifdef WIN32
  return t_tyChar( '\\' );
#else // Assume all other OS use /.
  return t_tyChar( '/' );
#endif
}
// Get the file separator not matching this OS.
template < class t_tyChar >
constexpr t_tyChar
TChGetOtherFileSeparator() noexcept
{
#ifdef WIN32
  return t_tyChar( '/' );
#else // Assume all other OS use /.
  return t_tyChar( '\\' );
#endif
}
// File separator in any character type:
template < class t_tyChar >
constexpr const t_tyChar *
PszGetFileSeparator() noexcept
{
#ifdef WIN32
  return "\\";
#else // Assume all other OS use /.
  return "/";
#endif
}
// Get the file separator not matching this OS.
template < class t_tyChar >
constexpr const t_tyChar *
PszGetOtherFileSeparator() noexcept
{
#ifdef WIN32
  return "/";
#else // Assume all other OS use /.
  return "\\";
#endif
}

inline vtyFileHandle
FileGetStdInHandle() noexcept
{
#ifdef WIN32
  return GetStdHandle( STD_INPUT_HANDLE );
#elif defined( __APPLE__ ) || defined( __linux__ )
  return STDIN_FILENO;
#endif
}
inline vtyFileHandle
FileGetStdOutHandle() noexcept
{
#ifdef WIN32
  return GetStdHandle( STD_OUTPUT_HANDLE );
#elif defined( __APPLE__ ) || defined( __linux__ )
  return STDOUT_FILENO;
#endif
}

// Close the file and set the handle to vkhInvalidFileHandle.
inline int
FileClose( vtyFileHandle & _rhFile ) noexcept
{
#ifdef WIN32
  int iResult = ::CloseHandle( _rhFile ) ? 0 : -1;
#elif defined( __APPLE__ ) || defined( __linux__ )
  int iResult = ::close( _rhFile );
#endif
  _rhFile = vkhInvalidFileHandle;
  return iResult;
}

inline vtyFileHandle
OpenReadOnlyFile( const char * _pszFileName ) noexcept
{
#ifdef WIN32
  vtyFileHandle hFile = ::CreateFile( _pszFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
#elif defined( __APPLE__ ) || defined( __linux__ )
  vtyFileHandle hFile = open( _pszFileName, O_RDONLY );
#endif
  return hFile;
}
// This will create or truncate an existing the file specified by _pszFileName.
inline vtyFileHandle
CreateWriteOnlyFile( const char * _pszFileName, FileSharing _fs = FileSharing::NoSharing ) noexcept
{
#ifdef WIN32
  DWORD dwShare = ( FileSharing::ShareRead == ( FileSharing::ShareRead & _fs ) ? FILE_SHARE_READ : 0 ) |
                  ( FileSharing::ShareWrite == ( FileSharing::ShareWrite & _fs ) ? FILE_SHARE_WRITE : 0 );
  DWORD dwAccess = GENERIC_WRITE;
  vtyFileHandle hFile = ::CreateFile( _pszFileName, dwAccess, dwShare, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
#elif defined( __APPLE__ ) || defined( __linux__ )
  // REVIEW:<dbien>: By default it looks like Linux files are shared for read at least. Need to decide if we need to change this and honor the _fs flags.
  vtyFileHandle hFile = open( _pszFileName, O_WRONLY | O_CREAT | O_TRUNC, 0666 );
#endif
  return hFile;
}
// This will create or truncate an existing the file specified by _pszFileName.
inline vtyFileHandle
CreateReadWriteFile( const char * _pszFileName ) noexcept
{
#ifdef WIN32
  vtyFileHandle hFile = ::CreateFile( _pszFileName, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
#elif defined( __APPLE__ ) || defined( __linux__ )
  vtyFileHandle hFile = open( _pszFileName, O_RDWR | O_CREAT | O_TRUNC, 0666 );
#endif
  return hFile;
}

template < class t_TyFOpenReadWrite >
inline vtyFileHandle
CreateFileMaybeReadWrite( const char * _pszFileName )
{
  return CreateWriteOnlyFile( _pszFileName );
}
template <>
inline vtyFileHandle
CreateFileMaybeReadWrite< true_type >( const char * _pszFileName )
{
  return CreateReadWriteFile( _pszFileName );
}
// Get the page size. We cache it locally as well so that once we call the system call once that's the only time we will need to.
inline size_t
GetPageSize() noexcept
{
  static const size_t stPageSize = []() noexcept
  {
#ifdef WIN32
    SYSTEM_INFO si;
    GetSystemInfo( &si );
    return si.dwPageSize;
#else
    return getpagesize();
#endif
  }();
  return stPageSize;
}

// Define a "mapped memory handle" which contains all that we need to perform the unmapping. Under Windows this is just a pv, under Linux this is a (pv,size).
class MappedMemoryHandle
{
  typedef MappedMemoryHandle _tyThis;

public:
  MappedMemoryHandle() = default;
  MappedMemoryHandle( MappedMemoryHandle const & ) = default;
  MappedMemoryHandle & operator=( MappedMemoryHandle const & ) = default;
#ifdef WIN32
  MappedMemoryHandle( void * _pv ) noexcept
    : m_pv( _pv )
  {
  }
  void * Pv() const noexcept { return m_pv; }
  void Clear() noexcept { m_pv = vkpvNullMapping; }
  void swap( MappedMemoryHandle & _r ) noexcept { std::swap( m_pv, _r.m_pv ); }
  bool operator==( _tyThis const & _r ) const noexcept { return m_pv == _r.m_pv; }

protected:
  void * m_pv{ vkpvNullMapping };
#elif defined( __APPLE__ ) || defined( __linux__ )
  MappedMemoryHandle( void * _pv, size_t _st ) noexcept
    : m_prpv( _pv, _st )
  {
  }
  void Clear() noexcept
  {
    m_prpv.first = vkpvNullMapping;
    m_prpv.second = 0;
  }
  void * Pv() const noexcept { return m_prpv.first; }
  size_t length() const noexcept { return m_prpv.second; }
  void swap( MappedMemoryHandle & _r ) noexcept { m_prpv.swap( _r.m_prpv ); }
  bool operator==( _tyThis const & _r ) const noexcept { return m_prpv == _r.m_prpv; }

protected:
  std::pair< void *, size_t > m_prpv{ vkpvNullMapping, 0 };
#endif
public:
  operator void *() const noexcept { return Pv(); }
  bool FFailedMapping() const noexcept { return Pv() == vkpvNullMapping; }
  bool FIsNull() const noexcept { return FFailedMapping(); }
};
typedef MappedMemoryHandle vtyMappedMemoryHandle;

// Map the file readonly. Return the size of the mapping in _pu64SizeMapping. Map at position *_pu64AtPosition if _pu64AtPosition.
// If _pu64AtPosition is specified then it is updated correctly by the page size - i.e. upon return *_pu64AtPosition will be equal to *_pu64AtPosition %
// dwPageSize. It's up to the caller to offset the returned void pointer appropriately.
inline vtyMappedMemoryHandle
MapReadOnlyHandle( vtyFileHandle _hFile, uint64_t * _pu64SizeMapping = nullptr, uint64_t * _pu64AtPosition = nullptr ) noexcept
{
#ifdef WIN32
  static_assert( sizeof( LARGE_INTEGER ) == sizeof( *_pu64SizeMapping ) );
  uint64_t u64AtPosition = !_pu64AtPosition ? 0 : *_pu64AtPosition;
  uint64_t u64AtPositionRemainder = ( u64AtPosition % GetPageSize() );
  uint64_t u64AtPositionAligned = u64AtPosition - u64AtPositionRemainder;
  if ( _pu64AtPosition )
    *_pu64AtPosition = u64AtPositionRemainder; // update for the caller.
  if ( !!_pu64SizeMapping )
  {
    BOOL fGetFileSize;
    if ( !( fGetFileSize = GetFileSizeEx( _hFile, (PLARGE_INTEGER)_pu64SizeMapping ) ) || ( u64AtPosition >= *_pu64SizeMapping ) )
    {
      if ( !fGetFileSize )
      {
        if ( !!_pu64SizeMapping )
          *_pu64SizeMapping = ( numeric_limits< uint64_t >::max )(); // indicate to the caller that we didn't get the file size.
      }
      else
        SetLastErrNo( ERROR_INVALID_PARAMETER ); // Can't map a zero size file or a file at a posiion beyond or equal to the end.
      return vtyMappedMemoryHandle();
    }
    *_pu64SizeMapping -= u64AtPositionAligned;
  }
  // This will also fail if the backing file happens to be of zero size. No reason to call GetFileSizeEx() under Windows unless the caller wants the size.
  vtyFileHandle hMapping = ::CreateFileMappingA( _hFile, NULL, PAGE_READONLY, 0, 0, NULL );
  if ( !hMapping ) // CreateFileMappingA return 0 on failure - annoyingly enough.
    return vtyMappedMemoryHandle();
  void * pvFile = ::MapViewOfFile( hMapping, FILE_MAP_READ, DWORD( u64AtPositionAligned >> 32 ), (DWORD)u64AtPositionAligned, 0 );
  ::CloseHandle( hMapping ); // Close the mapping because MapViewOfFile maintains an internal handle on it.
  if ( !pvFile )
    return vtyMappedMemoryHandle();
  return vtyMappedMemoryHandle( pvFile );
#elif defined( __APPLE__ ) || defined( __linux__ )
  // We must specify a size to mmap under linux:
  struct stat bufStat;
  static_assert( sizeof( bufStat.st_size ) == sizeof( *_pu64SizeMapping ) );
  int iStat = ::fstat( _hFile, &bufStat );
  if ( -1 == iStat )
  {
    if ( !!_pu64SizeMapping )
      *_pu64SizeMapping = ( numeric_limits< uint64_t >::max )(); // indicate to the caller that we didn't get the file size.
    return vtyMappedMemoryHandle();
  }
  uint64_t u64AtPosition = !_pu64AtPosition ? 0 : *_pu64AtPosition;
  if ( u64AtPosition >= bufStat.st_size )
  {
    SetLastErrNo( EINVAL );
    return vtyMappedMemoryHandle();
  }
  uint64_t u64AtPositionRemainder = ( u64AtPosition % GetPageSize() );
  uint64_t u64AtPositionAligned = u64AtPosition - u64AtPositionRemainder;
  if ( _pu64AtPosition )
    *_pu64AtPosition = u64AtPositionRemainder; // update for the caller.
  if ( !!_pu64SizeMapping )
    *_pu64SizeMapping = bufStat.st_size - u64AtPositionAligned;
  void * pvFile = ::mmap( 0, bufStat.st_size - u64AtPositionAligned, PROT_READ, MAP_NORESERVE | MAP_SHARED, _hFile, u64AtPositionAligned );
  if ( MAP_FAILED == pvFile )
    return vtyMappedMemoryHandle();
  return vtyMappedMemoryHandle( pvFile, bufStat.st_size - u64AtPositionAligned );
#endif
}
// To map writeable you must map read/write - it seems on both Linux and Windows.
inline vtyMappedMemoryHandle
MapReadWriteHandle( vtyFileHandle _hFile, uint64_t * _pu64SizeMapping = 0, uint64_t * _pu64AtPosition = nullptr ) noexcept
{
#ifdef WIN32
  uint64_t u64AtPosition = !_pu64AtPosition ? 0 : *_pu64AtPosition;
  uint64_t u64AtPositionRemainder = ( u64AtPosition % GetPageSize() );
  uint64_t u64AtPositionAligned = u64AtPosition - u64AtPositionRemainder;
  if ( _pu64AtPosition )
    *_pu64AtPosition = u64AtPositionRemainder; // update for the caller.
  if ( !!_pu64SizeMapping )
  {
    BOOL fGetFileSize;
    if ( !( fGetFileSize = GetFileSizeEx( _hFile, (PLARGE_INTEGER)_pu64SizeMapping ) ) || ( u64AtPosition >= *_pu64SizeMapping ) )
    {
      if ( !fGetFileSize )
      {
        if ( !!_pu64SizeMapping )
          *_pu64SizeMapping = ( numeric_limits< uint64_t >::max )(); // indicate to the caller that we didn't get the file size.
      }
      else
        SetLastErrNo( ERROR_INVALID_PARAMETER ); // Can't map a zero size file or a file at a posiion beyond or equal to the end.
      return vtyMappedMemoryHandle();
    }
    *_pu64SizeMapping -= u64AtPositionAligned;
  }
  // This will also fail if the backing file happens to be of zero size. No reason to call GetFileSizeEx() under Windows unless the caller wants the size.
  vtyFileHandle hMapping = ::CreateFileMappingA( _hFile, NULL, PAGE_READWRITE, 0, 0, NULL );
  if ( !hMapping ) // CreateFileMappingA return 0 on failure - annoyingly enough.
    return vtyMappedMemoryHandle();
  void * pvFile = ::MapViewOfFile( hMapping, FILE_MAP_WRITE, DWORD( u64AtPositionAligned >> 32 ), (DWORD)u64AtPositionAligned,
                                   0 ); // FILE_MAP_WRITE means read/write according to docs.
  ::CloseHandle( hMapping );            // Close the mapping because MapViewOfFile maintains an internal handle on it.
  if ( !pvFile )
    return vtyMappedMemoryHandle();
  return vtyMappedMemoryHandle( pvFile );
#elif defined( __APPLE__ ) || defined( __linux__ )
  // We must specify a size to mmap under linux:
  struct stat bufStat;
  static_assert( sizeof( bufStat.st_size ) == sizeof( *_pu64SizeMapping ) );
  int iStat = ::fstat( _hFile, &bufStat );
  if ( -1 == iStat )
  {
    if ( !!_pu64SizeMapping )
      *_pu64SizeMapping = ( numeric_limits< uint64_t >::max )(); // indicate to the caller that we didn't get the file size.
    return vtyMappedMemoryHandle();
  }
  uint64_t u64AtPosition = !_pu64AtPosition ? 0 : *_pu64AtPosition;
  if ( u64AtPosition >= bufStat.st_size )
  {
    SetLastErrNo( EINVAL );
    return vtyMappedMemoryHandle();
  }
  uint64_t u64AtPositionRemainder = ( u64AtPosition % GetPageSize() );
  uint64_t u64AtPositionAligned = u64AtPosition - u64AtPositionRemainder;
  if ( _pu64AtPosition )
    *_pu64AtPosition = u64AtPositionRemainder; // update for the caller.
  if ( !!_pu64SizeMapping )
    *_pu64SizeMapping = bufStat.st_size - u64AtPositionAligned;
  void * pvFile = ::mmap( 0, bufStat.st_size - u64AtPositionAligned, PROT_READ | PROT_WRITE, MAP_NORESERVE | MAP_SHARED, _hFile, u64AtPositionAligned );
  if ( MAP_FAILED == pvFile )
    return vtyMappedMemoryHandle();
  return vtyMappedMemoryHandle( pvFile, bufStat.st_size - u64AtPositionAligned );
#endif
}

// Unmap the mapping given the handle and set the handle to a null handle.
int
UnmapHandle( vtyMappedMemoryHandle const & _rhmm ) noexcept;

// This provides a general usage method that returns a void* and closes all other files and handles, etc. Clean and is a major usage scenario.
// Return the size of the mapping in _rstSizeMapping.
// We don't throw here and we don't log - this is meant as a utility method and a black box.
inline vtyMappedMemoryHandle
MapReadOnlyFilename( const char * _pszFileName, uint64_t * _pu64SizeMapping = 0 ) noexcept
{
#ifdef WIN32
  vtyFileHandle hFile = ::CreateFile( _pszFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
  if ( INVALID_HANDLE_VALUE == hFile )
    return vtyMappedMemoryHandle();
  vtyMappedMemoryHandle hmm = MapReadOnlyHandle( hFile, _pu64SizeMapping );
  ::CloseHandle( hFile );
  return hmm;
#elif defined( __APPLE__ ) || defined( __linux__ )
  int fd = ::open( _pszFileName, O_RDONLY );
  if ( -1 == fd )
    return vtyMappedMemoryHandle();
  vtyMappedMemoryHandle hmm = MapReadOnlyHandle( fd, _pu64SizeMapping );
  ::close( fd );
  return hmm;
#endif
}

inline bool
FIsConsoleFileHandle( vtyFileHandle _hFile ) noexcept
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

inline int
GetFileAttrs( const char * _pszFileName, vtyFileAttr & _rfa ) noexcept
{
#ifdef WIN32
  return GetFileAttributesEx( _pszFileName, GetFileExInfoStandard, &_rfa ) ? 0 : -1;
#elif defined( __APPLE__ ) || defined( __linux__ )
  PrepareErrNo();
  return ::stat( _pszFileName, &_rfa );
#endif
}
inline bool
FIsDirectory_FileAttrs( vtyFileAttr const & _rfa ) noexcept
{
#ifdef WIN32
  // May need to modify this:
  return !!( FILE_ATTRIBUTE_DIRECTORY & _rfa.dwFileAttributes );
#elif defined( __APPLE__ ) || defined( __linux__ )
  return !!S_ISDIR( _rfa.st_mode );
#endif
}
inline bool
FDirectoryExists( const char * _pszDir ) noexcept
{
  vtyFileAttr fa;
  int iRes = GetFileAttrs( _pszDir, fa );
  if ( !!iRes )
    return false;
  return FIsDirectory_FileAttrs( fa );
}
inline bool
FIsFile_FileAttrs( vtyFileAttr const & _rfa ) noexcept
{
#ifdef WIN32
  // May need to modify this - it's not really clear what you would check for here...
  return !( FILE_ATTRIBUTE_DIRECTORY & _rfa.dwFileAttributes );
#elif defined( __APPLE__ ) || defined( __linux__ )
  return !!S_ISREG( _rfa.st_mode );
#endif
}
inline bool
FFileExists( const char * _pszFile ) noexcept
{
  vtyFileAttr fa;
  int iRes = GetFileAttrs( _pszFile, fa );
  if ( !!iRes )
    return false;
  return FIsFile_FileAttrs( fa );
}
inline int
FileTouch( const char * _pszFileName ) noexcept
{
  if ( FFileExists( _pszFileName ) )
    return 0;
  vtyFileHandle hFile = CreateWriteOnlyFile( _pszFileName );
  if ( vkhInvalidFileHandle == hFile )
    return -1;
  return FileClose( hFile );
}

// Handle attributes and related methods:
#ifdef WIN32
typedef BY_HANDLE_FILE_INFORMATION vtyHandleAttr;
#elif defined( __APPLE__ ) || defined( __linux__ )
typedef struct stat vtyHandleAttr;
#endif

inline int
GetHandleAttrs( vtyFileHandle _hFile, vtyHandleAttr & _rha ) noexcept
{
#ifdef WIN32
  return GetFileInformationByHandle( _hFile, &_rha ) ? 0 : -1;
#elif defined( __APPLE__ ) || defined( __linux__ )
  PrepareErrNo();
  return ::fstat( _hFile, &_rha );
#endif
}
inline uint64_t
GetSize_HandleAttr( vtyHandleAttr const & _rha ) noexcept
{
#ifdef WIN32
  return ( uint64_t( _rha.nFileSizeHigh ) << 32ull ) + uint64_t( _rha.nFileSizeLow );
#elif defined( __APPLE__ ) || defined( __linux__ )
  return _rha.st_size;
#endif
}
inline bool
FIsRegularFile_HandleAttr( vtyHandleAttr const & _rha ) noexcept
{
#ifdef WIN32
  // May need to modify this:
  return !( ( FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_DIRECTORY ) & _rha.dwFileAttributes );
#elif defined( __APPLE__ ) || defined( __linux__ )
  return !!S_ISREG( _rha.st_mode );
#endif
}
// return the file size from the handle of uint64_t(-1) if an error is encountered.
inline uint64_t
GetFileSizeFromHandle( vtyFileHandle _hFile )
{
  vtyHandleAttr ha;
  int iRes = GetHandleAttrs( _hFile, ha );
  if ( !!iRes )
    return uint64_t( -1 );
  return GetSize_HandleAttr( ha );
}

// Seeking files:
// We always use a 64bit seek offset for files - we normalize to this regardless if we are compiling in 32bit.
typedef int64_t vtySeekOffset;
#ifdef WIN32
typedef DWORD vtySeekWhence;
static const vtySeekWhence vkSeekBegin = FILE_BEGIN;
static const vtySeekWhence vkSeekCur = FILE_CURRENT;
static const vtySeekWhence vkSeekEnd = FILE_END;
#elif defined( __APPLE__ ) || defined( __linux__ )
typedef int vtySeekWhence;
static const vtySeekWhence vkSeekBegin = SEEK_SET;
static const vtySeekWhence vkSeekCur = SEEK_CUR;
static const vtySeekWhence vkSeekEnd = SEEK_END;
#endif
inline int
FileSeek( vtyFileHandle _hFile, vtySeekOffset _off, vtySeekWhence _whence, vtySeekOffset * _poffResult = 0 ) noexcept;

// Seek and return the offset, throw on seek failure.
inline vtySeekOffset
NFileSeekAndThrow( vtyFileHandle _hFile, vtySeekOffset _off, vtySeekWhence _whence );

// Reading and writing files:
// For the sake of consistency between 32bit/64bit Windows and Linux and others we accept uint64_t as the sizes here.
// Under 32bit clearly we can't read something larger than the address space and so we throw an exception if we receive an
//  argument larger than 32bit max. In the case of Windows 64bit we will loop and call ReadFile() multiple times when the argument
//  is larger than 32bit max since the methods only accept 32bit arguments.
int
FileRead( vtyFileHandle _hFile, void * _pvBuffer, uint64_t _u64NBytesToRead, uint64_t * _pu64NBytesRead = 0 ) noexcept;
int
FileWrite( vtyFileHandle _hFile, const void * _pvBuffer, uint64_t _u64NBytesToWrite, uint64_t * _pu64NBytesWritten = 0 ) noexcept;
// This method always throws on failure or not writing the number of bytes requested.
void
FileWriteOrThrow( vtyFileHandle _hFile, const void * _pvBuffer, uint64_t _u64NBytesToWrite );

// Set the size of the file bigger or smaller. I avoid using truncate to set the file larger in linux due to performance concerns.
// There is no expectation that this results in zeros in the new portion of a larger file.
inline int
FileSetSize( vtyFileHandle _hFile, uint64_t _u64Size ) noexcept
{
#ifdef WIN32
  static_assert( sizeof( _u64Size ) == sizeof( LARGE_INTEGER ) );
  LARGE_INTEGER li;
  li.QuadPart = _u64Size;
  if ( !SetFilePointerEx( _hFile, li, NULL, FILE_BEGIN ) )
    return -1;
  return SetEndOfFile( _hFile ) ? 0 : -1;
#elif defined( __APPLE__ ) || defined( __linux__ )
  struct stat bufStat;
  static_assert( sizeof( _u64Size ) == sizeof( bufStat.st_size ) );
  int iResult = ::fstat( _hFile, &bufStat );
  if ( -1 == iResult )
    return -1;
  if ( _u64Size < bufStat.st_size )
#if INTPTR_MAX == INT32_MAX
    return ::ftruncate64( _hFile, _u64Size );
#else
    return ::ftruncate( _hFile, _u64Size );
#endif
  else if ( _u64Size > bufStat.st_size )
  { // ftruncate writes zeros and it is unclear if that cause perf issues. We don't care about zeros here.
#if INTPTR_MAX == INT32_MAX
    vtySeekOffset posEnd = ::lseek64( _hFile, _u64Size - 1, SEEK_SET );
#else
    vtySeekOffset posEnd = ::lseek( _hFile, _u64Size - 1, SEEK_SET );
#endif
    if ( -1ll == posEnd )
      return -1;
    errno = 0;
    return ( 1 == ::write( _hFile, "", 1 ) ) ? 0 : -1; // write a single byte to grow the file to s_knGrowFileByBytes.
  }
  return 0; // no-op.
#endif
}

// Delete a file.
inline int
FileDelete( const char * _pszFileName ) noexcept
{
  if ( !FFileExists( _pszFileName ) )
    return 0;
#ifdef WIN32
  return DeleteFile( _pszFileName ) ? 0 : -1;
#elif defined( __APPLE__ ) || defined( __linux__ )
  return unlink( _pszFileName );
#endif
}
// A file copy utility that uses the compatibility layer.
int
FileCopy( const char * _pszDest, const char * _pszSrc, bool _fDeleteOnFail, uint64_t _u64BlockSize = 65536 ) noexcept( false );
// A file copy that won't throw but just return an error on failure.
int
FileCopyNoThrow( const char * _pszDest, const char * _pszSrc, bool _fDeleteOnFail, uint64_t _u64BlockSize = 65536 ) noexcept;

// Endian stuff:
// Supply a constexpr bool for endianness so that we can use it in templatization, etc.
static constexpr bool vkfIsBigEndian = ( std::endian::little != std::endian::big ) && ( std::endian::native == std::endian::big );
static constexpr bool vkfIsLittleEndian = ( std::endian::little == std::endian::big ) || ( std::endian::native == std::endian::little );
typedef integral_constant< bool, vkfIsBigEndian > vTyFIsBigEndian;
typedef integral_constant< bool, vkfIsLittleEndian > vTyFIsLittleEndian;

// We supply a SwitchEndian for a byte to allow conditional compilations to compile without having to pull out a base class.
template < class t_TyT >
inline void
SwitchEndian( t_TyT & _t ) noexcept
  requires( sizeof( t_TyT ) == 1 )
{
  assert( false );
}
template < class t_TyT >
inline void
SwitchEndian( t_TyT & _t ) noexcept
  requires( sizeof( t_TyT ) == 2 )
{
#ifdef _MSC_VER
  (unsigned short &)_t = _byteswap_ushort( (unsigned short)_t );
#else  //!_MSC_VER
  (uint16_t &)_t = __builtin_bswap16( (uint16_t)_t );
#endif //!_MSC_VER
}
template < class t_TyT >
inline void
SwitchEndian( t_TyT & _t ) noexcept
  requires( sizeof( t_TyT ) == 4 )
{
#ifdef _MSC_VER
  (unsigned long &)_t = _byteswap_ulong( (unsigned long)_t );
#else  //!_MSC_VER
  (uint32_t &)_t = __builtin_bswap32( (uint32_t)_t );
#endif //!_MSC_VER
}
template < class t_TyT >
inline void
SwitchEndian( t_TyT & _t ) noexcept
  requires( sizeof( t_TyT ) == 8 )
{
#ifdef _MSC_VER
  (unsigned __int64 &)_t = _byteswap_uint64( (unsigned __int64)_t );
#else  //!_MSC_VER
  (uint64_t &)_t = __builtin_bswap64( (uint64_t)_t );
#endif //!_MSC_VER
}
template < class t_TyT >
void
SwitchEndian( t_TyT * _ptBeg, t_TyT * _ptEnd ) noexcept
{
  for ( t_TyT * ptCur = _ptBeg; _ptEnd != ptCur; ++ptCur )
    SwitchEndian( *ptCur );
}
template < class t_TyT >
void
SwitchEndian( t_TyT * _ptBeg, size_t _stLen ) noexcept
{
  t_TyT * const ptEnd = _ptBeg + _stLen;
  for ( t_TyT * ptCur = _ptBeg; ptEnd != ptCur; ++ptCur )
    SwitchEndian( *ptCur );
}

// Directory entries:
// Windows supplies tons more info than Linux but currently we don't need to do any of that multiplatform.
// I only added accessors below for things I need now of course.
#ifdef WIN32
typedef WIN32_FIND_DATAA vtyDirectoryEntry;
#elif defined( __APPLE__ ) || defined( __linux__ )
typedef struct dirent vtyDirectoryEntry;
#endif

inline const char *
PszGetName_DirectoryEntry( vtyDirectoryEntry const & _rde ) noexcept
{
#ifdef WIN32
  return _rde.cFileName;
#elif defined( __APPLE__ ) || defined( __linux__ )
  return _rde.d_name;
#endif
}
inline bool
FIsDir_DirectoryEntry( vtyDirectoryEntry const & _rde ) noexcept
{
#ifdef WIN32
  return !!( FILE_ATTRIBUTE_DIRECTORY & _rde.dwFileAttributes );
#elif defined( __APPLE__ ) || defined( __linux__ )
  return !!( DT_DIR & _rde.d_type );
#endif
}

// Time methods:
int
LocalTimeFromTime( const time_t * _ptt, struct tm * _ptmDest ) noexcept;

#ifdef WIN32
typedef UUID vtyUUID;
#elif defined( __APPLE__ ) || defined( __linux__ )
typedef uuid_t vtyUUID;
#endif

// UUID stuff:
static const size_t vkstUUIDNChars = 36;
static const size_t vkstUUIDNCharsWithNull = vkstUUIDNChars + 1;
typedef char vtyUUIDString[ vkstUUIDNCharsWithNull ];

void
UUIDCreate( vtyUUID & _ruuid ) noexcept;
int
UUIDToString( const vtyUUID & _ruuid, char * _rgcBuffer, const size_t _knBuf ) noexcept;
int
UUIDFromString( const char * _rgcBuffer, vtyUUID & _ruuid ) noexcept;

__BIENUTIL_END_NAMESPACE
