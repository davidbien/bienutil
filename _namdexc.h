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
// If this is defined then we will base the named exception on std::exception, otherwise it may be based on STLPORT's stlp_std::exception.

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
  typedef basic_string< char, char_traits<char>, t_TyAllocator > string_type;
  typedef exception _tyExceptionBase;

  _t__Named_exception()
  {
    strncpy( m_rgcExceptionName, "_t__Named_exception", s_stBufSize );
    m_rgcExceptionName[s_stBufSize - 1] = '\0';
  }
  _t__Named_exception(const string_type& __str) {
    strncpy(m_rgcExceptionName, __str.c_str(), s_stBufSize);
    m_rgcExceptionName[s_stBufSize - 1] = '\0';
  }
  virtual const char* what() const _BIEN_NOTHROW { return m_rgcExceptionName; }

  template < class t__TyTraits, class t__TyAllocator >
  void SetWhat(basic_string<char, t__TyTraits, t__TyAllocator> const & _str)
  {
    SetWhat(_str.c_str(), _str.length());
  }

  void SetWhat(const char * _pszWhat, size_t _stLen)
  {
    size_t stMinLen = (std::min)(_stLen, s_stBufSize);
    strncpy(m_rgcExceptionName, _pszWhat, stMinLen);
    m_rgcExceptionName[stMinLen - 1] = '\0';
  }
private:
  static const size_t s_stBufSize = 512;
  char m_rgcExceptionName[s_stBufSize];
};

} // namespace std

#ifdef __NAMDDEXC_STDBASE
#pragma pop_macro("std")
#endif //__NAMDDEXC_STDBASE

#endif //___NAMDEXC_H___
