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
#include "_aloctrt.h"
#include "_namdexc.h"
#include "_bitutil.h"

__BIENUTIL_BEGIN_NAMESPACE

namespace n_VanEmdeBoasTreeImpl
{
    constexpr size_t KNextIntegerSize( size_t _st )
    {
        --_st;
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
    struct MapToIntType
    {
        typedef uint8_t _tyType;
    };
    template <>
    struct MapToIntType< USHRT_MAX >
    {
        typedef uint16_t _tyType;
    };
    template <>
    struct MapToIntType< UINT_MAX >
    {
        typedef uint32_t _tyType;
    };
    template <>
    struct MapToIntType< ULLONG_MAX >
    {
        typedef uint64_t _tyType;
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
    constexpr t_tyUint LowerSqrt( const t_tyUint _ui )
    {
        if ( !FIsPow2( _ui ) )
            throw std::logic_error("LowerSqrt(): _ui is not a power of two.");
        // Note that this will return 1 as the sqrt of 2. This is reasonably by design since 1 is the closest thing to the sqrt of 2 that is an integer.
        return ( t_tyUint(1) << ( Log2( _ui ) / 2 ) );
    }
    // The "upper square root" is 2^((k+1)/2) for 2^k when k is odd.
    template < class t_tyUint >
    constexpr t_tyUint UpperSqrt( const t_tyUint _ui )
    {
        if ( !FIsPow2( _ui ) )
            throw std::logic_error("UpperSqrt(): _ui is not a power of two.");
        // Note that this will return 1 as the sqrt of 2. Probably we would return 2 but it doesn't matter for the Van Emde Boas tree impl because we should never call this with 2.
        t_tyUint nLg2 = Log2( _ui );
        return ( t_tyUint(1) << ( ( nLg2 / 2 ) + ( nLg2 % 2 ) ) );
    }

    // Note that when converting to Windows we need to use _BitScanForward/Reverse for the intrinsics.
    template < class t_tyInt >
    t_tyInt Clz( t_tyInt _n )
    {
        static_assert( sizeof(_n) <= sizeof( uint32_t ) ); // Make sure we are only getting here for 32bit and less.
        // In the base we just cast:
        return __builtin_clz( uint32_t( _n ) ) - ( sizeof( uint32_t ) - sizeof( _n ) );
    }
    template <>
    uint64_t Clz< uint64_t >( uint64_t _n )
    {
        static_assert( sizeof(_n) == sizeof( uint64_t ) );
        return __builtin_clzl( _n );
    }
    template <>
    int64_t Clz< int64_t >( int64_t _n )
    {
        static_assert( sizeof(_n) == sizeof( int64_t ) );
        return __builtin_clzl( (int64_t)_n );
    }
    template < class t_tyInt >
    t_tyInt Ctz( t_tyInt _n )
    {
        static_assert( sizeof(_n) <= sizeof( uint32_t ) ); // Make sure we are only getting here for 32bit and less.
        // In the base we just cast:
        return __builtin_ctz( uint32_t( _n ) );
    }
    template <>
    uint64_t Ctz< uint64_t >( uint64_t _n )
    {
        static_assert( sizeof(_n) == sizeof( uint64_t ) );
        return __builtin_ctzl( _n );
    }
    template <>
    int64_t Ctz< int64_t >( int64_t _n )
    {
        static_assert( sizeof(_n) == sizeof( int64_t ) );
        return __builtin_ctzl( (int64_t)_n );
    }
}

// _VebFixedBase:
// This is a base class to implement various specializations of VebTreeFixed<>.
// Using this enables us to implement a specialization VebTreeFixed<256> that is a pure bitmask.
// This makes the VebTree almost as small as a bitvector of the same length - and hence potentially useful.
template < class t_tyUint, size_t t_kstNUints, size_t t_kstUniverse = sizeof( t_tyUint ) * CHAR_BIT * t_kstNUints >
class _VebFixedBase
{
    typedef _VebFixedBase _tyThis;
public:
    static constexpr size_t s_kstNUints = t_kstNUints;
    static constexpr size_t s_kstUniverse = t_kstUniverse;
    // We still accept and return arguments based upon the size of the universe:
    static constexpr size_t s_kstUIntMax = n_VanEmdeBoasTreeImpl::KNextIntegerSize( t_kstUniverse );
    typedef n_VanEmdeBoasTreeImpl::t_tyMapToIntType< s_kstUIntMax > _tyImplType;
    static constexpr _tyImplType s_kitNoPredecessor = t_kstUniverse - 1;
    typedef t_tyUint _tyUint;
    static const size_t s_kstNBitsUint = sizeof( _tyUint ) * CHAR_BIT;
    
    _VebFixedBase() = default;
    // We don't support construction with a universe, but we do support an Init() call for genericity.
    void Init( size_t _stUniverse )
    {
        assert( _stUniverse <= s_kstUniverse );
    }
    void _Deinit()
    {
    }
    // No reason not to allow copy construction:
    _VebFixedBase( _VebFixedBase const & ) = default;
    _tyThis & operator = ( _tyThis const & ) = default;
    // Need to override these to do them in the *expected* manner - i.e. leaving the data structure in at least a consistent state.
    // As VebTreeWrap leaves its objects empty we need to leave all the VebTreeFixed objects empty after any move operation.
    _VebFixedBase( _VebFixedBase && _rr )
    {
        memcpy( m_rgUint, _rr.m_rgUint, sizeof m_rgUint );
        memset( &_rr.m_rgUint, 0, sizeof m_rgUint );
    }
    _tyThis operator = ( _VebFixedBase && _rr )
    {
        memcpy( m_rgUint, _rr.m_rgUint, sizeof m_rgUint );
        memset( &_rr.m_rgUint, 0, sizeof m_rgUint );
        return *this;
    }
    bool operator == ( _tyThis const & ) const = default;
    ~_VebFixedBase() = default;
    void swap( _tyThis & _r )
    {
        t_tyUint rgUint[ t_kstNUints ];
        memcpy( rgUint, m_rgUint, sizeof m_rgUint );
        memcpy( m_rgUint, _r.m_rgUint, sizeof m_rgUint );
        memcpy( _r.m_rgUint, rgUint, sizeof m_rgUint );
    }

    bool FEmpty( bool = false ) const
    {
        return !FHasAnyElements();
    }
    bool FHasAnyElements() const
    {
        const _tyUint * pnCur = m_rgUint;
        const _tyUint * const pnEnd = m_rgUint + s_kstNUints;
        for ( ; pnEnd != pnCur; ++pnCur )
            if ( !!*pnCur )
                return true;
        return false;
    }
    bool FHasOneElement() const
    {
        // A rarely used method but we will implement it.
        const _tyUint * pnNonZero = nullptr;
        const _tyUint * pnCur = m_rgUint;
        const _tyUint * const pnEnd = m_rgUint + s_kstNUints;
        for ( ; pnEnd != pnCur; ++pnCur )
        {
            if ( !!*pnCur )
            {
                if ( pnNonZero )
                    return false;
                else
                    pnNonZero = pnCur;
            }
        }
        if ( fHasNonZero )
            return !( *pnNonZero & ( *pnNonZero - 1) );
        return false;
    }
    _tyImplType NMin() const
    {
        _tyImplType nMin;
        if ( !FHasMin( nMin ) )
            THROWNAMEDEXCEPTION("VebTreeFixed::NMin(): No elements in tree.");
        return nMin;
    }
    bool FHasMin( _tyImplType & _nMin ) const
    {
        const _tyUint * pnCur = m_rgUint;
        const _tyUint * const pnEnd = m_rgUint + s_kstNUints;
        for ( ; pnEnd != pnCur; ++pnCur )
        {
            if ( !!*pnCur )
            {
                _nMin = n_VanEmdeBoasTreeImpl::Ctz( *pnCur ) + ( pnCur - m_rgUint ) * s_kstNBitsUint;
                return true;
            }
        }
        return false;
    }
    _tyImplType NMax() const
    {
        _tyImplType nMax;
        if ( !FHasMax( nMax ) )
            THROWNAMEDEXCEPTION("VebTreeFixed::NMax(): No elements in tree.");
        return nMax;
    }
    bool FHasMax( _tyImplType & _nMax ) const
    {
        const _tyUint * pnCur = m_rgUint + s_kstNUints;
        for ( ; m_rgUint != pnCur--; )
        {
            if ( !!*pnCur )
            {
                _nMax = s_kstNBitsUint - 1 - n_VanEmdeBoasTreeImpl::Clz( *pnCur ) + ( pnCur - m_rgUint ) * s_kstNBitsUint;
                return true;
            }
        }
        return false;
    }
    void Clear()
    {
        memset( m_rgUint, 0, sizeof( m_rgUint ) );
    }
    // In this method we must clear and set all bits appropriately according to the call parameters:
    // 1) Must clear all bits up until _pnFirstInsert if present.
    // 2) Must set all bits up until _pnLastElement if present but needn't touch any bits beyond that because they shouldn't have ever been modified.
    void InsertAll( const _tyImplType _pnFirstInsert = nullptr, const _tyImplType * _pnLastElement = nullptr )
    {
        assert( !_pnFirstInsert || !!*_pnFirstInsert );
        assert( !_pnLastElement || ( *_pnLastElement != s_kstUniverse-1 ) ); // Shouldn't be passing _pnLastElement if it is meaningless.
        assert(  !_pnFirstInsert || !_pnLastElement || ( *_pnFirstInsert <= *_pnLastElement ) );
        size_t stFirstInsert = _pnFirstInsert ? *_pnFirstInsert : 0;
        size_t stLastElement;
        if ( _pnLastElement )
            stLastElement = *_pnLastElement;

        _tyUint * pnCurThis = m_rgUint;
        _tyUint * const pnEndThis = m_rgUint + ( _pnLastElement ? ( ( stLastElement / s_kstNBitsUint ) + 1 ) : s_kstNUints );
        size_t stBitCur = 0;
        for ( ; pnEndThis != pnCurThis; ++pnCurThis, stBitCur += s_kstNBitsUint )
        {
            size_t stBitEnd = stBitCur + s_kstNBitsUint;
            if ( stFirstInsert )
            {
                if ( stBitEnd <= stFirstInsert )
                    *pnCurThis = 0;
                else
                {
                    *pnCurThis = ~( ( 1 << stFirstInsert ) - 1 );
                    stFirstInsert = 0; // No longer need to potentially process.
                }
            }
            else
                *pnCurThis = numeric_limits< _tyUint >::max();
        }
        // Now update the last processed element by removing any bits beyond the last element:
        if ( _pnLastElement && !!( ( ( stLastElement + 1 ) % s_kstNBitsUint ) ) )
            pnCurThis[-1] &= ( 1 << ( stLastElement + 1 ) ) - 1;
    }
    void Insert( _tyImplType _x )
    {
        assert( _x < s_kstUniverse );
        _tyUint * pn = &m_rgUint[ _x / s_kstNBitsUint ];
        assert( !( *pn & ( _tyUint(1) << ( _x % s_kstNBitsUint ) ) ) ); // We shouldn't be doing this.
        *pn |= ( _tyUint(1) << ( _x % s_kstNBitsUint ) );
    }
    // Return true if the element was inserted, false if it already existed.
    bool FCheckInsert( _tyImplType _x )
    {
        if ( FHasElement( _x ) )
            return false;
        Insert( _x );
        return true;
    }
    void Delete( _tyImplType _x )
    {
        assert( _x < s_kstUniverse );
        _tyUint * pn = &m_rgUint[ _x / s_kstNBitsUint ];
        assert( !!( *pn & ( _tyUint(1) << ( _x % s_kstNBitsUint ) ) ) ); // We shouldn't be doing this.
        *pn &= ~( _tyUint(1) << ( _x % s_kstNBitsUint ) );
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
        const _tyUint * pn = &m_rgUint[ _x / s_kstNBitsUint ];
        return *pn & ( _tyUint(1) << ( _x % s_kstNBitsUint ) );
    }
    // Return the next element after _x or 0 if there is no such element.
    _tyImplType NSuccessor( _tyImplType _x ) const
    {
        assert( _x < s_kstUniverse );
        if ( _x >= s_kstUniverse-1 )
            return 0;
        ++_x;
        const _tyUint * pn = &m_rgUint[ _x / s_kstNBitsUint ];
        _x %= s_kstNBitsUint;
        _tyUint nMasked = *pn & ~( ( _tyUint(1) << _x ) - 1 );
        if ( !nMasked )
        {
            // Must search remaining elements if any:
            const _tyUint * const pnEnd = m_rgUint + s_kstNUints;
            for ( ; pnEnd != ++pn; )
            {
                if ( *pn )
                    return n_VanEmdeBoasTreeImpl::Ctz( *pn ) + ( ( pn - m_rgUint ) * s_kstNBitsUint );
            }
            return 0;
        }
        else
        {
            // The first bit in nMasked is the successor.
            return n_VanEmdeBoasTreeImpl::Ctz( nMasked ) + ( ( pn - m_rgUint ) * s_kstNBitsUint );
        }
    }
    // Return the previous element before _x or s_kstUniverse-1 if there is no such element.
    _tyImplType NPredecessor( _tyImplType _x ) const
    {
        assert( _x < s_kstUniverse );
        if ( !_x )
            return s_kitNoPredecessor;
        --_x;
        const _tyUint * pn = &m_rgUint[ _x / s_kstNBitsUint ];
        _x %= s_kstNBitsUint;
        _tyUint nMasked = *pn;
        if ( ( _x + 1 ) % s_kstNBitsUint )
            nMasked &= ( ( _tyUint(1) << ( _x + 1 ) ) - 1 );
        if ( !nMasked )
        {
            // Must search remaining elements if any:
            for ( ; m_rgUint != pn--; )
            {
                if ( *pn )
                    return s_kstNBitsUint - 1 - n_VanEmdeBoasTreeImpl::Clz( *pn ) + ( ( pn - m_rgUint ) * s_kstNBitsUint );
            }
            return s_kitNoPredecessor;
        }
        else
        {
            // The first bit in nMasked is the successor.
            return s_kstNBitsUint - 1 - n_VanEmdeBoasTreeImpl::Clz( nMasked ) + ( ( pn - m_rgUint ) * s_kstNBitsUint );
        }
    }

// Allow bitwise operations as we can manage them.
    _tyThis & operator |= ( _tyThis const & _r )
    {
        _tyUint * pnCurThis = m_rgUint;
        _tyUint * const pnEndThis = m_rgUint + s_kstNUints;
        _tyUint * pnCurThat = _r.m_rgUint;
        for ( ; pnEndThis != pnCurThis; ++pnCurThis, ++pnCurThat )
            *pnCurThis |= *pnCurThat;
        return *this;
    }
    _tyThis & operator &= ( _tyThis const & _r )
    {
        _tyUint * pnCurThis = m_rgUint;
        _tyUint * const pnEndThis = m_rgUint + s_kstNUints;
        _tyUint * pnCurThat = _r.m_rgUint;
        for ( ; pnEndThis != pnCurThis; ++pnCurThis, ++pnCurThat )
            *pnCurThis &= *pnCurThat;
        return *this;
    }
    _tyThis & operator ^= ( _tyThis const & _r )
    {
        _tyUint * pnCurThis = m_rgUint;
        _tyUint * const pnEndThis = m_rgUint + s_kstNUints;
        _tyUint * pnCurThat = _r.m_rgUint;
        for ( ; pnEndThis != pnCurThis; ++pnCurThis, ++pnCurThat )
            *pnCurThis ^= *pnCurThat;
        return *this;
    }
    _tyThis & BitwiseInvert( _tyImplType * _pnLastElement = nullptr )
    {
        size_t stLastElement;
        if ( _pnLastElement )
            stLastElement = *_pnLastElement;
        _tyUint * pnCurThis = m_rgUint;
        _tyUint * const pnEndThis = m_rgUint + ( _pnLastElement ? ( ( stLastElement / s_kstNBitsUint ) + 1 ) : s_kstNUints );
        for ( ; pnEndThis != pnCurThis; ++pnCurThis )
            *pnCurThis = ~*pnCurThis;
        if ( _pnLastElement && !!( ( ( stLastElement + 1 ) % s_kstNBitsUint ) ) )
            pnCurThis[-1] &= ( 1 << ( stLastElement + 1 ) ) - 1;        
        return *this;
    }

protected:
    t_tyUint m_rgUint[ t_kstNUints ]{};
};

template < size_t t_kstUniverse >
class VebTreeFixed;

// For some scenarios we will need a specialized VebTreeFixed< 2 >.
// This version is likely a bit more optimal than _VebFixedBase<> so we will use it.
// I like the impl but I have contemplated changing it to be just 2 simple on/off bits.
// The current impl uses each bit as the value that is present there.
// There are advantages to the current impl I believe vs. a traditional bitmask approach.
template <>
class VebTreeFixed< 2 >
{
    typedef VebTreeFixed _tyThis;
public:
    static const size_t s_kstUniverse = 2;
    typedef uint8_t _tyImplType;
    static const _tyImplType s_kitNoPredecessor = s_kstUniverse - 1;

    VebTreeFixed() = default;
    // We don't support construction with a universe, but we do support an Init() call for genericity.
    void Init( size_t _stUniverse )
    {
        assert( _stUniverse <= s_kstUniverse );
    }
    void _Deinit()
    {
    }
    // No reason not to allow copy construction:
    VebTreeFixed( VebTreeFixed const & ) = default;
    _tyThis & operator = ( _tyThis const & ) = default;
    // Need to override these to do them in the *expected* manner - i.e. leaving the data structure in at least a consistent state.
    // As VebTreeWrap leaves its objects empty we need to leave all the VebTreeFixed objects empty after any move operation.
    VebTreeFixed( VebTreeFixed && _rr )
        : m_byVebTree2( _rr.m_byVebTree2 )
    {
        _rr.m_byVebTree2 = 1;
    }
    _tyThis operator = ( VebTreeFixed && _rr )
    {
        m_byVebTree2 = _rr.m_byVebTree2;
        _rr.m_byVebTree2 = 1;
        return *this;
    }
    bool operator == ( _tyThis const & ) const = default;
    ~VebTreeFixed() = default;
    void swap( _tyThis & _r )
    {
        std::swap( m_byVebTree2, _r.m_byVebTree2 );
    }

    bool FEmpty( bool = false ) const
    {
        return !FHasAnyElements();
    }
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
    void InsertAll()
    {
        m_byVebTree2 = 0b10;
    }
    // Note: Insert() assumes that _x is not in the set. If _x may be in the set then use CheckInsert().
    void Insert( _tyImplType _x )
    {
        assert( _x < s_kstUniverse );
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
        assert( _x < s_kstUniverse );
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
    // Return the next element after _x or 0 if there is no such element.
    _tyImplType NSuccessor( _tyImplType _x ) const
    {
        assert( _x < s_kstUniverse );
        return !_x && ( 0b10 & m_byVebTree2 );
    }
    // Return the previous element before _x or s_kstUniverse-1 if there is no such element.
    _tyImplType NPredecessor( _tyImplType _x ) const
    {
        assert( _x < s_kstUniverse );
        return !( _x && !( 0b01 & m_byVebTree2 ) );
    }

// Allow bitwise operations as we can manage them.
    _tyThis & operator |= ( _tyThis const & _r )
    {
        switch( _r.m_byVebTree2 )
        {
            case 0b01: // nil
            break; // Don't modify it.
            case 0b00:
                m_byVebTree2 &= 0b10;
            break;
            case 0b11:
                m_byVebTree2 |= 0b10;
            break;
            case 0b10: // Both.
                m_byVebTree2 = 0b10;
            break;
        }
        return *this;
    }
    _tyThis & operator &= ( _tyThis const & _r )
    {
        switch( _r.m_byVebTree2 )
        {
            case 0b01: // nil
                m_byVebTree2 = 0b01;
            break;
            case 0b00:
                m_byVebTree2 &= 0b01;
            break;
            case 0b11:
                m_byVebTree2 |= 0b01;
            break;
            case 0b10: // Both.
            break; // Don't modify it.
        }
        return *this;
    }
    _tyThis & operator ^= ( _tyThis const & _r )
    {
        if ( 0b01 != m_byVebTree2 )
        {
            if ( m_byVebTree2 == _r.m_byVebTree2 )
                m_byVebTree2 = 0b01; // nil.
            else
            switch( _r.m_byVebTree2 )
            {
                case 0b01: // nil
                break; // no change
                case 0b00:
                    switch( m_byVebTree2 )
                    {
                        case 0b11:
                            m_byVebTree2 = 0b10;
                        break;
                        case 0b10:
                            m_byVebTree2 = 0b11;
                        break;
                    }
                break;
                case 0b11:
                    switch( m_byVebTree2 )
                    {
                        case 0b00:
                            m_byVebTree2 = 0b10;
                        break;
                        case 0b10:
                            m_byVebTree2 = 0b00;
                        break;
                    }
                break;
                case 0b10: // Both.
                    switch( m_byVebTree2 )
                    {
                        case 0b00:
                            m_byVebTree2 = 0b11;
                        break;
                        case 0b11:
                            m_byVebTree2 = 0b00;
                        break;
                    }
                break;
            }
        }
        return *this;
    }
    _tyThis & BitwiseInvert()
    {
        switch( m_byVebTree2 )
        {
            case 0b01: // nil
                m_byVebTree2 = 0b10;
            break;
            case 0b00:
                m_byVebTree2 = 0b11;
            break;
            case 0b11:
                m_byVebTree2 = 0b00;
            break;
            case 0b10: // Both.
                m_byVebTree2 = 0b01; // nil
            break;
        }
        return *this;
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

// Use the _VebFixedBase for all specializations because it should be the smallest possible impl.
#define VEBTREE_USEFIXEDBASE 

#ifndef VEBTREE_USEFIXEDBASE 
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

    VebTreeFixed() = default;
    // We don't support construction with a universe, but we do support an Init() call for genericity.
    void Init( size_t _stUniverse )
    {
        assert( _stUniverse <= s_kstUniverse );
    }
    void _Deinit()
    {
    }
    // No reason not to allow copy construction:
    VebTreeFixed( VebTreeFixed const & ) = default;
    _tyThis & operator = ( _tyThis const & ) = default;
    // Need to override these to do them in the *expected* manner - i.e. leaving the data structure in at least a consistent state.
    // As VebTreeWrap leaves its objects empty we need to leave all the VebTreeFixed objects empty after any move operation.
    VebTreeFixed( VebTreeFixed && _rr )
    {
        std::swap( m_byVebTree4, _rr.m_byVebTree4 );
        std::swap( m_byVebTree2s, _rr.m_byVebTree2s );
    }
    _tyThis operator = ( _VebFixedBase && _rr )
    {
        m_byVebTree4 = _rr.m_byVebTree4;
        m_byVebTree2s = _rr.m_byVebTree2s;
        _rr.m_byVebTree4 =0;
        _rr.m_byVebTree2s =0x15;
        return *this;
    }
    bool operator == ( _tyThis const & ) const = default;
    ~VebTreeFixed() = default;
    void swap( _tyThis & _r )
    {
        std::swap( m_byVebTree4, _r.m_byVebTree4 );
        std::swap( m_byVebTree2s, _r.m_byVebTree2s );
    }

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

    bool FEmpty( bool = false ) const
    {
        return !FHasAnyElements() && ( m_byVebTree2s == 0x15 );
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
            default:
                assert( false );
                return false;
            break;
        }
    }
    // Return the next element after _x or 0 if there is no such element.
    _tyImplType NSuccessor( _tyImplType _x ) const
    {
        assert( _x < s_kstUniverse );
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
    // Return the previous element before _x or s_kstUniverse-1 if there is no such element.
    _tyImplType NPredecessor( _tyImplType _x ) const
    {
        assert( _x < s_kstUniverse );
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
    void _SetVeb2( _tyImplType _nCluster, _tyImplType _n )
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
    static const uint8_t s_kgrfHasMinMax = ( _tyUint(1) << ( CHAR_BIT - 1 ) ); // Use a bit for this because it is available and easiest.
    uint8_t m_byVebTree4{0}; // The (max,min) of Veb<4>. Max mask: 0xc, Min mask: 0x3. s_kgrfHasMinMax: 0x80.
    uint8_t m_byVebTree2s{0x15}; // The three Veb<2>s associated with this Veb<4>. (summary: 0x30, most significant:0xc, least significant:0x3). 0x15 is all nil.
};
#else // VEBTREE_USEFIXEDBASE
template <>
class VebTreeFixed< 4 > : public _VebFixedBase< uint8_t, 1, 4 >
{
    typedef _VebFixedBase< uint8_t, 1, 4 > _tyBase;
    typedef VebTreeFixed _tyThis;
public:
    using _tyBase::_tyBase;
    using _tyBase::Init;
    using _tyBase::_Deinit;;
    using _tyBase::operator =;
    using _tyBase::operator ==;
    using _tyBase::swap;
    using _tyBase::FEmpty;
    using _tyBase::FHasAnyElements;
    using _tyBase::FHasOneElement;
    using _tyBase::NMin;
    using _tyBase::FHasMin;
    using _tyBase::NMax;
    using _tyBase::FHasMax;
    using _tyBase::Clear;
    using _tyBase::Insert;
    using _tyBase::FCheckInsert;
    using _tyBase::Delete;
    using _tyBase::FCheckDelete;
    using _tyBase::FHasElement;
    using _tyBase::NSuccessor;
    using _tyBase::NPredecessor;
    using _tyBase::operator |=;
};

template <>
class VebTreeFixed< 8 > : public _VebFixedBase< uint8_t, 1 >
{
    typedef _VebFixedBase< uint8_t, 1 > _tyBase;
    typedef VebTreeFixed _tyThis;
public:
    using _tyBase::_tyBase;
    using _tyBase::Init;
    using _tyBase::_Deinit;
    using _tyBase::operator =;
    using _tyBase::operator ==;
    using _tyBase::swap;
    using _tyBase::FEmpty;
    using _tyBase::FHasAnyElements;
    using _tyBase::FHasOneElement;
    using _tyBase::NMin;
    using _tyBase::FHasMin;
    using _tyBase::NMax;
    using _tyBase::FHasMax;
    using _tyBase::Clear;
    using _tyBase::Insert;
    using _tyBase::FCheckInsert;
    using _tyBase::Delete;
    using _tyBase::FCheckDelete;
    using _tyBase::FHasElement;
    using _tyBase::NSuccessor;
    using _tyBase::NPredecessor;
    using _tyBase::operator |=;
};

template <>
class VebTreeFixed< 16 > : public _VebFixedBase< uint16_t, 1 >
{
    typedef _VebFixedBase< uint16_t, 1 > _tyBase;
    typedef VebTreeFixed _tyThis;
public:
    using _tyBase::_tyBase;
    using _tyBase::Init;
    using _tyBase::_Deinit;
    using _tyBase::operator =;
    using _tyBase::operator ==;
    using _tyBase::swap;
    using _tyBase::FEmpty;
    using _tyBase::FHasAnyElements;
    using _tyBase::FHasOneElement;
    using _tyBase::NMin;
    using _tyBase::FHasMin;
    using _tyBase::NMax;
    using _tyBase::FHasMax;
    using _tyBase::Clear;
    using _tyBase::Insert;
    using _tyBase::FCheckInsert;
    using _tyBase::Delete;
    using _tyBase::FCheckDelete;
    using _tyBase::FHasElement;
    using _tyBase::NSuccessor;
    using _tyBase::NPredecessor;
    using _tyBase::operator |=;
};

template <>
class VebTreeFixed< 32 > : public _VebFixedBase< uint32_t, 1 >
{
    typedef _VebFixedBase< uint32_t, 1 > _tyBase;
    typedef VebTreeFixed _tyThis;
public:
    using _tyBase::_tyBase;
    using _tyBase::Init;
    using _tyBase::_Deinit;
    using _tyBase::operator =;
    using _tyBase::operator ==;
    using _tyBase::swap;
    using _tyBase::FEmpty;
    using _tyBase::FHasAnyElements;
    using _tyBase::FHasOneElement;
    using _tyBase::NMin;
    using _tyBase::FHasMin;
    using _tyBase::NMax;
    using _tyBase::FHasMax;
    using _tyBase::Clear;
    using _tyBase::Insert;
    using _tyBase::FCheckInsert;
    using _tyBase::Delete;
    using _tyBase::FCheckDelete;
    using _tyBase::FHasElement;
    using _tyBase::NSuccessor;
    using _tyBase::NPredecessor;
    using _tyBase::operator |=;
};

template <>
class VebTreeFixed< 64 > : public _VebFixedBase< uint64_t, 1 >
{
    typedef _VebFixedBase< uint64_t, 1 > _tyBase;
    typedef VebTreeFixed _tyThis;
public:
    using _tyBase::_tyBase;
    using _tyBase::Init;
    using _tyBase::_Deinit;
    using _tyBase::operator =;
    using _tyBase::operator ==;
    using _tyBase::swap;
    using _tyBase::FEmpty;
    using _tyBase::FHasAnyElements;
    using _tyBase::FHasOneElement;
    using _tyBase::NMin;
    using _tyBase::FHasMin;
    using _tyBase::NMax;
    using _tyBase::FHasMax;
    using _tyBase::Clear;
    using _tyBase::Insert;
    using _tyBase::FCheckInsert;
    using _tyBase::Delete;
    using _tyBase::FCheckDelete;
    using _tyBase::FHasElement;
    using _tyBase::NSuccessor;
    using _tyBase::NPredecessor;
    using _tyBase::operator |=;
};

template <>
class VebTreeFixed< 128 > : public _VebFixedBase< uint64_t, 2 >
{
    typedef _VebFixedBase< uint64_t, 2 > _tyBase;
    typedef VebTreeFixed _tyThis;
public:
    using _tyBase::_tyBase;
    using _tyBase::Init;
    using _tyBase::_Deinit;
    using _tyBase::operator =;
    using _tyBase::operator ==;
    using _tyBase::swap;
    using _tyBase::FEmpty;
    using _tyBase::FHasAnyElements;
    using _tyBase::FHasOneElement;
    using _tyBase::NMin;
    using _tyBase::FHasMin;
    using _tyBase::NMax;
    using _tyBase::FHasMax;
    using _tyBase::Clear;
    using _tyBase::Insert;
    using _tyBase::FCheckInsert;
    using _tyBase::Delete;
    using _tyBase::FCheckDelete;
    using _tyBase::FHasElement;
    using _tyBase::NSuccessor;
    using _tyBase::NPredecessor;
    using _tyBase::operator |=;
};

template <>
class VebTreeFixed< 256 > : public _VebFixedBase< uint64_t, 4 >
{
    typedef _VebFixedBase< uint64_t, 4 > _tyBase;
    typedef VebTreeFixed _tyThis;
public:
    using _tyBase::_tyBase;
    using _tyBase::Init;
    using _tyBase::_Deinit;
    using _tyBase::operator =;
    using _tyBase::operator ==;
    using _tyBase::swap;
    using _tyBase::FEmpty;
    using _tyBase::FHasAnyElements;
    using _tyBase::FHasOneElement;
    using _tyBase::NMin;
    using _tyBase::FHasMin;
    using _tyBase::NMax;
    using _tyBase::FHasMax;
    using _tyBase::Clear;
    using _tyBase::Insert;
    using _tyBase::FCheckInsert;
    using _tyBase::Delete;
    using _tyBase::FCheckDelete;
    using _tyBase::FHasElement;
    using _tyBase::NSuccessor;
    using _tyBase::NPredecessor;
    using _tyBase::operator |=;
};

// And that should be good. We can use VebTreeFixed< 256 > as the base cluster summary type and then use VebTreeFixed< 65536 > for the cluster type
//  and we should be able to efficiently hold millions of elements.

#endif // VEBTREE_USEFIXEDBASE

// VebTreeFixed:
// We will choose to have LowerSqrt(t_kstUniverse) trees of VebTreeFixed< UpperSqrt(t_kstUniverse) >.
template < size_t t_kstUniverse >
class VebTreeFixed
{
    typedef VebTreeFixed _tyThis;
    static_assert( n_VanEmdeBoasTreeImpl::FIsPow2( t_kstUniverse ) ); // always power of 2.
public:
    // Choose impl type based on the range of values.
    static constexpr size_t s_kstUniverse = t_kstUniverse;
    static constexpr size_t s_kstUIntMax = n_VanEmdeBoasTreeImpl::KNextIntegerSize( t_kstUniverse );
    typedef n_VanEmdeBoasTreeImpl::t_tyMapToIntType< s_kstUIntMax > _tyImplType;
    static constexpr _tyImplType s_kitNoPredecessor = t_kstUniverse - 1;
    static constexpr size_t s_kstUniverseSqrtLower = n_VanEmdeBoasTreeImpl::LowerSqrt( t_kstUniverse );
    static constexpr size_t s_kstUniverseSqrtUpper = n_VanEmdeBoasTreeImpl::UpperSqrt( t_kstUniverse );
    typedef VebTreeFixed< s_kstUniverseSqrtUpper > _tySubtree;
    typedef typename _tySubtree::_tyImplType _tyImplTypeSubtree;
    static constexpr _tyImplTypeSubtree s_kstitNoPredecessorSubtree = s_kstUniverseSqrtUpper-1;
    typedef VebTreeFixed< s_kstUniverseSqrtLower > _tySummaryTree;
    typedef typename _tySummaryTree::_tyImplType _tyImplTypeSummaryTree;
    static constexpr _tyImplTypeSummaryTree s_kstitNoPredecessorSummaryTree = s_kstUniverseSqrtLower-1;

    VebTreeFixed() = default;
    // We don't support construction with a universe, but we do support an Init() call for genericity.
    void Init( size_t _stUniverse )
    {
        assert( _stUniverse <= s_kstUniverse );
    }
    void _Deinit()
    {
    }
    // No reason not to allow copy construction:
    VebTreeFixed( VebTreeFixed const & ) = default;
    _tyThis & operator = ( _tyThis const & ) = default;
    // Need to override these to do them in the *expected* manner - i.e. leaving the data structure in at least a consistent state.
    VebTreeFixed( VebTreeFixed && _rr )
        : m_stSummary( std::move( _rr.m_stSummary ) )
    {
        m_nMin = _rr.m_nMin;
        m_nMax = _rr.m_nMax;
        _rr._SetEmptyMinMax();
        memcpy( m_rgstSubtrees, _rr.m_rgstSubtrees, sizeof m_rgstSubtrees );
        // memset(...) - we can't just memset here because we don't know how our cluster is implemented.
        // We could either move each cluster object or call clear on each cluster object now.
        _tySubtree * pstCur = &_rr.m_rgstSubtrees[ m_stSummary.NMin() ];
        _tySubtree * const pstEnd = ( &_rr.m_rgstSubtrees[ m_stSummary.NMax() ] ) + 1;
        for ( ; pstEnd != pstCur; ++pstCur )
            pstCur->Clear();
    }
    _tyThis & operator = ( _tyThis && _rr )
    {
        m_stSummary = std::move( _rr.m_stSummary );
        m_nMin = _rr.m_nMin;
        m_nMax = _rr.m_nMax;
        _rr.SetEmptyMinMax();
        memcpy( m_rgstSubtrees, _rr.m_rgstSubtrees, sizeof m_rgstSubtrees );
        // memset(...) - we can't just memset here because we don't know how our cluster is implemented.
        // We could either move each cluster object or call clear on each cluster object now.
        _tySubtree * pstCur = &_rr.m_rgstSubtrees[ m_stSummary.NMin() ];
        _tySubtree * const pstEnd = ( &_rr.m_rgstSubtrees[ m_stSummary.NMax() ] ) + 1;
        for ( ; pstEnd != pstCur; ++pstCur )
            pstCur->Clear();
    }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wambiguous-reversed-operator"
    bool operator == ( _tyThis const & ) const = default;
#pragma GCC diagnostic pop
    ~VebTreeFixed() = default;
    void swap( _tyThis & _r )
    {
        { //B
            uint8_t rgbyBuffer( sizeof( m_rgstSubtrees ) );
            memcpy( rgbyBuffer, m_rgstSubtrees, sizeof m_rgstSubtrees );
            memcpy( m_rgstSubtrees, _r.m_rgstSubtrees, sizeof m_rgstSubtrees );
            memcpy( _r.m_rgstSubtrees, rgbyBuffer, sizeof m_rgstSubtrees );
        } //EB
        m_stSummary.swap( _r.m_stSummary );
        std::swap( m_nMin, _r.m_nMin );
        std::swap( m_nMax, _r.m_nMax );
    }

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

    bool FEmpty( bool _fRecurse = false ) const
    {
        if ( FHasAnyElements() )
            return false;
        if ( !_fRecurse )
            return true;
        const _tySubtree * pstCur = m_rgstSubtrees;
        const _tySubtree * const pstEnd = m_rgstSubtrees + s_kstUniverseSqrtLower;
        for ( ; pstEnd != pstCur; ++pstCur )
        {
            if ( !pstCur->FEmpty( true ) )
                return false;
        }
        return m_stSummary.FEmpty( true );
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
                // Use the summary elements to minimize the number of subelements that we touch:
                _tyImplTypeSummaryTree nClusterCur = m_stSummary.NMin();
                do
                    m_rgstSubtrees[ nClusterCur ].Clear();
                while(  ( nClusterCur = m_stSummary.NSuccessor( nClusterCur ) ) );
                m_stSummary.Clear();
            }
            _SetEmptyMinMax();
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
            _SetEmptyMinMax();
        }
        else
        {
            if ( _x == m_nMin )
            {
                _tyImplTypeSummaryTree nFirstCluster = m_stSummary.NMin();
                _x = NIndex( nFirstCluster, m_rgstSubtrees[ nFirstCluster ].NMin() );
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
        assert( _x < t_kstUniverse );
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
        assert( _x < t_kstUniverse );
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

// Allow bitwise operations as we can manage them.
    _tyThis & operator |= ( _tyThis const & _r )
    {
        if ( !_r.FHasAnyElements() )
            return *this; // nop
        // Process the min:
        if ( !FHasElement( _r.NMin() ) )
            Insert( _r.NMin() ); // We know that the minimum for this cluster is correct now and we processed the minimum of the other cluster.
        if ( _r.FHasOneElement() )
            return *this; // all done.
        if ( _r.NMax() > m_nMax )
            m_nMax = _r.NMax();

        // Now call each sub-object of interest - which is all clusters of _r that contain any data at all.
        // To do this quickly we should use the summary info of _r which will let us know populated cluster afap.
        _tyImplTypeSummaryTree nClusterCur = _r.m_stSummary.NMin();
        do
        {
            const _tySubTree & rstThat = _r.m_rgstSubtrees[ nClusterCur ];
            assert( rstThat.FHasAnyElements() );
            _tySubTree & rstThis = m_rgstSubtrees[ nClusterCur ];
            if ( !rstThis.FHasAnyElements() )
                m_stSummary.Insert( nClusterCur ); // Update the summary.
            rstThis |= rstThat; // do the deed.
        }
        while( nClusterCur = _r.m_stSummary.NSuccessor( nClusterCur ) );
        return *this;
    }
    // This is more difficult than the |= because it may change the minumum from below.
    _tyThis & operator &= ( _tyThis const & _r )
    {
        // Boundary conditions:
        if ( !_r.FHasAnyElements() )
        {
            Clear();
            return *this;
        }
        if ( _r.FHasOneElement() )
        {
            bool fHasEl = FHasElement( _r.NMin() );
            Clear();
            if ( fHasEl )
                Insert( _r.NMin() );
            return *this;
        }
        if ( !FHasAnyElements() )
            return *this;
        if ( FHasOneElement() )
        {
            if ( !_r.FHasElement( NMin() ) )
                Clear();
            return *this;
        }

        // Don't change the minimum for now because we don't have a specific candidate to set it to.
        // We move through successive clusters in each set. When a cluster is not present in _r then we
        //  Clear() this's cluster and remove it from the summary.
        // The cluster will merely set its (min,max appropriately) and we will act appropriately after that
        //  i.e. either removing its minimum if it is our minimum and then checking for emptiness, etc.
        size_t stMinExisting = _r.FHasElement( NMin() ) ? size_t( NMin() ) : s_kstUniverse;
        size_t stMinCur = stMinExisting;
        bool fFoundMinCur = false; // We can know when we have found new minimum and not test anymore.
        _tyImplType nMaxCur = _r.FHasElement( NMax() ) ? NMax() : ( ( s_kstUniverse != stMinExisting ) ? NMin() : 0 );

        // Now call each sub-object of interest - which is all clusters of *this that contain any data at all.
        // To do this quickly we should use the summary info of _r which will let us know populated cluster afap.
        _tyImplTypeSummaryTree nClusterCur = m_stSummary.NMin();
        do
        {
            const _tySubTree & rstThat = _r.m_rgstSubtrees[ nClusterCur ];
            _tySubTree & rstThis = m_rgstSubtrees[ nClusterCur ];
            rstThis &= rstThat; // do the deed.
            if ( !rstThis.FHasAnyElements() )
                m_stSummary.Delete( nClusterCur ); // Update the summary.
            else
            {
                if ( !fFoundMinCur )
                {
                    fFoundMinCur = true; // Need only test this once.
                    _tyImplType nMinTest = NIndex( nClusterCur, rstThis.NMin() );
                    if ( nMinTest < stMinCur ) )
                    {
                        // We know we have found the "ultimate minimum" at this point because we will only see greater elements from here on out:
                        stMinCur = nMinTest;
                        assert( stMinCur != stMinExisting ); // stMinExisting is not in the cluster so it cannot be the min.
                        rstThis.Delete( rstThis.NMin() ); // We own this minimum now since we will store it in this object.
                        if ( !rstThis.FHasAnyElements() )
                        {
                            m_stSummary.Delete( nClusterCur ); // Update the summary.
                            continue; // No reason to test the max below.
                        }
                    }
                }
                _tyImplType nMaxTest = NIndex( nClusterCur, rstThis.NMax() );
                if ( nMaxTest > nMaxCur )
                    nMaxCur = nMaxTest;
            }
        }
        while( nClusterCur = m_stSummary.NSuccessor( nClusterCur ) );
        // Now deal with the found min/max:
        if ( stMinCur == s_kstUniverse )
        {
            assert( 0 == nMaxCur );
            assert( !m_stSummary.FHasAnyElements() );
            _SetEmptyMinMax();
        }
        else
        {
            assert( ( s_kstUniverse == stMinExisting ) || ( stMinCur > stMinExisting ) );
            m_nMin = (_tyImplType)stMinCur;
            m_nMax = nMaxCur;
            if ( s_kstUniverse != stMinExisting )
                Insert( (_tyImplType)stMinExisting );
            assert( ( m_nMin != m_nMax ) || !m_stSummary.FHasAnyElements() );
        }
        return *this;
    }
    _tyThis & operator ^= ( _tyThis const & _r )
    {
        if ( !_r.FHasAnyElements() )
            return *this; // nop
        
        bool fHasAnyElements = FHasAnyElements();
        // We don't want to process _r's minimum now because otherwise we will have to process it again eventually anyway.
        bool fHasMinThat = fHasAnyElements && FHasElement( _r.NMin() );
        if ( _r.FHasOneElement() )
        {
            if ( !fHasMinThat )
                Insert( _r.NMin() ); // We know that the minimum for this cluster is correct now and we processed the minimum of the other cluster.
            else
                Delete( _r.NMin() );
            return *this;
        }
//#error HERE.
        // No need to process _r's max at this time because it may change below.
        _tyImplType nMinExisting = m_nMin; // Must hold on to this so we can remove/insert it below.
        size_t stMinCur = s_kstUniverse;
        m_nMax = 0;
        m_nMin = s_kstUniverse-1; // We will at least encounter nMinExisting as an element after inversion since it cannot be in the clusters.

        // Now call each sub-object of interest - which is all clusters of _r that contain any data at all.
        // To do this quickly we should use the summary info of _r which will let us know populated cluster afap.
        _tyImplTypeSummaryTree nClusterCur = _r.m_stSummary.NMin();
        do
        {
            const _tySubTree & rstThat = _r.m_rgstSubtrees[ nClusterCur ];
            assert( rstThat.FHasAnyElements() );
            _tySubTree & rstThis = m_rgstSubtrees[ nClusterCur ];
            if ( !rstThis.FHasAnyElements() )
                m_stSummary.Insert( nClusterCur ); // Update the summary.
            rstThis |= rstThat; // do the deed.
        }
        while( nClusterCur = _r.m_stSummary.NSuccessor( nClusterCur ) );
        return *this;
    }

    void InsertAll( const _tyImplType _pnFirstInsert = nullptr, const _tyImplType * _pnLastElement = nullptr )
    {
        assert( !_pnFirstInsert || !!*_pnFirstInsert );
        assert( !_pnLastElement || ( *_pnLastElement != s_kstUniverse-1 ) ); // Shouldn't be passing _pnLastElement if it is meaningless.
        assert(  !_pnFirstInsert || !_pnLastElement || ( *_pnFirstInsert <= *_pnLastElement ) );

        // Algorithm:
        // Set m_nMin to 0, set m_nMax to m_nLastElement.
        // For first cluster call InsertAll with (min,min) to skip inserting the minth element.
        m_nMin = !_pnFirstInsert ? 0 : *_pnFirstInsert;
        m_nMax = !_pnLastElement ? s_kstUniverse-1 : *_pnLastElement;
        if ( m_nMin != m_nMax )
        {
            _tyImplTypeSubtree nFirstInsert = m_nMin + 1; // Don't need to worry about this going over a cluster boundary due to impl.
            size_t stClustersProcess = _pnLastElement ? ( size_t( NCluster( *_pnLastElement ) ) + 1 ) : STClusters();
            _tyImplTypeSubtree nLastElementSubtree;
            _tyImplTypeSubtree * pnLastElementSubtree = nullptr;
            if ( _pnLastElement && !!( NElInCluster( *_pnLastElement + 1 ) ) )
            {
                nLastElementSubtree = NElInCluster( *_pnLastElement );
                pnLastElementSubtree = nLastElementSubtree;
            }
            _tySubtree * pstCur = &m_rgstSubtrees[0];
            _tySubtree * const pstEnd = pstCur + stClustersProcess;
            pstCur->InsertAll( &nFirstInsert, ( 1 == stClustersProcess ) ? pnLastElementSubtree : 0 );
            for ( ++pstCur; pstEnd != pstCur; ++pstCur )
                pstCur->InsertAll( 0, ( pstEnd-1 == pstCur ) ? pnLastElementSubtree : 0 );
            // Now we must do a batch insert into the summary because we don't know its current state - we must use InsertAll().
            _tyImplTypeSummaryTree nLastElementSummary;
            _tyImplTypeSummaryTree * pnLastElementSummary = nullptr;
            if ( stClustersProcess != STClusters() )
            {
                nLastElementSummary = stClustersProcess - 1;
                pnLastElementSummary = nLastElementSummary;
            }
            m_stSummary.InsertAll( nullptr, pnLastElementSummary );
        }
    }
    // Bitwise inversion.
    // If _pnLastElement then we will limit application to elements below that limit.
    _tyThis & BitwiseInvert( const _tyImplType * _pnLastElement = nullptr )
    {
        // Boundary conditions:
        if ( !FHasAnyElements() )
        {
            InsertAll( nullptr, _pnLastElement );
            return *this;
        }
        if ( FHasOneElement() )
        {
            _tyImplType nEl = NMin();
            InsertAll( nullptr, _pnLastElement );
            Delete( nEl );
            return *this;
        }
        // We know that the current min cannot be in the result and we need to make sure to delete the current min from
        // its cluster because it will be incorrectly set in that cluster.
        _tyImplType nMinExisting = m_nMin; // Must hold on to this so we can remove it below.
        bool fFoundMin = false;
        m_nMax = 0;
        m_nMin = s_kstUniverse-1; // We will at least encounter nMinExisting as an element after inversion since it cannot be in the clusters.
        size_t stClustersProcess = _pnLastElement ? ( size_t( NCluster( *_pnLastElement ) ) + 1 ) : STClusters();
        _tyImplTypeSubtree nLastElementSubtree;
        _tyImplTypeSubtree * pnLastElementSubtree = nullptr;
        if ( _pnLastElement && !!( NElInCluster( *_pnLastElement + 1 ) ) )
        {
            nLastElementSubtree = NElInCluster( *_pnLastElement );
            pnLastElementSubtree = nLastElementSubtree;
        }
        // Move through all clusters of the object since all clusters will be modified.
        _tySubtree * pstCur = &m_rgstSubtrees[0];
        _tySubtree * const pstEnd = pstCur + stClustersProcess;
        for ( _tyImplTypeSummaryTree nClusterCur = 0; pstEnd != pstCur; ++pstCur, ++nClusterCur )
        {
            bool fInSummary = pstCur->FHasAnyElements();
            pstCur->BitwiseInvert( ( pstEnd-1 == pstCur ) ? pnLastElementSubtree : 0 );
            if ( !fInSummary )
                m_stSummary.Insert( nClusterCur );
            _tyImplTypeSubtree nMaxCluster;
            if ( pstCur->FHasMax( nMaxCluster ) )
            {
                _tyImplType nMaxTest = NIndex( nClusterCur, nMaxCluster );
                if ( nMaxTest > m_nMax )
                    m_nMax = nMaxTest;
                if ( !fFoundMin )
                {
                    fFoundMin = true;
                    m_nMin = NIndex( nClusterCur, pstCur->NMin() ); // This is the resultant minimum - and this container owns it now.
                    pstCur->Delete( pstCur->NMin() )
                    if ( !pstCur->FHasAnyElements() )
                        m_stSummary.Delete( nClusterCur );
                }
            }
            else
            {
                assert( fInSummary );
                m_stSummary.Delete( nClusterCur );
            }
        }
        // m_nMin and m_nMax are already updated, remove the old m_nMin because it will be set in the cluster.
        assert( m_nMin <= nMinExisting );
        Delete( nMinExisting );
        return *this;
    }
protected:
    void _SetEmptyMinMax()
    {
        m_nMin = t_kstUniverse-1;
        m_nMax = 0;
    }
    _tyImplType m_nMin{t_kstUniverse-1}; // Setting the min to be greater than the max is the indicator that there are no elements.
    _tyImplType m_nMax{0};
    _tySubtree m_rgstSubtrees[ s_kstUniverseSqrtLower ]; // The Subtrees.
    _tySummaryTree m_stSummary; // The summary tree.
};

// VebTreeWrap:
// This wraps a variable size set of VebTreeFixed clusters. The constituent cluster sizes must be chosen to accomodate the potential universe required.
// This allows us to choose a "cluster VebTreeFixed<N>" appropriately for the approximate size of the VebTrees we are going to create.
// Note that the max number of elements is ( t_kstUniverseCluster * t_kstUniverseCluster ). If that number is exceeded we will throw.
// We conserve memory by only create the leftmost N clusters needed to hold the number of actual elements in the set.
// If one cascades the SummaryTree to also be of type VebTreeWrap (and its summary to be VebTreeFixed<>) then we can efficiently hold
//  billions of elements.
template < size_t t_kstUniverseCluster, class t_tySummaryClass = VebTreeFixed< t_kstUniverseCluster >, class t_tyAllocator = std::allocator< char > >
class VebTreeWrap
{
    typedef VebTreeWrap _tyThis;
    static_assert( n_VanEmdeBoasTreeImpl::FIsPow2( t_kstUniverseCluster ) ); // always power of 2.
public:
    typedef t_tyAllocator _tyAllocator;
    // Choose impl type based on the range of values.
    static constexpr size_t s_kstUniverse = t_kstUniverseCluster * t_kstUniverseCluster;
    static constexpr size_t s_kstUIntMax = n_VanEmdeBoasTreeImpl::KNextIntegerSize( s_kstUniverse );
    typedef n_VanEmdeBoasTreeImpl::t_tyMapToIntType< s_kstUIntMax > _tyImplType;
    static constexpr _tyImplType s_kitNoPredecessor = s_kstUniverse - 1;
    static constexpr size_t s_kstUniverseCluster = t_kstUniverseCluster;
    typedef VebTreeFixed< t_kstUniverseCluster > _tySubtree; // We are composed of fixed size clusters.
    typedef typename _tySubtree::_tyImplType _tyImplTypeSubtree;
    static constexpr _tyImplTypeSubtree s_kstitNoPredecessorSubtree = t_kstUniverseCluster-1;
    static constexpr size_t s_kstUniverseSummary = t_tySummaryClass::s_kstUniverse;
    typedef t_tySummaryClass _tySummaryTree;
    typedef typename _tySummaryTree::_tyImplType _tyImplTypeSummaryTree;
    static constexpr _tyImplTypeSummaryTree s_kstitNoPredecessorSummaryTree = s_kstUniverseSummary-1;

    VebTreeWrap() = default;
    VebTreeWrap( t_tyAllocator const & _rAlloc )
        : m_rgstSubtrees( _rAlloc )
    {        
    }
    // Allow a contructor that passes in the size to allow for genericity with VebTreeWrap<>.
    VebTreeWrap( size_t _stNElements, _tyAllocator const & _rAlloc = _tyAllocator() )
        : m_rgstSubtrees( _rAlloc )
    {
        Init( _stNElements );
    }
    void Init( size_t _stNElements )
    {
        if ( m_rgstSubtrees.size() )
            _Deinit(); // We could do a lot better here but it takes a bit of work. I.e. we could keep the existing blocks and Clear() them.
        if ( _stNElements > s_kstUniverse )
            THROWNAMEDEXCEPTION( "VebTreeWrap::Init(): _stNElements[%lu] is greater than the allowable universe size[%lu].", _stNElements, s_kstUniverse );
        size_t nClusters = ( (_stNElements-1) / t_kstUniverseCluster ) + 1;
        _tyRgSubtrees rgstSubtrees( m_rgstSubtrees.get_allocator() ); // must pass custody of instanced allocator.
        rgstSubtrees.reserve( nClusters ); // Not sure if this does what we want, but pretty sure it won't hurt.
        rgstSubtrees.resize( nClusters ); // May throw.
        // Now that we have allocated everything for this, we allow for the summary to also be dynamic:
        m_stSummary.Init( nClusters );
        m_rgstSubtrees.swap( rgstSubtrees );
        m_nLastElement = _tyImplType(_stNElements-1 );
    }
    // No reason not to allow copy construction:
    VebTreeWrap( VebTreeWrap const & ) = default;
    _tyThis & operator = ( _tyThis const & ) = default;
    VebTreeWrap( VebTreeWrap && _rr )
        :   m_rgstSubtrees( std::move( _rr.m_rgstSubtrees ) ),
            m_stSummary( std::move( _rr.m_stSummary ) )
    {
        m_nLastElement = _rr.m_nLastElement; // No need to update _rr.m_nLastElement to anything in particular.
        m_nMin = _rr.m_nMin;
        m_nMax = _rr.m_nMax;
        _rr._SetEmptyMinMax();
    }
    _tyThis & operator = ( _tyThis && _rr )
    {
        m_rgstSubtrees = std::move( _rr.m_rgstSubtrees );
        m_stSummary = std::move( _rr.m_stSummary );
        m_nLastElement = _rr.m_nLastElement; // No need to update _rr.m_nLastElement to anything in particular.
        m_nMin = _rr.m_nMin;
        m_nMax = _rr.m_nMax;
        _rr._SetEmptyMinMax();
        return *this;
    }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wambiguous-reversed-operator"
    bool operator == ( _tyThis const & ) const = default;
#pragma GCC diagnostic pop
    ~VebTreeWrap() = default;
    void _Deinit()
    {
        // m_nLastElement = 0; only valid when m_rgstSubtrees has elements.
        m_rgstSubtrees.clear();
        m_stSummary._Deinit();
        _SetEmptyMinMax();
    }
    void swap( _tyThis & _r )
    {
        m_stSummary.swap( _r.m_stSummary );
        m_rgstSubtrees.swap( _r.m_rgstSubtrees );
        std::swap( m_nMin, _r.m_nMin );
        std::swap( m_nMax, _r.m_nMax );
        std::swap( m_nLastElement, _r.m_nLastElement );
    }

    // This returns the maximum number of distinct elements for this VebTree.
    static size_t NUniverse()
    {
        return s_kstUniverse;
    }
    // Returns the size for this set of element - with which it was constucted or initialized.
    size_t NSize() const
    {
        return m_nLastElement + 1;
    }

    static _tyImplTypeSubtree NCluster( _tyImplType _x ) // high
    {
        return _tyImplTypeSubtree( _x / t_kstUniverseCluster );
    }
    static _tyImplTypeSubtree NElInCluster( _tyImplType _x ) // low
    {
        return _tyImplTypeSubtree( _x % t_kstUniverseCluster );
    }
    static _tyImplType NIndex( _tyImplTypeSubtree _x, _tyImplTypeSubtree _y )
    {
        return ( _x * t_kstUniverseCluster ) + _y;
    }

    bool FEmpty( bool _fRecurse = false ) const
    {
        if ( FHasAnyElements() )
            return false;
        if ( !_fRecurse )
            return true;
        const _tySubtree * pstCur = &m_rgstSubtrees[0];
        const _tySubtree * const pstEnd = pstCur + STClusters();
        for ( ; pstEnd != pstCur; ++pstCur )
        {
            if ( !pstCur->FEmpty( true ) )
                return false;
        }
        return m_stSummary.FEmpty( true );
    }
    bool FHasAnyElements() const
    {
        return m_nMax >= m_nMin;
    }
    bool FHasOneElement() const
    {
        return m_nMin == m_nMax;
    }
    size_t STClusters() const
    {
        return m_rgstSubtrees.size();
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
                // Use the summary elements to minimize the number of subelements that we touch:
                _tyImplTypeSummaryTree nClusterCur = m_stSummary.NMin();
                do
                    m_rgstSubtrees[ nClusterCur ].Clear();
                while( ( nClusterCur = m_stSummary.NSuccessor( nClusterCur ) ) );
                m_stSummary.Clear();
            }
            _SetEmptyMinMax();
        }
    }
    // Note: Insert() assumes that _x is not in the set. If _x may be in the set then use CheckInsert().
    void Insert( _tyImplType _x )
    {
        if ( _x > m_nLastElement )
            THROWNAMEDEXCEPTION( "VebTreeWrap::Insert(): _x[%lu] is greater than m_nLastElement[%lu].", size_t(_x), size_t(m_nLastElement) );
        assert( !FHasElement( _x ) ); // We don't want to check this and throw because we want the caller to use CheckInsert() instead.
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
        if ( _x > m_nLastElement )
            THROWNAMEDEXCEPTION( "VebTreeWrap::Delete(): _x[%lu] is greater than m_nLastElement[%lu].", size_t(_x), size_t(m_nLastElement) );
        assert( FHasElement( _x ) );
        if ( m_nMin == m_nMax )
        {
            assert( _x == m_nMax );
            _SetEmptyMinMax();
        }
        else
        {
            if ( _x == m_nMin )
            {
                _tyImplTypeSummaryTree nFirstCluster = m_stSummary.NMin();
                _x = NIndex( nFirstCluster, m_rgstSubtrees[ nFirstCluster ].NMin() );
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
        if ( _x > m_nLastElement )
            THROWNAMEDEXCEPTION( "VebTreeWrap::FHasElement(): _x[%lu] is greater than m_nLastElement[%lu].", size_t(_x), size_t(m_nLastElement) );
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
        if ( _x > m_nLastElement )
            THROWNAMEDEXCEPTION( "VebTreeWrap::NSuccessor(): _x[%lu] is greater than m_nLastElement[%lu].", size_t(_x), size_t(m_nLastElement) );
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
    // Return the previous element before _x or s_kstUniverse-1 if there is no such element.
    _tyImplType NPredecessor( _tyImplType _x ) const
    {
        if ( _x > m_nLastElement )
            THROWNAMEDEXCEPTION( "VebTreeWrap::NSuccessor(): _x[%lu] is greater than m_nLastElement[%lu].", size_t(_x), size_t(m_nLastElement) );
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
            if ( s_kstitNoPredecessorSummaryTree == nPredecessiveCluster )
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

// Allow bitwise operations as we can manage them.
    _tyThis & operator |= ( _tyThis const & _r )
    {
        // Currently we only allow oring between sets of the same size:
        if ( NSize() != _r.NSize() )
            THROWNAMEDEXCEPTION("VebTreeFixed::operator |=(): NSize()[%ul] doesn't match _r.NSize()[%ul].", NSize(), _r.NSize() );
        if ( !_r.FHasAnyElements() )
            return *this; // nop
        // Process the min:
        if ( !FHasElement( _r.NMin() ) )
           Insert( _r.NMin() ); // We know that the minimum for this cluster is correct now and we processed the minimum of the other cluster.
        if ( _r.FHasOneElement() )
            return *this; // all done.
        if ( _r.NMax() > m_nMax )
            m_nMax = _r.NMax();

        // Now call each sub-object of interest - which is all clusters of _r that contain any data at all.
        // To do this quickly we should use the summary info of _r which will let us know populated cluster afap.
        _tyImplTypeSummaryTree nClusterCur = _r.m_stSummary.NMin();
        do
        {
            const _tySubTree & rstThat = _r.m_rgstSubtrees[ nClusterCur ];
            assert( rstThat.FHasAnyElements() );
            _tySubTree & rstThis = m_rgstSubtrees[ nClusterCur ];
            if ( !rstThis.FHasAnyElements() )
                m_stSummary.Insert( nClusterCur ); // Update the summary.
            rstThis |= rstThat; // do the deed.
        }
        while( nClusterCur = _r.m_stSummary.NSuccessor( nClusterCur ) );
        return *this;
    }
    // This is more difficult than the |= because it may change the minumum from below.
    _tyThis & operator &= ( _tyThis const & _r )
    {
        // Currently we only allow anding between sets of the same size:
        if ( NSize() != _r.NSize() )
            THROWNAMEDEXCEPTION("VebTreeFixed::operator &=(): NSize()[%ul] doesn't match _r.NSize()[%ul].", NSize(), _r.NSize() );
        
        // Boundary conditions:
        if ( !_r.FHasAnyElements() )
        {
            Clear();
            return *this;
        }
        if ( _r.FHasOneElement() )
        {
            bool fHasEl = FHasElement( _r.NMin() );
            Clear();
            if ( fHasEl )
                Insert( _r.NMin() );
            return *this;
        }
        if ( !FHasAnyElements() )
            return *this;
        if ( FHasOneElement() )
        {
            if ( !_r.FHasElement( NMin() ) )
                Clear();
            return *this;
        }

        // Don't change the minimum for now because we don't have a specific candidate to set it to.
        // We move through successive clusters in each set. When a cluster is not present in _r then we
        //  Clear() this's cluster and remove it from the summary.
        // The cluster will merely set its (min,max appropriately) and we will act appropriately after that
        //  i.e. either removing its minimum if it is our minimum and then checking for emptiness, etc.
        size_t stMinExisting = _r.FHasElement( NMin() ) ? size_t( NMin() ) : s_kstUniverse;
        size_t stMinCur = ( s_kstUniverse != stMinExisting ) ? stMinExisting : NSize();
        bool fFoundMinCur = false; // We can know when we have found new minimum and not test anymore.
        _tyImplType nMaxCur = _r.FHasElement( NMax() ) ? NMax() : ( ( s_kstUniverse != stMinExisting ) ? NMin() : 0 );

        // Now call each sub-object of interest - which is all clusters of *this that contain any data at all.
        // To do this quickly we should use the summary info of _r which will let us know populated cluster afap.
        _tyImplTypeSummaryTree nClusterCur = m_stSummary.NMin();
        do
        {
            const _tySubTree & rstThat = _r.m_rgstSubtrees[ nClusterCur ];
            _tySubTree & rstThis = m_rgstSubtrees[ nClusterCur ];
            rstThis &= rstThat; // do the deed.
            if ( !rstThis.FHasAnyElements() )
                m_stSummary.Delete( nClusterCur ); // Update the summary.
            else
            {
                if ( !fFoundMinCur )
                {
                    fFoundMinCur = true; // Need only test this once.
                    _tyImplType nMinTest = NIndex( nClusterCur, rstThis.NMin() );
                    if ( nMinTest < stMinCur ) )
                    {
                        // We know we have found the "ultimate minimum" at this point because we will only see greater elements from here on out:
                        stMinCur = nMinTest;
                        rstThis.Delete( rstThis.NMin() ); // We own this minimum now since we will store it in this object.
                        if ( !rstThis.FHasAnyElements() )
                        {
                            m_stSummary.Delete( nClusterCur ); // Update the summary.
                            continue; // No reason to test the max below.
                        }
                    }
                }
                _tyImplType nMaxTest = NIndex( nClusterCur, rstThis.NMax() );
                if ( nMaxTest > nMaxCur )
                    nMaxCur = nMaxTest;
            }
        }
        while( nClusterCur = m_stSummary.NSuccessor( nClusterCur ) );
        // Now deal with the found min/max:
        if ( stMinCur == NSize() )
        {
            assert( 0 == nMaxCur );
            assert( !m_stSummary.FHasAnyElements() );
            _SetEmptyMinMax();
        }
        else
        {
            assert( ( s_kstUniverse == stMinExisting ) || ( stMinCur > stMinExisting ) );
            m_nMin = (_tyImplType)stMinCur;
            m_nMax = nMaxCur;
            if ( s_kstUniverse != stMinExisting )
                Insert( (_tyImplType)stMinExisting );
            assert( ( m_nMin != m_nMax ) || !m_stSummary.FHasAnyElements() );
        }
        return *this;
    }
    void InsertAll()
    {
        // Algorithm:
        // Set m_nMin to 0, set m_nMax to m_nLastElement.
        // For first cluster call InsertAll with (min,min) to skip inserting the minth element.
        m_nMin = 0;
        m_nMax = m_nLastElement;
        if ( m_nLastElement > m_nMin )   // boundary.
        {
            _tyImplTypeSubtree nFirstInsert = 1;
            _tyImplTypeSubtree nLastElementSubtree;
            _tyImplTypeSubtree * pnLastElementSubtree = 0;
            if ( !!( NSize() % t_kstUniverseCluster ) ) // Unless we end at a cluster boundary...
            {
                nLastElementSubtree = NElInCluster( m_nLastElement );
                pnLastElementSubtree = &nLastElementSubtree;
            }
            _tySubtree * pstCur = &m_rgstSubtrees[0];
            _tySubtree * const pstEnd = pstCur + STClusters();
            pstCur->InsertAll( &nFirstInsert, ( 1 == STClusters() ) ? pnLastElementSubtree : 0 );
            for ( ++pstCur; pstEnd != pstCur; ++pstCur )
                pstCur->InsertAll( 0, ( pstEnd-1 == pstCur ) ? pnLastElementSubtree : 0 );
                
            // Now we must do a batch insert into the summary because we don't know its current state - we must use InsertAll().
            _tyImplTypeSummaryTree nLastElementSummary;
            _tyImplTypeSummaryTree * pnLastElementSummary = nullptr;
            if ( t_kstUniverseCluster != STClusters() )
            {
                nLastElementSummary = STClusters() - 1;
                pnLastElementSummary = nLastElementSummary;
            }
            m_stSummary.InsertAll( nullptr, pnLastElementSummary );
        }
    }
    // Bitwise inversion.
    _tyThis & BitwiseInvert()
    {
        // Boundary conditions:
        if ( !FHasAnyElements() )
        {
            InsertAll();
            return *this;
        }
        if ( FHasOneElement() )
        {
            _tyImplType nEl = NMin();
            InsertAll();
            Delete( nEl );
            return *this;
        }
        // We know that the current min cannot be in the result and we need to make sure to delete the current min from
        // its cluster because it will be incorrectly set in that cluster.
        _tyImplType nMinExisting = m_nMin; // Must hold on to this so we can remove it below.
        bool fFoundMin = false;
        m_nMax = 0;
        m_nMin = NSize()-1; // We will at least encounter nMinExisting as an element after inversion since it cannot be in the clusters.
        _tyImplTypeSubtree nLastElementSubtree;
        const _tyImplTypeSubtree * pnLastElementSubtree = nullptr;
        if ( NSize() % t_kstUniverseCluster )
        {
            nLastElementSubtree = NElInCluster( m_nLastElement );
            pnLastElementSubtree = &nLastElementSubtree;
        }
        // Move through all clusters of the object since all clusters will be modified.
        _tySubtree * pstCur = &m_rgstSubtrees[0];
        _tySubtree * const pstEnd = pstCur + STClusters();
        for ( _tyImplTypeSummaryTree nClusterCur = 0; pstEnd != pstCur; ++pstCur, ++nClusterCur )
        {
            bool fInSummary = pstCur->FHasAnyElements();
            pstCur->BitwiseInvert( ( pstEnd-1 == pstCur ) ? pnLastElementSubtree : 0  ); // Pass in any limit on the last cluster to avoid setting bits beyond the end.
            if ( !fInSummary )
                m_stSummary.Insert( nClusterCur );
            _tyImplTypeSubtree nMaxCluster;
            if ( pstCur->FHasMax( nMaxCluster ) )
            {
                _tyImplType nMaxTest = NIndex( nClusterCur, nMaxCluster );
                if ( nMaxTest > m_nMax )
                    m_nMax = nMaxTest;
                if ( !fFoundMin )
                {
                    fFoundMin = true;
                    m_nMin = NIndex( nClusterCur, pstCur->NMin() ); // This is the resultant minimum - and this container owns it now.
                    pstCur->Delete( pstCur->NMin() )
                    if ( !pstCur->FHasAnyElements() )
                        m_stSummary.Delete( nClusterCur );
                }
            }
            else
            {
                assert( fInSummary );
                m_stSummary.Delete( nClusterCur );
            }
        }
        // m_nMin and m_nMax are already updated, remove the old m_nMin because it will be set in the cluster.
        assert( m_nMin <= nMinExisting );
        Delete( nMinExisting ); // This works even if m_nMin == nMinExisting.
        return *this;
    }

protected:
    void _SetEmptyMinMax()
    {
        m_nMin = s_kstUniverse-1;
        m_nMax = 0;
    }
    typedef typename _Alloc_traits< _tySubtree, t_tyAllocator >::allocator_type _tyAllocSubtree;
    typedef vector< _tySubtree, _tyAllocSubtree > _tyRgSubtrees;
    _tyRgSubtrees m_rgstSubtrees;
    _tySummaryTree m_stSummary;
    _tyImplType m_nLastElement; // Only valid when m_rgstSubtrees.size() > 0. No reason to set to any particular value.
    _tyImplType m_nMin{s_kstUniverse-1}; // The collection is empty when m_nMin > m_nMax.
    _tyImplType m_nMax{0};
};

__BIENUTIL_END_NAMESPACE
