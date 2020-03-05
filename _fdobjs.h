#pragma once
// _fdobjs.h
// FdObj: Object(s) for the management lifetime of an fd.

// FdObj: The basest object just knows about the fd and will close it if open on destruct.
// Note that this object is not explicitly multithread aware.
class FdObj
{
  typedef FdObj _tyThis;
public:
  FdObj()
    : m_fd(-1)
  {
  }
  FdObj(int _fd)
    : m_fd(_fd)
  {
  }
  virtual ~FdObj()
  {
    if (FIsOpen())
      (void)_Close(m_fd); // Nothing to do about close errors on this codepath - throwing out of destructor is against the rules.
  }
private:
  void _SetFd(int _fd) // This to be used by inheriting classes in constructors.
  {
    assert(!FIsOpen());
    m_fd = _fd;
  }
  static int _Close(int _fd)
  {
    assert(-1 != _fd);
    return close(_fd);
  }
public:
  virtual int Close()
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
