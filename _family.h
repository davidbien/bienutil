#if 0 // !__FAMILIES		// A concept that would require a lot of work :-)

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// This module declares a family that implements 
//	"call function on destruct" functionality.

// families:
// 1) Accept ordered sets of classes.
//		This can be used to generate:
//			a) function parameters.
//			b) structures that are synonomous with the set of ordered classes.
//				i) copy/assignment.
// 2) typedefs in a family.
//		a) If the typedef is "fatally" dependent upon the type that is a classset
//			then the type is treated as an empty classset.
//			i) "Fatal" dependency is 

family <	classset[0,1] f_CSObject, // ordered set of zero or one classes.
					classset[0] f_CSParams,		// ordered set of zero or many classes.
					class f_CRetVal >					// normal template argument.
class FFDtor
{
	typedef family FFDtor	_TyThis;	// "family FFDtor" translates to the name of the implemented template.
	typedef FFDtor	_TyThis;				// ditto.
	typedef f_CRetVal ( f_CObject:: * _TyFunc )( f_CSParams );	// f_CSParams - as function parameter - is expanded - no dependency.
	typedef f_CSObject *		_TyObjectPtr;	// Pointer to object - in the case of single member classsets the type is the same as the original type.
	typedef f_CSParams *		_TyParams;		// Pointer to classset structure type - fatally dependent.

public:
	
	FFDtor()
		: m_pFunc( 0 )
	{
	}
	FFDtor( _TyObjectPtr _po,
					f_CSParams _rgparams,
					_TyFunc _pFunc )
		: m_po( _po ),
			m_rgparams( _rgparams ),
			m_pFunc( _pFunc )
	{
	}

	~FFDtor()
	{
		_ReleaseInt( m_pFunc );
	}

	f_CRetVal	Release()
	{
		_TyFunc pFunc = m_pFunc;
		_ReleaseInt(  m_pFunc = 0, pFunc );
	}

	void	Reset()
	{
		m_pFunc = 0;
	}
	void	Reset(	_TyObjectPtr _po,
								f_CSParams _rgparams,
								_TyFunc _pFunc )
	{
		if ( _po && _pFunc )
		{
			m_po = _po;
			m_pFunc = _pFunc;
			m_rgparams = _rgparams;
		}
		else
		{
			m_pFunc = 0;
		}
	}

protected:

	_TyObjectPtr	m_po;
	f_CSParams		m_rgparams;
	_TyFunc				m_pFunc;

	f_CRetVal	_ReleaseInt( _TyFunc _pFunc )
	{
		if ( _pFunc )
		{
			return m_po->(*_pFunc)( m_rgparams );
		}
	}
};


family FFDtor implement template < [], [ n = 0...3 ], class t_CRetVal > class FFDtor_global[n];
family FFDtor implement template < class t_CObject, [ n = 0...3 ], class t_CRetVal > class FFDtor_object[n];

#endif // 0 __FAMILIES
