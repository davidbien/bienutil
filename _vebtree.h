#pragma once

// _vebtree.h
// Van Emde Boas tree implementation.
// dbien : 06JUN2020
// To minimize space we allow a "fixed size" impl to be grafted on to a "variable size" impl
//  at some point in the tree hierarchy.
// This also keeps memory access local.

#include <assert.h>
#include <stddef.h>
#include <cstdint>
#include <climits>
#include "_namdexc.h"

namespace n_VanEmdeBoasTreeImpl
{
    constexpr size_t KNextIntegerSize( size_t _st )
    {
        if ( _st > UINT_MAX )
            return ULLONG_MAX; // This should work for Windows and Linux.
        if ( _st > USHRT_MAX )
            return UINT_MAX;
        if ( _st > UCHAR_MAX )
            return USHRT_MAX;
        return UCHAR_MAX;
    }
    // MapToIntType
    template < size_t t_stMaxSize >
    class MapToIntType
    {
        typedef unsigned char _tyType;
    };
    template <>
    class MapToIntType< USHRT_MAX >
    {
        typedef unsigned short _tyType;
    };
    template <>
    class MapToIntType< UINT_MAX >
    {
        typedef unsigned int _tyType;
    };
    template <>
    class MapToIntType< ULLONG_MAX >
    {
        typedef unsigned long long _tyType;
    };
    template < size_t t_stMaxSize >
    using t_tyMapToIntType = typename MapToIntType< t_stMaxSize >::_tyType;

    constexpr bool FIsPow2( size_t _st )
    {
        return !( _st & ( _st - 1 ) ); // only one bit set.
    }
    template < class t_tyUint >
    constexpr t_tyUint Log2( t_tyUint _ui )
    {
        return UMSBitSet( _ui );
    }

    // The "lower square root" is just the normal square root as truncation occurs normally.
    template < class t_tyUint >
    constexpr t_tyUint LowerSqrt( t_tyUint _ui )
    {
        if ( !FIsPow2( _ui ) )
            throw std::logic_error("LowerSqrt(): _ui is not a power of two.");
        // Note that this will return 1 as the sqrt of 2. This is reasonably by design since 1 is the closest thing to the sqrt of 2 that is an integer.
        return ( 1 << ( Log2( _ui ) / 2 ) );
    }
    // The "upper square root" is 2^((k+1)/2) for 2^k when k is odd.
    template < class t_tyUint >
    constexpr t_tyUint UpperSqrt( t_tyUint _ui )
    {
        if ( !FIsPow2( _ui ) )
            throw std::logic_error("UpperSqrt(): _ui is not a power of two.");
        // Note that this will return 1 as the sqrt of 2. Probably we would return 2 but it doesn't matter for the Van Emde Boas tree impl because we should never call this with 2.
        t_tyUint nLg2 = Log2( _ui );
        return ( 1 << ( ( nLg2 / 2 ) + ( nLg2 % 2 ) ) );
    }
}

template < size_t t_kstUniverse >
class VebTreeFixed;

// For some scenarios we will need a specialized VebTreeFixed< 2 >.
template <>
class VebTreeFixed< 2 >
{
    // TODO.
};


// For VebTreeFixed<2> we only need two bits as far as I can tell (and maybe only 3 of those values in reality).
// This means we will have VebTreeFixed<4> contain 3 VebTreeFixed<2>s in a single byte and still have 2 bits left over.
// So we will specially implement VebTreeFixed<4> as the most-base class of the recursive class structure.
template <>
class VebTreeFixed< 4 >
{
    typedef VebTreeFixed _tyThis;
public:
    typedef uint8_t _tyImplType;

    static _tyImplType NCluster( _tyImplType _x ) // high
    {
        return _x / 2;
    }
    static _tyImplType NElInCluster( _tyImplType _x ) // low
    {
        return _x % 2;
    }
    static _tyImplType NIndex( _tyImplType _x, _tyImplType _y )
    {
        return ( _x * 2 ) + _y;
    }

    bool FHasAnyElements() const
    {
        return !!( s_kgrfHasMinMax & m_byVebTree4 );
    }
    bool FHasOneElement() const
    {
        return FHasAnyElements() && ( _NMin() == _NMax() );
    }
    _tyImplType NMin() const
    {
        if ( !FHasAnyElements() )
            THROWNAMEDEXCEPTION("VebTreeFixed::NMin(): No elements in tree.");
        return _NMin();
    }
    _tyImplType NMax() const
    {
        if ( !FHasAnyElements() )
            THROWNAMEDEXCEPTION("VebTreeFixed::NMax(): No elements in tree.");
        return _NMax();
    }
    bool FHasMember( _tyImplType _x ) const
    {
        assert( _x < 4 );
        if ( !FHasAnyElements() )
            return false;
        if ( ( _x == _NMin() ) || ( _x == _NMax() ) )
            return true;
        if ( _NMin() == _NMax() )
            return false;
        // Now check the two sub-elements.
        _tyImplType nCluster = NCluster( _x );
        switch( _GetVeb2( nCluster ) )
        {
            case 1:
                // (min > max) means both are nil:
                return false;
            break;
            case 2:
                // (min,max) is (0,1) - both elements are present:
                return true;
            break;
            case 3:
                return !!NElInCluster( _x );
            break;
            case 0:
                return !NElInCluster( _x );
            break;
        }
    }
    // Return the next bit after _x or 0 if there is no such bit.
    _tyImplType NSuccessor( _tyImplType _x ) const
    {
        if ( FHasAnyElements() && ( _x < _NMin() ) )
            return _NMin();
        
        _tyImplType nCluster = NCluster( _x );
        _tyImplType nEl = NElInCluster( _x );
        if ( !nEl && !!( 0x2 & _GetVeb2( nCluster ) ) )
            return NIndex( nCluster, 1 );
        else
        {
            nSuccCluster = _GetVeb2Summary()
        }


        {
            switch( ( m_byVebTree2s >> ( nCluster << 1 ) ) & 0x3 )
            {
                case 1:
                    // (min > max) means both are nil:
                    break;
                break;
                case 2:
                    // (min,max) is (0,1) - both elements are present:
                    return true;
                break;
                case 3:
                    return !!NElInCluster( _x );
                break;
                case 0:
                    return !NElInCluster( _x );
                break;
            }
        }


    }
protected:
    _tyImplType _NMin() const
    {
        assert( FHasAnyElements() );
        return m_byVebTree4 & 0x3;
    }
    _tyImplType _NMax() const
    {
        assert( FHasAnyElements() );
        return ( m_byVebTree4 >> 2 ) & 0x3;
    }
    _tyImplType _GetVeb2( _tyImplType _nCluster )
    {
        assert( _nCluster < 2 );
        return ( ( m_byVebTree2s >> ( _nCluster << 1 ) ) & 0x3 );
    }

    // Note that there are at least 6 bits wasted below and probably we could create Veb<16> as the most-class to do even better than we are doing.
    static const uint8_t s_kgrfHasMinMax = ( 1 << ( CHAR_BIT - 1 ) ); // Use a bit for this because it is available and easiest.
    uint8_t m_byVebTree4{0}; // The (max,min) of Veb<4>. Max mask: 0xc, Min mask: 0x3. s_kgrfHasMinMax: 0x80.
    uint8_t m_byVebTree2s{0x15}; // The three Veb<2>s associated with this Veb<4>. (summary: 0x30, most significant:0xc, least significant:0x3). 0x15 is all nil.
};

// VebTreeFixed:
// We will choose to have LowerSqrt(t_kstUniverse) trees of VebTreeFixed< UpperSqrt(t_kstUniverse) > since that
//  allows us to get away with not implementing VebTreeFixed<2> directly.
template < size_t t_kstUniverse >
class VebTreeFixed
{
    typedef VebTreeFixed _tyThis;
    static_assert( n_VanEmdeBoasTreeImpl::FIsPow2( t_kstUniverse ) ); // always power of 2.
public:
    // Choose impl type based on the range of values.
    static const size_t s_kstUIntMax = n_VanEmdeBoasTreeImpl::KNextIntegerSize( t_kstUniverse );
    typedef n_VanEmdeBoasTreeImpl::t_tyMapToIntType< s_kstUIntMax > _tyImplType;
    static const size_t s_kstUniverseSqrtLower = n_VanEmdeBoasTreeImpl::LowerSqrt( t_kstUniverse );
    static const size_t s_kstUniverseSqrtUpper = n_VanEmdeBoasTreeImpl::UpperSqrt( t_kstUniverse );
    typedef VebTreeFixed< s_kstUniverseSqrtUpper > _tySubTree;
    typedef VebTreeFixed< s_kstUniverseSqrtLower > _tySummaryTree;

    static _tyImplType NCluster( _tyImplType _x ) // high
    {
        return _x / s_kstUniverseSqrtUpper;
    }
    static _tyImplType NElInCluster( _tyImplType _x ) // low
    {
        return _x % s_kstUniverseSqrtUpper;
    }
    static _tyImplType NIndex( _tyImplType _x, _tyImplType _y )
    {
        return ( _x * s_kstUniverseSqrtUpper ) + _y;
    }

    bool FHasAnyElements() const
    {
        return m_nMax >= m_nMin;
    }
    bool FHasOneElement() const
    {
        return m_nMin == m_nMax;
    }
    _tyImplType NMin() const
    {
        if ( !FHasAnyElements() )
            THROWNAMEDEXCEPTION("VebTreeFixed::NMin(): No elements in tree.");
        return m_nMin;
    }
    _tyImplType NMax() const
    {
        if ( !FHasAnyElements() )
            THROWNAMEDEXCEPTION("VebTreeFixed::NMax(): No elements in tree.");
        return m_nMax;
    }
    bool FHasMember( _tyImplType _x ) const
    {
        assert( _x < t_kstUniverse );
        if ( !FHasAnyElements() )
            return false;
        if ( ( _x == m_nMin ) || ( _x == m_nMax ) )
            return true;
        if ( m_nMin == m_nMax )
            return false;
        // Now check the sub-elements.
        return m_rgstSubTrees[ NCluster(_x) ].FHasMember( NElInCluster( _x );
    }
protected:
    _tyImplType m_nMin{1}; // Setting the min to be greater than the max is the indicator that there are no elements.
    _tyImplType m_nMax{0};
    _tySubTree m_rgstSubTrees[ s_kstUniverseSqrtLower ]; // The subtrees.
    _tySummaryTree m_stSummary; // The summary tree.
};
