#ifndef __BITUTIL_H
#define __BITUTIL_H

#include "bienutil.h"
#include "_util.h"

// _bitutil.h

// bit utility methods:

#include <cstdint>

__BIENUTIL_BEGIN_NAMESPACE

// Lookup table for MSB of a nibble:
static constexpr int v_rgiBit[16] = { 0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3 };

inline
constexpr unsigned int _MSBitSet8( uint8_t _u8Test )
{
	uint8_t nShift = ( _u8Test > 0x0f ) ? 4 : 0;
	_u8Test = v_rgiBit[ ( _u8Test >> nShift ) & 0x0f ] + nShift;
	return _u8Test;
}
inline
constexpr unsigned int _MSBitSet16( uint16_t _u16Test )
{
	uint16_t nShift;
	if ( _u16Test > 0x00ff )
		nShift = ( _u16Test > 0x0fff ) ? 12 : 8;
	else
		nShift = ( _u16Test > 0x000f ) ? 4 : 0;
	_u16Test = v_rgiBit[ ( _u16Test >> nShift ) & 0x000f ] + nShift;
	return _u16Test;
}
inline
constexpr unsigned int _MSBitSet32( uint32_t _u32Test )
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
	_u32Test = v_rgiBit[ ( _u32Test >> nShift ) & 0x0000000f ] + nShift;
	return _u32Test;
}
inline
constexpr uint64_t _MSBitSet64( uint64_t _u64Test )
{
	uint64_t nShift;
	if ( _u64Test > 0x00000000ffffffff )
	{
		if ( _u64Test > 0x0000ffffffffffff )
		{
			if ( _u64Test > 0x00ffffffffffffff )
				nShift = ( _u64Test > 0x0ffffffffffffff ) ? 60 : 56;
			else
				nShift = ( _u64Test > 0x000ffffffffffff ) ? 52 : 48;
		}
		else
		{
			if ( _u64Test > 0x000000ffffffffff )
				nShift = ( _u64Test > 0x00000fffffffffff ) ? 44 : 40;
			else
				nShift = ( _u64Test > 0x0000000fffffffff ) ? 36 : 32;
		}
	}
	else
	{
		if ( _u64Test > 0x000000000000ffff )
		{
			if ( _u64Test > 0x0000000000ffffff )
				nShift = ( _u64Test > 0x000000000fffffff ) ? 28 : 24;
			else
				nShift = ( _u64Test > 0x00000000000fffff ) ? 20 : 16;
		}
		else
		{
			if ( _u64Test > 0x00000000000000ff )
				nShift = ( _u64Test > 0x0000000000000fff ) ? 12 : 8;
			else
				nShift = ( _u64Test > 0x000000000000000f ) ? 4 : 0;
		}
	}
	_u64Test = v_rgiBit[ ( _u64Test >> nShift ) & 0x000000000000000f ] + nShift;
	return _u64Test;
}

// This methods finds the finds the most significant bit set in <tTest>.
template < class t_tyT >
constexpr t_tyT UMSBitSet( t_tyT tTest );

template <>
constexpr uint64_t UMSBitSet< uint64_t >( uint64_t _tTest )
{
	return _MSBitSet64( _tTest );		
}
template <>
constexpr uint32_t UMSBitSet< uint32_t >( uint32_t _tTest )
{
	return _MSBitSet32( _tTest );		
}
template <>
constexpr uint16_t UMSBitSet< uint16_t >( uint16_t _tTest )
{
	return _MSBitSet16( _tTest );		
}
template <>
constexpr uint8_t UMSBitSet< uint8_t >( uint8_t _tTest )
{
	return _MSBitSet8( _tTest );		
}

__BIENUTIL_END_NAMESPACE

#endif //__BITUTIL_H
