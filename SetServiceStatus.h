#pragma once

// SetServiceStatus.h
// dbien
// 28FEB2024
// I told copilot to create this class and it did a pretty good job. Thanks Obama!!!

#include "bienutil.h"
#include <windows.h>

__BIENUTIL_BEGIN_NAMESPACE

class CSetServiceStatus
{
private:
  SERVICE_STATUS m_ServiceStatus;
  SERVICE_STATUS_HANDLE m_StatusHandle;

public:
  CSetServiceStatus()
  {
    ZeroMemory( &m_ServiceStatus, sizeof( m_ServiceStatus ) );
    m_StatusHandle = NULL;
  }

  bool RegisterServiceCtrlHandler( LPCWSTR _kServiceName, LPHANDLER_FUNCTION _kHandlerFunction, bool _fSetStartingStatus = true )
  {
    m_StatusHandle = ::RegisterServiceCtrlHandler( _kServiceName, _kHandlerFunction );
    if ( m_StatusHandle == NULL )
    {
      return false;
    }

    if ( _fSetStartingStatus )
    {
      return SetStatus( SERVICE_START_PENDING );
    }

    return true;
  }
  bool SetStatus( DWORD _kCurrentState, DWORD _kWin32ExitCode = NO_ERROR, DWORD _kWaitHint = 0 )
  {
    static DWORD s_dwCheckPoint = 1;

    m_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    m_ServiceStatus.dwCurrentState = _kCurrentState;
    m_ServiceStatus.dwWin32ExitCode = _kWin32ExitCode;
    m_ServiceStatus.dwWaitHint = _kWaitHint;

    if ( _kCurrentState == SERVICE_START_PENDING )
      m_ServiceStatus.dwControlsAccepted = 0;
    else
      m_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    if ( ( _kCurrentState == SERVICE_RUNNING ) || ( _kCurrentState == SERVICE_STOPPED ) )
      m_ServiceStatus.dwCheckPoint = 0;
    else
      m_ServiceStatus.dwCheckPoint = s_dwCheckPoint++;

    return ::SetServiceStatus( m_StatusHandle, &m_ServiceStatus );
  }
};

__BIENUTIL_END_NAMESPACE
