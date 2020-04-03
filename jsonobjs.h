#pragma once

// jsonobjs.h
// JSON objects for reading/writing using jsonstrm.h.
// dbien 01APR2020

#include "jsonstrm.h"

// This exception will get thrown if the user of the read cursor does something inappropriate given the current context.
class json_objects_bad_usage_exception : public std::_t__Named_exception< __JSONSTRM_DEFAULT_ALLOCATOR >
{
    typedef std::_t__Named_exception< __JSONSTRM_DEFAULT_ALLOCATOR > _TyBase;
public:
    typedef t_tyCharTraits _tyCharTraits;

    json_objects_bad_usage_exception( const string_type & __s ) 
        : _TyBase( __s ) 
    {
    }
    json_objects_bad_usage_exception( const char * _pcFmt, va_list _args )
        : _TyBase( _pcFmt, _args )
    {
    }
};
// By default we will always add the __FILE__, __LINE__ even in retail for debugging purposes.
#define THROWJSONBADUSAGE( MESG... ) ExceptionUsage<json_objects_bad_usage_exception>::ThrowFileLine( __FILE__, __LINE__, MESG )

// JsoIterator:
// This iterator may be iterating an object or an array.
// For an object we iterate in key order.
// For an array we iterate in index order.
template < class t_tyChar, bool t_kfConst >
class JsoIterator
{
    typedef JsoIterator _tyThis;
public:    
    typedef std::conditional_t< t_kfConst, typename JsoObject< t_tyChar >::_tyConstIterator, typename JsoObject< t_tyChar >::_tyIterator > _tyObjectIterator;
    typedef std::conditional_t< t_kfConst, typename JsoArray< t_tyChar >::_tyConstIterator, typename JsoArray< t_tyChar >::_tyIterator > _tyArrayIterator;
    typedef JsoValue< t_tyChar > _tyJsoValue;
    typedef std::conditional_t< t_kfConst, const _tyJsoValue, _tyJsoValue > _tyQualJsoValue;
    typedef typename JsoObject< t_tyChar >::_tyMapValueType _tyKeyValueType; // This type is only used by objects.
    typedef std::conditional_t< t_kfConst, const _tyKeyValueType, _tyKeyValueType > _tyQualKeyValueType;

    JsoIterator() = default;
    JsoIterator( const JsoIterator & _r )
        : m_fObjectIterator( _r.m_fObjectIterator )
    {
        m_fObjectIterator ? _CreateIterator( _r._GetObjectIterator() ) : _CreateIterator( _r._GetArrayIterator() );
    }
    JsoIterator( const JsoIterator && _rr )
        : m_fObjectIterator( _rr.m_fObjectIterator )
    {
        m_fObjectIterator ? _CreateIterator( std::move( _rr._GetObjectIterator() ) ) : _CreateIterator( std::move( _rr._GetArrayIterator() ) );
    }

    explicit JsoIterator( _tyObjectIterator const & _rit )
        : m_fObjectIterator( true )
    {
        _CreateIterator( _rit );
    }
    JsoIterator( _tyObjectIterator && _rrit )
        : m_fObjectIterator( true )
    {
        _CreateIterator( std::move( _rrit ) );
    }
    explicit JsoIterator( _tyArrayIterator const & _rit )
        : m_fObjectIterator( false )
    {
        _CreateIterator( _rit );
    }
    JsoIterator( _tyArrayIterator && _rrit )
        : m_fObjectIterator( false )
    {
        _CreateIterator( std::move( _rrit ) );
    }

    JsoIterator & operator ++()
    {
        m_fObjectIterator ? ++_GetObjectIterator() : ++_GetArrayIterator();
        return *this;
    }
    JsoIterator operator ++(int)
    {
        if ( m_fObjectIterator )
        {
            _tyObjectIterator it = _GetObjectIterator()++;
            return JsoIterator( std::move( it ) );
        }
        else
        {
            _tyArrayIterator it = _GetArrayIterator()++;
            return JsoIterator( std::move( it ) );
        }
    }
    JsoIterator & operator --()
    {
        m_fObjectIterator ? --_GetObjectIterator() : --_GetArrayIterator();
        return *this;
    }
    JsoIterator operator --(int)
    {
        if ( m_fObjectIterator )
        {
            _tyObjectIterator it = _GetObjectIterator()--;
            return JsoIterator( std::move( it ) );
        }
        else
        {
            _tyArrayIterator it = _GetArrayIterator()--;
            return JsoIterator( std::move( it ) );
        }
    }

    // This always returns the value for both objects and arrays since it has to return the same thing.
    _tyQualJsoValue & operator * () const
    {
        return m_fObjectIterator ? _GetObjectIterator()->second : *_GetArrayIterator();
    }
    _tyQualJsoValue * operator -> () const
    {
        return m_fObjectIterator ? &_GetObjectIterator()->second : &*_GetArrayIterator();
    }
    _tyQualKeyValueType & GetKeyValue() const
    {
        if ( ! m_fObjectIterator )
            THROWJSONBADUSAGE( "JsoIterator::GetKeyValue(): At an array not an object.")
        return *_GetObjectIterator();
    }

    bool operator == ( const _tyThis & _r ) const
    {
        if ( m_fObjectIterator != _r.m_fObjectIterator )
            return false;
        return m_fObjectIterator ? ( _r._GetObjectIterator() == _GetObjectIterator() ) : ( _r._GetArrayIterator() == _GetArrayIterator() );
    }
    bool operator != ( const _tyThis & _r ) const
    {
        return !this->operator == ( _r );
    }
protected:
    void _CreateIterator( _tyObjectIterator const & _rit )
    {
        assert( !m_pvIterator );
        assert( m_fObjectIterator );
        m_pvIterator = new _tyObjectIterator( _rit );
    }
    void _CreateIterator( _tyObjectIterator && _rrit )
    {
        assert( !m_pvIterator );
        assert( m_fObjectIterator );
        m_pvIterator = new _tyObjectIterator( std::move( _rrit ) );
    }
    void _CreateIterator( _tyArrayIterator const & _rit )
    {
        assert( !m_pvIterator );
        assert( !m_fObjectIterator );
        m_pvIterator = new _tyArrayIterator( _rit );
    }
    void _CreateIterator( _tyArrayIterator && _rrit )
    {
        assert( !m_pvIterator );
        assert( !m_fObjectIterator );
        m_pvIterator = new _tyArrayIterator( std::move( _rrit ) );
    }

    const _tyObjectIterator & _GetObjectIterator() const
    {
        if ( !m_pvIterator )
            THROWJSONBADUSAGE( "JsoIterator::_GetObjectIterator(): Not connected to iterator." );
        if ( !m_fObjectIterator )
            THROWJSONBADUSAGE( "JsoIterator::_GetObjectIterator(): Called on array." );
        return *(_tyObjectIterator*)m_pvIterator;
    }
    const _tyArrayIterator & _GetArrayIterator() const
    {
        if ( !m_pvIterator )
            THROWJSONBADUSAGE( "JsoIterator::_GetArrayIterator(): Not connected to iterator." );
        if ( m_fObjectIterator )
            THROWJSONBADUSAGE( "JsoIterator::_GetArrayIterator(): Called on object." );
        return *(_tyArrayIterator*)m_pvIterator;
    }

    void * m_pvIterator{nullptr}; // This is a pointer to a dynamically allocated iterator of the appropriate type.
    bool m_fObjectIterator{true};
};

// JsoValue:
// Every JSON object is a value. In fact every single JSON object is represented by the class JsoValue because that is the best spacewise
//  way of doing things. We embed the string/object/array within this class to implement the different JSON objects.
template < class t_tyChar >
class JsoValue
{
    typedef JsoValue _tyThis;
public:

    typedef JsoIterator< t_tyChar, false > iterator;
    typedef JsoIterator< t_tyChar, true > const_iterator;
protected:
    JsoValue() = default;
    virtual ~JsoValue() = default;
    JsoValue( const JsoValue & ) = default;
    JsoValue &operator=( const JsoValue & ) = default;
public:

    static void MakeJsoValue( EJsonValueType _jvt, _tyPtrJsoValue & _ptrNew )
    {
        switch( _jvt )
        {
        case ejvtObject:
            _ptrNew = std::make_unique< JsoObject >();
            break;
        case ejvtArray:
            _ptrNew = std::make_unique< JsoArray >();
            break;
        case ejvtNull:
        case ejvtFalse:
        case ejvtTrue:
            _ptrNew = std::make_unique< _tyJsoSimpleValue >( _jvt );
            break;
        case ejvtString:
        case ejvtNumber:
            _ptrNew = std::make_unique< JsoStrOrNum >( _jvt );
            break;
        default:
            THROWJSONBADUSAGE( "JsoValue::MakeJsoValue(): Unknown JsonValueType [%hhu].", _jvt );
            break;
        }
    }

    EJsonValueType JvtGetValueType() const
    {
        return m_jvtType;
    }
    bool FIsNull() const
    {
        return ejvtNull == m_jvtType;
    }
    bool FIsBoolean() const
    {
        return( ejvtTrue == m_jvtType ) || ( ejvtFalse == m_jvtType );
    }
    bool FIsTrue() const
    {
        return ejvtTrue == m_jvtType;
    }
    bool FIsFalse() const
    {
        return ejvtFalse == m_jvtType;
    }
    void GetValue( bool & _rf ) const
    {
        if ( !FIsBoolean() )        
            THROWJSONBADUSAGE( "JsoValue::GetValue(bool): Called on non-boolean." );
    }
    virtual const _tyStdStr & StrGet() const
    {
        THROWJSONBADUSAGE( "JsoValue::StrGet(): Called on non-string/num." );
    }
    virtual _tyStdStr & StrGet()
    {
        THROWJSONBADUSAGE( "JsoValue::StrGet(): Called on non-string/num." );
    }
    // Various number conversion methods.
    template < class t_tyNum >
    void _GetValue( _tyLPCSTR _pszFmt, t_tyNum & _rNumber ) const
    {
        if ( ejvtNumber != JvtGetValueType() ) 
            THROWJSONBADUSAGE( "JsonReadCursor::GetValue(various int): Not at a numeric value type." );

        // The presumption is that sscanf won't read past any decimal point if scanning a non-floating point number.
        int iRet = sscanf( StrGet().c_str(), _pszFmt, &_rNumber );
        assert( 1 == iRet ); // Due to the specification of number we expect this to always succeed.
    }
    void GetValue( uint8_t & _rby ) const { _GetValue( "%hhu", _rby ); }
    void GetValue( int8_t & _rsby ) const { _GetValue( "%hhd", _rsby ); }
    void GetValue( uint16_t & _rus ) const { _GetValue( "%hu", _rus ); }
    void GetValue( int16_t & _rss ) const { _GetValue( "%hd", _rss ); }
    void GetValue( uint32_t & _rui ) const { _GetValue( "%u", _rui ); }
    void GetValue( int32_t & _rsi ) const { _GetValue( "%d", _rsi ); }
    void GetValue( uint64_t & _rul ) const { _GetValue( "%lu", _rul ); }
    void GetValue( int64_t & _rsl ) const { _GetValue( "%ld", _rsl ); }
    void GetValue( float & _rfl ) const { _GetValue( "%e", _rfl ); }
    void GetValue( double & _rdbl ) const { _GetValue( "%le", _rdbl ); }
    void GetValue( long double & _rldbl ) const { _GetValue( "%Le", _rldbl ); }

    // abstracts:
    template < class t_tyJsonInputStream >
    virtual void FromJSONStream( JsonReadCursor< t_tyJsonInputStream > & _jrc )
    {
        // The constructor would have already set the m_jvtType.
        assert( _jrc.JvtGetValueType() == m_jvtType );
    }
    template < class t_tyJsonOutputStream >
    virtual void ToJSONStream( JsonValueLife< t_tyJsonOutputStream > & _jvl ) const = 0;

    // Array index can be used on either array or object.
    // Will throw semanticerror due to bad index access if accessed beyond the end.
    _tyAggregateWriteProxy & operator [] ( size_t _st )
    {
        return GetEl( _st );
    }
    const JsoValue & operator [] ( size_t _st ) const
    {
        return GetEl( _st );
    }
    virtual _tyAggregateWriteProxy & GetEl( size_t _st )
    {
        THROWJSONBADUSAGE( "JsoValue::GetEl(size_t): Called on non-array." );
    }
    virtual const JsoValue & GetEl( size_t _st ) const
    {
        THROWJSONBADUSAGE( "JsoValue::GetEl(size_t): Called on non-array." );
    }

    JsoValue & operator [] ( _tyLPCSTR _psz )
    {
        return GetEl( _psz );
    }
    const JsoValue & operator [] ( _tyLPCSTR _psz ) const
    {
        return GetEl( _psz );
    }
    virtual JsoValue & GetEl( _tyLPCSTR _psz )
    {
        THROWJSONBADUSAGE( "JsoValue::GetEl(_tyLPCSTR): Called on non-object." );
    }
    virtual const JsoValue & GetEl( _tyLPCSTR _psz ) const
    {
        THROWJSONBADUSAGE( "JsoValue::GetEl(_tyLPCSTR): Called on non-object." );
    }

    virtual iterator begin()
    {
        THROWJSONBADUSAGE( "JsoValue::begin(): Called on non-aggregate." );
    }
    virtual const_iterator begin() const
    {
        THROWJSONBADUSAGE( "JsoValue::begin(): Called on non-aggregate." );
    }
    virtual iterator end()
    {
        THROWJSONBADUSAGE( "JsoValue::end(): Called on non-aggregate." );
    }
    virtual const_iterator end() const
    {
        THROWJSONBADUSAGE( "JsoValue::end(): Called on non-aggregate." );
    }

    // Return a clone of the current object.
    virtual void Clone( unique_ptr< JsoValue > & _rpjv ) const = 0;

protected:
    EJsonValueType m_jvtType{ ejvtJsonValueTypeCount };
};

// JsoSimpleValue:
// Represents a non-aggregate, non-string value - i.e. true, false or null.
template < class t_tyChar >
class JsoSimpleValue : public JsoValue< t_tyChar >
{
    typedef JsoSimpleValue _tyThis;
    typedef JsoValue< t_tyChar > _tyBase;
public:
    ~JsoSimpleValue() override = default;
    JsoSimpleValue( const JsoSimpleValue & ) = default;
    JsoSimpleValue &operator=( const JsoSimpleValue & ) = default;

    explicit JsoSimpleValue()
    {
        m_jvtType = ejvtNull;
    }
    explicit JsoSimpleValue( bool _f )
    {
        m_jvtType = _f ? ejvtTrue : ejvtFalse;
    }
    explicit JsoSimpleValue( EJsonValueType _jvtType )
        : _tyBase( _jvtType )
    {
        if ( ( m_jvtType != ejvtNull ) && ( m_jvtType != ejvtFalse ) && ( m_jvtType != ejvtTrue ) )
            THROWJSONBADUSAGE( "JsoSimpleValue::JsoSimpleValue(EJsonValueType): Non simple value type passed [%hhu].", _jvtType );
    }

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

    // Return a clone of the current object.
    void Clone( unique_ptr< JsoValue > & _rpjv ) const override
    {
        _rpjv = std::make_unique< JsoSimpleValue >( *this );
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
    ~JsoStrOrNum() override = default;
    JsoStrOrNum( const JsoStrOrNum & ) = default;
    JsoStrOrNum &operator=( const JsoStrOrNum & ) = default;

    explicit JsoStrOrNum( EJsonValueType _jvtType )
        : _tyBase( _jvtType )
    {
        if ( ( m_jvtType != ejvtString ) && ( m_jvtType != ejvtNumber ) )
            THROWJSONBADUSAGE( "JsoStrOrNum::JsoStrOrNum(EJsonValueType): Non string or number value type passed [%hhu].", _jvtType );
    }
    JsoStrOrNum( _tyStdStr const & _rstr )
        :   _tyBase( ejvtString ),
            m_strStrOrNum( _rstr )
    {
    }
    const _tyStdStr & GetStr() const override
    {
        return m_strStrOrNum;
    }
    _tyStdStr & GetStr() override
    {
        return m_strStrOrNum;
    }

    // Return a clone of the current object.
    void Clone( unique_ptr< JsoValue > & _rpjv ) const override
    {
        _rpjv = std::make_unique< JsoStrOrNum >( *this );
    }

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

// JsoObject:
template < class t_tyChar >
class JsoObject : public JsoValue< t_tyChar >
{
    typedef JsoObject _tyThis;
    typedef JsoValue< t_tyChar > _tyBase;
public:
    typedef std::basic_string< t_tyChar > _tyStdStr;
    typedef JsoValue< t_tyChar > _tyJsoValue;
    typedef JsoSimpleValue< t_tyChar > _tyJsoSimpleValue;
    typedef JsoStrOrNum< t_tyChar > _tyJsoStrOrNum;
    typedef std::unique_ptr< _tyJsoValue > _tyPtrJsoValue;
    typedef std::map< _tyStdStr, _tyPtrJsoValue > _tyMapValues;
    typedef typename _tyMapValues::iterator _tyIterator;
    typedef typename _tyMapValues::const_iterator _tyConstIterator;

    JsoObject()
    : _tyBase( ejvtObject )
    {
    }
    virtual ~JsoObject() = default;
    JsoObject( const JsoObject & ) = default;
    JsoObject &operator=( const JsoObject & ) = default;

    // Return a clone of the current object.
    void Clone( unique_ptr< JsoValue > & _rpjv ) const override
    {
        _rpjv = std::make_unique< JsoObject >( *this );
    }

    void Clear()
    {
        m_mapValues.clear();
    }

    template < class t_tyJsonInputStream >
    void FromJSONStream( JsonReadCursor< t_tyJsonInputStream > & _jrc ) override
    {
        assert( m_mapValues.empty() ); // Note that this isn't required just that it is expected. Remove assertion if needed.
        _tyBase::FromJSONStream( _jrc );
        JsonRestoreContext< t_tyJsonInputStream > rxc( _jrc );
        if ( !_jrc.FMoveDown() )
            THROWJSONBADUSAGE( "JsoObject::FromJSONStream(EJsonValueType): FMoveDown() returned false unexpectedly." );
        for ( ; !_jrc.FAtEndOfAggregate(); (void)_jrc.FNextElement() )
        {
            _tyStdStr strKey;
            EJsonValueType jvt;
            bool f = _jrc.FGetKeyCurrent( strKey, jvt );
            if ( !f )
                THROWJSONBADUSAGE( "JsoObject::FromJSONStream(EJsonValueType): FGetKeyCurrent() returned false unexpectedly." );
            _tyPtrJsoValue ptrNew;
            _tyBase::MakeJsoValue( jvt, ptrNew );
            ptrNew->FromJSONStream( _jrc );
            std::pair< _tyMap::iterator, bool > pib = m_mapValues.try_emplace( std::move( strKey ), std::move( ptrNew ) );
            if ( !pib.second ) // key already exists.
                THROWJSONBADUSAGE( "JsoObject::FromJSONStream(EJsonValueType): Duplicate key found[%s] path[%s].", strKey.c_str(), _jrc.StrCurrentPath().c_str() );
        }
    }
    template < class t_tyJsonOutputStream >
    void ToJSONStream( JsonValueLife< t_tyJsonOutputStream > & _jvl ) const override
    {
        _tyConstIterator itCur = m_mapValues.begin();
        for ( ; itCur != m_mapValues.end; ++itCur )
        {
            JsonValueLife< t_tyJsonOutputStream > jvlObjectElement( _jvl, itCur->first.c_str(), itCur->second->JvtGetValueType() );
            itCur->second->ToJSONStream( jvlObjectElement );
        }
    }
protected:
    _tyMapValues m_mapValues;
};

