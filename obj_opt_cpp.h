#pragma once

// obj_opt_cpp.h
// The headers for the obj_opt.cpp module.
// dbien
// 25FEB2022

#ifdef _MSC_VER
#include <windows.h>
#endif //_MSC_VER

#ifdef WIN32
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#ifndef NDEBUG
    #define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#else
    #define DBG_NEW new
#endif
#else //!WIN32
#define DBG_NEW new
#endif //!WIN32

#define __NDEBUG_THROW
#define TRACESENABLED 0

#include <string>
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include "bienutil.h"
#include "syslogmgr.h"
#include "syslogmgr.inl"
#include "_compat.inl"
#include "obj_opt.h"
