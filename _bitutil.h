#ifndef __BITUTIL_H
#define __BITUTIL_H

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

#include "bienutil.h"
#include "_util.h"

// _bitutil.h

// bit utility methods:

#include <cstdint>

__BIENUTIL_BEGIN_NAMESPACE

#ifdef _MSC_VER
#pragma intrinsic(_BitScanReverse)
#endif //_MSC_VER

// Unless a constexpr is needed then these methods use intrinsics for speed:
template < class t_tyT >
size_t MSBitSet( t_tyT _tTest )
	requires( 8 == sizeof( t_tyT ) )
{
	Assert( _tTest );
#ifdef _MSC_VER
	unsigned long ulIndex;
	unsigned char ucRes = _BitScanReverse64( &ulIndex, (unsigned __int64)_tTest );
	return !ucRes ? (numeric_limits< size_t >::max)() : size_t(ulIndex);
#else // clang, gcc:
	if ( !_tTest )
		return (numeric_limits< size_t >::max)();
	int clz = __builtin_clzll( (unsigned long long)_tTest );
	return size_t( 63 - clz );
#endif 
}
template < class t_tyT >
size_t MSBitSet( t_tyT _tTest )
	requires( ( 4 == sizeof( t_tyT ) ) || ( 2 == sizeof( t_tyT ) ) || ( 1 == sizeof( t_tyT ) ) )
{
	Assert( _tTest );
#ifdef _MSC_VER
	static_assert( 4 == sizeof( unsigned long ) );
	unsigned long ulIndex;
	unsigned char ucRes = _BitScanReverse( &ulIndex, (unsigned long)_tTest );
	return !ucRes ? (numeric_limits< size_t >::max)() : size_t(ulIndex);
#else // clang, gcc:
	static_assert( 4 == sizeof( unsigned int ) );
	if ( !_tTest )
		return (numeric_limits< size_t >::max)();
	int clz = __builtin_clz( (unsigned int)_tTest );
	return size_t( 31 - clz );
#endif 
}

// Lookup table for MSB of a nibble:
static constexpr size_t v_rgiBit[16] = { 0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3 };

inline
constexpr size_t _KMSBitSet8( uint8_t _u8Test )
{
	size_t nShift = ( _u8Test > 0x0f ) ? 4 : 0;
	size_t nRet = v_rgiBit[ ( _u8Test >> nShift ) & 0x0f ] + nShift;
	return nRet;
}
inline
constexpr size_t _KMSBitSet16( uint16_t _u16Test )
{
	size_t nShift;
	if ( _u16Test > 0x00ff )
		nShift = ( _u16Test > 0x0fff ) ? 12 : 8;
	else
		nShift = ( _u16Test > 0x000f ) ? 4 : 0;
	size_t nRet = v_rgiBit[ ( _u16Test >> nShift ) & 0x000f ] + nShift;
	return nRet;
}
inline
constexpr size_t _KMSBitSet32( uint32_t _u32Test )
{
	uint32_t nShift;
	if ( _u32Test > 0x0000ffff )
	{
		if ( _u32Test > 0x00ffffff )
			nShift = ( _u32Test > 0x0fffffff ) ? 28 : 24;
		else
			nShift = ( _u32Test > 0x000fffff ) ? 20 : 16;
	}
	else
	{
		if ( _u32Test > 0x000000ff )
			nShift = ( _u32Test > 0x00000fff ) ? 12 : 8;
		else
			nShift = ( _u32Test > 0x0000000f ) ? 4 : 0;
	}
	size_t nRet = v_rgiBit[ ( _u32Test >> nShift ) & 0x0000000f ] + nShift;
	return nRet;
}
inline
constexpr size_t _KMSBitSet64( uint64_t _u64Test )
{
	uint64_t nShift;
	if ( _u64Test > 0x00000000ffffffff )
	{
		if ( _u64Test > 0x0000ffffffffffff )
		{
			if ( _u64Test > 0x00ffffffffffffff )
				nShift = ( _u64Test > 0x0fffffffffffffff ) ? 60 : 56;
			else
				nShift = ( _u64Test > 0x000fffffffffffff ) ? 52 : 48;
		}
		else
		{
			if ( _u64Test >	0x000000ffffffffff )
				nShift = ( _u64Test > 0x00000fffffffffff ) ? 44 : 40;
			else
				nShift = ( _u64Test > 0x0000000fffffffff ) ? 36 : 32;
		}
	}
	else
	{
		if ( _u64Test > 0x000000000000ffff )
		{
			if ( _u64Test >	0x0000000000ffffff )
				nShift = ( _u64Test > 0x000000000fffffff ) ? 28 : 24;
			else
				nShift = ( _u64Test > 0x00000000000fffff ) ? 20 : 16;
		}
		else
		{
			if ( _u64Test >	0x00000000000000ff )
				nShift = ( _u64Test > 0x0000000000000fff ) ? 12 : 8;
			else
				nShift = ( _u64Test > 0x000000000000000f ) ? 4 : 0;
		}
	}
	size_t nRet = v_rgiBit[ ( _u64Test >> nShift ) & 0x000000000000000f ] + nShift;
	return nRet;
}

// These methods finds the finds the most significant bit set in <tTest>.
// The "K" signifies constexpr - if you don't need constexpr then the non-K methods use intrinsics.
template < class t_tyT >
constexpr size_t KMSBitSet( t_tyT _tTest )
	requires( 8 == sizeof( t_tyT ) )
{
	return _KMSBitSet64( (uint64_t)_tTest );
}
template < class t_tyT >
constexpr size_t KMSBitSet( t_tyT _tTest )
	requires( 4 == sizeof( t_tyT ) )
{
	return _KMSBitSet32( (uint32_t)_tTest );		
}
template < class t_tyT >
constexpr size_t KMSBitSet( t_tyT _tTest )
	requires( 2 == sizeof( t_tyT ) )
{
	return _KMSBitSet16( (uint16_t)_tTest );		
}
template < class t_tyT >
constexpr size_t KMSBitSet( t_tyT _tTest )
	requires( 1 == sizeof( t_tyT ) )
{
	return _KMSBitSet8( (uint8_t)_tTest );		
}

__BIENUTIL_END_NAMESPACE

#endif //__BITUTIL_H
