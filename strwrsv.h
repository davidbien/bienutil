#pragma once

// strwrsv.h
// String with reserve buffer.
// dbien : 02APR2020
// Wraps std::basic_string<> or any object generic with std::basic_string<>.

#include <string>

// StrWRsv:
// String with reserve. We don't store a length because we assume that in the places we will use this we won't need a length often.
template < class t_tyStrBase = std::string, size_t t_kstReserve = 64 >
class StrWRsv
{
    typedef StrWRsv _tyThis;
    static_assert( t_kstReserve-1 >= sizeof( t_tyStrBase ) ); // We reserve a single byte for flags.
public:
    typedef typename t_tyStrBase::value_type _tyChar;
    typedef typename std::make_unsigned<_tyChar>::type _tyUnsignedChar;

    // We can't just provide a forwarding constructor because we want to be able to capture the cases where
    // we are initialized from a small enough string to use the reserve buffer.
    // So we'll provide some functionality now and we probably will have to add functionality later.

    StrWRsv()
    {
        _ClearFlags();
    }
    StrWRsv( _tyChar * _psz )
    {
        _ClearFlags();
        if ( !_psz )
            return;
        Assign( _psz );
    }
    void swap( _tyThis & _r )
    {
        _tyChar rgtcBuffer[ t_kstReserve ];
        memcpy( rgtcBuffer, _r.m_rgtcBuffer, sizeof( m_rgtcBuffer ) );
        memcpy( _r.m_rgtcBuffer, m_rgtcBuffer, sizeof( m_rgtcBuffer ) );
        memcpy( m_rgtcBuffer, rgtcBuffer, sizeof( m_rgtcBuffer ) );
    }

    const _tyChar * c_str() const
    {
        return FHasStringObj() ? static_cast< t_tyStrBase * >( m_rgtcBuffer )->c_str() : m_rgtcBuffer;
    }

    void _ClearFlags()
    {
        m_rgtcBuffer[ t_kstReserve-1 ] = 0;
    }
    _tyUnsignedChar UFlags() const
    {
        return static_cast< _tyUnsignedChar >( m_rgtcBuffer[ t_kstReserve-1 ] );
    }
    bool FHasStringObj() const
    {
        return UFlags() == std::numeric_limits< _tyUnsignedChar >::max();
    }
    void SetHasStringObj()
    {
        m_rgtcBuffer[ t_kstReserve-1 ] = std::numeric_limits< _tyUnsignedChar >::max();
    }
    void ClearHasStringObj()
    {
        _ClearFlags();
    }

    void clear()
    {
        if ( FHasStringObj() )
        {            
            _ClearFlags(); // Ensure that we don't double destruct somehow.
            static_cast< t_tyStrBase * >( m_rgtcBuffer )->~t_tyStrBase(); // destructors should never throw.
        }
        m_rgtcBuffer[ 0 ] = 0; // length = 0.
    }

    _tyThis & assign( _tyChar * _psz, size_t _stLen = std::numeric_limits<size_t>::max() )
    {
        if ( !_psz )
            THROWNAMEDEXCEPTION( "StrWRsv::assign(): null _psz." );
        if ( std::numeric_limits<size_t>::max() == _stLen )
            _stLen = StrNLen( _psz );
        if ( _stLen < t_kstReserve )
        {
            if ( FHasStringObj() )
            {
                _ClearFlags(); // Ensure that we don't double destruct somehow.
                static_cast< t_tyStrBase * >( m_rgtcBuffer )->~t_tyStrBase(); // destructors should never throw.
            }
            memcpy( m_rgtcBuffer, _psz, _stLen * sizeof(_tyChar) );
            m_rgtcBuffer[ _stLen ] = 0;
        }
        else
        if ( FHasStringObj() )
        {
            // Then reuse the existing string object for speed:
            static_cast< t_tyStrBase * >( m_rgtcBuffer )->assign( _psz, _stLen );
        }
        else
        {
            new( (void*)m_rgtcBuffer ) t_tyStrBase( _psz, _stLen ); // may throw.
            SetHasStringObj(); // throw-safe.
        }
        return *this;
    }

protected:
    _tyChar m_rgtcBuffer[ t_kstReserve ]{}; // This may contain a null terminated string or it may contain the string object.
    // The last byte are the flags which are zero when we are using the buffer, and non-zero (-1) when there is a string lifetime present.
};
