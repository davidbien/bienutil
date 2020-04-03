#pragma once

// jsonobjs.h
// JSON objects for reading/writing using jsonstrm.h.
// dbien 01APR2020

#include "jsonstrm.h"
#include "strwrsv.h"

// predeclare
template < class t_tyChar >
class JsoValue;
template < class t_tyChar >
class _JsoObject;
template < class t_tyChar >
class _JsoArray;
template < class t_tyChar, bool t_kfConst >
class JsoIterator;

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
    typedef std::conditional_t< t_kfConst, typename _JsoObject< t_tyChar >::_tyConstIterator, typename _JsoObject< t_tyChar >::_tyIterator > _tyObjectIterator;
    typedef std::conditional_t< t_kfConst, typename JsoArray< t_tyChar >::_tyConstIterator, typename JsoArray< t_tyChar >::_tyIterator > _tyArrayIterator;
    typedef JsoValue< t_tyChar > _tyJsoValue;
    typedef std::conditional_t< t_kfConst, const _tyJsoValue, _tyJsoValue > _tyQualJsoValue;
    typedef typename _JsoObject< t_tyChar >::_tyMapValueType _tyKeyValueType; // This type is only used by objects.
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
#if defined (__amd64__) || defined (_M_AMD64) || defined (__x86_64__) || defined (__ia64__)
#pragma pack(push,8) // Ensure that we pack this on an 8 byte boundary for 64bit compilation.
#else
#pragma pack(push,4) // Ensure that we pack this on an 4 byte boundary for 32bit compilation.
#endif

template < class t_tyChar >
class JsoValue
{
    typedef JsoValue _tyThis;
public:
    typedef std::basic_string< t_tyChar > _tyStdStr;
    typedef StrWRsv< _tyStdStr > _tyStrWRsv; // string with reserve buffer.
    typedef _JsoObject< t_tyChar > _tyJsonObject;
    typedef _JsoArray< t_tyChar > _tyJsonArray;
    typedef JsoIterator< t_tyChar, false > iterator;
    typedef JsoIterator< t_tyChar, true > const_iterator;

    JsoValue( EJsonValueType _jvt = ejvtJsonValueTypeCount )
    {
        if ( ejvtJsonValueTypeCount != _jvt )
            _AllocateValue( _jvt );
    }

    JsoValue( const JsoValue & _r )
    {
        *this = _r;
    }
    JsoValue & operator = ( const JsoValue & _r )
    {
        SetValueType( _r.GetValueType() );
        switch( GetValueType() )
        {
        case ejvtNull:
        case ejvtTrue:
        case ejvtFalse:
            break; // nothing to do
        case ejvtNumber:
        case ejvtString:
            StrGet() = _r.StrGet();
            break;
        case ejvtObject:
            _ObjectGet() = _r._ObjectGet();
            break;
        case ejvtArray:
            _ArrayGet() = _r._ArrayGet();
            break;
        default:
            assert( 0 ); // random value...
        case ejvtJsonValueTypeCount:
            break; // assigned to an empty object.
        }
    }

    EJsonValueType JvtGetValueType() const
    {
        return m_jvtType;
    }
    void SetValueType( const EJsonValueType _jvt )
    {
        if ( _jvt != m_jvt )
        {
            _ClearValue();
            _AllocateValue( _jvt );
        }
    }

    bool FIsNull() const
    {
        return ejvtNull == m_jvtType;
    }
    bool FIsBoolean() const
    {
        return ( ejvtTrue == m_jvtType ) || ( ejvtFalse == m_jvtType );
    }
    bool FIsTrue() const
    {
        return ejvtTrue == m_jvtType;
    }
    bool FIsFalse() const
    {
        return ejvtFalse == m_jvtType;
    }
    bool FIsString() const
    {
        return ( ejvtString == m_jvtType );
    }
    bool FIsNumber() const
    {
        return ( ejvtNumber == m_jvtType );
    }
    bool FIsObject() const
    {
        return ejvtObject == m_jvtType;
    }
    bool FIsArray() const
    {
        return ejvtArray == m_jvtType;
    }
    void GetValue( bool & _rf ) const
    {
        if ( ejvtTrue == m_jvtType )
            _rf = true;
        else
        if ( ejvtFalse == m_jvtType )
            _rf = false;
        else
            THROWJSONBADUSAGE( "JsoValue::GetValue(bool): Called on non-boolean." );
    }
    const _tyStrWRsv & StrGet() const
    {
        if ( !FIsString() && !FIsNumber() )
            THROWJSONBADUSAGE( "JsoValue::StrGet(): Called on non-string/num." );
        return *static_cast< const _tyStrWRsv * >( m_rgbyValBuf );
    }
    _tyStdStr & StrGet()
    {
        if ( !FIsString() && !FIsNumber() )
            THROWJSONBADUSAGE( "JsoValue::StrGet(): Called on non-string/num." );
        return *static_cast< _tyStrWRsv * >( m_rgbyValBuf );
    }
    const _tyObjectInternal & _ObjectGet() const
    {
        if ( !FIsObject() )
            THROWJSONBADUSAGE( "JsoValue::_ObjectGet(): Called on non-Object." );
        return *static_cast< const _tyObjectInternal * >( m_rgbyValBuf );
    }
    _tyObjectInternal & _ObjectGet()
    {
        if ( !FIsObject() )
            THROWJSONBADUSAGE( "JsoValue::_ObjectGet(): Called on non-Object." );
        return *static_cast< _tyObjectInternal * >( m_rgbyValBuf );
    }
    const _tyArrayInternal & _ArrayGet() const
    {
        if ( !FIsArray() )
            THROWJSONBADUSAGE( "JsoValue::_ArrayGet(): Called on non-Array." );
        return *static_cast< const _tyArrayInternal * >( m_rgbyValBuf );
    }
    _tyArrayInternal & _ArrayGet()
    {
        if ( !FIsArray() )
            THROWJSONBADUSAGE( "JsoValue::_ArrayGet(): Called on non-Array." );
        return *static_cast< _tyArrayInternal * >( m_rgbyValBuf );
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
    void FromJSONStream( JsonReadCursor< t_tyJsonInputStream > & _jrc )
    {
        // The constructor would have already set the m_jvtType.
        assert( _jrc.JvtGetValueType() == m_jvtType );
    }
    template < class t_tyJsonOutputStream >
    void ToJSONStream( JsonValueLife< t_tyJsonOutputStream > & _jvl ) const
    {
        
    }

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
    JsoValue & GetEl( size_t _st )
    {
        return _ArrayGet().GetEl( _st );
    }
    const JsoValue & GetEl( size_t _st ) const
    {
        return _ArrayGet().GetEl( _st );
    }

    JsoValue & operator [] ( _tyLPCSTR _psz )
    {
        return GetEl( _psz );
    }
    const JsoValue & operator [] ( _tyLPCSTR _psz ) const
    {
        return GetEl( _psz );
    }
    JsoValue & GetEl( _tyLPCSTR _psz )
    {
        return _ObjectGet().GetEl( _psz );
    }
    const JsoValue & GetEl( _tyLPCSTR _psz ) const
    {
        return _ObjectGet().GetEl( _psz );
    }

    iterator begin()
    {
        if ( !FIsAggregate() )
            THROWJSONBADUSAGE( "JsoValue::begin(): Called on non-aggregate." );
    }
    const_iterator begin() const
    {
        if ( !FIsAggregate() )
            THROWJSONBADUSAGE( "JsoValue::begin(): Called on non-aggregate." );
    }
    iterator end()
    {
        if ( !FIsAggregate() )
            THROWJSONBADUSAGE( "JsoValue::end(): Called on non-aggregate." );
    }
    const_iterator end() const
    {
        if ( !FIsAggregate() )
            THROWJSONBADUSAGE( "JsoValue::end(): Called on non-aggregate." );
    }

protected:
    void _ClearValue()
    {
        EJsonValueType jvt = m_jvtType;
        m_jvtType = ejvtJsonValueTypeCount;.
        switch( jvt )
        {
        case ejvtNumber:
        case ejvtString:
            static_cast< _tyStrWRsv * >( m_rgbyValBuf )->~_tyStrWRsv();
            break;
        case ejvtObject:
            static_cast< _tyObjectInternal * >( m_rgbyValBuf )->~_tyObjectInternal();
            break;
        case ejvtArray:
            static_cast< _tyArrayInternal * >( m_rgbyValBuf )->~_tyArrayInternal();
            break;
        default:
            assert( 0 ); // random value...
        case ejvtNull:
        case ejvtTrue:
        case ejvtFalse:
        case ejvtJsonValueTypeCount:
            break;
        }
    }
    void _AllocateValue( const EJsonValueType _jvt )
    {
        assert( ejvtJsonValueTypeCount == GetValueType() );
        switch( _jvt )
        {
        case ejvtNumber:
        case ejvtString:
            new( m_rgbyValBuf ) _tyStrWRsv();
            break;
        case ejvtObject:
            new( m_rgbyValBuf ) _tyObjectInternal();
            break;
        case ejvtArray:
            new( m_rgbyValBuf ) _tyArrayInternal();
            break;
        default:
            assert( 0 ); // random value...
        case ejvtNull:
        case ejvtTrue:
        case ejvtFalse:
        case ejvtJsonValueTypeCount:
            break;
        }
        m_jvtType = _jvt; // throw-safety.
    }

    // Put the object buffer as the first member because then it will have alignment of the object - which is 8(64bit) or 4(32bit).
    constexpr size_t s_kstSizeValBuf = std::max( sizeof(_tyStrWRsv), std::max( sizeof(_tyJsoObject), sizeof(_tyJsoArray) ) );
    uint8_t m_rgbyValBuf[ s_kstSizeValBuf ];
    EJsonValueType m_jvtType{ ejvtJsonValueTypeCount };
};

#pragma pack(pop)

// _JsoObject:
// This is internal impl only - never exposed to the user of JsoValue.
template < class t_tyChar >
class _JsoObject
{
    typedef _JsoObject _tyThis;
public:
    typedef t_tyChar _tyChar;
    typedef const t_tyChar * _tyLPCSTR;
    typedef std::basic_string< t_tyChar > _tyStdStr;
    typedef StrWRsv< _tyStdStr > _tyStrWRsv; // string with reserve buffer.
    typedef JsoValue< t_tyChar > _tyJsoValue;
    typedef std::map< _tyStrWRsv, _tyJsoValue > _tyMapValues;
    typedef typename _tyMapValues::iterator _tyIterator;
    typedef typename _tyMapValues::const_iterator _tyConstIterator;
    typedef _tyMapValues::value_type _tyMapValueType;

    _JsoObject() = default;
    ~_JsoObject() = default;
    _JsoObject( const _JsoObject & ) = default;
    _JsoObject &operator=( const _JsoObject & ) = default;

    void Clear()
    {
        m_mapValues.clear();
    }
    _tyIterator begin()
    {
        return m_mapValues.begin();
    }
    _tyConstIterator begin() const
    {
        return m_mapValues.begin();
    }
    _tyIterator end()
    {
        return m_mapValues.end();
    }
    _tyConstIterator end() const
    {
        return m_mapValues.end();
    }

    // Throws if there is no such el.
    _tyMapValueType & GetEl( _tyLPCSTR _psz )
    {
        _tyIterator it = m_mapValues.find( _psz );
        if ( m_mapValues.end() == it )
            THROWJSONBADUSAGE( "_JsoObject::GetEl(): No such key [%s]", _psz );
    }
    const _tyMapValueType & GetEl( _tyLPCSTR _psz ) const
    {
        return const_cast< _tyThis * >( this )->GetEl( _psz );
    }

    template < class t_tyJsonInputStream >
    void FromJSONStream( JsonReadCursor< t_tyJsonInputStream > & _jrc )
    {
        assert( m_mapValues.empty() ); // Note that this isn't required just that it is expected. Remove assertion if needed.
        _tyBase::FromJSONStream( _jrc );
        JsonRestoreContext< t_tyJsonInputStream > rxc( _jrc );
        if ( !_jrc.FMoveDown() )
            THROWJSONBADUSAGE( "_JsoObject::FromJSONStream(EJsonValueType): FMoveDown() returned false unexpectedly." );
        for ( ; !_jrc.FAtEndOfAggregate(); (void)_jrc.FNextElement() )
        {
            _tyStdStr strKey;
            EJsonValueType jvt;
            bool f = _jrc.FGetKeyCurrent( strKey, jvt );
            if ( !f )
                THROWJSONBADUSAGE( "_JsoObject::FromJSONStream(EJsonValueType): FGetKeyCurrent() returned false unexpectedly." );
            _tyPtrJsoValue ptrNew;
            _tyBase::MakeJsoValue( jvt, ptrNew );
            ptrNew->FromJSONStream( _jrc );
            std::pair< _tyMap::iterator, bool > pib = m_mapValues.try_emplace( std::move( strKey ), std::move( ptrNew ) );
            if ( !pib.second ) // key already exists.
                THROWJSONBADUSAGE( "_JsoObject::FromJSONStream(EJsonValueType): Duplicate key found[%s] path[%s].", strKey.c_str(), _jrc.StrCurrentPath().c_str() );
        }
    }
    template < class t_tyJsonOutputStream >
    void ToJSONStream( JsonValueLife< t_tyJsonOutputStream > & _jvl ) const
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

