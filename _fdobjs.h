#pragma once

// _fdobjs.h
// FdObj: Object(s) for the management lifetime of an fd.

// FdObj: The basest object just knows about the fd and will close it if open on destruct.
class FdObj
{
  typedef FdObj _tyThis;
public:
  FdObj()
    : m_fd(-1)
  {
  }
  ~FdObj()
  {
    (void)_Close(m_fd);
  }
  static int _Close(int _fd)
  {
    return (-1 != _fd) ? 0 : close(_fd);
  }
  int Close()
  {
    if (FIsOpen())
    {
      int fd = m_fd;
      m_fd = -1;
      return _Close(fd);
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

  int m_fd;
};

// We can imagine some possible additional base-level fd-objects here.
