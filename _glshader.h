#pragma once

// _glshader.h
// Object to maintain the lifetime of a shader.
// dbien
// 04FEB2022

#include "bienutil.h"
#include "_assert.h"

__BIENUTIL_BEGIN_NAMESPACE

class GLShader
{
  typedef GLShader _TyThis;
public:
  GLShader() = delete;
  GLShader( GLShader const & ) = delete;
  GLShader & operator=( GLShader const & ) = delete;
  GLShader( GLShader && _rr ) noexcept
  {
    std::swap( m_uShaderId, _rr.m_uShaderId );
  }
  GLShader & operator=( GLShader && _rr ) noexcept
  {
    GLShader acquire( std::move( _rr ) );
    swap( acquire );
    return *this;
  }
  void swap( _TyThis & _r ) noexcept
  {
    std::swap( m_uShaderId, _r.m_uShaderId );
  }

  GLShader( GLenum _eShaderType ) noexcept(false)
  {
    VerifyThrowSz( !!( m_uShaderId = glCreateShader( _eShaderType ) ), "Error creating shader." );
  }
  // This always throws on compiler error.
  GLShader( GLenum _eShaderType, const char * _pszShaderSource, bool _fLogErrors = true, bool _fLogSuccess = false ) noexcept(false)
  {
    VerifyThrowSz( !!( m_uShaderId = glCreateShader( _eShaderType ) ), "Error creating shader." );
    bool fCompiled = FCompileShader( _pszShaderSource, _fLogErrors, false, _fLogSuccess );
    if ( !fCompiled )
    {
      glDeleteShader( m_uShaderId );
      VerifyThrowSz( fCompiled, "Error compiling shader." );
    }
  }
  // Load a shader of the given type from a file. Always throws on compilation error or loading of file error.
  GLShader( const char * _pszShaderFile, GLenum _eShaderType, bool _fLogErrors = true, bool _fLogSuccess = false ) noexcept(false)
  {
    VerifyThrowSz( !!( m_uShaderId = glCreateShader( _eShaderType ) ), "Error creating shader." );
    bool fCompiled = FCompileShaderFile( _pszShaderFile, _fLogErrors, false, _fLogSuccess );
    if ( !fCompiled )
    {
      glDeleteShader( m_uShaderId );
      VerifyThrowSz( fCompiled, "Error compiling shader file." );
    }
  }
  ~GLShader() noexcept
  {
    if ( !!m_uShaderId )
      glDeleteShader( m_uShaderId );
  }
  void Release() noexcept
  {
    if ( !!m_uShaderId )
    {
      GLuint uShaderId = m_uShaderId;
      m_uShaderId = 0;
      glDeleteShader( uShaderId );
    }
  }
  GLuint UGetShaderId() const noexcept
  {
    return m_uShaderId;
  }
  // compilation via a filename.
  bool FCompileShaderFile( const char * _pszShaderFile, bool _fLogErrors = true, bool _fThrow = false, bool _fLogSuccess = false ) noexcept(false)
  {
    FileObj foShaderFile( OpenReadOnlyFile( _pszShaderFile ) );
    if ( !foShaderFile.FIsOpen() )
    {
      if ( _fThrow )
        VerifyThrowSz( false, "Unable to open shader file [%s].", _pszShaderFile );
      return false;
    }
    string strShaderSource;
    uint64_t u64Size = GetFileSizeFromHandle( foShaderFile.HFileGet() );
    if ( uint64_t(-1) == u64Size )
    {
      if ( _fThrow )
        VerifyThrowSz( false, "Unable to get shader file size [%s].", _pszShaderFile );
      return false;
    }
    if ( ( u64Size + 1ull ) > (numeric_limits< size_t >::max)() )
    {
      if ( _fThrow )
        VerifyThrowSz( false, "Shader file [%s] is too large [%llu].", _pszShaderFile, u64Size );
      return false;
    }
    strShaderSource.resize( (size_t)u64Size );
    if ( !!FileRead( foShaderFile.HFileGet(), &strShaderSource[0], u64Size * sizeof(char), nullptr ) )
    {
      if ( _fThrow )
        VerifyThrowSz( false, "Error reading [%llu] bytes from shader file [%s].", u64Size, _pszShaderFile );
      return false;
    }
    return FCompileShader( strShaderSource.c_str(), _fLogErrors, _fThrow, _fLogSuccess );
  }
  // simple compilation via a single null terminated string.
  bool FCompileShader( const char * _pszShaderSource, bool _fLogErrors = true, bool _fThrow = false, bool _fLogSuccess = false ) noexcept(false)
  {
    Assert( !!glIsShader( m_uShaderId ) );
    glShaderSource( m_uShaderId, 1, &_pszShaderSource, nullptr );
    glCompileShader( m_uShaderId );
    bool fFailed;
    if ( ( !( fFailed = !GetCompileStatus() ) && _fLogErrors ) || _fLogSuccess )
    {
      GLint nCharsLog; // includes NULL terminating character.
      glGetShaderiv( m_uShaderId, GL_INFO_LOG_LENGTH, &nCharsLog );
      Assert( !fFailed || ( nCharsLog > 1 ) ); // We expect log info on failure to compile.
      if ( fFailed || ( nCharsLog > 1 ) ) // don't log no info on success.
      {
        string strLog;
        if ( nCharsLog > 1 )
        {
          strLog.resize( nCharsLog - 1 ); // reserves nCharsLog.
          GLsizei nFilled;
          glGetShaderInfoLog( m_uShaderId, nCharsLog, &nFilled, &strLog[0] );
          Assert( nFilled == ( nCharsLog - 1 ) );
          strLog[ nCharsLog - 1 ] = 0; // ensure null termination regardless.
        }
        const char * pszFmt = strLog.length() ? "InfoLog:%s:%s \"%s\"" : "InfoLog:%s:%s nologinfo";
        LOGSYSLOG( fFailed ? eslmtError : eslmtInfo, pszFmt, PszShaderTypeName(), fFailed ? "FAILED" : "SUCCEEDED", &strLog[0] );
      }
    }
    VerifyThrowSz( !fFailed || !_fThrow, "Compile of shader failed." );
    return !fFailed;
  }
  bool GetCompileStatus() const noexcept
  {
    Assert( !!glIsShader( m_uShaderId ) );
    GLint iStatus;
    glGetShaderiv( m_uShaderId, GL_COMPILE_STATUS, &iStatus );
    return !!iStatus;
  }
  GLenum GetShaderType() const noexcept
  {
    Assert( !!glIsShader( m_uShaderId ) );
    GLint iType;
    glGetShaderiv( m_uShaderId, GL_COMPILE_STATUS, &iType );
    return (GLenum)iType;
  }
  const char * PszShaderTypeName() const noexcept
  {
    switch( GetShaderType() )
    {
      case GL_VERTEX_SHADER:
        return "GL_VERTEX_SHADER";
      case GL_GEOMETRY_SHADER:
        return "GL_GEOMETRY_SHADER";
      case GL_FRAGMENT_SHADER:
        return "GL_FRAGMENT_SHADER";
      default:
        return "ERROR_SHADER_TYPE";
    }
  }

protected:
  GLuint m_uShaderId{0u};
};

__BIENUTIL_END_NAMESPACE
