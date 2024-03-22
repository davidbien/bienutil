#pragma once

// http_headers.h
// Simple classes for dealing with HTTP headers - mostly for multipart messages.
// dbien
// 20MAR2024
#include "bienutil.h"
#include <string>
#include <vector>
#include <optional>

__BIENUTIL_BEGIN_NAMESPACE

class CHttpHeaderAttr
{
public:
  CHttpHeaderAttr( const std::string & name, const std::string & value )
    : m_name( name )
    , m_value( value )
  {
  }

  const std::string & GetName() const { return m_name; }
  const std::string & GetValue() const { return m_value; }

private:
  std::string m_name;
  std::string m_value;
};

class CHttpHeader
{
public:
  CHttpHeader( const std::string & name, const std::string & value )
    : m_name( name )
    , m_value( value )
  {
  }

  void AddAttribute( const CHttpHeaderAttr & attr ) { m_attributes.push_back( attr ); }
  void AddAttribute( const CHttpHeaderAttr && attr ) { m_attributes.push_back( std::move( attr ) ); }

  const std::string & GetName() const { return m_name; }
  const std::string & GetValue() const { return m_value; }
  const std::vector< CHttpHeaderAttr > & GetAttributes() const { return m_attributes; }
  std::optional< CHttpHeaderAttr > FindAttribute( const std::string & attrName ) const
  {
    auto it = std::find_if( m_attributes.begin(), m_attributes.end(), [ &attrName ]( const CHttpHeaderAttr & attr ) { return attr.GetName() == attrName; } );
    return it != m_attributes.end() ? std::optional< CHttpHeaderAttr >( *it ) : std::nullopt;
  }

private:
  std::string m_name;
  std::string m_value;
  std::vector< CHttpHeaderAttr > m_attributes;
};

class CHttpHeaders
{
public:
  void AddHeader( const CHttpHeader & header ) { m_headers.push_back( header ); }
  void AddHeader( CHttpHeader && header ) { m_headers.push_back( std::move( header ) ); }

  const std::vector< CHttpHeader > & GetHeaders() const { return m_headers; }

  std::optional< CHttpHeader > FindHeader( const std::string & headerName ) const
  {
    auto it = std::find_if( m_headers.begin(), m_headers.end(), [ &headerName ]( const CHttpHeader & header ) { return header.GetName() == headerName; } );

    if ( it != m_headers.end() )
    {
      return *it;
    }
    else
    {
      return std::nullopt;
    }
  }

  template < typename t_TyString > size_t PopulateFromStr( const t_TyString & _kstr )
  {
    std::string_view sv( _kstr );
    size_t startOfData = 0;

    while ( !sv.empty() )
    {
      auto endOfLinePos = sv.find_first_of( "\r\n" );
      bool isWindowsEOL = ( endOfLinePos != std::string_view::npos ) && ( sv[ endOfLinePos ] == '\r' );

      auto line = sv.substr( 0, endOfLinePos );
      if ( line.find_first_not_of( " \t" ) == std::string_view::npos )
      {
        startOfData += endOfLinePos != std::string_view::npos ? endOfLinePos + ( isWindowsEOL ? 2 : 1 ) : sv.size();
        break;
      }

      auto colonPos = line.find( ':' );
      if ( colonPos != std::string::npos )
      {
        std::string_view name = line.substr( 0, colonPos );
        name = name.substr( name.find_first_not_of( " \t" ) );
        name = name.substr( 0, name.find_last_not_of( " \t" ) + 1 );
        auto semicolonPos = line.find( ';' );
        std::string_view value =
            ( semicolonPos != std::string_view::npos ) ? line.substr( colonPos + 1, semicolonPos - colonPos - 1 ) : line.substr( colonPos + 1 );
        value = value.substr( value.find_first_not_of( " \t" ) );
        value = value.substr( 0, value.find_last_not_of( " \t" ) + 1 );

        CHttpHeader header{ std::string( name ), std::string( value ) };

        while ( semicolonPos != std::string::npos )
        {
          line = line.substr( semicolonPos + 1 );
          semicolonPos = line.find( ';' );
          size_t posEndValue = semicolonPos != std::string::npos ? semicolonPos : line.size();

          auto equalsPos = line.find( '=' );
          if ( equalsPos != std::string::npos )
          {
            std::string_view attrName = line.substr( 0, equalsPos );
            attrName = attrName.substr( attrName.find_first_not_of( " \t" ) );
            attrName = attrName.substr( 0, attrName.find_last_not_of( " \t" ) + 1 );
            std::string_view attrValue = line.substr( equalsPos + 1, posEndValue - ( equalsPos + 1 ) );
            attrValue = attrValue.substr( attrValue.find_first_not_of( " \t" ) );
            attrValue = attrValue.substr( 0, attrValue.find_last_not_of( " \t" ) + 1 );

            // If the attribute value is enclosed in double quotes, use the value within the quotes
            if ( attrValue.length() > 1 && attrValue.front() == '"' && attrValue.back() == '"' )
            {
              attrValue = attrValue.substr( 1, attrValue.size() - 2 );
            }

            CHttpHeaderAttr attr{ std::string( attrName ), std::string( attrValue ) };
            header.AddAttribute( std::move( attr ) );
          }
        }

        AddHeader( std::move( header ) );
      }

      if ( endOfLinePos != std::string_view::npos )
        endOfLinePos += isWindowsEOL ? 2 : 1;
      startOfData += endOfLinePos != std::string_view::npos ? endOfLinePos : sv.size();
      sv.remove_prefix( endOfLinePos );
    }

    return startOfData;
  }

private:
  std::vector< CHttpHeader > m_headers;
};

__BIENUTIL_END_NAMESPACE
