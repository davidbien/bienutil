#pragma once

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _pairfxn.h

// Select members from a pair for passing to a unary function.

__BIENUTIL_BEGIN_NAMESPACE

template < class t_TyPair, class t_TyUnaryFunction >
struct _unary2nd 
{
	t_TyUnaryFunction	m_f1;

	_unary2nd( t_TyUnaryFunction const & _rUnary )
		: m_f1( _rUnary )
	{
	}
	_unary2nd( _unary2nd const & _r )
		: m_f1( _r.m_f1 )
	{
	}

  typename t_TyUnaryFunction::result_type
	operator()( const t_TyPair & __x ) const 
	{
    return m_f1( __x.second );
  }
};

template < class t_TyPair, class t_TyUnaryFunction >
_unary2nd< t_TyPair, t_TyUnaryFunction >
unary2nd( t_TyUnaryFunction const & _f, t_TyPair const & )
{
	return _unary2nd< t_TyPair, t_TyUnaryFunction >( _f );
}

template < class t_TyPair, class t_TyUnaryFunction >
struct _unary1st
{
	t_TyUnaryFunction	m_f1;

	_unary1st( t_TyUnaryFunction const & _rUnary )
		: m_f1( _rUnary )
	{
	}
	_unary1st( _unary1st const & _r )
		: m_f1( _r.m_f1 )
	{
	}

  typename t_TyUnaryFunction::result_type
	operator()( const t_TyPair & __x ) const 
	{
    return m_f1( __x.first );
  }
};

template < class t_TyPair, class t_TyUnaryFunction >
_unary1st< t_TyPair, t_TyUnaryFunction >
unary1st( t_TyUnaryFunction const & _f, t_TyPair const & )
{
	return _unary1st< t_TyPair, t_TyUnaryFunction >( _f );
}

__BIENUTIL_END_NAMESPACE
