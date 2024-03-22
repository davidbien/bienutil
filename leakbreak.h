#pragma once

// leakbreak.h
// dbien
// 21MAR2024

#include "bienutil.h"
#include <fstream>
#include <regex>
#include <string>
#include <crtdbg.h>
#include <iostream>

__BIENUTIL_BEGIN_NAMESPACE

inline void
setBreakpointsOnAllocations( const std::string & filename )
{
  std::ifstream file( filename );
  if ( !file.is_open() )
  {
    std::cerr << "Could not open file: " << filename << std::endl;
    return;
  }

  std::string line;
  std::regex allocationRegex( "\\{(\\d+)\\} normal block" );
  while ( std::getline( file, line ) )
  {
    std::smatch match;
    if ( std::regex_search( line, match, allocationRegex ) && match.size() > 1 )
    {
      int allocationNumber = std::stoi( match[ 1 ].str() );
      _CrtSetBreakAlloc( allocationNumber );
    }
  }
}

__BIENUTIL_END_NAMESPACE
