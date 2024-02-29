#pragma once

// SetServiceStatus.h
// dbien
// 28FEB2024
// I told copilot to create this class and it did a pretty good job. Thanks Obama!!!

#include "bienutil.h"
#include <shared_mutex>
#include <optional>
#include <windows.h>

__BIENUTIL_BEGIN_NAMESPACE

class CSetServiceStatus
{
private:
  SERVICE_STATUS m_ServiceStatus;
  SERVICE_STATUS_HANDLE m_StatusHandle;
  std::shared_mutex m_mutex; // Mutex for read/write lock

public:
  CSetServiceStatus()
  {
    ZeroMemory(&m_ServiceStatus, sizeof(m_ServiceStatus));
    m_StatusHandle = NULL;
  }

  bool RegisterServiceCtrlHandler( LPCSTR _kServiceName, LPHANDLER_FUNCTION _kHandlerFunction, bool _fSetStartingStatus = true )
  {
    m_StatusHandle = ::RegisterServiceCtrlHandler( _kServiceName, _kHandlerFunction );
    if ( m_StatusHandle == NULL )
    {
      return false;
    }

    if ( _fSetStartingStatus )
    {
      return !!SetStatus( SERVICE_START_PENDING );
    }

    return true;
  }

  DWORD GetStatus()
  {
    std::shared_lock<std::shared_mutex> lock(m_mutex); // Read lock
    return m_ServiceStatus.dwCurrentState;
  }

  // A nullopt is returned on failure, otherwise the previous state is returned. If the previous state is _kCurrentState then no windows API call was made.
  std::optional< DWORD > SetStatus(DWORD _kCurrentState, DWORD _kWin32ExitCode = NO_ERROR, DWORD _kWaitHint = 0)
  {
    static DWORD s_dwCheckPoint = 1;

    std::unique_lock<std::shared_mutex> lock(m_mutex); // Write lock

    // Due to race conditions we may have already succeeded in setting the current state:
    DWORD dwAssociatedState;
    switch( _kCurrentState )
    {
      case SERVICE_STOP_PENDING:
        dwAssociatedState = SERVICE_STOPPED;
        break;
      case SERVICE_START_PENDING:
      case SERVICE_CONTINUE_PENDING:
        dwAssociatedState = SERVICE_RUNNING;
        break;
      case SERVICE_PAUSE_PENDING:
        dwAssociatedState = SERVICE_PAUSED;
        break;
      default:
        dwAssociatedState = _kCurrentState;
        break;
    }
    if ( ( m_ServiceStatus.dwCurrentState == _kCurrentState ) || ( m_ServiceStatus.dwCurrentState == dwAssociatedState ) )
      return m_ServiceStatus.dwCurrentState;

    // Store the previous state
    DWORD dwPreviousState = m_ServiceStatus.dwCurrentState;

    m_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    m_ServiceStatus.dwCurrentState = _kCurrentState;
    m_ServiceStatus.dwWin32ExitCode = _kWin32ExitCode;
    m_ServiceStatus.dwWaitHint = _kWaitHint;

    if ( ( SERVICE_START_PENDING == _kCurrentState ) || ( SERVICE_CONTINUE_PENDING == _kCurrentState ) )
      m_ServiceStatus.dwControlsAccepted = 0;
    else
    {
      m_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
      if ( ( SERVICE_RUNNING == _kCurrentState ) || ( SERVICE_PAUSED == _kCurrentState ) )
        m_ServiceStatus.dwControlsAccepted |= SERVICE_ACCEPT_PAUSE_CONTINUE;
    }

    if ((_kCurrentState == SERVICE_RUNNING) || (_kCurrentState == SERVICE_STOPPED))
      m_ServiceStatus.dwCheckPoint = 0;
    else
      m_ServiceStatus.dwCheckPoint = s_dwCheckPoint++;

    if (!::SetServiceStatus(m_StatusHandle, &m_ServiceStatus))
    {
      // If SetServiceStatus fails, revert to the previous state
      m_ServiceStatus.dwCurrentState = dwPreviousState;
      return std::nullopt;
    }

    return dwPreviousState;
  }
};

__BIENUTIL_END_NAMESPACE
