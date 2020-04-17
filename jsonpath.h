#pragma once

// jsonpath.h
// JSON path objects and algorithms.
// dbien 09APR2020

#include <cstdint>

// JsonPath:
// This represents the complete path object.
template < class t_tyChar >
class JsonPath
{
    typedef JsonPath _tyThis;
public:
    typedef t_tyChar _tyChar;
    typedef std::basic_string< _tyChar > _tyStdStr;
    typedef StrWRsv< _tyStdStr > _tyStrWRsv;

    JsonPath() = default;


};

// _JsonPathElement:
// This reprsents a segment of a JaonPath.
// (.|..)( [name|*] | [ '['range|index|setofnames|expression']' ] )
// When array notation is used then any leading single dot is redundant.
template < class t_tyChar >
class _JsonPathElement
{
    typedef _JsonPathElement _tyThis;
public:
    typedef t_tyChar _tyChar;
    typedef std::basic_string< _tyChar > _tyStdStr;
    typedef StrWRsv< _tyStdStr > _tyStrWRsv;

};

// _JsonExpression
// An expression that is within square brackets.
// Note that any numeric constant without a decimal point '.' will be treated as an integer
//  and will cause *truncation* of any corresponding JSON value.
template < class t_tyChar >
class _JsonExpression
{
    typedef _JsonExpression _tyThis;
public:
    typedef t_tyChar _tyChar;
    typedef std::basic_string< _tyChar > _tyStdStr;
    typedef StrWRsv< _tyStdStr > _tyStrWRsv;

};

enum _EJsonExprType : uint8_t
{
// Leaves - these form the basis of any expression.
    ejetConstant, // A constant value. This is a JsoValue.
    ejetArraySlice, // An array slice.
    ejetSetOfKeys, // A set of 2 more more keys - referred to sometimes as a union - one or more of these names maybe matched.
    ejetSingleKey, // a key must exist - not that if a key is included in an equation then we cannot match without it - keeping in mind that ORs are conditional.
    ejetLastLeaf = ejetSingleKey,
// Unary:
    ejetNot,
    ejetBitwiseNot,
    ejetLastUnary = ejetBitwiseNot,
// Binary:
    ejetEquals,
    ejetLessThan,
    ejetLessThanOrEqual,
    ejetGreaterThan,
    ejetGreaterThanOrEqual,
    ejetEJsonExprType // Leave this at the end.
};
typedef _EJsonExprType EJsonExprType;

//_JsonExprOpBase
// A single operation within a JsonExpression.
// Abstract base class.
template < class t_tyChar >
class _JsonExprOpBase
{
    typedef _JsonExprOpBase _tyThis;
public:
    typedef t_tyChar _tyChar;
    typedef std::basic_string< _tyChar > _tyStdStr;
    typedef StrWRsv< _tyStdStr > _tyStrWRsv;

    virtual ~_JsonExprOpBase() = 0;
    _JsonExprOpBase( EJsonExprType _jet = ejetEJsonExprType )
        : m_jet( _jet )
    {
    }

protected:
    EJsonExprType m_jet{ejetEJsonExprType};
};

//_JsonExprArgBase<T,0>:
// A single operation within a JsonExpression.
// Abstract base class.
template < class t_tyChar, size_t t_knArgs >
class _JsonExprArgBase : public _JsonExprOpBase< t_tyChar >
{
    typedef _JsonExprOpBase< t_tyChar > _tyBase;
    typedef _JsonExprArgBase _tyThis;
public:
    typedef t_tyChar _tyChar;
    typedef std::basic_string< _tyChar > _tyStdStr;
    typedef StrWRsv< _tyStdStr > _tyStrWRsv;

    virtual ~_JsonExprArgBase() = 0;
    _JsonExprArgBase( EJsonExprType _jet = ejetEJsonExprType )
        : _tyBase( _jet )
    {
    }

protected:
};

//_JsonExprArgBase<T,1>:
// A single operation within a JsonExpression.
// Abstract base class.
template < class t_tyChar >
class _JsonExprArgBase< t_tyChar, 1 > : public _JsonExprOpBase< t_tyChar >
{
    typedef _JsonExprOpBase< t_tyChar > _tyBase;
    typedef _JsonExprArgBase _tyThis;
public:
    typedef _JsonExprOpBase< t_tyChar > _tyExprOpBase;
    typedef t_tyChar _tyChar;
    typedef std::basic_string< _tyChar > _tyStdStr;
    typedef StrWRsv< _tyStdStr > _tyStrWRsv;

    virtual ~_JsonExprArgBase() = 0;
    _JsonExprArgBase( EJsonExprType _jet = ejetEJsonExprType )
        : _tyBase( _jet )
    {
    }

protected:
    _tyExprOpBase * m_peopArg0{nullptr};
};

//_JsonExprArgBase<T,2>:
// A single operation within a JsonExpression.
// Abstract base class.
template < class t_tyChar >
class _JsonExprArgBase< t_tyChar, 2 > : public _JsonExprOpBase< t_tyChar >
{
    typedef _JsonExprOpBase< t_tyChar > _tyBase;
    typedef _JsonExprArgBase _tyThis;
public:
    typedef _JsonExprOpBase< t_tyChar > _tyExprOpBase;
    typedef t_tyChar _tyChar;
    typedef std::basic_string< _tyChar > _tyStdStr;
    typedef StrWRsv< _tyStdStr > _tyStrWRsv;

    virtual ~_JsonExprArgBase() = 0;
    _JsonExprArgBase( EJsonExprType _jet = ejetEJsonExprType )
        : _tyBase( _jet )
    {
    }

protected:
    _tyExprOpBase * m_peopArg0{nullptr};
    _tyExprOpBase * m_peopArg1{nullptr};
};

// Leaves:
// _JsonExprLeafConstant:
template < class t_tyChar >
class _JsonExprLeafConstant : public _JsonExprArgBase< t_tyChar, 0 >
{
    typedef _JsonExprArgBase< t_tyChar, 0 > _tyBase;
    typedef  _JsonExprLeafConstant _tyThis;
public:
    typedef t_tyChar _tyChar;
    typedef std::basic_string< _tyChar > _tyStdStr;
    typedef StrWRsv< _tyStdStr > _tyStrWRsv;
    typedef JsoValue< _tyChar > _tyJsoValue;

    _JsonExprLeafConstant()
        : _tyBase( ejetConstant )
    {
    }

protected:
    _tyJsoValue m_jvValue;
};

template < class t_tyChar >
class _JsonExprLeafArraySlice : public _JsonExprArgBase< t_tyChar, 0 >
{
    typedef _JsonExprArgBase< t_tyChar, 0 > _tyBase;
    typedef  _JsonExprLeafArraySlice _tyThis;
public:
    typedef t_tyChar _tyChar;
    typedef std::basic_string< _tyChar > _tyStdStr;
    typedef StrWRsv< _tyStdStr > _tyStrWRsv;
    typedef JsoValue< _tyChar > _tyJsoValue;

    _JsonExprLeafArraySlice()
        : _tyBase( ejetArraySlice )
    {
    }

protected:
    typedef int32_t _tyIdxType;
    _tyIdxType m_idxBegin{};
    _tyIdxType m_idxEnd{};
    _tyIdxType m_idxStep{};
};

// _JsonExprLeafSingleKey:
template < class t_tyChar >
class _JsonExprLeafSingleKey : public _JsonExprArgBase< t_tyChar, 0 >
{
    typedef _JsonExprArgBase< t_tyChar, 0 > _tyBase;
    typedef  _JsonExprLeafSingleKey _tyThis;
public:
    typedef t_tyChar _tyChar;
    typedef std::basic_string< _tyChar > _tyStdStr;
    typedef StrWRsv< _tyStdStr > _tyStrWRsv;
    typedef JsoValue< _tyChar > _tyJsoValue;

    _JsonExprLeafSingleKey()
        : _tyBase( ejetSingleKey )
    {
    }

protected:
    _tyStrWRsv m_strKey;
};

// _JsonExprLeafSetOfKeys:
// This leaf has limited modifiers. There is no syntax for "!"(not) for instance currently.
template < class t_tyChar >
class _JsonExprLeafSetOfKeys : public _JsonExprArgBase< t_tyChar, 0 >
{
    typedef _JsonExprArgBase< t_tyChar, 0 > _tyBase;
    typedef  _JsonExprLeafSetOfKeys _tyThis;
public:
    typedef t_tyChar _tyChar;
    typedef std::basic_string< _tyChar > _tyStdStr;
    typedef StrWRsv< _tyStdStr > _tyStrWRsv;
    typedef JsoValue< _tyChar > _tyJsoValue;
    typedef std::vector< _tyStrWRsv > _tyRgKeys;

    _JsonExprLeafSetOfKeys()
        : _tyBase( ejetSetOfKeys )
    {
    }

protected:
    _tyRgKeys m_rgKeys;
};

// Unary operators:
// _JsonExprOpNot:
template < class t_tyChar >
class _JsonExprOpNot : public _JsonExprArgBase< t_tyChar, 1 >
{
    typedef _JsonExprArgBase< t_tyChar, 1 > _tyBase;
    typedef  _JsonExprOpNot _tyThis;
public:
    typedef t_tyChar _tyChar;
    typedef std::basic_string< _tyChar > _tyStdStr;
    typedef StrWRsv< _tyStdStr > _tyStrWRsv;
    typedef JsoValue< _tyChar > _tyJsoValue;

    _JsonExprOpNot()
        : _tyBase( ejetNot )
    {
    }
};
// _JsonExprOpBitwiseNot:
template < class t_tyChar >
class _JsonExprOpBitwiseNot : public _JsonExprArgBase< t_tyChar, 1 >
{
    typedef _JsonExprArgBase< t_tyChar, 1 > _tyBase;
    typedef  _JsonExprOpBitwiseNot _tyThis;
public:
    typedef t_tyChar _tyChar;
    typedef std::basic_string< _tyChar > _tyStdStr;
    typedef StrWRsv< _tyStdStr > _tyStrWRsv;
    typedef JsoValue< _tyChar > _tyJsoValue;

    _JsonExprOpBitwiseNot()
        : _tyBase( ejetBitwiseNot )
    {
    }
};

// Binary operators:
// _JsonExprOpEquals:
template < class t_tyChar >
class _JsonExprOpEquals : public _JsonExprArgBase< t_tyChar, 2 >
{
    typedef _JsonExprArgBase< t_tyChar, 1 > _tyBase;
    typedef  _JsonExprOpEquals _tyThis;
public:
    typedef t_tyChar _tyChar;
    typedef std::basic_string< _tyChar > _tyStdStr;
    typedef StrWRsv< _tyStdStr > _tyStrWRsv;
    typedef JsoValue< _tyChar > _tyJsoValue;

    _JsonExprOpEquals()
        : _tyBase( ejetEquals )
    {
    }
};
// _JsonExprOpLessThan:
template < class t_tyChar >
class _JsonExprOpLessThan : public _JsonExprArgBase< t_tyChar, 2 >
{
    typedef _JsonExprArgBase< t_tyChar, 1 > _tyBase;
    typedef  _JsonExprOpLessThan _tyThis;
public:
    typedef t_tyChar _tyChar;
    typedef std::basic_string< _tyChar > _tyStdStr;
    typedef StrWRsv< _tyStdStr > _tyStrWRsv;
    typedef JsoValue< _tyChar > _tyJsoValue;

    _JsonExprOpLessThan()
        : _tyBase( ejetLessThan )
    {
    }
};
// _JsonExprOpLessThanOrEqual:
template < class t_tyChar >
class _JsonExprOpLessThanOrEqual : public _JsonExprArgBase< t_tyChar, 2 >
{
    typedef _JsonExprArgBase< t_tyChar, 1 > _tyBase;
    typedef  _JsonExprOpLessThanOrEqual _tyThis;
public:
    typedef t_tyChar _tyChar;
    typedef std::basic_string< _tyChar > _tyStdStr;
    typedef StrWRsv< _tyStdStr > _tyStrWRsv;
    typedef JsoValue< _tyChar > _tyJsoValue;

    _JsonExprOpLessThanOrEqual()
        : _tyBase( ejetLessThanOrEqual )
    {
    }
};
// _JsonExprOpGreaterThan:
template < class t_tyChar >
class _JsonExprOpGreaterThan : public _JsonExprArgBase< t_tyChar, 2 >
{
    typedef _JsonExprArgBase< t_tyChar, 1 > _tyBase;
    typedef  _JsonExprOpGreaterThan _tyThis;
public:
    typedef t_tyChar _tyChar;
    typedef std::basic_string< _tyChar > _tyStdStr;
    typedef StrWRsv< _tyStdStr > _tyStrWRsv;
    typedef JsoValue< _tyChar > _tyJsoValue;

    _JsonExprOpGreaterThan()
        : _tyBase( ejetGreaterThan )
    {
    }
};
// _JsonExprOpGreaterThanOrEqual:
template < class t_tyChar >
class _JsonExprOpGreaterThanOrEqual : public _JsonExprArgBase< t_tyChar, 2 >
{
    typedef _JsonExprArgBase< t_tyChar, 1 > _tyBase;
    typedef  _JsonExprOpGreaterThanOrEqual _tyThis;
public:
    typedef t_tyChar _tyChar;
    typedef std::basic_string< _tyChar > _tyStdStr;
    typedef StrWRsv< _tyStdStr > _tyStrWRsv;
    typedef JsoValue< _tyChar > _tyJsoValue;

    _JsonExprOpGreaterThanOrEqual()
        : _tyBase( ejetGreaterThanOrEqual )
    {
    }
};
