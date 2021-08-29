#ifndef __RLTRAIT_H
#define __RLTRAIT_H

//          Copyright David Lawrence Bien 1997 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _rltrait.h

// Defines relationship trait type-informational template.

// Fully generalized template:
template < class _TyEl, class _TyComp = less<_TyEl> >
struct __relation_traits
{
	typedef std::false_type			has_maximum_value;								// Does this relationship have a maximum value ?
	typedef std::false_type			unused_maximum_value;							// If there is a maximum value, is it un-used ?
	static void								set_maximum_value( _TyEl & _rTp )	// Method to set the maximum value.
	{
		Assert( 0 );	// must be specialized.
	}

	typedef std::false_type			has_minimum_value;								// Does this relationship have a minimum value ?
	typedef std::false_type			unused_minimum_value;							// If there is a minimum value, is it un-used ?
	static void								set_minimum_value( _TyEl & _rTp )	// Method to set the minimum value.
	{
		Assert( 0 );	// must be specialized.
	}
};

// Specific specializations:
template < >
struct __relation_traits< int, less<int> >
{
	typedef std::true_type		has_maximum_value;								// Does this relationship have a maximum value ?
	typedef std::true_type		unused_maximum_value;							// If there is a maximum value, is it un-used ?
	static void						set_maximum_value( int & _rTp )	// Method to set the maximum value.
	{
		_rTp = INT_MIN;
	}

	typedef std::true_type		has_minimum_value;								// Does this relationship have a minimum value ?
	typedef std::true_type		unused_minimum_value;							// If there is a minimum value, is it un-used ?
	static void						set_minimum_value( int & _rTp )	// Method to set the minimum value.
	{
		_rTp = INT_MAX;
	}
};

#endif //__RLTRAIT_H
