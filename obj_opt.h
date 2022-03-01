#pragma once

// obj_opt.h
// This module optimizes an object loaded with tiny_obj_loader and stores it in a binary format for fast loading.
// This is the shared header which declares the embedded class containing object specifications.
// dbien
// 24FEB2022

#include "bienutil.h"
#include <limits>
#include <unordered_map>

__BIENUTIL_BEGIN_NAMESPACE

struct OptObjAttribs
{
private:
  typedef OptObjAttribs _TyThis;
public:
  uint32_t m_nVertices;
  uint32_t m_nIndices;
  float m_flMaxDistance{0.0f}; // The maximum distance to any point from (0,0,0).
  // Distance to farthest point in each quadrant - allows the production of an estimated "geographic center".
  float m_rgflMaxDistByQuadrant[8] = { 0.0f };

  static size_t NGetQuadrant( float _x, float _y, float _z )
  {
    bool fX = ( _x < 0 );
    bool fY = ( _y < 0 );
    bool fZ = ( _z < 0 );
    return (int)fX + fY * 2 + fZ * 4;
  }
};

__BIENUTIL_END_NAMESPACE
