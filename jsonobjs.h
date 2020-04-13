#pragma once

// jsonobjs.h
// JSON objects for reading/writing using jsonstrm.h.
// dbien 01APR2020

#include <vector>
#include <map>
#include <c++/v1/compare>
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
    typedef std::conditional_t< t_kfConst, typename _JsoArray< t_tyChar >::_tyConstIterator, typename _JsoArray< t_tyChar >::_tyIterator > _tyArrayIterator;
    typedef JsoValue< t_tyChar > _tyJsoValue;
    typedef std::conditional_t< t_kfConst, const _tyJsoValue, _tyJsoValue > _tyQualJsoValue;
    typedef typename _JsoObject< t_tyChar >::_tyMapValueType _tyKeyValueType; // This type is only used by objects.
    typedef std::conditional_t< t_kfConst, const _tyKeyValueType, _tyKeyValueType > _tyQualKeyValueType;

    ~JsoIterator()
    {
        if ( !!m_pvIterator )
            _DestroyIterator( m_pvIterator );
    }
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
    JsoIterator & operator = ( JsoIterator const & _r )
    {
        if ( this != &_r )
        {
            Clear();
            m_fObjectIterator = _r.m_fObjectIterator;
            m_fObjectIterator ? _CreateIterator( _r._GetObjectIterator() ) : _CreateIterator( _r._GetArrayIterator() );
        }
        return *this;
    }
    JsoIterator & operator = ( JsoIterator && _rr )
    {
        if ( this != &_rr )
        {
            Clear();
            swap( _rr );
        }
        return *this;
    }

    void swap( JsoIterator & _r )
    {
        std::swap( m_fObjectIterator, _r.m_fObjectIterator );
        std::swap( m_pvIterator, _r.m_pvIterator );
    }

    void Clear()
    {
        if ( !!m_pvIterator )
        {
            void * pv = m_pvIterator;
            m_pvIterator = nullptr;
            _DestroyIterator( pv );
        }
    }

    JsoIterator & operator ++()
    {
        m_fObjectIterator ? ++_GetObjectIterator() : ++_GetArrayIterator();
        return *this;
    }
    JsoIterator operator ++(int)
    {
        if ( m_fObjectIterator )
            return JsoIterator( _GetObjectIterator()++ );
        else
            return JsoIterator( _GetArrayIterator()++ );
    }
    JsoIterator & operator --()
    {
        m_fObjectIterator ? --_GetObjectIterator() : --_GetArrayIterator();
        return *this;
    }
    JsoIterator operator --(int)
    {
        if ( m_fObjectIterator )
            return JsoIterator( _GetObjectIterator()-- );
        else
            return JsoIterator( _GetArrayIterator()-- );
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
    void _DestroyIterator( void * _pv )
    {
        assert( !!_pv );
        if ( m_fObjectIterator )
            delete (_tyObjectIterator*)_pv;
        else
            delete (_tyArrayIterator*)_pv;
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
    typedef t_tyChar _tyChar;
    typedef const _tyChar * _tyLPCSTR;
    typedef std::basic_string< t_tyChar > _tyStdStr;
    typedef StrWRsv< _tyStdStr > _tyStrWRsv; // string with reserve buffer.
    typedef _JsoObject< t_tyChar > _tyJsoObject;
    typedef _JsoArray< t_tyChar > _tyJsoArray;
    typedef JsoIterator< t_tyChar, false > iterator;
    typedef JsoIterator< t_tyChar, true > const_iterator;

    ~JsoValue()
    {
        _ClearValue();
    }
    JsoValue( EJsonValueType _jvt = ejvtJsonValueTypeCount )
    {
        if ( ejvtJsonValueTypeCount != _jvt )
            _AllocateValue( _jvt );
    }
    JsoValue( const JsoValue & _r )
    {
        *this = _r;
    }
    JsoValue( JsoValue && _rr )
    {
        if ( _rr.JvtGetValueType() != ejvtJsonValueTypeCount )
        {
            switch( _rr.JvtGetValueType() )
            {
            case ejvtNumber:
            case ejvtString:
                new( m_rgbyValBuf ) _tyStrWRsv( std::move( _rr.StrGet() ) );
                break;
            case ejvtObject:
                new( m_rgbyValBuf ) _tyJsoObject( std::move( _rr._ObjectGet() ) );
                break;
            case ejvtArray:
                new( m_rgbyValBuf ) _tyJsoArray( std::move( _rr._ArrayGet() ) );
                break;
            default:
                assert( 0 ); // random value...
            case ejvtNull:
            case ejvtTrue:
            case ejvtFalse:
            case ejvtJsonValueTypeCount:
                break;
            }
            m_jvtType = _rr.JvtGetValueType(); // throw-safety.
        }
    }
    JsoValue & operator = ( const JsoValue & _r )
    {
        if ( this != &_r )
        {
            SetValueType( _r.JvtGetValueType() );
            switch( JvtGetValueType() )
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
        return *this;
    }
    JsoValue & operator = ( JsoValue && _rr )
    {
        if ( this != &_r )
        {
            Clear();
            if ( _rr.JvtGetValueType() != ejvtJsonValueTypeCount )
            {
                switch( _rr.JvtGetValueType() )
                {
                case ejvtNumber:
                case ejvtString:
                    new( m_rgbyValBuf ) _tyStrWRsv( std::move( _rr.StrGet() ) );
                    break;
                case ejvtObject:
                    new( m_rgbyValBuf ) _tyJsoObject( std::move( _rr._ObjectGet() ) );
                    break;
                case ejvtArray:
                    new( m_rgbyValBuf ) _tyJsoArray( std::move( _rr._ArrayGet() ) );
                    break;
                default:
                    assert( 0 ); // random value...
                case ejvtNull:
                case ejvtTrue:
                case ejvtFalse:
                case ejvtJsonValueTypeCount:
                    break;
                }
                m_jvtType = _rr.JvtGetValueType(); // throw-safety.
            }
        }
        return *this;
    }
    void swap( JsoValue & _r )
    {
        std::swap( m_jvtType, _r.m_jvtType );
        if ( !_r.FEmpty() || !FEmpty() )
        {
            uint8_t rgbyValBuf[ s_kstSizeValBuf ];
            memcpy( rgbyValBuf, _r.m_rgbyValBuf, sizeof( m_rgbyValBuf ) );
            memcpy( _r.m_rgbyValBuf, m_rgbyValBuf, sizeof( m_rgbyValBuf ) );
            memcpy( m_rgbyValBuf, rgbyValBuf, sizeof( m_rgbyValBuf ) );
        }
    }

    EJsonValueType JvtGetValueType() const
    {
        return m_jvtType;
    }
    void SetValueType( const EJsonValueType _jvt )
    {
        if ( _jvt != m_jvtType )
        {
            _ClearValue();
            _AllocateValue( _jvt );
        }
    }
    void Clear()
    {
        SetValueType( ejvtJsonValueTypeCount );
    }

// Compare objects:
    std::strong_ordering operator <=> ( _tyThis const & _r ) const
    {
        return ICompare( _r );
    }
    std::strong_ordering ICompare( _tyThis const & _r ) const
    {
        auto comp = (int)JvtGetValueType() <=> (int)_r.JvtGetValueType(); // Arbitrary comparison between different types.
        if ( !comp )
        {
            switch( JvtGetValueType() )
            {
            case ejvtNull:
            case ejvtTrue:
            case ejvtFalse:
                break;
            case ejvtNumber:
            case ejvtString:
                // Note that I don't mean to compare numbers as numbers - only as the strings they are represented in.
                comp = StrGet() <=> _r.StrGet();
                break;
            case ejvtObject:
                comp = _ObjectGet() <=> _r._ObjectGet();
                break;
            case ejvtArray:
                comp = _ArrayGet() <=> _r._ArrayGet();
                break;
            default:
            case ejvtJsonValueTypeCount:
                THROWJSONBADUSAGE( "JsoValue::ICompare(): invalid value type [%hhu].", JvtGetValueType() );
                break;
            }
        }
        return comp;
    }

    bool FEmpty() const
    {
        return JvtGetValueType() == ejvtJsonValueTypeCount;
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
    void GetBoolValue( bool & _rf ) const
    {
        if ( ejvtTrue == m_jvtType )
            _rf = true;
        else
        if ( ejvtFalse == m_jvtType )
            _rf = false;
        else
            THROWJSONBADUSAGE( "JsoValue::GetBoolValue(bool): Called on non-boolean." );
    }
    const _tyStrWRsv & StrGet() const
    {
        if ( !FIsString() && !FIsNumber() )
            THROWJSONBADUSAGE( "JsoValue::StrGet(): Called on non-string/num." );
        return *static_cast< const _tyStrWRsv * >( (const void*)m_rgbyValBuf );
    }
    _tyStrWRsv & StrGet()
    {
        if ( !FIsString() && !FIsNumber() )
            THROWJSONBADUSAGE( "JsoValue::StrGet(): Called on non-string/num." );
        return *static_cast< _tyStrWRsv * >( (void*)m_rgbyValBuf );
    }
    const _tyJsoObject & _ObjectGet() const
    {
        if ( !FIsObject() )
            THROWJSONBADUSAGE( "JsoValue::_ObjectGet(): Called on non-Object." );
        return *static_cast< const _tyJsoObject * >( (const void*)m_rgbyValBuf );
    }
    _tyJsoObject & _ObjectGet()
    {
        if ( !FIsObject() )
            THROWJSONBADUSAGE( "JsoValue::_ObjectGet(): Called on non-Object." );
        return *static_cast< _tyJsoObject * >( (void*)m_rgbyValBuf );
    }
    const _tyJsoArray & _ArrayGet() const
    {
        if ( !FIsArray() )
            THROWJSONBADUSAGE( "JsoValue::_ArrayGet(): Called on non-Array." );
        return *static_cast< const _tyJsoArray * >( (const void*)m_rgbyValBuf );
    }
    _tyJsoArray & _ArrayGet()
    {
        if ( !FIsArray() )
            THROWJSONBADUSAGE( "JsoValue::_ArrayGet(): Called on non-Array." );
        return *static_cast< _tyJsoArray * >( (void*)m_rgbyValBuf );
    }
    // Various number conversion methods.
    template < class t_tyNum >
    void _GetValue( _tyLPCSTR _pszFmt, t_tyNum & _rNumber ) const
    {
        if ( ejvtNumber != JvtGetValueType() ) 
            THROWJSONBADUSAGE( "JsoValue::_GetValue(various int): Not at a numeric value type." );

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

    // Setting methods: These overwrite the existing element at this location.
    void SetNull()
    {
        SetValueType( ejvtJsonValueTypeCount );
    }
    void SetBoolValue( bool _f )
    {
        SetValueType( _f ? ejvtTrue : ejvtFalse );
    }
    _tyThis & operator = ( bool _f )
    {
        SetBoolValue( _f );
    }
    void SetStringValue( _tyLPCSTR _psz, size_t _stLen = std::numeric_limits< size_t >::max() )
    {
        if ( _stLen == std::numeric_limits< size_t >::max() )
            _stLen = StrNLen( _psz );
        SetValueType( ejvtString );
        StrGet().assign( _psz, _stLen );
    }
    template < class t_tyStr >
    void SetStringValue( t_tyStr const & _rstr )
    {
        SetStringValue( _rstr.c_str(), _rstr.length() );
    }
    template < class t_tyStr >
    void SetStringValue( t_tyStr && _rrstr )
    {
        SetValueType( ejvtString );
        StrGet() = std::move( _rrstr );
    }
    _tyThis & operator = ( const _tyStdStr & _rstr )
    {
        SetStringValue( _rstr.c_str(), _rstr.length() );
        return *this;
    }
    _tyThis & operator = ( const _tyStrWRsv & _rstr )
    {
        SetStringValue( _rstr.c_str(), _rstr.length() );
        return *this;
    }
    _tyThis & operator = ( _tyStdStr && _rrstr )
    {
        SetValueType( ejvtString );
        StrGet() = std::move( _rrstr );
        return *this;
    }
    _tyThis & operator = ( _tyStrWRsv && _rrstr )
    {
        SetValueType( ejvtString );
        StrGet() = std::move( _rrstr );
        return *this;
    }

    void SetValue( uint8_t _by )
    {
        _SetValue( "%hhu", _by );
    }
    void SetValue( int8_t _sby )
    {
        _SetValue( "%hhd", _sby );
    }
    void SetValue( uint16_t _us )
    {
        _SetValue( "%hu", _us );
    }
    _tyThis & operator = ( uint16_t _us )
    {
        SetValue( _us );
        return *this;
    }
    void SetValue( int16_t _ss )
    {
        _SetValue( "%hd", _ss );
    }
    _tyThis & operator = ( int16_t _ss )
    {
        SetValue( _ss );
        return *this;
    }
    void SetValue( uint32_t _ui )
    {
        _SetValue( "%u", _ui );
    }
    _tyThis & operator = ( uint32_t _ui )
    {
        SetValue( _ui );
        return *this;
    }
    void SetValue( int32_t _si )
    {
        _SetValue( "%d", _si );
    }
    _tyThis & operator = ( int32_t _si )
    {
        SetValue( _si );
        return *this;
    }
    void SetValue( uint64_t _ul )
    {
        _SetValue( "%lu", _ul );
    }
    _tyThis & operator = ( uint64_t _ul )
    {
        SetValue( _ul );
        return *this;
    }
    void SetValue( int64_t _sl )
    {
        _SetValue( "%ld", _sl );
    }
    _tyThis & operator = ( int64_t _sl )
    {
        SetValue( _sl );
        return *this;
    }
    void SetValue( double _dbl )
    {
        _SetValue( "%lf", _dbl );
    }
    _tyThis & operator = ( double _dbl )
    {
        SetValue( _dbl );
        return *this;
    }
    void SetValue( long double _ldbl )
    {
        _SetValue( "%Lf", _ldbl );
    }
    _tyThis & operator = ( long double _ldbl )
    {
        SetValue( _ldbl );
        return *this;
    }

    template < class t_tyJsonInputStream >
    void FromJSONStream( JsonReadCursor< t_tyJsonInputStream > & _jrc )
    {
        // The constructor would have already set the m_jvtType.
        assert( _jrc.JvtGetValueType() == m_jvtType );
        switch( JvtGetValueType() )
        {
        case ejvtNull:
        case ejvtTrue:
        case ejvtFalse:
            break;
        case ejvtNumber:
        case ejvtString:
            _jrc.GetValue( StrGet() );
            break;
        case ejvtObject:
            _ObjectGet().FromJSONStream( _jrc );
            break;
        case ejvtArray:
            _ArrayGet().FromJSONStream( _jrc );
            break;
        default:
        case ejvtJsonValueTypeCount:
            THROWJSONBADUSAGE( "JsoValue::FromJSONStream(): invalid value type [%hhu].", JvtGetValueType() );
            break;
        }    
    }
    // FromJSONStream with a filter method.
    // The filtering takes place in the objects and arrays because these create sub-values.
    // The filter is not applied to the root value itself.
    template < class t_tyJsonInputStream, class t_tyFilter >
    void FromJSONStream( JsonReadCursor< t_tyJsonInputStream > & _jrc, t_tyFilter & _rfFilter )
    {
        // The constructor would have already set the m_jvtType.
        assert( _jrc.JvtGetValueType() == m_jvtType );
        switch( JvtGetValueType() )
        {
        case ejvtNull:
        case ejvtTrue:
        case ejvtFalse:
            break;
        case ejvtNumber:
        case ejvtString:
            _jrc.GetValue( StrGet() );
            break;
        case ejvtObject:
            _ObjectGet().FromJSONStream( _jrc, _rfFilter );
            break;
        case ejvtArray:
            _ArrayGet().FromJSONStream( _jrc, _rfFilter );
            break;
        default:
        case ejvtJsonValueTypeCount:
            THROWJSONBADUSAGE( "JsoValue::FromJSONStream(): invalid value type [%hhu].", JvtGetValueType() );
            break;
        }    
    }
    template < class t_tyJsonOutputStream >
    void ToJSONStream( JsonValueLife< t_tyJsonOutputStream > & _jvl ) const
    {
        switch( JvtGetValueType() )
        {
        case ejvtNull:
        case ejvtTrue:
        case ejvtFalse:
            break; // nothing to do - _jvl has already been created with the correct value type.
        case ejvtNumber:
        case ejvtString:
            _jvl.RJvGet().PCreateStringValue()->assign( StrGet() );
            break;
        case ejvtObject:
            _ObjectGet().ToJSONStream( _jvl );
            break;
        case ejvtArray:
            _ArrayGet().ToJSONStream( _jvl );
            break;
        default:
        case ejvtJsonValueTypeCount:
            THROWJSONBADUSAGE( "JsoValue::ToJSONStream(): invalid value type [%hhu].", JvtGetValueType() );
            break;
        }    
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
#if 0
    // This will either create, change or leave everything the same.
    JsoValue & CreateEl( _tyLPCSTR _psz, EJsonValueType _jvt )
    {
        return _ObjectGet().GetEl( _psz );
    }
#endif //0

    iterator begin()
    {
        if ( ejvtObject == JvtGetValueType() )
            return iterator( _ObjectGet().begin() );
        else
        if ( ejvtArray == JvtGetValueType() )
            return iterator( _ArrayGet().begin() );
        else
            THROWJSONBADUSAGE( "JsoValue::begin(): Called on non-aggregate." );
    }
    const_iterator begin() const
    {
        if ( ejvtObject == JvtGetValueType() )
            return const_iterator( _ObjectGet().begin() );
        else
        if ( ejvtArray == JvtGetValueType() )
            return const_iterator( _ArrayGet().begin() );
        else
            THROWJSONBADUSAGE( "JsoValue::begin(): Called on non-aggregate." );
    }
    iterator end()
    {
        if ( ejvtObject == JvtGetValueType() )
            return iterator( _ObjectGet().end() );
        else
        if ( ejvtArray == JvtGetValueType() )
            return iterator( _ArrayGet().end() );
        else
            THROWJSONBADUSAGE( "JsoValue::end(): Called on non-aggregate." );
    }
    const_iterator end() const
    {
        if ( ejvtObject == JvtGetValueType() )
            return const_iterator( _ObjectGet().end() );
        else
        if ( ejvtArray == JvtGetValueType() )
            return const_iterator( _ArrayGet().end() );
        else
            THROWJSONBADUSAGE( "JsoValue::end(): Called on non-aggregate." );
    }

protected:
    template < class t_tyNum >
    void _SetValue( _tyLPCSTR _pszFmt, t_tyNum _num )
    {
      const int knNum = 512;
      char rgcNum[ knNum ];
      int nPrinted = snprintf( rgcNum, knNum, _pszFmt, _num );
      assert( nPrinted < knNum );
      SetValueType( ejvtNumber );
      StrGet().assign( rgcNum, std::min( nPrinted, knNum-1 ) );
    }
    void _ClearValue()
    {
        EJsonValueType jvt = m_jvtType;
        m_jvtType = ejvtJsonValueTypeCount;
        switch( jvt )
        {
        case ejvtNumber:
        case ejvtString:
            static_cast< _tyStrWRsv * >( (void*)m_rgbyValBuf )->~_tyStrWRsv();
            break;
        case ejvtObject:
            static_cast< _tyJsoObject * >( (void*)m_rgbyValBuf )->~_tyJsoObject();
            break;
        case ejvtArray:
            static_cast< _tyJsoArray * >( (void*)m_rgbyValBuf )->~_tyJsoArray();
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
        assert( ejvtJsonValueTypeCount == JvtGetValueType() );
        switch( _jvt )
        {
        case ejvtNumber:
        case ejvtString:
            new( m_rgbyValBuf ) _tyStrWRsv();
            break;
        case ejvtObject:
            new( m_rgbyValBuf ) _tyJsoObject();
            break;
        case ejvtArray:
            new( m_rgbyValBuf ) _tyJsoArray();
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
    static constexpr size_t s_kstSizeValBuf = std::max( sizeof(_tyStrWRsv), std::max( sizeof(_tyJsoObject), sizeof(_tyJsoArray) ) );
    uint8_t m_rgbyValBuf[ s_kstSizeValBuf ]; // We aren't initializing this on purpose.
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
    typedef JsonCharTraits< _tyChar > _tyCharTraits;
    typedef const t_tyChar * _tyLPCSTR;
    typedef std::basic_string< t_tyChar > _tyStdStr;
    typedef StrWRsv< _tyStdStr > _tyStrWRsv; // string with reserve buffer.
    typedef JsoValue< t_tyChar > _tyJsoValue;
    typedef std::map< _tyStrWRsv, _tyJsoValue > _tyMapValues;
    typedef typename _tyMapValues::iterator _tyIterator;
    typedef typename _tyMapValues::const_iterator _tyConstIterator;
    typedef typename _tyMapValues::value_type _tyMapValueType;

    _JsoObject() = default;
    ~_JsoObject() = default;
    _JsoObject( const _JsoObject & ) = default;
    _JsoObject &operator=( const _JsoObject & ) = default;
    _JsoObject( _JsoObject && ) = default;
    _JsoObject & operator =( _JsoObject && ) = default;

#ifdef NDEBUG
    void AssertValid( bool _fRecursive ) { }
#else
    void AssertValid( bool _fRecursive )
    {
        _tyConstIterator itCur = m_mapValues.begin();
        const _tyConstIterator itEnd = m_mapValues.end();
        for ( ; itCur != itEnd; ++itCur )
        {
            _tyChar tc = itCur->first[0];
            itCur->first[0] = 'a';
            itCur->first[0] = tc;
            if ( _fRecursive )
                itCur->second.AssertValid( true );
            else
                assert( ejvtJsonValueTypeCount != itCur->second.JvtGetValueType() );
        }
    }
#endif

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

    std::strong_ordering operator <=> ( _tyThis const & _r ) const
    {
        return m_mapValues <=> _r.m_mapValues;
    }

    template < class t_tyJsonInputStream >
    void FromJSONStream( JsonReadCursor< t_tyJsonInputStream > & _jrc )
    {
        assert( m_mapValues.empty() ); // Note that this isn't required just that it is expected. Remove assertion if needed.
        JsonRestoreContext< t_tyJsonInputStream > rxc( _jrc );
        if ( !_jrc.FMoveDown() )
            THROWJSONBADUSAGE( "_JsoObject::FromJSONStream(EJsonValueType): FMoveDown() returned false unexpectedly." );
        for ( ; !_jrc.FAtEndOfAggregate(); (void)_jrc.FNextElement() )
        {
            _tyStrWRsv strKey;
            EJsonValueType jvt;
            bool f = _jrc.FGetKeyCurrent( strKey, jvt );
            if ( !f )
                THROWJSONBADUSAGE( "_JsoObject::FromJSONStream(EJsonValueType): FGetKeyCurrent() returned false unexpectedly." );
            _tyJsoValue jvValue( jvt );
            jvValue.FromJSONStream( _jrc );
            std::pair< _tyIterator, bool > pib = m_mapValues.try_emplace( std::move( strKey ), std::move( jvValue ) );
            if ( !pib.second ) // key already exists.
                THROWBADJSONSTREAM( "_JsoObject::FromJSONStream(EJsonValueType): Duplicate key found[%s].", strKey.c_str() );
        }
    }
    template < class t_tyJsonInputStream, class t_tyFilter >
    void FromJSONStream( JsonReadCursor< t_tyJsonInputStream > & _jrc, _tyJsoValue const & _rjvContainer, t_tyFilter & _rfFilter )
    {
        assert( m_mapValues.empty() ); // Note that this isn't required just that it is expected. Remove assertion if needed.
        if ( !_rfFilter( _rjvContainer, _jrc ) )
            return; // Leave this object empty.
        JsonRestoreContext< t_tyJsonInputStream > rxc( _jrc );
        if ( !_jrc.FMoveDown() )
            THROWJSONBADUSAGE( "_JsoObject::FromJSONStream(EJsonValueType): FMoveDown() returned false unexpectedly." );
        for ( ; !_jrc.FAtEndOfAggregate(); (void)_jrc.FNextElement() )
        {
            if ( !_rfFilter( _rjvContainer, _jrc ) )
                continue; // Skip this element.
            _tyStrWRsv strKey;
            EJsonValueType jvt;
            bool f = _jrc.FGetKeyCurrent( strKey, jvt );
            if ( !f )
                THROWJSONBADUSAGE( "_JsoObject::FromJSONStream(EJsonValueType): FGetKeyCurrent() returned false unexpectedly." );
            _tyJsoValue jvValue( jvt );
            jvValue.FromJSONStream( _jrc );
            std::pair< _tyIterator, bool > pib = m_mapValues.try_emplace( std::move( strKey ), std::move( jvValue ) );
            if ( !pib.second ) // key already exists.
                THROWBADJSONSTREAM( "_JsoObject::FromJSONStream(EJsonValueType): Duplicate key found[%s].", strKey.c_str() );
        }
    }
    
    template < class t_tyJsonOutputStream >
    void ToJSONStream( JsonValueLife< t_tyJsonOutputStream > & _jvl ) const
    {
        _tyConstIterator itCur = m_mapValues.begin();
        const _tyConstIterator itEnd = m_mapValues.end();
        for ( ; itCur !=itEnd; ++itCur )
        {
            JsonValueLife< t_tyJsonOutputStream > jvlObjectElement( _jvl, itCur->first.c_str(), itCur->second.JvtGetValueType() );
            itCur->second.ToJSONStream( jvlObjectElement );
        }
    }
protected:
    _tyMapValues m_mapValues;
};

// _JsoArray:
// This is internal impl only - never exposed to the user of JsoValue.
template < class t_tyChar >
class _JsoArray
{
    typedef _JsoArray _tyThis;
public:
    typedef t_tyChar _tyChar;
    typedef const t_tyChar * _tyLPCSTR;
    typedef std::basic_string< t_tyChar > _tyStdStr;
    typedef StrWRsv< _tyStdStr > _tyStrWRsv; // string with reserve buffer.
    typedef JsoValue< t_tyChar > _tyJsoValue;
    typedef std::vector< _tyJsoValue > _tyVectorValues;
    typedef typename _tyVectorValues::iterator _tyIterator;
    typedef typename _tyVectorValues::const_iterator _tyConstIterator;
    typedef typename _tyVectorValues::value_type _tyVectorValueType;

    _JsoArray() = default;
    ~_JsoArray() = default;
    _JsoArray( const _JsoArray & ) = default;
    _JsoArray &operator=( const _JsoArray & ) = default;
    _JsoArray( _JsoArray && ) = default;
    _JsoArray & operator =( _JsoArray && ) = default;

    void Clear()
    {
        m_vecValues.clear();
    }
    _tyIterator begin()
    {
        return m_vecValues.begin();
    }
    _tyConstIterator begin() const
    {
        return m_vecValues.begin();
    }
    _tyIterator end()
    {
        return m_vecValues.end();
    }
    _tyConstIterator end() const
    {
        return m_vecValues.end();
    }

    // Throws if there is no such el.
    _tyVectorValueType & GetEl( size_t _st )
    {
        if ( _st > m_vecValues.size() )
            THROWJSONBADUSAGE( "_JsoArray::GetEl(): _st[%zu] exceeds array size[%zu].", _st, m_vecValues.size() );
        _tyIterator it = m_vecValues.find( _psz );
        if ( m_vecValues.end() == it )
            THROWJSONBADUSAGE( "_JsoArray::GetEl(): No such key [%s]", _psz );
    }
    const _tyVectorValueType & GetEl( _tyLPCSTR _psz ) const
    {
        return const_cast< _tyThis * >( this )->GetEl( _psz );
    }

    std::strong_ordering operator <=> ( _tyThis const & _r ) const
    {
        return m_vecValues <=> _r.m_vecValues;
    }

    template < class t_tyJsonInputStream >
    void FromJSONStream( JsonReadCursor< t_tyJsonInputStream > & _jrc )
    {
        assert( m_vecValues.empty() ); // Note that this isn't required just that it is expected. Remove assertion if needed.
        JsonRestoreContext< t_tyJsonInputStream > rxc( _jrc );
        if ( !_jrc.FMoveDown() )
            THROWJSONBADUSAGE( "_JsoArray::FromJSONStream(EJsonValueType): FMoveDown() returned false unexpectedly." );
        for ( ; !_jrc.FAtEndOfAggregate(); (void)_jrc.FNextElement() )
        {
            _tyJsoValue jvValue( _jrc.JvtGetValueType() );
            jvValue.FromJSONStream( _jrc );
            m_vecValues.emplace_back( std::move( jvValue ) );
        }
    }
    template < class t_tyJsonInputStream, class t_tyFilter >
    void FromJSONStream( JsonReadCursor< t_tyJsonInputStream > & _jrc, _tyJsoValue const & _rjvContainer, t_tyFilter & _rfFilter )
    {
        assert( m_vecValues.empty() ); // Note that this isn't required just that it is expected. Remove assertion if needed.
        if ( !_rfFilter( _rjvContainer, _jrc ) )
            return; // Leave this array empty.
        JsonRestoreContext< t_tyJsonInputStream > rxc( _jrc );
        if ( !_jrc.FMoveDown() )
            THROWJSONBADUSAGE( "_JsoArray::FromJSONStream(EJsonValueType): FMoveDown() returned false unexpectedly." );
        for ( ; !_jrc.FAtEndOfAggregate(); (void)_jrc.FNextElement() )
        {
            if ( !_rfFilter( _rjvContainer, _jrc ) )
                return; // Skip this array element.
            _tyJsoValue jvValue( _jrc.JvtGetValueType() );
            jvValue.FromJSONStream( _jrc );
            m_vecValues.emplace_back( std::move( jvValue ) );
        }
    }

    template < class t_tyJsonOutputStream >
    void ToJSONStream( JsonValueLife< t_tyJsonOutputStream > & _jvl ) const
    {
        _tyConstIterator itCur = m_vecValues.begin();
        _tyConstIterator itEnd = m_vecValues.end();
        for ( ; itCur != itEnd; ++itCur )
        {
            JsonValueLife< t_tyJsonOutputStream > jvlArrayElement( _jvl, itCur->JvtGetValueType() );
            itCur->ToJSONStream( jvlArrayElement );
        }
    }
protected:
    _tyVectorValues m_vecValues;
};

namespace n_JSONObjects
{

// Read data from a ReadCursor into a JSON object.
template < class t_tyJsonInputStream >
void StreamReadJsoValue( JsonReadCursor< t_tyJsonInputStream > & _jrc, JsoValue< typename t_tyJsonInputStream::_tyChar > & _jv )
{
    _jv.FromJSONStream( _jrc );
}

template < class t_tyJsonInputStream >
auto JsoValueStreamRead( JsonReadCursor< t_tyJsonInputStream > & _jrc )
    -> JsoValue< typename t_tyJsonInputStream::_tyChar >
{
    JsoValue< typename t_tyJsonInputStream::_tyChar > jv( _jrc.JvtGetValueType() );
    jv.FromJSONStream( _jrc );
    return jv; // we expect clang/gcc to employ NRVO.
}

template < class t_tyJsonInputStream, class t_tyJsonOutputStream >
struct StreamJSONObjects
{
    typedef t_tyJsonInputStream _tyJsonInputStream;
    typedef t_tyJsonOutputStream _tyJsonOutputStream;
    typedef typename _tyJsonInputStream::_tyCharTraits _tyCharTraits;
    static_assert( std::is_same_v< _tyCharTraits, typename _tyJsonOutputStream::_tyCharTraits > );
    typedef JsonFormatSpec< _tyCharTraits > _tyJsonFormatSpec;
    typedef JsonReadCursor< _tyJsonInputStream > _tyJsonReadCursor;
    typedef JsonValueLife< _tyJsonOutputStream > _tyJsonValueLife;
    typedef std::pair< const char *, int > _tyPrFilenameFd;

    static void Stream( const char * _pszInputFile, _tyPrFilenameFd _prfnfdOutput, bool _fReadOnly, bool _fCheckSkippedKey, const _tyJsonFormatSpec * _pjfs )
    {
        typedef JsoValue< typename t_tyJsonInputStream::_tyChar > _tyJsoValue;
        _tyJsoValue jvRead;
        {//B
            _tyJsonInputStream jis;
            jis.Open( _pszInputFile );
            _tyJsonReadCursor jrc;
            jis.AttachReadCursor( jrc );
            jvRead.SetValueType( jrc.JvtGetValueType() );
            jvRead.FromJSONStream( jrc );
        }//EB

        if ( !_fReadOnly )
        {
            // Open the write file to which we will be streaming JSON.
            _tyJsonOutputStream jos;
            if ( !!_prfnfdOutput.first )
                jos.Open( _prfnfdOutput.first ); // Open by default will truncate the file.
            else
                jos.AttachFd( _prfnfdOutput.second );
            _tyJsonValueLife jvl( jos, jvRead.JvtGetValueType(), _pjfs );
            jvRead.ToJSONStream( jvl );
        }
    }
    static void Stream( int _fdInput, _tyPrFilenameFd _prfnfdOutput, bool _fReadOnly, bool _fCheckSkippedKey, const _tyJsonFormatSpec * _pjfs )
    {
        typedef JsoValue< typename t_tyJsonInputStream::_tyChar > _tyJsoValue;
        _tyJsoValue jvRead;
        {//B
            _tyJsonInputStream jis;
            jis.AttachFd( _fdInput );
            _tyJsonReadCursor jrc;
            jis.AttachReadCursor( jrc );
            jvRead.SetValueType( jrc.JvtGetValueType() );
            jvRead.FromJSONStream( jrc );
        }//EB

        if ( !_fReadOnly )
        {
            // Open the write file to which we will be streaming JSON.
            _tyJsonOutputStream jos;
            if ( !!_prfnfdOutput.first )
                jos.Open( _prfnfdOutput.first ); // Open by default will truncate the file.
            else
                jos.AttachFd( _prfnfdOutput.second );
            _tyJsonValueLife jvl( jos, jvRead.JvtGetValueType(), _pjfs );
            jvRead.ToJSONStream( jvl );
        }
    }
    static void Stream( const char * _pszInputFile, const char * _pszOutputFile, bool _fReadOnly, bool _fCheckSkippedKey, const _tyJsonFormatSpec * _pjfs )
    {
        typedef JsoValue< typename t_tyJsonInputStream::_tyChar > _tyJsoValue;
        _tyJsoValue jvRead;
        {//B
            _tyJsonInputStream jis;
            jis.Open( _pszInputFile );
            _tyJsonReadCursor jrc;
            jis.AttachReadCursor( jrc );
            jvRead.SetValueType( jrc.JvtGetValueType() );
            jvRead.FromJSONStream( jrc );
        }//EB

        if ( !_fReadOnly )
        {
            // Open the write file to which we will be streaming JSON.
            _tyJsonOutputStream jos;
            jos.Open( _pszOutputFile ); // Open by default will truncate the file.
            _tyJsonValueLife jvl( jos, jvRead.JvtGetValueType(), _pjfs );
            jvRead.ToJSONStream( jvl );
        }
    }
    static void Stream( int _fdInput, const char * _pszOutputFile, bool _fReadOnly, bool _fCheckSkippedKey, const _tyJsonFormatSpec * _pjfs )
    {
        typedef JsoValue< typename t_tyJsonInputStream::_tyChar > _tyJsoValue;
        _tyJsoValue jvRead;
        {//B
            _tyJsonInputStream jis;
            jis.AttachFd( _fdInput );
            _tyJsonReadCursor jrc;
            jis.AttachReadCursor( jrc );
            jvRead.SetValueType( jrc.JvtGetValueType() );
            jvRead.FromJSONStream( jrc );
        }//EB

        if ( !_fReadOnly )
        {
            // Open the write file to which we will be streaming JSON.
            _tyJsonOutputStream jos;
            jos.Open( _pszOutputFile ); // Open by default will truncate the file.
            _tyJsonValueLife jvl( jos, jvRead.JvtGetValueType(), _pjfs );
            jvRead.ToJSONStream( jvl );
        }
    }
};

} // namespace n_JSONObjects
