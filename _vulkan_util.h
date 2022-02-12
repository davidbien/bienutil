#pragma once

// _vulkanutil.h
// Vulkan utility stuff.
// dbien
// 11FEB2022

#include "bienutil.h"
#include "_compat.h"
#include <vector>

__BIENUTIL_BEGIN_NAMESPACE

// ReadSPIRVShaderCode:
// Reads a shader SPIR-V binary - validates that it's size is a multiple of an uint32_t.
inline bool FReadSPIRVShaderCode( const char * _pszFileName, vector< uint32_t > & _rrg32, bool _fThrow = true ) noexcept(false)
{
  FileObj foSpirV( OpenReadOnlyFile( _pszFileName ) );
  if ( !foSpirV.FIsOpen() )
  {
    if ( _fThrow )
      VerifyThrowSz( false, "Unable to open shader file [%s].", _pszFileName );
    return false;
  }
  uint64_t u64Size = GetFileSizeFromHandle( foSpirV.HFileGet() );
  if ( uint64_t(-1) == u64Size )
  {
    if ( _fThrow )
      VerifyThrowSz( false, "Unable to get shader file size [%s].", _pszFileName );
    return false;
  }
  if ( ( u64Size + 1ull ) > (numeric_limits< size_t >::max)() )
  {
    if ( _fThrow )
      VerifyThrowSz( false, "Shader file [%s] is too large [%llu].", _pszFileName, u64Size );
    return false;
  }
  if ( !!( u64Size % sizeof( uint32_t ) ) )
  {
    if ( _fThrow )
      VerifyThrowSz( false, "Shader file [%s] size [%llu] is not a multiple of sizeof(uint32_t) = 4.", _pszFileName, u64Size );
    return false;
  }
  _rrg32.resize( )
  strShaderSource.resize( size_t(u64Size) / sizeof( uint32_t ) ) );
  if ( !!FileRead( foSpirV.HFileGet(), &strShaderSource[0], u64Size, nullptr ) )
  {
    if ( _fThrow )
      VerifyThrowSz( false, "Error reading [%llu] bytes from shader file [%s].", u64Size, _pszFileName );
    return false;
  }
  return true;
}

__BIENUTIL_END_NAMESPACE
