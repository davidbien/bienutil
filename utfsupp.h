
//          Copyright David Lawrence Bien 1997 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// utfsupp.h
// UTF support methods and objects.
// dbien: 18MAR2020
#error this has never been compiled or used.

#include <memory>
#include "_namdexc.h"

// Utf8Char:
// Represents a variable length multibyte character encoding for a UTF8 character.
class Utf8Char
{
    typedef Utf8Char _tyThis;
public:

    Utf8Char * operator &() = delete; // This can't be read from a stream, etc - guard against misuse.

    static unsigned int GetLengthFromC( uint8_t _c )
    {
        if ( _c < 0x80 )
            return 1;
        else 
        if ( (_c >> 5 ) == 0x6 )
            return 2;
        else 
        if ( ( _c >> 4 ) == 0xe )
            return 3;
        else 
        if ( ( _c >> 3 ) == 0x1e )
            return 4;
        else
            return 0;
    }

    Utf8Char() = default;
    Utf8Char( uint8_t _cFirst )
    {
        (void)NSetFirstChar( _cFirst );
    }

    unsigned int NSetFirstChar( uint8_t _cFirst )
    {
        unsigned int len = GetLengthFromC( _cFirst );
        if ( !len )
            THROWNAMEDEXCEPTION( "Invalid lead byte for UTF8 [%hhx].", _cFirst );
        SetNCodes( len );
        m_rgc[0] = _cFirst;
        m_rgc[1] = 0xff;
        m_rgc[2] = 0xff;
        m_rgc[3] = 0xff;
        return len-1;
    }
    
    bool FSingleCode() const
    {
        return 1 == NCodes();
    }
    unsigned int NCodes() const
    {
        return ( m_byFlags & 0x3 ) + 1;
    }
    void SetNCodes( unsigned int _byNCodes )
    {
        Assert( _byNCodes > 0 );
        Assert( _byNCodes < 5 );
        m_byFlags = ( m_byFlags & ~'\x03' ) | ( (uint8_t)_byNCodes & 0x3 );
    }

    uint8_t * begin()
    {
        return m_rgc;
    }
    const uint8_t * begin() const
    {
        return m_rgc;
    }
    uint8_t * end()
    {
        return m_rgc + NCodes();
    }
    const uint8_t * end() const
    {
        return m_rgc + NCodes();
    }

    char GetChar() const
    {
        // Return the single character that much be here.
        if ( NCodes() != 1 )
            THROWNAMEDEXCEPTION( "Got called for a multibyte UTF8." );
        return m_rgc[1];
    }
    bool operator == ( char _c ) const
    {
        return ( 1 == NCodes() ) && ( m_rgc[0] == _c );
    }
protected:
    uint8_t m_rgc[4]{0,0xff,0xff,0xff};
    uint8_t m_byFlags{0};
};

// Utf16Char: Stub this for now.
class Utf16Char
{

};