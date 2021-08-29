#ifndef __BASEMAP_H___
#define __BASEMAP_H___

//          Copyright David Lawrence Bien 1997 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _basemap.h

// Map to a base class.
// An object must specialize the template to support.

__BIENUTIL_BEGIN_NAMESPACE

template < class t_Ty >
struct __map_to_base_class
{
  typedef t_Ty  _TyBase;  // Default just maps to same type.
};

__BIENUTIL_END_NAMESPACE

#endif //__BASEMAP_H___
