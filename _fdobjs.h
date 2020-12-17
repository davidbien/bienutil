#pragma once
// _fdobjs.h
// FdObj: Object(s) for the management lifetime of an fd.

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

#include <fcntl.h>
#include "_assert.h"
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
    errno = 0;
    off_t offRes = ::lseek( m_fd, off, SEEK_SET );
    if ( -1 == offRes )
      THROWNAMEDEXCEPTIONERRNO( errno, "::lseek() failed." );
  }

protected:
  static int _Close(int _fd)
  {
    Assert( -1 != _fd );
    return ::close(_fd);
  }
  int m_fd{-1};
};

// We can imagine some possible additional base-level fd-objects here.
