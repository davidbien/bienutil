#ifndef __SETUTIL_H
#define __SETUTIL_H

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _setutil.h

// Set utilities.

__BIENUTIL_BEGIN_NAMESPACE

// Since sets and maps don't have a templatized copy constructor ( or assignment operator )
// ( and since the structural copy is likely faster ) - we have an object that chooses
//	the right system:
template < class t_TySet1, class t_TySet2 >
struct __copy_set
{
	static void	copy( t_TySet1 & _rsetTo, t_TySet2 const & _rsetFrom )
	{
		// Different types - use slower copy:
		_rsetTo.clear();
		_rsetTo.insert( _rsetFrom.begin(), _rsetFrom.end() );
	}
};

template < class t_TySet >
struct __copy_set< t_TySet, t_TySet >
{
	static void copy( t_TySet & _rsetTo, t_TySet const & _rsetFrom )
	{
		// Same types - use structural copy:
		_rsetTo = _rsetFrom;
	}
};

// A version of set union that will output to a set:
template <	class t_TyInputIter1, class t_TyInputIter2, 
						class t_TyOutputSet, class t_TyCompare >
void 
set_set_union(	t_TyInputIter1 __first1, t_TyInputIter1 __last1,
								t_TyInputIter2 __first2, t_TyInputIter2 __last2,
								t_TyOutputSet & _setResult, t_TyCompare __comp) 
{	
	t_TyOutputSet::iterator	itResult = _setResult.end();
	while (__first1 != __last1 && __first2 != __last2) 
	{
    if ( __comp( *__first1, *__first2 ) ) 
		{
			_setResult.insert( itResult, *__first1 );
      ++__first1;
    }
    else if (__comp(*__first2, *__first1)) 
		{
			_setResult.insert( itResult, *__first2 );
      ++__first2;
    }
    else 
		{
			_setResult.insert( itResult, *__first1 );
      ++__first1;
      ++__first2;
    }
  }

	// Now copy remainder of either set:
	for ( ; __first1 != __last1; ++__first1 )
	{
		_setResult.insert( itResult, *__first1 );
	}
	for ( ; __first2 != __last2; ++__first2 )
	{
		_setResult.insert( itResult, *__first2 );
	}
}

template <	class t_TyInputIter1, class t_TyInputIter2, 
						class t_TyOutputSet, class t_TyCompare >
void
set_set_intersection(	t_TyInputIter1 __first1, t_TyInputIter1 __last1,
											t_TyInputIter2 __first2, t_TyInputIter2 __last2,
											t_TyOutputSet & _setResult, t_TyCompare __comp ) 
{
	t_TyOutputSet::iterator	itResult = _setResult.end();
  while ( __first1 != __last1 && __first2 != __last2 )
	{
    if ( __comp( *__first1, *__first2 ) )
		{
      ++__first1;
		}
    else 
		if ( __comp( *__first2, *__first1 ) )
		{
      ++__first2;
		}
    else 
		{
			_setResult.insert( itResult, *__first1 );
      ++__first1;
      ++__first2;
    }
	}
}

__BIENUTIL_END_NAMESPACE

#endif //__SETUTIL_H
