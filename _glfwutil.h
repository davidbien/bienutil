#pragma once

// _glfwutil.h
// Utility functions for GLFW.
// dbien
// 03FEB2022

#include "syslogmgr.h"

__BIENUTIL_BEGIN_NAMESPACE

// Note that we don't have any thread synchonization here but since the syslog stuff is per-threaed and thus
//  threadsafe we should be fine.
inline void GLFWErrorCallback( int _nCode, const char * _szDescription )
{
  LOGSYSLOG( eslmtError,  "GLFWErrorCallback: nCode[0x%x]: %s", _nCode, !_szDescription ? "nodesc" : _szDescription );
}

__BIENUTIL_END_NAMESPACE
