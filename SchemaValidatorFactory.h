#pragma once

// SchemaValidatorFactory
// Creates a json_schema::json_validator from a path to a schema file for loading referenced schema files within that root schema file.
// dbien
// 29FEB2024

#include "bienutil.h"
#include "bien_assert.h"
#include <memory>
#include <string>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <nlohmann/json-schema.hpp>

__BIENUTIL_BEGIN_NAMESPACE

class NlohmannSchemaValidatorFileLoaderFactory
{
private:
  std::filesystem::path m_pathSchemaRoot;
public:
  NlohmannSchemaValidatorFileLoaderFactory( const std::string& _kpathSchemaRoot ) 
    : m_pathSchemaRoot( _kpathSchemaRoot )
  {
  }
  // We are creating this to be a standalone validator object - no reference to this object.
  nlohmann::json_schema::json_validator CreateValidator() const
  {
    std::filesystem::path pathSchemaRoot = m_pathSchemaRoot;
    return nlohmann::json_schema::json_validator( [ pathSchemaRoot ]( const nlohmann::json_uri& _uri, nlohmann::json& _value )
    {
      VerifyThrowSz( _uri.scheme().empty() || ( _uri.scheme() == "file" ), "Only supporting file referenced schemas." );
      // We want the path from the pathSchemaRoot and we will find the uri relative to that.
      std::string strPath = _uri.path();
      size_t stAt = ( strPath[0] == '/' ) ? 1 : 0;
      std::filesystem::path pathUri = pathSchemaRoot / &strPath.at( stAt );
      std::ifstream schemaFile( pathUri.c_str() );
      VerifyThrowSz( schemaFile.is_open(), "Could not open schema file:[%s]", pathUri.generic_string().c_str() );
      schemaFile >> _value;
    } );
  }
  std::shared_ptr< nlohmann::json_schema::json_validator > CreateSharedValidator() const
  {
    return std::make_shared< nlohmann::json_schema::json_validator >( CreateValidator() );
  }
};

__BIENUTIL_END_NAMESPACE