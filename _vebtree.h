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
    typedef VebTreeFixed _tyThis;
public:
    static const size_t s_kstUniverse = 2;
    typedef uint8_t _tyImplType;
    static const _tyImplType s_kitNoPredecessor = s_kstUniverse - 1;

    bool FHasAnyElements() const
    {
        return m_byVebTree2 != 0b01;
    }
    bool FHasOneElement() const
    {
        return ( m_byVebTree2 == 0b00 ) || ( m_byVebTree2 == 0b11 );
    }
    _tyImplType NMin() const
    {
        if ( !FHasAnyElements() )
            THROWNAMEDEXCEPTION("VebTreeFixed::NMin(): No elements in tree.");
        return _NMin();
    }
    bool FHasMin( _tyImplType & _nMin ) const
    {
        if ( FHasAnyElements() )
        {
            _nMin = _NMin();
            return true;
        }
        return false;
    }
    _tyImplType NMax() const
    {
        if ( !FHasAnyElements() )
            THROWNAMEDEXCEPTION("VebTreeFixed::NMax(): No elements in tree.");
        return _NMax();
    }
    bool FHasMax( _tyImplType & _nMax ) const
    {
        if ( FHasAnyElements() )
        {
            _nMax = _NMax();
            return true;
        }
        return false;
    }
    void Clear()
    {
        _ClearHasElements();
    }
    // Note: Insert() assumes that _x is not in the set. If _x may be in the set then use CheckInsert().
    void Insert( _tyImplType _x )
    {
        switch( m_byVebTree2 )
        {
            case 0b01: // nil
                m_byVebTree2 = _x | ( _x << 1 );
            break;
            case 0b00:
                assert( 1 == _x );
            case 0b11:
                assert( ( 0b00 == m_byVebTree2 ) || !_x );
                m_byVebTree2 = 0b10;
            break;
            case 0b10:
                assert( false ); // we shouldn't have this element in the set.
            break; // Don't modify it.
        }
    }
    // Return true if the element was inserted, false if it already existed.
    bool FCheckInsert( _tyImplType _x )
    {
        if ( FHasElement( _x ) )
            return false;
        Insert( _x );
        return true;
    }
    // Note: Delete() assumes that _x is in the set. If _x may not be in the set then use CheckDelete().
    void Delete( _tyImplType _x )
    {
        switch( m_byVebTree2 )
        {
            case 0b01: // nil
                assert( false ); // Should have this element.
            break;
            case 0b00:
                assert( 0 == _x );
            case 0b11:
                assert( ( 0b00 == m_byVebTree2 ) || ( 1 == _x ) );
                m_byVebTree2 = 0b01; // No elements remaining.
            break;
            case 0b10: // Both.
                m_byVebTree2 = !_x | ( (!_x) << 1 );
            break; // Don't modify it.
        }
    }
    // Return true if the element was deleted, false if it already existed.
    bool FCheckDelete( _tyImplType _x )
    {
        if ( !FHasElement( _x ) )
            return false;
        Delete( _x );
        return true;
    }
    bool FHasElement( _tyImplType _x ) const
    {
        return !_x ? !( m_byVebTree2 & 0b01 ) : !!( m_byVebTree2 & 0b10 );
    }
    // Return the next bit after _x or 0 if there is no such bit.
    _tyImplType NSuccessor( _tyImplType _x ) const
    {
        return !_x && ( 0b10 & m_byVebTree2 );
    }
    // Return the next bit after _x or 0 if there is no such bit.
    _tyImplType NPredecessor( _tyImplType _x ) const
    {
        return _x && !( 0b01 & m_byVebTree2 );
    }
protected:
    void _ClearHasElements()
    {
        m_byVebTree2 = 0b01;
    }
    _tyImplType _NMin() const
    {
        assert( FHasAnyElements() );
        return m_byVebTree2 & 0b01;
    }
    _tyImplType _NMax() const
    {
        assert( FHasAnyElements() );
        return m_byVebTree2 >> 1;
    }
    _tyImplType m_byVebTree2{1}; // Initialize to empty tree.
};


// For VebTreeFixed<2> we only need two bits as far as I can tell (and maybe only 3 of those values in reality).
// This means we will have VebTreeFixed<4> contain 3 VebTreeFixed<2>s in a single byte and still have 2 bits left over.
// So we will specially implement VebTreeFixed<4> as the most-base class of the recursive class structure.
template <>
class VebTreeFixed< 4 >
{
    typedef VebTreeFixed _tyThis;
public:
    static const size_t s_kstUniverse = 4;
    static const size_t s_kstUniverseSqrt = 2;
    typedef uint8_t _tyImplType;
    static const _tyImplType s_kitNoPredecessor = s_kstUniverse - 1;

    static _tyImplType NCluster( _tyImplType _x ) // high
    {
        return _x / s_kstUniverseSqrt;
    }
    static _tyImplType NElInCluster( _tyImplType _x ) // low
    {
        return _x % s_kstUniverseSqrt;
    }
    static _tyImplType NIndex( _tyImplType _x, _tyImplType _y )
    {
        return ( _x * s_kstUniverseSqrt ) + _y;
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
    bool FHasMin( _tyImplType & _nMin ) const
    {
        if ( FHasAnyElements() )
        {
            _nMin = _NMin();
            return true;
        }
        return false;
    }
    _tyImplType NMax() const
    {
        if ( !FHasAnyElements() )
            THROWNAMEDEXCEPTION("VebTreeFixed::NMax(): No elements in tree.");
        return _NMax();
    }
    bool FHasMax( _tyImplType & _nMax ) const
    {
        if ( FHasAnyElements() )
        {
            _nMax = _NMax();
            return true;
        }
        return false;
    }
    void Clear()
    {
#ifndef NDEBUG
        // In debug lets verify things.
        if ( FHasAnyElements() )
        {
            if ( FHasOneElement() )
                assert( 0x15 == m_byVebTree2s ); // should already be nils.
            else
                m_byVebTree2s = 0x15;
            _ClearHasElements();
        }
#else
        // In retail lets just clear things cuz it is easy:
        m_byVebTree2s = 0x15;
        _ClearHasElements();
#endif
    }
    // Note: Insert() assumes that _x is not in the set. If _x may be in the set then use CheckInsert().
    void Insert( _tyImplType _x )
    {
        assert( _x < s_kstUniverse );
        assert( !FHasElement( _x ) );
        if ( !FHasAnyElements() )
        {
            _SetMin( _x );
            _SetMax( _x );
            _SetHasElements();
        }
        else
        {
            if ( _x < _NMin() )
            {
                _tyImplType nTemp = _NMin();
                _SetMin( _x );
                _x = nTemp;
            }
            _tyImplType nCluster = NCluster( _x );
            _tyImplType nEl = NElInCluster( _x );
            if ( 0b01 == _GetVeb2( nCluster ) )
            {
                if ( !nCluster )
                    _SetVeb2Summary( _GetVeb2Summary() & 0x2 ); // Clear the zeroth bit.
                else
                    _SetVeb2Summary( _GetVeb2Summary() | 0x2 ); // Set the oneth bit.
            }
            if ( !nEl )
                _SetVeb2( nCluster, _GetVeb2( nCluster ) & 0x2 ); // Clear the zeroth bit.
            else
                _SetVeb2( nCluster, _GetVeb2( nCluster ) | 0x2 ); // Set the second bit.
            if ( _x > _NMax() )
                _SetMax( _x );
        }
    }
    // Return true if the element was inserted, false if it already existed.
    bool FCheckInsert( _tyImplType _x )
    {
        if ( FHasElement( _x ) )
            return false;
        Insert( _x );
        return true;
    }
    // Note: Delete() assumes that _x is in the set. If _x may not be in the set then use CheckDelete().
    void Delete( _tyImplType _x )
    {
        assert( FHasElement( _x ) );
        if ( _NMin() == _NMax() )
        {
            assert( _x == _NMax() );
            _ClearHasElements(); 
        }
        else
        {
            if ( _x == _NMin() )
            {
                _tyImplType nFirstCluster = _GetVeb2Summary() & 0x1;
                _x = NIndex( nFirstCluster, _GetVeb2( nFirstCluster ) & 0x1 );
                _SetMin( _x );
            }
            _tyImplType nCluster = NCluster( _x );
            _tyImplType nEl = NElInCluster( _x );
            _tyImplType nDeleteVeb2Result;
            { //B - delete nEl out of nCluster.
                switch( _GetVeb2( nCluster ) )
                {
                    case 0b01:
                        assert( false ); // bad - we should have something here.
                    case 0b00:
                        assert( ( 0b00 != _GetVeb2( nCluster ) ) || ( 0 == nEl ) );
                    case 0b11:
                        assert( ( 0b11 != _GetVeb2( nCluster ) ) || ( 1 == nEl ) );
                        nDeleteVeb2Result = 0b01; // No elements remaining.
                        break;
                    case 0b10:
                        nDeleteVeb2Result = ( 1 == nEl ) ? 0b00 : 0b11;
                        break;
                }
                _SetVeb2( nCluster, nDeleteVeb2Result );
            } //EB

            if ( 0b01 == nDeleteVeb2Result ) // No elements left.
            {
                _tyImplType nDeleteSummaryResult;
                { //B - delete nCluster out of the summary.
                    switch( _GetVeb2Summary() )
                    {
                        case 0b01:
                            assert( false ); // bad - we should have something here.
                        case 0b00:
                            assert( ( 0b00 != _GetVeb2Summary() ) || ( 0 == nCluster ) );
                        case 0b11:
                            assert( ( 0b11 != _GetVeb2Summary() ) || ( 1 == nCluster ) );
                            nDeleteSummaryResult = 0b01; // No elements remaining.
                            break;
                        case 0b10:
                            nDeleteSummaryResult = ( 1 == nCluster ) ? 0b00 : 0b11;
                            break;
                    }
                    _SetVeb2Summary( nDeleteSummaryResult );
                } //EB
                if ( _x == _NMax() )
                {
                    if ( 0b01 == nDeleteSummaryResult )
                    {
                        _SetMax( _NMin() );
                    }
                    else
                    {
                        _tyImplType nSummaryMax = nDeleteSummaryResult >> 1;
                        _SetMax( NIndex( nSummaryMax, ( _GetVeb2( nSummaryMax ) >> 1 ) ) );
                    }
                }
            }
            else
            if ( _x == _NMax() )
                _SetMax( NIndex( nCluster, ( _GetVeb2( nCluster ) >> 1 ) ) );
        }
    }
    // Return true if the element was deleted, false if it already existed.
    bool FCheckDelete( _tyImplType _x )
    {
        if ( !FHasElement( _x ) )
            return false;
        Delete( _x );
        return true;
    }
    bool FHasElement( _tyImplType _x ) const
    {
        assert( _x < s_kstUniverse );
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
            case 0b01:
                // (min > max) means both are nil:
                return false;
            break;
            case 0b10:
                // (min,max) is (0,1) - both elements are present:
                return true;
            break;
            case 0b11:
                return !!NElInCluster( _x );
            break;
            case 0b00:
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
        if ( !nEl && !!( 0x2 & _GetVeb2( nCluster ) ) ) // max must be 1.
            return NIndex( nCluster, 1 );
        else
        if ( !nCluster && !!( 0x2 & _GetVeb2Summary() ) )
        {
            const _tyImplType nSuccCluster = 1; // always.
            return NIndex( nSuccCluster, ( 0x1 & _GetVeb2( nSuccCluster ) ) );
        }
        return 0; // No successor.
    }
    // Return the next bit after _x or 0 if there is no such bit.
    _tyImplType NPredecessor( _tyImplType _x ) const
    {
        if ( FHasAnyElements() && ( _x > _NMax() ) )
            return _NMax();
        
        _tyImplType nCluster = NCluster( _x );
        _tyImplType nEl = NElInCluster( _x );
        if ( !!nEl && !( 0x1 & _GetVeb2( nCluster ) ) ) // min must be zero.
            return NIndex( nCluster, 0 );
        else
        if ( nCluster && !( 0x1 & _GetVeb2Summary() ) )
        {
            const _tyImplType nPredCluster = 0; // always.
            return NIndex( nPredCluster, ( 0x2 & _GetVeb2( nPredCluster ) ) ? 1 : 0 );
        }
        else
        if ( FHasAnyElements() && ( _x > _NMin() ) )
            return _NMin();
        return s_kitNoPredecessor; // No predecessor.
    }

protected:
    void _SetHasElements()
    {
        m_byVebTree4 |= s_kgrfHasMinMax;
    }
    void _ClearHasElements()
    {
        m_byVebTree4 &= ~s_kgrfHasMinMax;
    }
    void _SetMin( _tyImplType _n )
    {
        assert( _n < s_kstUniverse );
        m_byVebTree4 = ( m_byVebTree4 & ~0x3 ) | _n;
    }
    _tyImplType _NMin() const
    {
        assert( FHasAnyElements() );
        return m_byVebTree4 & 0x3;
    }
    void _SetMax( _tyImplType _n )
    {
        assert( _n < s_kstUniverse );
        m_byVebTree4 = ( m_byVebTree4 & ~0xc ) | ( _n << 2 );
    }
    _tyImplType _NMax() const
    {
        assert( FHasAnyElements() );
        return ( m_byVebTree4 >> 2 ) & 0x3;
    }
    _tyImplType _GetVeb2( _tyImplType _nCluster ) const
    {
        assert( _nCluster < 2 );
        return ( ( m_byVebTree2s >> ( _nCluster << 1 ) ) & 0x3 );
    }
    _tyImplType _SetVeb2( _tyImplType _nCluster, _tyImplType _n )
    {
        assert( _nCluster < 2 );
        m_byVebTree2s = ( m_byVebTree2s & ~( 0x03 << ( _nCluster << 1 ) ) ) | ( _n << ( _nCluster << 1 ) );
    }
    _tyImplType _GetVeb2Summary() const
    {
        return ( ( m_byVebTree2s >> 4 ) & 0x3 );
    }
    void _SetVeb2Summary( _tyImplType _n )
    {
        assert( _n < s_kstUniverse );
        m_byVebTree2s = ( m_byVebTree2s & ~0x30 ) | ( _n << 4 );
    }

    // Note that there are at least 6 bits wasted below and probably we could create Veb<16> as the most-class to do even better than we are doing.
    static const uint8_t s_kgrfHasMinMax = ( 1 << ( CHAR_BIT - 1 ) ); // Use a bit for this because it is available and easiest.
    uint8_t m_byVebTree4{0}; // The (max,min) of Veb<4>. Max mask: 0xc, Min mask: 0x3. s_kgrfHasMinMax: 0x80.
    uint8_t m_byVebTree2s{0x15}; // The three Veb<2>s associated with this Veb<4>. (summary: 0x30, most significant:0xc, least significant:0x3). 0x15 is all nil.
};

// VebTreeFixed:
// We will choose to have LowerSqrt(t_kstUniverse) trees of VebTreeFixed< UpperSqrt(t_kstUniverse) >.
template < size_t t_kstUniverse >
class VebTreeFixed
{
    typedef VebTreeFixed _tyThis;
    static_assert( n_VanEmdeBoasTreeImpl::FIsPow2( t_kstUniverse ) ); // always power of 2.
public:
    // Choose impl type based on the range of values.
    static const size_t s_kstUniverse = t_kstUniverse;
    static const size_t s_kstUIntMax = n_VanEmdeBoasTreeImpl::KNextIntegerSize( t_kstUniverse );
    typedef n_VanEmdeBoasTreeImpl::t_tyMapToIntType< s_kstUIntMax > _tyImplType;
    static const _tyImplType s_kitNoPredecessor = t_kstUniverse - 1;
    static const size_t s_kstUniverseSqrtLower = n_VanEmdeBoasTreeImpl::LowerSqrt( t_kstUniverse );
    static const size_t s_kstUniverseSqrtUpper = n_VanEmdeBoasTreeImpl::UpperSqrt( t_kstUniverse );
    typedef VebTreeFixed< s_kstUniverseSqrtUpper > _tySubtree;
    typedef typename _tySubtree::_tyImplType _tyImplTypeSubtree;
    static const _tyImplTypeSubtree s_kstitNoPredecessorSubtree = s_kstUniverseSqrtUpper-1;
    typedef VebTreeFixed< s_kstUniverseSqrtLower > _tySummaryTree;
    typedef typename _tySummaryTree::_tyImplType _tyImplTypeSummaryTree;
    static const _tyImplTypeSummaryTree s_kstitNoPredecessorSummaryTree = s_kstUniverseSqrtLower-1;

    static _tyImplTypeSubtree NCluster( _tyImplType _x ) // high
    {
        return _tyImplTypeSubtree( _x / s_kstUniverseSqrtUpper );
    }
    static _tyImplTypeSubtree NElInCluster( _tyImplType _x ) // low
    {
        return _tyImplTypeSubtree( _x % s_kstUniverseSqrtUpper );
    }
    static _tyImplType NIndex( _tyImplTypeSubtree _x, _tyImplTypeSubtree _y )
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
    bool FHasMin( _tyImplType & _nMin ) const
    {
        if ( FHasAnyElements() )
        {
            _nMin = m_nMin;
            return true;
        }
        return false;
    }
    _tyImplType NMax() const
    {
        if ( !FHasAnyElements() )
            THROWNAMEDEXCEPTION("VebTreeFixed::NMax(): No elements in tree.");
        return m_nMax;
    }
    bool FHasMax( _tyImplType & _nMax ) const
    {
        if ( FHasAnyElements() )
        {
            _nMax = m_nMax;
            return true;
        }
        return false;
    }
    void Clear()
    {
        if ( FHasAnyElements() )
        {
            if ( !FHasOneElement() )
            {
                // Use the summary endpoints to minimize the loop iterations:
                _tySubtree * pstCur = &m_rgstSubtrees[ m_stSummary.NMin() ];
                _tySubtree * const pstEnd = ( &m_rgstSubtrees[ m_stSummary.NMax() ] ) + 1;
                for ( ; pstEnd != pstCur; ++pstCur )
                {
                    // It takes constant time to find out whether a subtree needs clearing so no need to recurse into the summary to find more details.
                    pstCur->Clear();
                }
                m_stSummary.Clear();
            }
            m_nMax = 0;
            m_nMin = 1;
        }
    }
    // Note: Insert() assumes that _x is not in the set. If _x may be in the set then use CheckInsert().
    void Insert( _tyImplType _x )
    {
        assert( _x < s_kstUniverse );
        assert( !FHasElement( _x ) );
        if ( !FHasAnyElements() )
            m_nMin = m_nMax = _x;
        else
        {
            if ( _x < m_nMin )
                std::swap( _x, m_nMin );
            _tyImplTypeSubtree nCluster = NCluster( _x );
            _tyImplTypeSubtree nEl = NElInCluster( _x );
            if ( !m_rgstSubtrees[nCluster].FHasAnyElements() )
                m_stSummary.Insert( nCluster );
            m_rgstSubtrees[nCluster].Insert( nEl );
            if ( _x > m_nMax )
                m_nMax = _x;
        }
    }
    // Return true if the element was inserted, false if it already existed.
    bool FCheckInsert( _tyImplType _x )
    {
        if ( FHasElement( _x ) )
            return false;
        Insert( _x );
        return true;
    }
    // Note: Delete() assumes that _x is in the set. If _x may not be in the set then use CheckDelete().
    void Delete( _tyImplType _x )
    {
        assert( FHasElement( _x ) );
        if ( m_nMin == m_nMax )
        {
            assert( _x == m_nMax );
            m_nMin = 1;            
            m_nMax = 0;            
        }
        else
        {
            if ( _x == m_nMin )
            {
                _tyImplTypeSummaryTree nFirstCluster = m_stSummary.NMin();
                _x = NIndex( FirstCluster, m_rgstSubtrees[ nFirstCluster ].NMin() );
                m_nMin = _x;
            }
            _tyImplTypeSubtree nCluster = NCluster( _x );
            _tyImplTypeSubtree nEl = NElInCluster( _x );
            m_rgstSubtrees[ nCluster ].Delete( nEl );
            if ( !m_rgstSubtrees[ nCluster ].FHasAnyElements() )
            {
                m_stSummary.Delete( nCluster );
                if ( _x == m_nMax )
                {
                    if ( !m_stSummary.FHasAnyElements() )
                    {
                        m_nMax = m_nMin;
                    }
                    else
                    {
                        _tyImplTypeSummaryTree nSummaryMax = m_stSummary.NMax();
                        m_nMax = NIndex( nSummaryMax, m_rgstSubtrees[ nSummaryMax ].NMax() );
                    }
                }
            }
            else
            if ( _x == m_nMax )
                m_nMax = NIndex( nCluster, m_rgstSubtrees[ nCluster ].NMax() );
        }
    }
    // Return true if the element was deleted, false if it already existed.
    bool FCheckDelete( _tyImplType _x )
    {
        if ( !FHasElement( _x ) )
            return false;
        Delete( _x );
        return true;
    }
    bool FHasElement( _tyImplType _x ) const
    {
        assert( _x < t_kstUniverse );
        if ( !FHasAnyElements() )
            return false;
        if ( ( _x == m_nMin ) || ( _x == m_nMax ) )
            return true;
        if ( m_nMin == m_nMax )
            return false;
        // Now check the sub-elements.
        return m_rgstSubtrees[ NCluster(_x) ].FHasElement( NElInCluster( _x ) );
    }
    // Return the next element after _x or 0 if there is no such element.
    _tyImplType NSuccessor( _tyImplType _x ) const
    {
        if ( FHasAnyElements() && ( _x < m_nMin ) )
            return m_nMin;
        
        _tyImplTypeSubtree nCluster = NCluster( _x );
        _tyImplTypeSubtree nEl = NElInCluster( _x );
        _tyImplTypeSubtree nMaxCluster;
        bool fMaxCluster = m_rgstSubtrees[nCluster].FHasMax( nMaxCluster );
        if ( fMaxCluster && ( nEl < nMaxCluster ) )
        {
            _tyImplTypeSubtree nOffsetSubtree = m_rgstSubtrees[nCluster].NSuccessor( nEl );
            assert( !!nOffsetSubtree );
            return NIndex( nCluster, nOffsetSubtree );
        }
        else
        {
            _tyImplTypeSummaryTree nSuccessiveCluster = m_stSummary.NSuccessor( nCluster );
            if ( !!nSuccessiveCluster )
            {
                _tyImplTypeSubtree nOffsetSubtree = m_rgstSubtrees[nSuccessiveCluster].NMin();
                return NIndex( nSuccessiveCluster, nOffsetSubtree );
            }
        }
        return 0; // No successor.
    }
    // Return the previous element before _x or t_kstUniverse-1 if there is no such element.
    _tyImplType NPredecessor( _tyImplType _x ) const
    {
        if ( FHasAnyElements() && ( _x > m_nMax ) )
            return m_nMax;
        
        _tyImplTypeSubtree nCluster = NCluster( _x );
        _tyImplTypeSubtree nEl = NElInCluster( _x );
        _tyImplTypeSubtree nMinCluster;
        bool fMinCluster = m_rgstSubtrees[nCluster].FHasMin( nMinCluster );
        if ( fMinCluster && ( nEl > nMinCluster ) )
        {
            _tyImplTypeSubtree nOffsetSubtree = m_rgstSubtrees[nCluster].NPredecessor( nEl );
            assert( s_kstitNoPredecessorSubtree != nOffsetSubtree ); // we should have found a predecessor.
            return NIndex( nCluster, nOffsetSubtree );
        }
        else
        {
            _tyImplTypeSummaryTree nPredecessiveCluster = m_stSummary.NPredecessor( nCluster );
            if ( s_kstitNoPredecessorSummaryTree ==  nPredecessiveCluster )
            {
                if ( FHasAnyElements() && ( _x > m_nMin ) )
                    return m_nMin;
            }
            else
            {
                _tyImplTypeSubtree nOffsetSubtree = m_rgstSubtrees[nPredecessiveCluster].NMax();
                return NIndex( nPredecessiveCluster, nOffsetSubtree );
            }
        }
        return s_kitNoPredecessor; // No predecessor.
    }
protected:
    _tyImplType m_nMin{1}; // Setting the min to be greater than the max is the indicator that there are no elements.
    _tyImplType m_nMax{0};
    _tySubtree m_rgstSubtrees[ s_kstUniverseSqrtLower ]; // The Subtrees.
    _tySummaryTree m_stSummary; // The summary tree.
};

// VebTreeVariable:
// This allows us to choose a "cluster VebTreeFixed<N>" appropriately for the approximate size of the VebTrees we are going to create.
template < size_t t_kstUniverseCluster, class t_tyAllocator = std::allocator< char > >
class VebTreeVariable
{
    typedef VebTreeVariable _tyThis;
public:
    // Choose impl type based on the range of values.
    static const size_t s_kstUniverse = t_kstUniverse;
    static const size_t s_kstUIntMax = n_VanEmdeBoasTreeImpl::KNextIntegerSize( t_kstUniverse );
    typedef n_VanEmdeBoasTreeImpl::t_tyMapToIntType< s_kstUIntMax > _tyImplType;
    static const _tyImplType s_kitNoPredecessor = t_kstUniverse - 1;
    static const size_t s_kstUniverseSqrtLower = n_VanEmdeBoasTreeImpl::LowerSqrt( t_kstUniverse );
    static const size_t s_kstUniverseSqrtUpper = n_VanEmdeBoasTreeImpl::UpperSqrt( t_kstUniverse );
    typedef VebTreeFixed< s_kstUniverseSqrtUpper > _tySubtree;
    typedef typename _tySubtree::_tyImplType _tyImplTypeSubtree;
    static const _tyImplTypeSubtree s_kstitNoPredecessorSubtree = s_kstUniverseSqrtUpper-1;
    typedef VebTreeFixed< s_kstUniverseSqrtLower > _tySummaryTree;
    typedef typename _tySummaryTree::_tyImplType _tyImplTypeSummaryTree;
    static const _tyImplTypeSummaryTree s_kstitNoPredecessorSummaryTree = s_kstUniverseSqrtLower-1;

    static _tyImplTypeSubtree NCluster( _tyImplType _x ) // high
    {
        return _tyImplTypeSubtree( _x / s_kstUniverseSqrtUpper );
    }
    static _tyImplTypeSubtree NElInCluster( _tyImplType _x ) // low
    {
        return _tyImplTypeSubtree( _x % s_kstUniverseSqrtUpper );
    }
    static _tyImplType NIndex( _tyImplTypeSubtree _x, _tyImplTypeSubtree _y )
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
    bool FHasMin( _tyImplType & _nMin ) const
    {
        if ( FHasAnyElements() )
        {
            _nMin = m_nMin;
            return true;
        }
        return false;
    }
    _tyImplType NMax() const
    {
        if ( !FHasAnyElements() )
            THROWNAMEDEXCEPTION("VebTreeFixed::NMax(): No elements in tree.");
        return m_nMax;
    }
    bool FHasMax( _tyImplType & _nMax ) const
    {
        if ( FHasAnyElements() )
        {
            _nMax = m_nMax;
            return true;
        }
        return false;
    }
    void Clear()
    {
        if ( FHasAnyElements() )
        {
            if ( !FHasOneElement() )
            {
                // Use the summary endpoints to minimize the loop iterations:
                _tySubtree * pstCur = &m_rgstSubtrees[ m_stSummary.NMin() ];
                _tySubtree * const pstEnd = ( &m_rgstSubtrees[ m_stSummary.NMax() ] ) + 1;
                for ( ; pstEnd != pstCur; ++pstCur )
                {
                    // It takes constant time to find out whether a subtree needs clearing so no need to recurse into the summary to find more details.
                    pstCur->Clear();
                }
                m_stSummary.Clear();
            }
            m_nMax = 0;
            m_nMin = 1;
        }
    }
    // Note: Insert() assumes that _x is not in the set. If _x may be in the set then use CheckInsert().
    void Insert( _tyImplType _x )
    {
        assert( _x < s_kstUniverse );
        assert( !FHasElement( _x ) );
        if ( !FHasAnyElements() )
            m_nMin = m_nMax = _x;
        else
        {
            if ( _x < m_nMin )
                std::swap( _x, m_nMin );
            _tyImplTypeSubtree nCluster = NCluster( _x );
            _tyImplTypeSubtree nEl = NElInCluster( _x );
            if ( !m_rgstSubtrees[nCluster].FHasAnyElements() )
                m_stSummary.Insert( nCluster );
            m_rgstSubtrees[nCluster].Insert( nEl );
            if ( _x > m_nMax )
                m_nMax = _x;
        }
    }
    // Return true if the element was inserted, false if it already existed.
    bool FCheckInsert( _tyImplType _x )
    {
        if ( FHasElement( _x ) )
            return false;
        Insert( _x );
        return true;
    }
    // Note: Delete() assumes that _x is in the set. If _x may not be in the set then use CheckDelete().
    void Delete( _tyImplType _x )
    {
        assert( FHasElement( _x ) );
        if ( m_nMin == m_nMax )
        {
            assert( _x == m_nMax );
            m_nMin = 1;            
            m_nMax = 0;            
        }
        else
        {
            if ( _x == m_nMin )
            {
                _tyImplTypeSummaryTree nFirstCluster = m_stSummary.NMin();
                _x = NIndex( FirstCluster, m_rgstSubtrees[ nFirstCluster ].NMin() );
                m_nMin = _x;
            }
            _tyImplTypeSubtree nCluster = NCluster( _x );
            _tyImplTypeSubtree nEl = NElInCluster( _x );
            m_rgstSubtrees[ nCluster ].Delete( nEl );
            if ( !m_rgstSubtrees[ nCluster ].FHasAnyElements() )
            {
                m_stSummary.Delete( nCluster );
                if ( _x == m_nMax )
                {
                    if ( !m_stSummary.FHasAnyElements() )
                    {
                        m_nMax = m_nMin;
                    }
                    else
                    {
                        _tyImplTypeSummaryTree nSummaryMax = m_stSummary.NMax();
                        m_nMax = NIndex( nSummaryMax, m_rgstSubtrees[ nSummaryMax ].NMax() );
                    }
                }
            }
            else
            if ( _x == m_nMax )
                m_nMax = NIndex( nCluster, m_rgstSubtrees[ nCluster ].NMax() );
        }
    }
    // Return true if the element was deleted, false if it already existed.
    bool FCheckDelete( _tyImplType _x )
    {
        if ( !FHasElement( _x ) )
            return false;
        Delete( _x );
        return true;
    }
    bool FHasElement( _tyImplType _x ) const
    {
        assert( _x < t_kstUniverse );
        if ( !FHasAnyElements() )
            return false;
        if ( ( _x == m_nMin ) || ( _x == m_nMax ) )
            return true;
        if ( m_nMin == m_nMax )
            return false;
        // Now check the sub-elements.
        return m_rgstSubtrees[ NCluster(_x) ].FHasElement( NElInCluster( _x ) );
    }
    // Return the next element after _x or 0 if there is no such element.
    _tyImplType NSuccessor( _tyImplType _x ) const
    {
        if ( FHasAnyElements() && ( _x < m_nMin ) )
            return m_nMin;
        
        _tyImplTypeSubtree nCluster = NCluster( _x );
        _tyImplTypeSubtree nEl = NElInCluster( _x );
        _tyImplTypeSubtree nMaxCluster;
        bool fMaxCluster = m_rgstSubtrees[nCluster].FHasMax( nMaxCluster );
        if ( fMaxCluster && ( nEl < nMaxCluster ) )
        {
            _tyImplTypeSubtree nOffsetSubtree = m_rgstSubtrees[nCluster].NSuccessor( nEl );
            assert( !!nOffsetSubtree );
            return NIndex( nCluster, nOffsetSubtree );
        }
        else
        {
            _tyImplTypeSummaryTree nSuccessiveCluster = m_stSummary.NSuccessor( nCluster );
            if ( !!nSuccessiveCluster )
            {
                _tyImplTypeSubtree nOffsetSubtree = m_rgstSubtrees[nSuccessiveCluster].NMin();
                return NIndex( nSuccessiveCluster, nOffsetSubtree );
            }
        }
        return 0; // No successor.
    }
    // Return the previous element before _x or t_kstUniverse-1 if there is no such element.
    _tyImplType NPredecessor( _tyImplType _x ) const
    {
        if ( FHasAnyElements() && ( _x > m_nMax ) )
            return m_nMax;
        
        _tyImplTypeSubtree nCluster = NCluster( _x );
        _tyImplTypeSubtree nEl = NElInCluster( _x );
        _tyImplTypeSubtree nMinCluster;
        bool fMinCluster = m_rgstSubtrees[nCluster].FHasMin( nMinCluster );
        if ( fMinCluster && ( nEl > nMinCluster ) )
        {
            _tyImplTypeSubtree nOffsetSubtree = m_rgstSubtrees[nCluster].NPredecessor( nEl );
            assert( s_kstitNoPredecessorSubtree != nOffsetSubtree ); // we should have found a predecessor.
            return NIndex( nCluster, nOffsetSubtree );
        }
        else
        {
            _tyImplTypeSummaryTree nPredecessiveCluster = m_stSummary.NPredecessor( nCluster );
            if ( s_kstitNoPredecessorSummaryTree ==  nPredecessiveCluster )
            {
                if ( FHasAnyElements() && ( _x > m_nMin ) )
                    return m_nMin;
            }
            else
            {
                _tyImplTypeSubtree nOffsetSubtree = m_rgstSubtrees[nPredecessiveCluster].NMax();
                return NIndex( nPredecessiveCluster, nOffsetSubtree );
            }
        }
        return s_kitNoPredecessor; // No predecessor.
    }
protected:
    _tyImplType m_nMin{1}; // Setting the min to be greater than the max is the indicator that there are no elements.
    _tyImplType m_nMax{0};
    _tySubtree m_rgstSubtrees[ s_kstUniverseSqrtLower ]; // The Subtrees.
    _tySummaryTree m_stSummary; // The summary tree.
};