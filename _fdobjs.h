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
  FdObj( int _fd, bool _fOwnFd = true )
    : m_fd (_fd ),
      m_fOwnFd( _fOwnFd )
  {
  }
  FdObj( FdObj && _rr )
  {
    swap( _rr );
  }
  FdObj & operator = ( FdObj && _rr )
  {
    FdObj fdo( std::move( _rr ) );
    swap( fdo );
  }
  void swap( FdObj & _r )
  {
    std::swap( m_fd, _r.m_fd );
  }
  ~FdObj()
  {
    if ( m_fOwnFd && FIsOpen() )
      (void)_Close( m_fd ); // Nothing to do about close errors on this codepath - throwing here seems like a bad idea.
  }
  bool FIsOpen() const
  {
    return -1 != m_fd;
  }
  operator int () const
  {
    return FdGet();
  }
  int FdGet() const
  {
    return m_fd;
  }
  void SetFd( const int _fd, bool _fOwnFd = true )
  {
    if ( _fd == m_fd  )
    {
      Assert( m_fOwnFd == _fOwnFd );
      return; // no-op.
    }
    Close();
    m_fd = _fd;
    m_fOwnFd = _fOwnFd;
  }
  int Close()
  {
    if ( FIsOpen() )
    {
      int fd = m_fd;
      m_fd = -1;
      return !m_fOwnFd ? 0 : _Close( fd );
    }
    return 0;
  }
protected:
  static int _Close(int _fd)
  {
    Assert( -1 != _fd );
    return ::close(_fd);
  }
  int m_fd{-1};
  bool m_fOwnFd{true}; // utility for when an object doesn't own the fd lifetime.
};

// We can imagine some possible additional base-level fd-objects here.
