#pragma once
#include <stdint.h>
#include <array>
#include "bienutil.h"

__BIENUTIL_BEGIN_NAMESPACE

template < size_t t_kN, typename t_TyT = uint8_t > class BitVector
{
  typedef BitVector _TyThis;

private:
  typedef std::array< t_TyT, ( t_kN + CHAR_BIT * sizeof( t_TyT ) - 1 ) / ( CHAR_BIT * sizeof( t_TyT ) ) > _TyArray;
  _TyArray m_rgT;
public:
  typedef t_TyT _TyT;
  static const size_t s_kN = t_kN;

  BitVector()
    : m_rgT{}
  {
  }
  ~BitVector() = default;
  BitVector( _TyThis const & ) = default;
  BitVector( _TyThis && ) = default;
  _TyThis & operator=( _TyThis const & ) = default;
  _TyThis & operator=( _TyThis && ) = default;

  auto operator<=>(const BitVector& _kother) const
  {
    const size_t klastIndex = t_kN / (CHAR_BIT * sizeof(t_TyT));
    for (size_t _kindex = 0; _kindex < klastIndex; ++_kindex)
    {
      if (auto cmp = m_rgT[_kindex] <=> _kother.m_rgT[_kindex]; cmp != 0)
        return cmp;
    }
    const size_t kremainingBits = t_kN % (CHAR_BIT * sizeof(t_TyT));
    if (kremainingBits != 0)
    {
      const t_TyT kmask = (t_TyT(1) << kremainingBits) - 1;
      if (auto cmp = (m_rgT[klastIndex] & kmask) <=> (_kother.m_rgT[klastIndex] & kmask); cmp != 0)
        return cmp;
    }
    return std::strong_ordering::equal;
  }
  bool test( const size_t _kindex ) const
  {
    const size_t kelIndex = _kindex / ( CHAR_BIT * sizeof( t_TyT ) );
    const size_t kbitIndex = _kindex % ( CHAR_BIT * sizeof( t_TyT ) );
    return ( m_rgT[ kelIndex ] & ( t_TyT( 1 ) << kbitIndex ) ) != 0;
  }
  void set( const size_t _kindex, const bool _kfvalue )
  {
    const size_t kelIndex = _kindex / ( CHAR_BIT * sizeof( t_TyT ) );
    const size_t kbitIndex = _kindex % ( CHAR_BIT * sizeof( t_TyT ) );
    if ( _kfvalue )
      m_rgT[ kelIndex ] |= ( t_TyT( 1 ) << kbitIndex );
    else
      m_rgT[ kelIndex ] &= ~( t_TyT( 1 ) << kbitIndex );
  }
  /// @brief set bit
  /// @param _kindex bit index 
  void set( const size_t _kindex )
  {
    const size_t kelIndex = _kindex / ( CHAR_BIT * sizeof( t_TyT ) );
    const size_t kbitIndex = _kindex % ( CHAR_BIT * sizeof( t_TyT ) );
    m_rgT[ kelIndex ] |= ( t_TyT( 1 ) << kbitIndex );
  }
  /// @brief reset bit
  /// @param _kindex bit index
  void reset( const size_t _kindex )
  {
    const size_t kelIndex = _kindex / ( CHAR_BIT * sizeof( t_TyT ) );
    const size_t kbitIndex = _kindex % ( CHAR_BIT * sizeof( t_TyT ) );
    m_rgT[ kelIndex ] &= ~( t_TyT( 1 ) << kbitIndex );
  }
  /// @brief flip bit
  /// @param _kindex bit index
  void flip(const size_t _kindex)
  {
    const size_t kelIndex = _kindex / (CHAR_BIT * sizeof(t_TyT));
    const size_t kbitIndex = _kindex % (CHAR_BIT * sizeof(t_TyT));
    m_rgT[kelIndex] ^= (t_TyT(1) << kbitIndex);
  }

  /// @brief return true if all bits are set
  // This method doesn't assume anything about the bits beyond t_kN.
  bool all() const
  {
    const size_t klastIndex = t_kN / (CHAR_BIT * sizeof(t_TyT));
    for (size_t _kindex = 0; _kindex < klastIndex; ++_kindex)
      if (m_rgT[_kindex] != ~t_TyT(0)) return false;
    const size_t kremainingBits = t_kN % (CHAR_BIT * sizeof(t_TyT));
    if (kremainingBits != 0)
    {
      const t_TyT kmask = (t_TyT(1) << kremainingBits) - 1;
      if ((m_rgT[klastIndex] & kmask) != kmask) return false;
    }
    return true;
  }
  /// @brief return true if any bits are set
  // This method doesn't assume anything about the bits beyond t_kN.
  bool any() const
  {
    const size_t klastIndex = t_kN / (CHAR_BIT * sizeof(t_TyT));
    for (size_t _kindex = 0; _kindex < klastIndex; ++_kindex)
      if (m_rgT[_kindex] != 0) return true;
    const size_t kremainingBits = t_kN % (CHAR_BIT * sizeof(t_TyT));
    if (kremainingBits != 0)
    {
      const t_TyT kmask = (t_TyT(1) << kremainingBits) - 1;
      if ((m_rgT[klastIndex] & kmask) != 0) return true;
    }
    return false;
  }
  /// @brief return true if no bits are set
  bool none() const
  {
    return !any();
  }
  /// @brief return number of bits set
  size_t count() const
  {
    size_t kcount = 0;
    const size_t klastIndex = t_kN / (CHAR_BIT * sizeof(t_TyT));
    for (size_t _kindex = 0; _kindex < klastIndex; ++_kindex)
    {
#if defined(__GNUC__) || defined(__clang__)
        if constexpr (sizeof(t_TyT) <= 4)
          kcount += __builtin_popcount(m_rgT[_kindex]);
        else
          kcount += __builtin_popcountll(m_rgT[_kindex]);
#elif defined(_MSC_VER)
        if constexpr (sizeof(t_TyT) <= 4)
          kcount += __popcnt(m_rgT[_kindex]);
        else
          kcount += __popcnt64(m_rgT[_kindex]);
#else
        // Fallback to manual bit counting for other compilers
        for (t_TyT _kbit = m_rgT[_kindex]; _kbit; _kbit &= _kbit - 1)
        {
          ++kcount;
        }
#endif
    }
    const size_t kremainingBits = t_kN % (CHAR_BIT * sizeof(t_TyT));
    if (kremainingBits != 0)
    {
      const t_TyT kmask = (t_TyT(1) << kremainingBits) - 1;
#if defined(__GNUC__) || defined(__clang__)
        if constexpr (sizeof(t_TyT) <= 4)
          kcount += __builtin_popcount(m_rgT[klastIndex] & kmask);
        else
          kcount += __builtin_popcountll(m_rgT[klastIndex] & kmask);
#elif defined(_MSC_VER)
        if constexpr (sizeof(t_TyT) <= 4)
          kcount += __popcnt(m_rgT[klastIndex] & kmask);
        else
          kcount += __popcnt64(m_rgT[klastIndex] & kmask);
#else
        // Fallback to manual bit counting for other compilers
        for (t_TyT _kbit = m_rgT[klastIndex] & kmask; _kbit; _kbit &= _kbit - 1)
        {
          ++kcount;
        }
#endif
    }
    return kcount;
  }
  // Bitwise operators *do not* take the count of bits into account - all inspecting methods must ignore data beyond the end.
  /// @brief bitwise AND
  _TyThis& operator&=(const _TyThis& _kother)
  {
    for (size_t _kindex = 0; _kindex < m_rgT.size(); ++_kindex)
      m_rgT[_kindex] &= _kother.m_rgT[_kindex];
    return *this;
  }
  /// @brief bitwise OR
  _TyThis& operator|=(const _TyThis& _kother)
  {
    for (size_t _kindex = 0; _kindex < m_rgT.size(); ++_kindex)
      m_rgT[_kindex] |= _kother.m_rgT[_kindex];
    return *this;
  }
  /// @brief bitwise XOR
  _TyThis& operator^=(const _TyThis& _kother)
  {
    for (size_t _kindex = 0; _kindex < m_rgT.size(); ++_kindex)
      m_rgT[_kindex] ^= _kother.m_rgT[_kindex];
    return *this;
  }
  /// @brief bitwise NOT
  _TyThis operator~() const
  {
    _TyThis result;
    for (size_t _kindex = 0; _kindex < m_rgT.size(); ++_kindex)
      result.m_rgT[_kindex] = ~m_rgT[_kindex];
    return result;
  }
  /// @brief shift left
  /// @param _kshift shift amount
  /// @return *this
  _TyThis& operator<<=(const size_t _kshift)
  {
    AssertSz( false, "untested");
    if (_kshift >= t_kN)
    {
      std::fill(m_rgT.begin(), m_rgT.end(), 0);
      return *this;
    }
    const size_t kwholeShifts = _kshift / (CHAR_BIT * sizeof(t_TyT));
    const size_t kbitShifts = _kshift % (CHAR_BIT * sizeof(t_TyT));
    if (kbitShifts == 0)
    {
      std::rotate(m_rgT.rbegin(), m_rgT.rbegin() + kwholeShifts, m_rgT.rend());
    }
    else
    {
      std::rotate(m_rgT.rbegin(), m_rgT.rbegin() + kwholeShifts + 1, m_rgT.rend());
      for (size_t _kindex = m_rgT.size() - kwholeShifts - 1; _kindex < m_rgT.size(); ++_kindex)
        m_rgT[_kindex] = (m_rgT[_kindex] << kbitShifts) | (m_rgT[_kindex + 1] >> ((CHAR_BIT * sizeof(t_TyT)) - kbitShifts));
      m_rgT[m_rgT.size() - kwholeShifts - 1] <<= kbitShifts;
    }
    std::fill(m_rgT.begin(), m_rgT.begin() + kwholeShifts, 0);
    return *this;
  }
  /// @brief shift right
  /// @param _kshift number of bits
  /// @return reference to *this
  _TyThis& operator>>=(const size_t _kshift)
  {
    AssertSz( false, "untested");
    if (_kshift >= t_kN)
    {
      std::fill(m_rgT.begin(), m_rgT.end(), 0);
      return *this;
    }
    const size_t kwholeShifts = _kshift / (CHAR_BIT * sizeof(t_TyT));
    const size_t kbitShifts = _kshift % (CHAR_BIT * sizeof(t_TyT));
    if (kbitShifts == 0)
    {
      std::rotate(m_rgT.begin(), m_rgT.begin() + kwholeShifts, m_rgT.end());
    }
    else
    {
      std::rotate(m_rgT.begin(), m_rgT.begin() + kwholeShifts + 1, m_rgT.end());
      for (size_t _kindex = kwholeShifts; _kindex < m_rgT.size() - 1; ++_kindex)
      {
        m_rgT[_kindex] = (m_rgT[_kindex] >> kbitShifts) | (m_rgT[_kindex + 1] << ((CHAR_BIT * sizeof(t_TyT)) - kbitShifts));
      }
      m_rgT[m_rgT.size() - kwholeShifts - 1] >>= kbitShifts;
    }
    const size_t kremainingBits = (t_kN - _kshift) % (CHAR_BIT * sizeof(t_TyT));
    if (kremainingBits != 0)
    {
      const t_TyT kmask = ~((t_TyT(1) << kremainingBits) - 1);
      m_rgT[(t_kN - _kshift - 1) / (CHAR_BIT * sizeof(t_TyT))] &= kmask;
    }
    std::fill(m_rgT.end() - kwholeShifts, m_rgT.end(), 0);
    return *this;
  }
  /// @brief shift left
  /// @param _kshift number of bits
  /// @return a copy of *this
  _TyThis operator<<(const size_t _kshift) const
  {
    _TyThis result(*this);
    result <<= _kshift;
    return result;
  }
  /// @brief shift right
  /// @param _kshift number of bits
  /// @return a copy of *this
  _TyThis operator>>(const size_t _kshift) const
  {
    _TyThis result(*this);
    result >>= _kshift;
    return result;
  }

  /// @brief Return the next set bit after the specified index.
  /// @param _kindex starting index or std::numeric_limits<size_t>::max() to start at the first set bit
  /// @return index of next set bit or std::numeric_limits<size_t>::max() if not found
  size_t get_next_bit( size_t _kindex = (std::numeric_limits<size_t>::max)() ) const
  {
    size_t nBitStart = _kindex + 1;
    if (nBitStart >= t_kN) return (std::numeric_limits<size_t>::max)();
    size_t kelIndex = nBitStart / (CHAR_BIT * sizeof(t_TyT));
    size_t kbitIndex = nBitStart % (CHAR_BIT * sizeof(t_TyT));
    while (kelIndex < m_rgT.size())
    {
      t_TyT mask = m_rgT[kelIndex] & (~t_TyT(0) << kbitIndex);
      if (mask != 0)
      {
        #ifdef _MSC_VER
          unsigned long index;
          if constexpr (sizeof(t_TyT) == 8)
            _BitScanForward64(&index, mask);
          else
            _BitScanForward(&index, mask);
          return kelIndex * (CHAR_BIT * sizeof(t_TyT)) + index;
        #elif defined(__GNUC__) || defined(__clang__)
          if constexpr (sizeof(t_TyT) == 8)
            return kelIndex * (CHAR_BIT * sizeof(t_TyT)) + __builtin_ctzll(mask);
          else
            return kelIndex * (CHAR_BIT * sizeof(t_TyT)) + __builtin_ctz(mask);
        #else
          // Brute force method for other compilers
          for (size_t i = 0; i < CHAR_BIT * sizeof(t_TyT); ++i)
          {
            if ((mask >> i) & 1)
            {
              return kelIndex * (CHAR_BIT * sizeof(t_TyT)) + i;
            }
          }
        #endif
      }
      // No set bit found in this element, continue with the next one
      ++kelIndex;
      kbitIndex = 0;
    }
    // No set bit found in any element
    return (std::numeric_limits<size_t>::max)();
  }
};

__BIENUTIL_END_NAMESPACE