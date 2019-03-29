#ifndef __BITUTIL_H
#define __BITUTIL_H

#include "bienutil.h"
#include "_util.h"

// _bitutil.h

// bit utility methods:

__BIENUTIL_BEGIN_NAMESPACE

// Lookup table for MSB of a nibble:
static const int	v_rgiBit[16] = { 0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3 };

inline
unsigned int __MSBitSet32( unsigned long ulTest )
{
	int		iShift;
	if ( ulTest > 0x0000ffff )
	{
		if ( ulTest > 0x00ffffff )
		{
			iShift = ( ulTest > 0x0fffffff ) ? 28 : 24;
		}
		else
		{
			iShift = ( ulTest > 0x000fffff ) ? 20 : 16;
		}
	}
	else
	{
		if ( ulTest > 0x000000ff )
		{
			iShift = ( ulTest > 0x00000fff ) ? 12 : 8;
		}
		else
		{
			iShift = ( ulTest > 0x0000000f ) ? 4 : 0;
		}
	}

	ulTest = v_rgiBit[ ( ulTest >> iShift ) & 0x0000000f ] + iShift;

	return ulTest;
}

// For Linux 64 bit the unsigned long is 64bits in length, for windows it is 32 bits.
inline
unsigned int __MSBitSet64( unsigned long long ulTest )
{
	int		iShift;
	if ( ulTest > 0x00000000ffffffff )
	{
		if ( ulTest > 0x0000ffffffffffff )
		{
			if ( ulTest > 0x00ffffffffffffff )
			{
				iShift = ( ulTest > 0x0ffffffffffffff ) ? 60 : 56;
			}
			else
			{
				iShift = ( ulTest > 0x000ffffffffffff ) ? 52 : 48;
			}
		}
		else
		{
			if ( ulTest > 0x000000ffffffffff )
			{
				iShift = ( ulTest > 0x00000fffffffffff ) ? 44 : 40;
			}
			else
			{
				iShift = ( ulTest > 0x0000000fffffffff ) ? 36 : 32;
			}
		}
	}
	else
	{
		if ( ulTest > 0x000000000000ffff )
		{
			if ( ulTest > 0x0000000000ffffff )
			{
				iShift = ( ulTest > 0x000000000fffffff ) ? 28 : 24;
			}
			else
			{
				iShift = ( ulTest > 0x00000000000fffff ) ? 20 : 16;
			}
		}
		else
		{
			if ( ulTest > 0x00000000000000ff )
			{
				iShift = ( ulTest > 0x0000000000000fff ) ? 12 : 8;
			}
			else
			{
				iShift = ( ulTest > 0x000000000000000f ) ? 4 : 0;
			}
		}
	}

	ulTest = v_rgiBit[ ( ulTest >> iShift ) & 0x000000000000000f ] + iShift;

	return ulTest;
}

// This methods finds the finds the most significant bit set in <tTest>.
template < class T >
unsigned int UMSBitSet( T tTest )
{
	__ASSERT_BOOL( ( sizeof( tTest ) == 4 ) || ( sizeof( tTest ) == 8 ) ); // we could impl easy for other but unneeded at this point.
	if ( sizeof( tTest ) == 4 )
		return __MSBitSet32( tTest );
	else
		return __MSBitSet64( (unsigned long long)tTest );		
}

__BIENUTIL_END_NAMESPACE

#endif //__BITUTIL_H
