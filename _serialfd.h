#pragma once
//          Copyright David Lawrence Bien 1997 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _serialfd.h
// FdSerial: This object communicates over a serial port using the termios library.

#include "_fdobjs.h"
#include <termios.h>

// TermiosCpp : public termios
// Inherit from the base struct to encapsulate operations on the object.

struct __termioscpp_init_gpsuart { }; // constructor tag for initializing a TermiosCpp to interact with a gps module.

enum ETermiosFields
{
  etf_c_iflag,
  etf_c_oflag,
  etf_c_cflag,
  etf_c_lflag,
  etf_c_line,
  etf_c_cc,
  etf_c_ispeed,
  etf_c_ospeed,
  etf_Count,
};

class TermiosWBitmask : public termios
{
  typedef termios _TyBase;
  typedef TermiosWBitmask _TyThis;
public:
  typedef unsigned int _tyBitmask;

  TermiosWBitmask()
  {
    memset(this, 0, sizeof *this);
  }
  TermiosWBitmask()

  TermiosWBitmask & operator = (TermiosWBitmask const & _r)
  {
    memcpy(this, &_r, sizeof _r);
  }
  TermiosWBitmask(__termioscpp_init_gpsuart)
  {
    options.c_cflag = B9600 | CS8 | CLOCAL | CREAD;
    options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = 0;
  }

  _tyBitmask m_grfBitsApply; // When this structure is used for setting the attributes of the device this bitmask determines which are modified from the existing attributes.
};


struct termios
{
  tcflag_t c_iflag;		/* input mode flags */
  tcflag_t c_oflag;		/* output mode flags */
  tcflag_t c_cflag;		/* control mode flags */
  tcflag_t c_lflag;		/* local mode flags */
  cc_t c_line;			/* line discipline */
  cc_t c_cc[NCCS];		/* control characters */
  speed_t c_ispeed;		/* input speed */
  speed_t c_ospeed;		/* output speed */
#define _HAVE_STRUCT_TERMIOS_C_ISPEED 1
#define _HAVE_STRUCT_TERMIOS_C_OSPEED 1
};

class FdSerial : protected FileObj
{
  typedef FileObj _TyBase;
  typedef FdSerial _TyThis;
public:
  typedef char _tyChar;
  typedef _tyChar * _tyPSTR;
  typedef const _tyChar * _tyPCSTR;

  FdSerial()
  {
  }
  FdSerial(_tyPCSTR _pcDevice, TermiosCpp)
  {
  }
  ~FdSerial()
  {
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

  int HFileGet() const
  {
    return m_fd;
  }

  int m_fd;
};

// We can imagine some possible additional base-level fd-objects here.
