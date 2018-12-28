/*
 * Copyright (c) 1997
 * Silicon Graphics Computer Systems, Inc.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Silicon Graphics makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 */

// dbien: This modified from SGI STL's v3.22 "stdexcept" header.

#ifndef ___NAMDEXC_H___
#define ___NAMDEXC_H___

#include <string>

//#define __NAMDDEXC_STDBASE
// If this is defined then we will base 

#ifdef __NAMDDEXC_STDBASE
#pragma push_macro("std")
#undef std
#endif //__NAMDDEXC_STDBASE

namespace std // This may be STLport as well, depending on how we are compiled.
{

// My version of SGI STL's named exception that accepts an allocator
//  ( I just like to see a no leaks after the debug run :-)
template < class t_TyAllocator = allocator< char > >
class _t__Named_exception : public exception {
public:
  typedef basic_string< char, _STL::char_traits<char>, t_TyAllocator > string_type;
  typedef exception _tyExceptionBase;

  _t__Named_exception()
  {
    strncpy( _M_name, "_t__Named_exception", _S_bufsize );
    _M_name[_S_bufsize - 1] = '\0';
  }
  _t__Named_exception(const string_type& __str) {
    strncpy(_M_name, __str.c_str(), _S_bufsize);
    _M_name[_S_bufsize - 1] = '\0';
  }
  virtual const char* what() const _STLP_NOTHROW { return _M_name; }

private:
  enum { _S_bufsize = 256 };
  char _M_name[_S_bufsize];
};

} // namespace std

#ifdef __NAMDDEXC_STDBASE
#pragma pop_macro("std")
#endif //__NAMDDEXC_STDBASE

#endif //___NAMDEXC_H___
