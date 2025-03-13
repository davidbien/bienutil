#pragma once
// vector_lohmann.inl
// JSON serialization support for std::vector using nlohmann::json.
// Provides automatic serialization for any vector whose elements implement ToJson() and FromJson() methods.

namespace nlohmann
{
using namespace __BIENUTIL_USE_NAMESPACE;

// Define the concept for types that have ToJson and FromJson
template<typename T>
concept JsonSerializable = requires(T t, const json& j) {
    { t.ToJson() } -> std::convertible_to<json>;
    { t.FromJson(j) };
};

// Specialization only for types that satisfy JsonSerializable
template < typename t_TyT > 
  requires JsonSerializable<t_TyT>
struct adl_serializer< std::vector< t_TyT > >
{
  static void to_json( json & j, const std::vector< t_TyT > & _rg )
  {
    for ( const auto & item : _rg )
      j.push_back( item.ToJson() );
  }
  static void from_json( const json & j, std::vector< t_TyT > & _rg )
  {
    _rg.clear();
    _rg.reserve( j.size() );
    for ( const auto & item : j )
    { 
      t_TyT element;
      element.FromJson( item );
      _rg.push_back( std::move( element ) );
    }
  }
};
} // namespace nlohmann