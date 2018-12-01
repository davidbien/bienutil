#ifndef __BITUTIL_H
#define __BITUTIL_H

#include "bienutil.h"
#include "_util.h"

// _bitutil.h

// bit utility methods:

// which algorithm ?
#define __UBITSET_USESWITCH		// Use big switch algorithm.
#define __UBITSET_USECOMPARE	// Use comparison algorithm.
#define __UBITSET_USELOOKUP8	// Use 8 bit lookup algorithm.
#define __UBITSET_USELOOKUP4	// Use 4 bit lookup algorithm.
#define __UBITSET_USECOMPLOOK_1	// use comp and then lookup - v1.
#define __UBITSET_USECOMPLOOK_2	// use comp and then lookup - v2.

__BIENUTIL_BEGIN_NAMESPACE

// Lookup table for MSB of a nibble:
static const int	v_rgiBit[16] = { 0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3 };

inline
unsigned int	__MSBitSet( unsigned long ulTest )
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

	ulTest = v_rgiBit[ ulTest >> iShift ] + iShift;

	return ulTest;
}

// This methods finds the finds the most significant bit set in <tTest>.
template < class T >
unsigned int	UMSBitSet( T tTest )
{
	return __MSBitSet( tTest );
}

// Specializations:
template <>
__MSC_INLINE
unsigned int	UMSBitSet( unsigned long ulTest )
{
	assert( CHAR_BIT * sizeof( unsigned long ) == 32 );	// algorithm is specific.

#if defined ( __UBITSET_USECOMPLOOK_1 )
// My guess is that this is the best we can do - barring a machine instruction!
// Potentially the second COMPLOOK may be faster.
// The complete comparison implementation could be faster if the compares were multiplexed!

	return __MSBitSet( ulTest );

#else //__UBITSET_USECOMPLOOK_1

#if defined ( __UBITSET_USECOMPLOOK_2 )

	// Find which bit is set:
	if ( ulTest > 0x00008000 )
	{
		if ( ulTest > 0x00800000 )
		{
			if ( ulTest > 0x08000000 )
			{
				ulTest = v_rgiBit[ ulTest >> 28 ] + 28;
			}
			else
			{
				ulTest = v_rgiBit[ ulTest >> 24 ] + 24;
			}
		}
		else
		{
			if ( ulTest > 0x00080000 )
			{
				ulTest = v_rgiBit[ ulTest >> 20 ] + 20;
			}
			else
			{
				ulTest = v_rgiBit[ ulTest >> 16 ] + 16;
			}
		}
	}
	else
	{
		if ( ulTest > 0x00000080 )
		{
			if ( ulTest > 0x00000800 )
			{
				ulTest = v_rgiBit[ ulTest >> 12 ] + 12;
			}
			else
			{
				ulTest = v_rgiBit[ ulTest >> 8 ] + 8;
			}
		}
		else
		{
			if ( ulTest > 0x00000008 )
			{
				ulTest = v_rgiBit[ ulTest >> 4 ] + 4;
			}
			else
			{
				ulTest = v_rgiBit[ ulTest ];
			}
		}
	}

#elif defined( __UBITSET_USECOMPARE )
#error incomplete
	// Find which bit is set:
	if ( ulTest > 0x00008000 )
	{
		if ( ulTest > 0x00800000 )
		{
			if ( ulTest > 0x08000000 )
			{
				if ( ulTest > 0x20000000 )
				{
					if ( ulTest == 0x80000000 )
					{
						ulTest = 31;
					}
					else
					{
						ulTest = 30;
					}
				}
				else
				{
					if ( ulTest == 0x20000000 )
					{
						ulTest = 29;
					}
					else
					{
						ulTest = 28;
					}
				}
			}
			else
			{
				if ( ulTest > 0x02000000 )
				{
					if ( ulTest > 0x08000000 )
					{
						ulTest = 27;
					}
					else
					{
						ulTest = 26;
					}
				}
				else
				{
				}
			}
		}
		else
		{
		}
	}
	else
	{
		if ( ulTest > 0x00000080 )
		{
			
		}
		else
		{
			if ( ulTest > 0x00000008 )
			{
				if ( ulTest > 0x00000040 )
				{
					
				}
				else
				{
					
				}
			}
			else
			{
				if ( ulTest > 0x00000002 )
				{
					if ( ulTest == 0x00000008 )
					{
						ulTest = 3;
					}
					else
					{
						ulTest = 2;
					}
				}
				else 
				{
					ulTest >>= 1;
				}
			}
		}
	}

#elif defined ( __UBITSET_USESWITCH )

	switch( ulTest )
	{
		case 0x00000001UL:
			ulTest = 0;
		break;
		case 0x00000002UL:
			ulTest = 1;
		break;
		case 0x00000004UL:
			ulTest = 2;
		break;
		case 0x00000008UL:
			ulTest = 3;
		break;
		case 0x00000010UL:
			ulTest = 4;
		break;
		case 0x00000020UL:
			ulTest = 5;
		break;
		case 0x00000040UL:
			ulTest = 6;
		break;
		case 0x00000080UL:
			ulTest = 7;
		break;
		case 0x00000100UL:
			ulTest = 8;
		break;
		case 0x00000200UL:
			ulTest = 9;
		break;
		case 0x00000400UL:
			ulTest = 10;
		break;
		case 0x00000800UL:
			ulTest = 11;
		break;
		case 0x00001000UL:
			ulTest = 12;
		break;
		case 0x00002000UL:
			ulTest = 13;
		break;
		case 0x00004000UL:
			ulTest = 14;
		break;
		case 0x00008000UL:
			ulTest = 15;
		break;
		case 0x00010000UL:
			ulTest = 16;
		break;
		case 0x00020000UL;
			ulTest = 17;
		break;
		case 0x00040000UL:
			ulTest = 18;
		break;
		case 0x00080000UL:
			ulTest = 19;
		break;
		case 0x00100000UL:
			ulTest = 20;
		break;
		case 0x00200000UL:
			ulTest = 21;
		break;
		case 0x00400000UL:
			ulTest = 22;
		break;
		case 0x00800000UL:
			ulTest = 23;
		break;
		case 0x01000000UL:
			ulTest = 24;
		break;
		case 0x02000000UL:
			ulTest = 25;
		break;
		case 0x04000000UL:
			ulTest = 26;
		break;
		case 0x08000000UL:
			ulTest = 27;
		break;
		case 0x10000000UL:
			ulTest = 28;
		break;
		case 0x20000000UL:
			ulTest = 29;
		break;
		case 0x40000000UL:
			ulTest = 30;
		break;
		case 0x80000000UL:
			ulTest = 31;
		break;
	}

#else //__UBITSET_USESWITCH
#error How to find log base 2 of ulTest.
#endif //__UBITSET_USESWITCH

	return ulTest;

#endif //__UBITSET_USECOMPLOOK_1


}

__BIENUTIL_END_NAMESPACE

#endif //__BITUTIL_H
