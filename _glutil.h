#pragma once

// _glutil.h
// OpenGL utility stuff.
// dbien
// 03FEB2022

#include "glad\glad.h"
#include "_strutil.h"
#include "syslogmgr.h"

__BIENUTIL_BEGIN_NAMESPACE

inline const char *
PszGetGLErrorCodeName( GLuint _uId )
{
  const char * pszErrorName;
  switch ( _uId )
  {
      case GL_INVALID_ENUM:
        pszErrorName = ""; 
      break;
      case GL_INVALID_VALUE:
        pszErrorName = "INVALID_VALUE"; 
      break;
      case GL_INVALID_OPERATION:
        pszErrorName = "INVALID_OPERATION"; 
      break;
      case GL_STACK_OVERFLOW:
        pszErrorName = "STACK_OVERFLOW"; 
      break;
      case GL_STACK_UNDERFLOW:
        pszErrorName = "STACK_UNDERFLOW"; 
      break;
      case GL_OUT_OF_MEMORY:
        pszErrorName = "OUT_OF_MEMORY"; 
      break;
      case GL_INVALID_FRAMEBUFFER_OPERATION:
        pszErrorName = "INVALID_FRAMEBUFFER_OPERATION"; 
      break;
      default:
        pszErrorName = nullptr;
      break;
  }
  return pszErrorName;
}

inline GLenum _gluCheckError( bool _fLog, const char *_szFile, int _nLine )
{
  static size_t s_nCurrentCall = 0;
  ++s_nCurrentCall;
  GLenum ecLastError = GL_NO_ERROR;
  GLenum ecCurError;
  while ( ( ecCurError = glGetError() ) != GL_NO_ERROR )
  {
    ecLastError = ecCurError;
    if ( _fLog )
    {
      string strErrorName;
      const char * pszErrorName = PszGetGLErrorCodeName( ecLastError = ecCurError );
      if ( !pszErrorName )
      {
        PrintfStdStr( strErrorName, "0x%x", ecCurError );
        pszErrorName = strErrorName.c_str();
      }
      LOGSYSLOG( eslmtError, "gluCheckError: nCall[%zu]: %s", s_nCurrentCall, pszErrorName );
    }
  }
  return ecLastError;
}
#define gluCheckError( fLog ) _gluCheckError( fLog, __FILE__, __LINE__ )

inline void APIENTRY 
gluDebugOutput( GLenum _eSource, GLenum _eType, GLuint _uId,  GLenum _eSeverity, 
                GLsizei _nbyLength, const GLchar * _szMessage,  const void *_pvUserParam )
{
  __BIENUTIL_USING_NAMESPACE
  // ignore non-significant error/warning codes
  if ( _uId == 131169u || _uId == 131185u || _uId == 131218u || _uId == 131204u ) 
    return; 

  const char * pszSource;
  string strSource;
  switch ( _eSource )
  {
      case GL_DEBUG_SOURCE_API:             
        pszSource = "API"; 
      break;
      case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        pszSource = "Window System";
      break;
      case GL_DEBUG_SOURCE_SHADER_COMPILER:
        pszSource = "Shader Compiler"; 
      break;
      case GL_DEBUG_SOURCE_THIRD_PARTY:
        pszSource = "Third Party"; 
      break;
      case GL_DEBUG_SOURCE_APPLICATION:
        pszSource = "Application"; 
      break;
      case GL_DEBUG_SOURCE_OTHER:
        pszSource = "Other";
      break;
      default:
        PrintfStdStr( strSource, "0x%x", _eSource );
        pszSource = strSource.c_str();
      break;
  }

  const char * pszType;
  string strType;
  switch( _eType )
  {
      case GL_DEBUG_TYPE_ERROR:
        pszType = "Error";
      break;
      case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        pszType = "Deprecated Behaviour"; 
      break;
      case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: 
        pszType = "Undefined Behaviour"; 
      break; 
      case GL_DEBUG_TYPE_PORTABILITY:
        pszType = "Portability"; 
      break;
      case GL_DEBUG_TYPE_PERFORMANCE:
        pszType = "Performance"; 
      break;
      case GL_DEBUG_TYPE_MARKER:
        pszType = "Marker"; 
      break;
      case GL_DEBUG_TYPE_PUSH_GROUP:
        pszType = "Push Group"; 
      break;
      case GL_DEBUG_TYPE_POP_GROUP:
        pszType = "Pop Group"; 
      break;
      case GL_DEBUG_TYPE_OTHER:
        pszType = "Other"; 
      break;
      default:
        PrintfStdStr( strType, "0x%x", _eType );
        pszType = strType.c_str();
      break;
  }
  
  const char * pszSeverity;
  string strSeverity;
  switch ( _eSeverity )
  {
      case GL_DEBUG_SEVERITY_HIGH:
        pszSeverity = "High"; 
      break;
      case GL_DEBUG_SEVERITY_MEDIUM:
        pszSeverity = "Medium"; 
      break;
      case GL_DEBUG_SEVERITY_LOW:
        pszSeverity = "Low"; 
      break;
      case GL_DEBUG_SEVERITY_NOTIFICATION:
        pszSeverity = "Notification"; 
      break;
      default:
        PrintfStdStr( strSeverity, "0x%x", _eSeverity );
        pszSeverity = strSeverity.c_str();
      break;
  }

  string strErrorName;
  const char * pszErrorName = PszGetGLErrorCodeName( _uId );
  if ( !pszErrorName )
  {
    PrintfStdStr( strErrorName, "0x%x", _uId );
    pszErrorName = strErrorName.c_str();
  }
  LOGSYSLOG( ( GL_DEBUG_SEVERITY_NOTIFICATION == _eSeverity ) ? eslmtInfo : eslmtError,  
    "%s:\"%s\" Src:%s T:%s S:%s", pszErrorName, _szMessage, pszSource, pszType, pszSeverity );
}

__BIENUTIL_END_NAMESPACE
