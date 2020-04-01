#pragma once

// jsonobjs.h
// JSON objects for reading/writing using jsonstrm.h.
// dbien 01APR2020

#include "jsonstrm.h"

// JsoValue:
// Every JSON object is a value.
template < class t_tyChar >
class JsoValue
{
    typedef JsoValue _tyThis;
public:    
    JsoValue() = default;
    virtual ~JsoValue() = default;
    JsoValue( const JsoValue & ) = default;
    JsoValue &operator=( const JsoValue & ) = default;

    EJsonValueType JvtGetValueType() const
    {
        return m_jvtType;
    }

    // abstracts:
    template < class t_tyJsonInputStream >
    virtual void FromJSONStream( JsonReadCursor< t_tyJsonInputStream > & _jrc )
    {
        // Grab the type off of the cursor:
        m_jvtType =_jrc.JvtGetValueType();
    }
    template < class t_tyJsonOutputStream >
    virtual void ToJSONStream( JsonValueLife< t_tyJsonOutputStream > & _jvl ) const = 0;

    // Array index can be used on either array or object.
    // Will throw semanticerror due to bad index access if accessed beyond the end.
    JsoValue & operator [] ( size_t _st )
    {
        return GetEl( _st );
    }
    const JsoValue & operator [] ( size_t _st ) const
    {
        return GetEl( _st );
    }
    virtual JsoValue & GetEl( size_t _st )
    {
        THROWBADJSONSEMANTICUSE
    }
    virtual const JsoValue & GetEl( size_t _st ) const = 0;


    JsoValue & operator [] ( _tyLPCSTR _psz )
    {
        return GetEl( _psz );
    }
    const JsoValue & operator [] ( _tyLPCSTR _psz ) const
    {
        return GetEl( _psz );
    }
    virtual JsoValue & GetEl( _tyLPCSTR _psz ) = 0;
    virtual const JsoValue & GetEl( _tyLPCSTR _psz ) const = 0;

protected:
    EJsonValueType m_jvtType;
};

// JsoSimpleValue:
// Represents a non-aggregate, non-string value - i.e. true, false or null.
template < class t_tyChar >
class JsoSimpleValue : public JsoValue< t_tyChar >
{
    typedef JsoSimpleValue _tyThis;
    typedef JsoValue< t_tyChar > _tyBase;
public:
    JsoSimpleValue() = default;
    virtual ~JsoSimpleValue() = default;
    JsoSimpleValue( const JsoSimpleValue & ) = default;
    JsoSimpleValue &operator=( const JsoSimpleValue & ) = default;

    template < class t_tyJsonInputStream >
    void FromJSONStream( JsonReadCursor< t_tyJsonInputStream > & _jrc ) override
    {
        _tyBase::FromJSONStream( _jrc );
        // We could explicitly skip the value but the _jrc infrastructure will do that for us.
    }
    template < class t_tyJsonOutputStream >
    void ToJSONStream( JsonValueLife< t_tyJsonOutputStream > & _jvl ) const override
    {
        JsonValueLife jvlArrayElement( *this, m_jvtType ); // If we are here we are either writing to the root value or at an array element.
    }
};

// JsoStrOrNum:
// Represents a non-aggregate, string value - i.e. either a string or a number.
template < class t_tyChar >
class JsoStrOrNum : public JsoValue< t_tyChar >
{
    typedef JsoStrOrNum _tyThis;
    typedef JsoValue< t_tyChar > _tyBase;
public:
    typedef std::basic_string< t_tyChar > _tyStdStr;
    JsoStrOrNum() = default;
    virtual ~JsoStrOrNum() = default;
    JsoStrOrNum( const JsoStrOrNum & ) = default;
    JsoStrOrNum &operator=( const JsoStrOrNum & ) = default;

    template < class t_tyJsonInputStream >
    void FromJSONStream( JsonReadCursor< t_tyJsonInputStream > & _jrc ) override
    {
        _tyBase::FromJSONStream( _jrc );
        _jrc.GetValue( m_strStrOrNum );
    }
    template < class t_tyJsonOutputStream >
    void ToJSONStream( JsonValueLife< t_tyJsonOutputStream > & _jvl ) const override
    {
        _jvl._WriteValue( JvtGetValueType(), _tyStdStr( m_strStrOrNum ) ); // a temporary should be an rr-value - ie. std::move() not necessary.
    }
protected:
    _tyStdStr m_strStrOrNum;
};


