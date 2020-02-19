#pragma once

#include <string>
#include <memory>

#include "_dbgthrw.h"
#include "_namdexc.h"

// jsonstrm.h
// This implements JSON streaming in/out.
// dbien: 16FEB2020

// Goals:
//  1) Allow multiple files types but templatize rather than using virtual methods to allow more inlining.
//  2) Stax parser for the input.
//      a) Should allow very large files to be perused without much internal memory usage.
//      b) Read straight into internal data structures via stax streaming rather than using intermediate format.
//      c) Even array values should be streamed in.
//      d) Lazily read values upon demand - but read once and store. This allows skipping around and ignoring data - still have to read it but don't have to store it.
//  3) Output goes to any streaming output object.
//  4) Since output and input rarely if ever mixed no reason to provide both at the same time.

// struct JsonCharTraits: Only specializations of this.
// This describes the type of file that the JSON files is persisted within.
template < class t_tyChar >
struct JsonCharTraits;
template < class t_tyCharTraits >
class JsonValue;
template < class t_tyCharTraits >
class JsonObject;
template < class t_tyCharTraits >
class JsonArray;
template < class t_tyJsonInputStream >
class JsonReadCursor;

#ifndef __JSONSTRM_DEFAULT_ALLOCATOR
#if defined(_USE_STLPORT) && !defined(NDEBUG)
#define __JSONSTRM_DEFAULT_ALLOCATOR __STD::_stlallocator< char, __STD::__debug_alloc< __STD::__malloc_alloc > >
#define __JSONSTRM_GET_ALLOCATOR(t) __STD::_stlallocator< t, __STD::__debug_alloc< __STD::__malloc_alloc > >
#else // defined(_USE_STLPORT) && !defined(NDEBUG)
#define __JSONSTRM_DEFAULT_ALLOCATOR __STD::allocator< char >
#define __JSONSTRM_GET_ALLOCATOR(t) __STD::allocator< t >
#endif // defined(_USE_STLPORT) && !defined(NDEBUG)
#endif //__JSONSTRM_DEFAULT_ALLOCATOR

#ifdef __NAMDDEXC_STDBASE
#pragma push_macro("std")
#undef std
#endif //__NAMDDEXC_STDBASE
class bad_json_stream : public std::_t__Named_exception< __JSONSTRM_DEFAULT_ALLOCATOR >
{
  typedef std::_t__Named_exception< __JSONSTRM_DEFAULT_ALLOCATOR > _TyBase;
public:
  bad_json_stream( const string_type & __s ) : _TyBase( __s ) {}
};
#ifdef __NAMDDEXC_STDBASE
#pragma pop_macro("std")
#endif //__NAMDDEXC_STDBASE

// JsonCharTraits< char > : Traits for 8bit char representation.
template <>
struct JsonCharTraits< char >
{
    typedef char _tyChar;
    typedef _tyChar * _tyLPSTR;
    typedef const _tyChar * _tyLPCSTR;
    typedef std::basic_string< _tyChar > _tyStdStr;

    static const _tyChar s_tcLeftSquareBr = '[';
    static const _tyChar s_tcRightSquareBr = ']';
    static const _tyChar s_tcLeftCurlyBr = '{';
    static const _tyChar s_tcRightCurlyBr = '}';
    static const _tyChar s_tcColon = ':';
    static const _tyChar s_tcComma = ',';
    static const _tyChar s_tcDoubleQuotes = '"';
    static const _tyChar s_tcBackSlash = '\\';
    static const _tyChar s_tcForwardSlash = '/';
    static const _tyChar s_tcMinus = '-';
    static const _tyChar s_tcPlus = '+';
    static const _tyChar s_tc0 = '0';
    static const _tyChar s_tc1 = '1';
    static const _tyChar s_tc2 = '2';
    static const _tyChar s_tc3 = '3';
    static const _tyChar s_tc4 = '4';
    static const _tyChar s_tc5 = '5';
    static const _tyChar s_tc6 = '6';
    static const _tyChar s_tc7 = '7';
    static const _tyChar s_tc8 = '8';
    static const _tyChar s_tc9 = '9';
    static const _tyChar s_tca = 'a';
    static const _tyChar s_tcb = 'b';
    static const _tyChar s_tcc = 'c';
    static const _tyChar s_tcd = 'd';
    static const _tyChar s_tce = 'e';
    static const _tyChar s_tcf = 'f';
    static const _tyChar s_tcA = 'A';
    static const _tyChar s_tcB = 'B';
    static const _tyChar s_tcC = 'C';
    static const _tyChar s_tcD = 'D';
    static const _tyChar s_tcE = 'E';
    static const _tyChar s_tcF = 'F';
    static const _tyChar s_tct = 't';
    static const _tyChar s_tcr = 'r';
    static const _tyChar s_tcu = 'u';
    static const _tyChar s_tcl = 'l';
    static const _tyChar s_tcs = 's';
    static const _tyChar s_tcn = 'n';
    static const _tyChar s_tcu = 'u';    

    // The formatting command to format a single character using printf style.
    static _tyLPCSTR s_szFormatChar; // = "%c";
};

JsonCharTraits< char >::_tyLPCSTR JsonCharTraits< char >::s_szFormatChar = "%c";

// JsonCharTraits< wchar_t > : Traits for 16bit char representation.
template <>
struct JsonCharTraits< wchar_t >
{
    typedef wchar_t _tyChar;
    typedef _tyChar * _tyLPSTR;
    typedef const _tyChar * _tyLPCSTR;
    using _tyStdStr = std::basic_string< _tyChar >;

    static const _tyChar s_tcLeftSquareBr = L'[';
    static const _tyChar s_tcRightSquareBr = L']';
    static const _tyChar s_tcLeftCurlyBr = L'{';
    static const _tyChar s_tcRightCurlyBr = L'}';
    static const _tyChar s_tcColon = L':';
    static const _tyChar s_tcComma = L',';
    static const _tyChar s_tcDoubleQuotes = L'"';
    static const _tyChar s_tcBackSlash = L'\\';
    static const _tyChar s_tcForwardSlash = L'/';
    static const _tyChar s_tcMinus = L'-';
    static const _tyChar s_tcPlus = L'+';
    static const _tyChar s_tc0 = L'0';
    static const _tyChar s_tc1 = L'1';
    static const _tyChar s_tc2 = L'2';
    static const _tyChar s_tc3 = L'3';
    static const _tyChar s_tc4 = L'4';
    static const _tyChar s_tc5 = L'5';
    static const _tyChar s_tc6 = L'6';
    static const _tyChar s_tc7 = L'7';
    static const _tyChar s_tc8 = L'8';
    static const _tyChar s_tc9 = L'9';
    static const _tyChar s_tca = L'a';
    static const _tyChar s_tcb = L'b';
    static const _tyChar s_tcc = L'c';
    static const _tyChar s_tcd = L'd';
    static const _tyChar s_tce = L'e';
    static const _tyChar s_tcf = L'f';
    static const _tyChar s_tcA = L'A';
    static const _tyChar s_tcB = L'B';
    static const _tyChar s_tcC = L'C';
    static const _tyChar s_tcD = L'D';
    static const _tyChar s_tcE = L'E';
    static const _tyChar s_tcF = L'F';
    static const _tyChar s_tct = L't';
    static const _tyChar s_tcr = L'r';
    static const _tyChar s_tcu = L'u';
    static const _tyChar s_tcl = L'l';
    static const _tyChar s_tcs = L's';
    static const _tyChar s_tcn = L'n';
    static const _tyChar s_tcu = L'u';

    // The formatting command to format a single character using printf style.
    static _tyLPCSTR s_szFormatChar; // = L"%c";
};

JsonCharTraits< wchar_t >::_tyLPCSTR JsonCharTraits< wchar_t >::s_szFormatChar = L"%c";

// JsonInputStream: Base class for input streams.
template < class t_tyCharTraits, class t_tyFilePos >
class JsonInputStreamBase
{
    typedef t_tyCharTraits _tyCharTraits;
    typedef t_tyFilePos _tyFilePos;
};

// JsonOutputStream: Base class for output streams.
template < class t_tyCharTraits, class t_tyFilePos >
class JsonOutputStreamBase
{
    typedef t_tyCharTraits _tyCharTraits;
    typedef t_tyFilePos _tyFilePos;
};

// _EJsonValueType: The types of values in a JsonValue object.
enum _EJsonValueType : char
{
    ejvtObject,
    ejvtArray,
    ejvtNumber,
    ejvtString,
    ejvtTrue,
    ejvtFalse,
    ejvtNull,
    ejvtJsonValueTypeCount
};
typedef _EJsonValueType EJsonValueType;

// class JsonValue:
// The base level object for a Json file is a JsonValue - i.e. a JSON file contains a JsonValue as its root object.
template < class t_tyCharTraits >
class JsonValue
{
    typedef JsonValue _tyThis;
public:
    using _tyCharTraits = t_tyCharTraits;
    using _tyStdStr = _tyCharTraits::_tyStdStr;
    using _tyJsonObject = JsonObject< t_tyCharTraits >;
    using _tyJsonArray = JsonArray< t_tyCharTraits >;

    // Note that this JsonValue very well may be referred to by a parent JsonValue.
    // JsonValues must be destructed carefully.

    JsonValue() = default;
    JsonValue( const JsonValue& ) = delete;
    JsonValue& operator=( const JsonValue& ) = delete;

    JsonValue( JsonValue * _pjvParent = nullptr_t, EJsonValueType _jvtType = ejvtJsonValueTypeCount )
        :   m_pjvParent( _pjvParent ),
            m_jvtType( _jvtType )
    {
        assert( !m_pvValue ); // Should have been zeroed by default member initialization on site.
    }

    JsonValue( JsonValue && _rr )
    {
        assert( FIsNull() );
        swap( _rr );
    }
    JsonValue & operator = ( JsonValue && _rr )
    {
        Destroy();
        swap( _rr );
    }
    void swap( JsonValue && _rr )
    {
        std::swap( _rr.m_pjvParent, m_pjvParent );
        std::swap( _rr.m_pvValue, m_pvValue );
        std::swap( _rr.m_jvtType, m_jvtType );
    }
    void Destroy() // Destroy and make null.
    {
        if ( m_pvValue )
        {
            _DestroyValue();
            m_pjvParent = 0;
            m_jvtType = ejvtJsonValueTypeCount;
        }
        assert( FIsNull() );
    }
    void DestroyValue() // Just destroy and nullify any associated dynamically allocated value.
    {
        if ( m_pvValue )
            _DestroyValue();
    }

    void SetPjvParent( const JsonValue * _pjvParent )
    {
        m_pjvParent = _pjvParent;
    }
    const JsonValue * PjvGetParent() const
    {
        return m_pjvParent == this ? 0 : m_pjvParent;
    }
    bool FCheckValidParent() const
    {
        return !!m_pjvParent; // Should always have some parent - it might be ourselves.
    }

    bool FIsNull() const
    {
        return !m_pjvParent && !m_pvValue && ( ejvtJsonValueTypeCount == m_jvtType );
    }
    bool FAtRootValue() const
    {
        return !PjvGetParent();
    }

    // We assume that we have sole use of this input stream - as any such sharing would need complex guarding otherwise.
    // We should never need to backtrack when reading from the stream - we may need to fast forward for sure but not backtrack.
    // As such we should always be right at the point we want to be within the file and should never need to seek backward to reach the point we need to.
    // This means that we shouldn't need to actually store any position context within the read cursor since the presumption is that we are always where we
    //  should be. We will store this info in debug and use this to ensure that we are where we think we should be.
    template < class t_tyInputStream, class t_tyJsonReadCursor >
    void AttachInputStream( t_tyInputStream & _ris, t_tyJsonReadCursor & _rjrc )
    {
        assert( FIsNull() ); // We should be null here - we should be at the root and have no parent.
        // Set the parent to ourselves as a sanity check.
        m_pjvParent = this;
        _rjrc.AttachInputStream( _ris, this );
    }

    // Return the type of the upcoming value from the first character.
    static EJsonValueType GetJvtTypeFromChar( _tyChar _tc )
    {
        switch( _tc )
        {
            case _tyCharTraits::s_tcLeftSquareBr:
                return ejvtArray;
            case _tyCharTraits::s_tcLeftCurlyBr:
                return ejvtObject;
            case _tyCharTraits::s_tcMinus:
            case _tyCharTraits::s_tc0:
            case _tyCharTraits::s_tc1:
            case _tyCharTraits::s_tc2:
            case _tyCharTraits::s_tc3:
            case _tyCharTraits::s_tc4:
            case _tyCharTraits::s_tc5:
            case _tyCharTraits::s_tc6:
            case _tyCharTraits::s_tc7:
            case _tyCharTraits::s_tc8:
            case _tyCharTraits::s_tc9:
                return ejvtNumber;
            case _tyCharTraits::s_tcDoubleQuotes:
                return ejvtString;
            case _tyCharTraits::s_tcf:
                return ejvtFalse;
            case _tyCharTraits::s_tct:
                return ejvtTrue;
            case _tyCharTraits::s_tcn:
                return ejvtNull;
            default:
                return ejvtJsonValueTypeCount;
        }
    }

protected:
    ~JsonValue() // Ensure that this JsonValue is destroyed by a trusted entity.
    {
        if ( m_pvValue )
            _DestroyValue();
    }
    void _DestroyValue()
    {
        void * pvValue = m_pvValue;
        m_pvValue = 0;
        _DestroyValue( pvValue )
    }
    void _DestroyValue( void * _pvValue )
    {
        // We need to destroy any connected objects:
        switch( m_jvtType )
        {
        case ejvtObject:
            delete (_tyJsonObject*)_pvValue;
            break;
        case ejvtArray:
            delete (_tyJsonArray*)_pvValue;
            break;
        case ejvtNumber:
        case ejvtString:
            delete (_tyStdStr*)_pvValue;
            break;
        case ejvtTrue:
        case ejvtFalse:
        case ejvtNull:
        default:
            assert( 0 );
            break;
        }
    }

    const JsonValue * m_pjvParent{}; // We make this const and then const_cast<> as needed.
    void * m_pvValue{}; // Just use void* since all our values are pointers to objects.
    // m_pvValue is one of:
    // for ejvtTrue, ejvtFalse, ejvtNull: nullptr_t
    // for ejvtObject: JsonObject *
    // for ejvtArray: JsonArray *
    // for ejvtNumber, ejvtString: _tyStdStr *
    EJsonValueType m_jvtType{ejvtJsonValueTypeCount}; // Type of the JsonValue.
};

// class JsonObject:
// This represents an object containing key:value pairs.
template < class t_tyCharTraits >
class JsonObject
{
    typedef JsonObject _tyThis;
public:
    using _tyCharTraits = t_tyCharTraits;
    using _tyStdStr = _tyCharTraits::_tyStdStr;

    JsonObject( const JsonValue * _pjvParent )
    {
        m_jsvCur.SetPjvParent( _pjvParent ); // Link the contained value to the parent value. This is a soft reference - we assume any parent will always exist.
    }

protected:
    _tyStdStr m_strCurLabel; // The current label for this object.
    JsonValue m_jsvCur; // The current JsonValue for this object.
};

// JsonArray:
// This represents an JSON array containing multiple values.
template < class t_tyCharTraits >
class JsonArray
{
    typedef JsonArray _tyThis;
public:
    using _tyCharTraits = t_tyCharTraits;
    using _tyStdStr = _tyCharTraits::_tyStdStr;

    JsonArray( const JsonValue * _pjvParent )
    {
        m_jsvCur.SetPjvParent( _pjvParent ); // Link the contained value to the parent value. This is a soft reference - we assume any parent will always exist.
    }

protected:
    JsonValue m_jsvCur; // The current JsonValue for this object.
};

// JsonReadContext:
// This is the context maintained for a read at a given point along the current path.
// The JsonReadCursor maintains a stack of these.
template < class t_tyJsonInputStream >
class JsonReadContext
{
    typedef JsonReadContext _tyThis;
    friend JsonReadCursor< t_tyJsonInputStream >;
public:
    typedef t_tyJsonInputStream _tyJsonInputStream;
    typedef typename t_tyJsonInputStream::_tyCharTraits _tyCharTraits;
    typedef typename _tyCharTraits::_tyChar _tyChar;
    typedef JsonValue< _tyCharTraits > _tyJsonValue;
    typedef typename t_tyJsonInputStream::_tyFilePos _tyFilePos;

    JsonReadContext( _tyJsonValue * _pjvCur = nullptr_t, JsonReadContext * _pjrcPrev = nullptr_t )
        :   m_pjrcPrev( _pjrcPrev ),
            m_pjvCur( _pjvCur )
    {
    }

    // Push _pjrcNewHead as the new head of stack before _pjrcHead.
    static void PushStack( std::unique_ptr< JsonReadContext > & _pjrcHead, std::unique_ptr< JsonReadContext > & _pjrcNewHead )
    {
        assert( !_pjrcHead->m_pjrcPrev );
        assert( !_pjrcNewHead->m_pjrcPrev );
        assert( !_pjrcNewHead->m_pjrcNext );
        _pjrcHead->m_pjrcPrev = this; // soft link.
        _pjrcNewHead->m_pjrcNext.swap( _pjrcHead );
        _pjrcHead.swap( _pjrcNewHead ); // new head is now the head and it points at old head.
    }
    static void PopStack( std::unique_ptr< JsonReadContext > & _pjrcHead )
    {
        std::unique_ptr< JsonReadContext > pjrcOldHead;
        _pjrcHead.swap( pjrcOldHead );
        pjrcOldHead->m_pjrcNext.swap( _pjrcHead );
    }

protected:
    _tyJsonValue * m_pjvCur; // We maintain a soft reference to the current JsonValue at this level.
    std::unique_ptr< JsonReadContext > m_pjrcNext{}; // Implement a simple doubly linked list.
    JsonReadContext * m_pjrcPrev{}; // soft reference to parent in list.
    _tyFilePos m_posPreWhitespace{}; // position at the start of parsing this element - before looking for WS.
    _tyFilePos m_posStartValue{}; // The start of the value for this element - after parsing WS.
    _tyFilePos m_posEndValue{}; // The end of the value for this element - before parsing WS beyond.
    EJsonValueType m_jvtCur{ejvtJsonValueTypeCount}; // The type of this value - because this always corresponds to a value.
    _tyChar m_tcFirst{}; // Only for the number type does this matter but since it does...
};

// class JsonReadCursor:
template < class t_tyJsonInputStream >
class JsonReadCursor
{
    typedef JsonReadCursor _tyThis;
public:
    typedef t_tyJsonInputStream _tyJsonInputStream;
    typedef typename t_tyJsonInputStream::_tyCharTraits _tyCharTraits;
    typedef typename _tyCharTraits::_tyChar _tyChar;
    typedef JsonValue< _tyCharTraits > _tyJsonValue;
    typedef JsonReadContext< _tyCharTraits > _tyJsonReadContext;

    JsonReadCursor() = default;
    JsonReadCursor( JsonReadCursor const & ) = delete;
    JsonReadCursor& operator = ( JsonReadCursor const & ) = delete;

    bool FAttached() const 
    { 
        assert( !m_pjrcCurrent == !m_pis );
        assert( !m_pjrcRootVal == !m_pis );
        assert( !m_pjrcContextStack == !m_pis );
        return !!m_pis;
    }

    // Returns true if the current value is an aggregate value - object or array.
    bool FAtAggregateValue() const 
    {
        assert( !!m_pjrcContextStack );
        return !!m_pjrcContextStack && ( ( ejvtArray == m_pjrcContextStack->m_jvtCur ) || ( ejvtObject == m_pjrcContextStack->m_jvtCur ) );
    }

    // Attach to the root of the JSON value tree.
    // We merely figure out the type of the value at this position.
    void AttachRoot( t_tyJsonInputStream & _ris )
    {
        assert( !FAttached() ); // We shouldn't have attached to the stream yet.
        std::unique_ptr< _tyJsonValue > pjvRootVal = td::make_unique<_tyJsonValue>();
        std::unique_ptr< _tyJsonReadContext > pjrcRoot = std::make_unique<_tyJsonReadContext>( &*pjvRootVal, (_tyJsonReadContext*)nullptr_t );
        pjrcRoot->m_posPreWhitespace = _ris.PosGet();
        _ris.SkipWhitespace();
        pjrcRoot->m_posStartValue = _ris.PosGet();
        assert( !pjrcRoot->m_posEndValue ); // We should have a 0 now - unset - must be >0 when set (invariant).
        if ( _ris.FAtEof() )
        {
            // A completely empty file is not a valid JSON file because it doesn't contain an actual JSON value.
            // So we throw:
            throw bad_json_stream( "Empty JSON file." );
        }
        // The first non-whitespace character tells us what the value type is:
        pjrcRoot->m_tcFirst = _ris.ReadChar();
        pjrcRoot->m_jvtCur = _tyJsonValue::GetJvtTypeFromChar( pjrcRoot->m_tcFirst );
        if ( ejvtJsonValueTypeCount == pjrcRoot->m_jvtCur )
        {
            static const int nbufsize = 1024;
            char buf[ nbufsize+1 ];
            { //B
                char bufFmt[ nbufsize+1 ];
                snprintf( bufFmt, sizeof(bufFmt), "Bad first JSON value character found[%s].", _tyCharTraits::s_szFormatChar );
                bufFmt[nbufsize] = 0;
                snprintf( buf, sizeof(buf), bufFmt, pjrcRoot->m_tcFirst );
                buf[nbufsize] = 0;
            } //EB
            throw bad_json_stream( buf );
        }
        // Set up current state:
        pjvRootVal->SetValueType( pjrcRoot->m_jvtCur );
        m_pjrcContextStack.swap( pjrcRoot ); // swap with any existing stack.
        m_pjrcCurrent = &*m_pjrcContextStack; // current position is soft reference.
        m_pjrcRootVal.swap( pjvRootVal ); // swap in root value for tree.
        m_pis = &_ris;
    }

    bool FMoveDown()
    {
        assert( !!m_pis ); // We should have been attached to a file by now.

        if ( !FAtAggregateValue() )
            return false; // Then we are at a leaf value.

        // If we have a parent then that is where we need to go - nothing to do besides that:
        if ( m_pjrcCurrent->m_pjrcPrev )
        {
            m_pjrcCurrent = m_pjrcCurrent->m_pjrcPrev;
            return true;
        }

        // If we haven't read the current value then we must read it - this is where some actual work gets done...
        // 1) For objects we read the first string and then we read the first character of the corresponding value.
        //      a) For objects of zero content we still create the subobject, it will just return that there are no (key,value) pairs.
        // 2) For arrays we read the first character of the first value in the array.
        //      a) For empty arrays we still create the subobject, it will just return that there are no values in the array.
        // In fact we must not have read the current value since we would have already pushed the context onto the stack and thus we wouldn't be here.
        assert( !m_pjrcCurrent->m_posEndValue );
        // We should be at the start of the value plus 1 character - this is important as we will be registered with the input stream throughout the streaming.
        assert( ( pjrcRoot->m_posStartValue + sizeof(_tyChar) ) == m_pis->PosGet() );

    }

    bool FMoveUp()
    {
        if ( !!m_pjrcCurrent->m_pjrcNext )
        {
            m_pjrcCurrent = &*m_pjrcCurrent->m_pjrcNext;
            return true;
        }
        return false; // ain't nowhere to go.
    }

    void swap( JsonReadCursor & _r )
    {
        std::swap( m_pis, _r.m_pis );
        std::swap( m_pjrcCurrent, _r.m_pjrcCurrent );
        m_pjrcRootVal.swap( _r.m_pjrcRootVal );
        m_pjrcContextStack.swap( _r.m_pjrcContextStack );
    }

protected:
    _tyJsonInputStream m_pis{}; // Soft reference to stream from which we read.
    std::unique_ptr< _tyJsonValue > m_pjrcRootVal; // Hard reference to the root value of the value tree.
    std::unique_ptr< _tyJsonReadContext > m_pjrcContextStack; // Implement a simple doubly linked list.
    _tyJsonReadContext * m_pjrcCurrent{}; // The current cursor position within context stack.
};
