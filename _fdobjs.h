#pragma once
// _hFileobjs.h
// FileObj: Object(s) for the management lifetime of an hFile.

//          Copyright David Lawrence Bien 1997 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

#include <fcntl.h>
#include "_compat.h"
#include "_assert.h"
#include <stdlib.h>
#ifndef WIN32
#include <unistd.h>
#endif //!WIN32
#include "_namdexc.h"

__BIENUTIL_BEGIN_NAMESPACE

// FileObj: The basest object just knows about the hFile and will close it if open on destruct.
// Note that this object is not explicitly multithread aware.
class FileObj
{
  typedef FileObj _tyThis;
public:
  FileObj( FileObj const & ) = delete;
  FileObj & operator=( FileObj const & ) = delete;
  FileObj() = default;
  explicit FileObj( vtyFileHandle _hFile, bool _fOwnFile = true )
    : m_hFile( _hFile ),
      m_fOwnFile( _fOwnFile )
  {
  }
  FileObj( FileObj && _rr ) noexcept
  {
    swap( _rr );
  }
  FileObj & operator = ( FileObj && _rr ) noexcept
  {
    FileObj fdo( std::move( _rr ) );
    swap( fdo );
    return *this;
  }
  void swap( FileObj & _r )
  {
    std::swap( m_hFile, _r.m_hFile );
  }
  ~FileObj()
  {
    if ( m_fOwnFile && FIsOpen() )
      (void)_Close( m_hFile ); // Nothing to do about close errors on this codepath - throwing here seems like a bad idea.
  }
  bool FIsOpen() const
  {
    return vkhInvalidFileHandle != m_hFile;
  }
  operator vtyFileHandle () const
  {
    return HFileGet();
  }
  vtyFileHandle HFileGet() const
  {
    return m_hFile;
  }
  void SetHFile( const vtyFileHandle _hFile, bool _fOwnFile = true )
  {
    if ( _hFile == m_hFile  )
    {
      m_fOwnFile = _fOwnFile; // Allow resetting of the ownership here.
      return; // no-op.
    }
    Close();
    m_hFile = _hFile;
    m_fOwnFile = _fOwnFile;
  }
  int Close()
  {
    if ( FIsOpen() )
    {
      vtyFileHandle hFile = m_hFile;
      m_hFile = vkhInvalidFileHandle;
      return !m_fOwnFile ? 0 : _Close( hFile );
    }
    return 0;
  }
protected:
  static int _Close(vtyFileHandle _hFile)
  {
    Assert( vkhInvalidFileHandle != _hFile );
    return FileClose( _hFile );
  }
  vtyFileHandle m_hFile{vkhInvalidFileHandle};
  bool m_fOwnFile{true}; // utility for when an object doesn't own the file lifetime.
};

// FileMappingObj:
// Maintains the lifetime of a file mapping.
class FileMappingObj
{
  typedef FileMappingObj _tyThis;
public:
  FileMappingObj( FileMappingObj const & ) = delete;
  FileMappingObj & operator=( FileMappingObj const & ) = delete;
  FileMappingObj() = default;
  explicit FileMappingObj( vtyMappedMemoryHandle const & _hmmFile, bool _fOwnFile = true )
    : m_hmmFile( _hmmFile ),
      m_fOwnFile( _fOwnFile )
  {
  }
  FileMappingObj( FileMappingObj && _rr ) noexcept
  {
    swap( _rr );
  }
  FileMappingObj & operator = ( FileMappingObj && _rr ) noexcept
  {
    FileMappingObj fmo( std::move( _rr ) );
    swap( fmo );
    return *this;
  }
  void swap( FileMappingObj & _r )
  {
    m_hmmFile.swap( _r.m_hmmFile );
  }
  ~FileMappingObj()
  {
    if ( m_fOwnFile && FIsOpen() )
      (void)_Close( m_hmmFile ); // Nothing to do about close errors on this codepath - throwing here seems like a bad idea.
  }
  void * Pv() const
  {
    Assert( FIsOpen() );
    return m_hmmFile.Pv();
  }
  uint8_t * Pby( size_t _stAtBytePosition = 0 )
  {
    Assert( FIsOpen() );
    return (uint8_t*)m_hmmFile.Pv() + _stAtBytePosition;
  }
  bool FIsOpen() const
  {
    return !m_hmmFile.FIsNull();
  }
  operator vtyMappedMemoryHandle () const
  {
    return HMMFileGet();
  }
  vtyMappedMemoryHandle const & HMMFileGet() const
  {
    return m_hmmFile;
  }
  void SetHMMFile( const vtyMappedMemoryHandle & _rhmmFile, bool _fOwnFile = true )
  {
    if ( _rhmmFile == m_hmmFile  )
    {
      m_fOwnFile = _fOwnFile; // Allow resetting of the ownership here.
      return; // no-op.
    }
    Close();
    m_hmmFile = _rhmmFile;
    m_fOwnFile = _fOwnFile;
  }
  int Close()
  {
    if ( FIsOpen() )
    {
      vtyMappedMemoryHandle hmmFile = m_hmmFile;
      m_hmmFile.Clear();
      return !m_fOwnFile ? 0 : _Close( hmmFile );
    }
    return 0;
  }
  // Caller assumes OWNERSHIP of mapped memory handle returned.
  void * PvTransferHandle()
  {
    void * pv = m_hmmFile.Pv();
    m_hmmFile.Clear();
    return pv;
  }
protected:
  static int _Close(vtyMappedMemoryHandle const & _rhmmFile)
  {
    Assert( !_rhmmFile.FIsNull() );
    return  UnmapHandle( _rhmmFile );
  }
  vtyMappedMemoryHandle m_hmmFile;
  bool m_fOwnFile{true};
};

__BIENUTIL_END_NAMESPACE
