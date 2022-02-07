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
    return true;
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
