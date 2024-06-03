#pragma once
// fixedbv_lohmann.inl
// Utility methods for FixedBV<>.

#include "fixedbv.h"
#include <nlohmann/json.hpp>

namespace nlohmann
{
using namespace __BIENUTIL_USE_NAMESPACE;
template < size_t t_kN, typename t_TyT > struct adl_serializer< FixedBV< t_kN, t_TyT > >
{
  static void to_json( json & j, const FixedBV< t_kN, t_TyT > & obj )
  {
    size_t index = obj.get_next_bit( std::numeric_limits< size_t >::max() );
    while ( index != std::numeric_limits< size_t >::max() )
    {
      j.push_back( index );
      index = obj.get_next_bit( index );
    }
  }
  static void from_json( const json & j, FixedBV< t_kN, t_TyT > & obj )
  {
    for ( size_t pos : j )
    {
      VerifyThrowSz( pos < t_kN, "Invalid bit index [%zu].", pos );
      obj.GetArray()[ pos / ( CHAR_BIT * sizeof( t_TyT ) ) ] |= t_TyT( 1 ) << ( pos % ( CHAR_BIT * sizeof( t_TyT ) ) );
    }
  }
};

} // namespace nlohmann
