#pragma once

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// strwrsv.h
// String with reserve buffer.
// dbien : 02APR2020
// Wraps std::basic_string<> or any object generic with std::basic_string<>.

#include <string>
#include <compare>

__BIENUTIL_BEGIN_NAMESPACE

// StrWRsv:
// String with reserve. We don't store a length because we assume that in the places we will use this we won't need a length often.
template <class t_tyStrBase = std::string, typename t_tyStrBase::size_type t_kstReserve = 48>
class StrWRsv
{
  typedef StrWRsv _tyThis;
  static_assert(t_kstReserve - 1 >= sizeof(t_tyStrBase)); // We reserve a single byte for flags.
public:
  typedef typename t_tyStrBase::value_type value_type;
  typedef typename t_tyStrBase::size_type size_type;
  typedef value_type _tyChar;
  typedef typename std::make_unsigned<_tyChar>::type _tyUnsignedChar;

  // We can't just provide a forwarding constructor because we want to be able to capture the cases where
  // we are initialized from a small enough string to use the reserve buffer.
  // So we'll provide some functionality now and we probably will have to add functionality later.

  StrWRsv()
  {
    Assert(!FHasStringObj());
  }
  StrWRsv(StrWRsv const &_r)
  {
    Assert(!FHasStringObj());
    assign(_r.c_str(), _r.length());
  }
  StrWRsv(StrWRsv &&_rr)
  {
    Assert(!FHasStringObj());
    swap(_rr);
  }
  StrWRsv(const _tyChar *_psz)
  {
    Assert(!FHasStringObj());
    if (!_psz)
      return;
    assign(_psz);
  }
  StrWRsv(t_tyStrBase const &_rstr)
  {
    Assert(!FHasStringObj());
    assign(_rstr.c_str(), _rstr.length());
  }
  ~StrWRsv()
  {
    _ClearStrObj();
  }
  _tyThis &operator=(_tyThis const &_r)
  {
    if (this != &_r)
      assign(_r.c_str(), _r.length());
    return *this;
  }
  _tyThis &operator=(_tyThis &_rr)
  {
    if (this != &_rr)
    {
      clear();
      swap(_rr);
    }
    return *this;
  }
  _tyThis &operator=(t_tyStrBase const &_rstr)
  {
    assign(_rstr.c_str(), _rstr.length());
    return *this;
  }
  _tyThis &operator=(t_tyStrBase &&_rrstr)
  {
    // In this case we will use the existing string even if it is small enough to put in the buffer.
    if (FHasStringObj())
      _PGetStrBase()->swap(_rrstr);
    else
    {
      new ((void *)m_rgtcBuffer) t_tyStrBase(std::move(_rrstr));
      SetHasStringObj();
    }
    return *this;
  }
  _tyThis &operator=(const _tyChar *_psz)
  {
    assign(_psz);
    return *this;
  }
  void swap(_tyThis &_r)
  {
    _tyChar rgtcBuffer[t_kstReserve];
    memcpy(rgtcBuffer, _r.m_rgtcBuffer, sizeof(m_rgtcBuffer));
    memcpy(_r.m_rgtcBuffer, m_rgtcBuffer, sizeof(m_rgtcBuffer));
    memcpy(m_rgtcBuffer, rgtcBuffer, sizeof(m_rgtcBuffer));
  }

  const _tyChar *c_str() const
  {
    return FHasStringObj() ? _PGetStrBase()->c_str() : m_rgtcBuffer;
  }
  size_type length() const
  {
    return FHasStringObj() ? _PGetStrBase()->length() : StrNLen(m_rgtcBuffer);
  }

  void _ClearFlags()
  {
    m_rgtcBuffer[t_kstReserve - 1] = 0;
  }
  _tyUnsignedChar UFlags() const
  {
    return static_cast<_tyUnsignedChar>(m_rgtcBuffer[t_kstReserve - 1]);
  }
  bool FHasStringObj() const
  {
    return UFlags() == (std::numeric_limits<_tyUnsignedChar>::max)();
  }
  void SetHasStringObj()
  {
    m_rgtcBuffer[t_kstReserve - 1] = (std::numeric_limits<_tyUnsignedChar>::max)();
  }
  void ClearHasStringObj()
  {
    _ClearFlags();
  }

  void _ClearStrObj()
  {
    if (FHasStringObj())
    {
      _ClearFlags();                  // Ensure that we don't double destruct somehow.
      _PGetStrBase()->~t_tyStrBase(); // destructors should never throw.
    }
  }
  void clear()
  {
    _ClearStrObj();
    m_rgtcBuffer[0] = 0; // length = 0.
  }
  void resize(size_type _stLen, _tyChar _ch = 0)
  {
    if (_stLen < t_kstReserve)
    {
      _ClearStrObj();
      _tyChar *const pcEnd = m_rgtcBuffer + _stLen;
      _tyChar *pc;
      for (pc = m_rgtcBuffer; pc != pcEnd; ++pc)
        *pc = _ch;
      *pc = 0;
    }
    else if (FHasStringObj())
    {
      _PGetStrBase()->resize(_stLen, _ch);
    }
    else
    {
      new ((void *)m_rgtcBuffer) t_tyStrBase(_stLen, _ch); // may throw.
      SetHasStringObj();                                   // throw-safe.
    }
  }

  _tyThis &assign(const _tyChar *_psz, size_type _stLen = (std::numeric_limits<size_type>::max)())
  {
    if (!_psz)
      THROWNAMEDEXCEPTION("null _psz.");
    if ((std::numeric_limits<size_type>::max)() == _stLen)
      _stLen = StrNLen(_psz);
    if (_stLen < t_kstReserve)
    {
      _ClearStrObj();
      memmove(m_rgtcBuffer, _psz, _stLen * sizeof(_tyChar)); // account for possible overlap.
      m_rgtcBuffer[_stLen] = 0;
    }
    else if (FHasStringObj())
    {
      // Then reuse the existing string object for speed:
      _PGetStrBase()->assign(_psz, _stLen);
    }
    else
    {
      new ((void *)m_rgtcBuffer) t_tyStrBase(_psz, _stLen); // may throw.
      SetHasStringObj();                                    // throw-safe.
    }
    return *this;
  }
  _tyThis &assign(const _tyChar *_pszBegin, const _tyChar *_pszEnd)
  {
    return assign(_pszBegin, _pszEnd, _pszEnd - _pszBegin);
  }
  _tyThis &assign(_tyThis const &_r)
  {
    return assign(_r.c_str(), _r.length());
  }
  _tyThis &assign(_tyThis &&_rr)
  {
    clear();
    swap(_rr);
    return *this;
  }

  _tyThis &operator+=(const _tyChar *_psz)
  {
    size_type stLenAdd = StrNLen(_psz);
    if (!FHasStringObj())
    {
      size_type stLenCur = length();
      if (stLenCur + stLenAdd < t_kstReserve)
      {
        memcpy(m_rgtcBuffer + stLenCur, _psz, stLenAdd);
        m_rgtcBuffer[stLenCur + stLenAdd] = 0;
      }
      else
      {
        _tyChar rgInit[stLenCur + stLenAdd];
        memcpy(rgInit, m_rgtcBuffer, stLenCur);
        memcpy(rgInit + stLenCur, _psz, stLenAdd);
        new ((void *)m_rgtcBuffer) t_tyStrBase(rgInit, stLenCur + stLenAdd); // may throw.
        SetHasStringObj();                                                   // throw-safe.
      }
    }
    else
    {
      *_PGetStrBase() += _psz;
    }
    return *this;
  }

  _tyChar const &operator[](size_type _st) const
  {
    Assert(_st <= length());
    return FHasStringObj() ? (*_PGetStrBase())[_st] : m_rgtcBuffer[_st];
  }
  _tyChar &operator[](size_type _st)
  {
    Assert(_st <= length());
    return FHasStringObj() ? (*_PGetStrBase())[_st] : m_rgtcBuffer[_st];
  }

  std::strong_ordering operator<=>(_tyThis const &_r) const
  {
    return ICompare(_r);
  }
  std::strong_ordering ICompare(_tyThis const &_r) const
  {
    return ICompareStr(c_str(), _r.c_str());
  }
  bool operator<(_tyThis const &_r) const
  {
    return ICompare(_r) < 0;
  }
  bool operator<=(_tyThis const &_r) const
  {
    return ICompare(_r) <= 0;
  }
  bool operator>(_tyThis const &_r) const
  {
    return ICompare(_r) > 0;
  }
  bool operator>=(_tyThis const &_r) const
  {
    return ICompare(_r) >= 0;
  }
  bool operator==(_tyThis const &_r) const
  {
    return ICompare(_r) == 0;
  }
  bool operator!=(_tyThis const &_r) const
  {
    return ICompare(_r) != 0;
  }
  friend std::ostream &operator<<(typename std::ostream &_ros, const _tyThis &_r)
  {
    typename std::ostream::sentry s(_ros);
    if (s)
      _ros.write(c_str(), length());
    return _ros;
  }
protected:
  t_tyStrBase *_PGetStrBase()
  {
    return static_cast<t_tyStrBase *>((void *)m_rgtcBuffer);
  }
  const t_tyStrBase *_PGetStrBase() const
  {
    return static_cast<const t_tyStrBase *>((const void *)m_rgtcBuffer);
  }

  _tyChar m_rgtcBuffer[t_kstReserve]{}; // This may contain a null terminated string or it may contain the string object.
                                        // The last byte are the flags which are zero when we are using the buffer, and non-zero (-1) when there is a string lifetime present.
};

__BIENUTIL_END_NAMESPACE
