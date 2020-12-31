#pragma once
// _hFileobjs.h
// FileObj: Object(s) for the management lifetime of an hFile.

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

#include <fcntl.h>
#include "_compat.h"
#include "_assert.h"
#include <stdlib.h>
#include <unistd.h>
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
  FileObj( vtyFileHandle _hFile, bool _fOwnFile = true )
    : m_hFile( _hFile ),
      m_fOwnFile( _fOwnFile )
  {
  }
  FileObj( FileObj && _rr )
  {
    swap( _rr );
  }
  FileObj & operator = ( FileObj && _rr )
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
  static int _Close(int _hFile)
  {
    Assert( vkhInvalidFileHandle != _hFile );
#ifdef WIN32
    return CloseHandle( _hFile ) ? 0 : -1;
#else
    return ::close(_hFile);
#endif
  }
  vtyFileHandle m_hFile{vkhInvalidFileHandle};
  bool m_fOwnFile{true}; // utility for when an object doesn't own the file lifetime.
};

// We can imagine some possible additional base-level hFile-objects here.

__BIENUTIL_END_NAMESPACE
