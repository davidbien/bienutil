#ifndef __OLEUTIL_H
#define __OLEUTIL_H

// _oleutil.h

// Define an exception type that is based on the standard C++ exception type ( allows
//	a single catch ):

#include <stdexcept>
#include "_namdexc.h"

__BIENUTIL_BEGIN_NAMESPACE

#ifdef __NAMDDEXC_STDBASE
#pragma push_macro("std")
#undef std
#endif //__NAMDDEXC_STDBASE
class ole_exception : public std::_t__Named_exception<>
{
	typedef std::_t__Named_exception<> _tyBase;
public:
	HRESULT	m_hr;

	ole_exception( HRESULT _hr ) : 
		_tyBase( "OLE" ),
		m_hr( _hr )
	{ 
    __DEBUG_STMT( int i = 0; ++i )
  }
};
#ifdef __NAMDDEXC_STDBASE
#pragma pop_macro("std")
#endif //__NAMDDEXC_STDBASE

#define __MTOKS( x, y )  x##y
#define __ADDLINE( x ) __MTOKS( x, __LINE__ )
#define __ThrowOLEError( _hr )	throw __BIENUTIL_NAMESPACE ole_exception( _hr );
#define __ThrowOLEFAIL( exp )	{ HRESULT __ADDLINE( _zzhr ) = (exp); \
  if ( FAILED( __ADDLINE( _zzhr ) ) )	__ThrowOLEError( __ADDLINE( _zzhr ) ); }

// shorthand:
#define __TOF( exp )	__ThrowOLEFAIL( exp )

inline HRESULT HrLastError()
{
  DWORD dw = GetLastError(); 
  HRESULT hr = HRESULT_FROM_WIN32( dw );
  return SUCCEEDED( hr ) ? E_UNEXPECTED : hr;
}

template < class t_IFace >
__INLINE void
__QI( IUnknown * _punk, t_IFace *& _rpi )
{
	__ThrowOLEFAIL( _punk->QueryInterface( __uuidof(t_IFace), reinterpret_cast< void** >( &_rpi ) ) );
}

template < class t_IFace >
__INLINE void
__CoCreateLocal( REFCLSID _rclsid, t_IFace *& _rpi )
{
	__ThrowOLEFAIL( CoCreateInstance( _rclsid, 0, CLSCTX_INPROC_SERVER, 
																		__uuidof(t_IFace), reinterpret_cast< void** >( &_rpi ) ) );
}

__BIENUTIL_END_NAMESPACE

#endif //__OLEUTIL_H
