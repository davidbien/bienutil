#pragma once

// _glprogram.h
// Utility object for maintaining lifetime of a glProgram.
// dbien
// 05FEB2022

#include <cstdarg>
#include "glad/glad.h"
#include "bienutil.h"
#include "_assert.h"

__BIENUTIL_BEGIN_NAMESPACE

class GLProgram
{
  typedef GLProgram _TyThis;
public:
  GLProgram() = default;
  GLProgram( GLProgram const & ) = delete;
  GLProgram & operator=( GLProgram const & ) = delete;
  GLProgram( GLProgram && _rr ) noexcept
  {
    std::swap( m_uProgramId, _rr.m_uProgramId );
  }
  GLProgram & operator=( GLProgram && _rr ) noexcept
  {
    GLProgram acquire( std::move( _rr ) );
    swap( acquire );
    return *this;
  }
  void swap( _TyThis & _r ) noexcept
  {
    std::swap( m_uProgramId, _r.m_uProgramId );
  }
  void AssertValid() const noexcept
  {
#if ASSERTSENABLED
    // We should either have a null program id or a valid and linked program here.
    if ( !m_uProgramId )
      return;
    GLint iLinkSuccess;
    glGetProgramiv( m_uProgramId, GL_LINK_STATUS, &iLinkSuccess );
    Assert( !!iLinkSuccess );
    // We validate here as a debugging aid - since this only runs in debug:
    glValidateProgram( m_uProgramId );
    GLint iValidateStatus;
    glGetProgramiv( m_uProgramId, GL_VALIDATE_STATUS, &iValidateStatus );
    Assert( !!iValidateStatus );
#endif //ASSERTSENABLED
  }
  // Construct and link the program from a set of shader ids.
  // Due to how variable arguments in C++ work must pass the count of shaders first.
  GLProgram( bool _fLogErrors, bool _fLogSuccess, size_t _nShaders, GLuint _uShaderId, ... ) noexcept(false)
  {
    va_list ap;
    va_start(ap, _uShaderId);
    try
    {
      (void)_FInit( true, _fLogErrors, _fLogSuccess, _nShaders, _uShaderId, ap );
      AssertValid();
    }
    catch( ... )
    {
      va_end(ap);
      throw;
    }
    va_end(ap);
  }
  bool FInit( bool _fThrowOnError, bool _fLogErrors, bool _fLogSuccess, size_t _nShaders, GLuint _uShaderId, ... ) noexcept(false)
  {
    AssertValid();
    Release();
    va_list ap;
    va_start(ap, _uShaderId);
    bool fSuccess;
    try
    {
      fSuccess = _FInit( _fThrowOnError, _fLogErrors, _fLogSuccess, _nShaders, _uShaderId, ap );
    }
    catch( ... )
    {
      Assert( _fThrowOnError );
      va_end(ap);
      throw;
    }
    va_end(ap);
    AssertValid();
    return fSuccess;
  }
  ~GLProgram()
  {
    Release();
  }
  void Release() noexcept
  {
    if ( !!m_uProgramId )
    {
      GLuint uProgramId = m_uProgramId;
      m_uProgramId = 0;
      glDeleteProgram( uProgramId );
    }
  }
  GLuint UGetProgram() const noexcept
  {
    return m_uProgramId;
  }
  void UseProgram() const noexcept
  {
    glUseProgram( m_uProgramId );
  }
  void SetInt( const char * _pszUniformName, GLint _i ) noexcept(false)
  {
    glUniform1i( _IGetUniform( _pszUniformName ), _i );
  }
  void SetFloat( const char * _pszUniformName, GLfloat _f ) noexcept(false)
  {
    glUniform1f( _IGetUniform( _pszUniformName ), _f );
  }

protected:
  GLint _IGetUniform( const char * _pszUniformName ) noexcept(false)
  {
    GLint iUniform = glGetUniformLocation( m_uProgramId, _pszUniformName );
    VerifyThrowSz( -1 != iUniform, "Uniform name [%s] not found.", _pszUniformName );
    return iUniform;
  }
  bool _FInit( bool _fThrowOnError, bool _fLogErrors, bool _fLogSuccess, size_t _nShaders, GLuint _uShaderId, va_list _ap ) noexcept(false)
  {
    Assert( _nShaders < 4 ); // only three shaders may be connected to a program.
    m_uProgramId = glCreateProgram(); // Must delete this before we leave the method if we fail.
    if ( _fThrowOnError )
      VerifyThrowSz( !!m_uProgramId, "glCreateProgram() failed." );
    else
    if ( !m_uProgramId )
      return false;
    glAttachShader( m_uProgramId, _uShaderId );

    if ( _nShaders-- > 1 )
    {
      // Add the remaining shaders:
      for ( ; _nShaders--; )
      {
        GLuint uShaderId = va_arg( _ap, GLuint );
        glAttachShader( m_uProgramId, uShaderId );
      }
    }

    glLinkProgram( m_uProgramId );
    GLint iLinkSuccess;
    glGetProgramiv( m_uProgramId, GL_LINK_STATUS, &iLinkSuccess );
    bool fFailed = !iLinkSuccess;
    if ( ( fFailed && _fLogErrors ) || _fLogSuccess )
    {
      GLint nCharsLog;
      glGetProgramiv( m_uProgramId, GL_INFO_LOG_LENGTH, &nCharsLog );
      if ( nCharsLog || fFailed )
      {
        // We don't log success if there is no info.
        string strLog;
        if ( nCharsLog > 1 )
        {
          strLog.resize( nCharsLog - 1 ); // reserves nCharsLog.
          GLsizei nFilled;
          glGetProgramInfoLog( m_uProgramId, nCharsLog, &nFilled, &strLog[0] );
          Assert( nFilled == ( nCharsLog - 1 ) );
          strLog[ nCharsLog - 1 ] = 0; // ensure null termination regardless.
        }
        const char * pszFmt = strLog.length() ? "InfoLog:%s \"%s\"" : "InfoLog:%s nologinfo";
        LOGSYSLOG( fFailed ? eslmtError : eslmtInfo, pszFmt, fFailed ? "FAILED" : "SUCCEEDED", &strLog[0] );      
      }
    }
    if ( fFailed )
    {
      glDeleteProgram( m_uProgramId );
      m_uProgramId = 0;
      if ( _fThrowOnError )
        VerifyThrowSz( !fFailed, "Failed to link program." );
    }
    return !fFailed;
  }
  
  GLuint m_uProgramId{0};
};

__BIENUTIL_END_NAMESPACE
