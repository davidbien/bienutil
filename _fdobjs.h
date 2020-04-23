#pragma once
// _fdobjs.h
// FdObj: Object(s) for the management lifetime of an fd.

#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include "_namdexc.h"

// FdObj: The basest object just knows about the fd and will close it if open on destruct.
// Note that this object is not explicitly multithread aware.
class FdObj
{
  typedef FdObj _tyThis;
public:
  FdObj( FdObj const & ) = delete;
  FdObj & operator=( FdObj const & ) = delete;
  FdObj() = default;
  FdObj( int _fd )
    : m_fd (_fd )
  {
  }
  ~FdObj()
  {
    if ( FIsOpen() )
      (void)_Close( m_fd ); // Nothing to do about close errors on this codepath - throwing here seems like a bad idea.
  }

  void SetFd( const int _fd )
  {
    if ( _fd == m_fd  )
      return; // no-op.
    Close();
    m_fd = _fd;
  }
  int Close()
  {
    if ( FIsOpen() )
    {
      int fd = m_fd;
      m_fd = -1;
      return _Close( fd );
    }
    return 0;
  }

  bool FIsOpen() const
  {
    return -1 != m_fd;
  }

  int FdGet() const
  {
    return m_fd;
  }

  void Seek( off_t off )
  {
    off_t offRes = ::lseek( m_fd, off, SEEK_SET );
    if ( -1 == offRes )
      THROWNAMEDEXCEPTIONERRNO( errno, "FdObj::Seek(): ::lseek() failed." );
  }

protected:
  static int _Close(int _fd)
  {
    assert( -1 != _fd );
    return ::close(_fd);
  }
  int m_fd{-1};
};

// We can imagine some possible additional base-level fd-objects here.
