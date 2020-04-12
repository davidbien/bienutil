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

    virtual ~_JsonExprOpBase() = default;

protected:
    EJsonExprType m_ejet{ejetEJsonExprType};
};

template < class t_tyChar >
class _JsonExprOpConstant : public _JsonExprOpBase< t_tyChar >
{
    typedef _JsonExprOpBase< t_tyChar > _tyBase;
    typedef  _JsonExprOpConstant _tyThis;
public:
    typedef t_tyChar _tyChar;
    typedef std::basic_string< _tyChar > _tyStdStr;
    typedef StrWRsv< _tyStdStr > _tyStrWRsv;
    typedef JsoValue< _tyChar > _tyJsoValue;

protected:
    _tyJsoValue m_jvValue;
};
