#pragma once

// obj_io.h
// This module optimizes an object loaded with tiny_obj_loader and stores it in a binary format for fast loading.
// dbien
// 24FEB2022

#include "bienutil.h"
#include "tiny_obj_loader.h"

__BIENUTIL_BEGIN_NAMESPACE

// We let the user specify the vertex class and then it may have colors, texture coords, etc. or not.
// We store the vertices in their native formate unless a method is present for I/O on the vertices.
// The optimization of objects can occur at runtime or if desired as a cmake initial pass on the obj files.

// CxVertexHasFileWrite: Does a type have write file capabilities?
template < typename t_TyVertex, typename t_TyFile >
concept CxVertexHasFileWrite = requires( t_TyVertex _v, t_TyFile _f )
{
  { _v.Write( _f ) };
};

// CxVertexHasMemIO: Does a type of memory I/O capabilities?
template < typename t_TyVertex >
concept CxVertexHasMemIO = requires( t_TyVertex _v, void * _pv, size_t _stLength )
{
  { t_TyVertex::GetMaxWriteLength(void) } -> size_t;
  { _v.ReadMem( _pv, _stLength ) } -> size_t;
  { _v.WriteMem( ( _pv ) ) } -> size_t;
};

// ObjOptimizer:
// This class will optimize objects by deduping vertices and then saving in an optimized format for fast loading.
template < typename t_TyVertex >
class ObjOptimizer
{
  typedef ObjOptimizer _TyThis;
public:
  typedef t_TyVertex _TyVertex;

  

  // OptimizeTinyObjShapes:
  // Read the vertices from all shapes in [_iterShapesBegin, _iterShapesEnd) in index-order and de-duplicate vertices.
  // Write the optimized vertices and indices to _pcFileOut 
  template < class t_TyIterShapes >
  void OptimizeTinyObjShapes( tinyobj::attrib_t _toAttribs, t_TyIterShapes _iterShapesBegin, t_TyIterShapes _iterShapesEnd, const char * _pcFileOut )
  {
    const size_t kstMaxVertexWriteSize = _TyVertex::GetMaxWriteLength(); // Ensure that our writeable mapped file always has at least this much space at the end.
    const size_t kstGrowBySizeBytes = kstMaxVertexWriteSize * 4096; // grow the file by this each time - we will truncate it at the end.

    // Open _pcFileOut as a mapped writeable file.
    FileObj foFile( CreateReadWriteFile( _pcFileOut ) );
    VerifyThrowSz( foFile.FIsOpen(), "Unable to open [%s] for reading and writing.", _pcFileOut );
    int iResult = FileSetSize( foFile.HFileGet(), kstGrowBySizeBytes ); // Set initial size.
    VerifyThrowSz( !iResult, "Failed to set file size on [%s], errno[%d].", _pcFileOut, GetLastErrNo() );
    FileMappingObj fmoFile( MapReadWriteHandle( foFile.HFileGet() ) );
    VerifyThrowSz( fmoFile.FIsOpen(), "Mapping failed for [%s].", _pcFileOut );
    size_t stCurrentMappedSize = kstGrowBySizeBytes;
    uint8_t * pbyCur = (uint8_t*)fmoFile.Pv();
    uint8_t * pbyEnd = pbyCur + stCurrentMappedSize;

    // First move through and count the vertices so that we can reserve space in the index array to avoid reallocation during loading. (speed)
    size_t nIndices = 0;
    for ( ; _iterShapesEnd != iterShapesCur; ++iterShapesCur )
      nIndices += iterShapesCur->mesh.indices.size();
    VerifyThrowSz( nIndices < (numeric_limits< uint32_t >::max)(), "nIndices is larger than UINT32_MAX." );
    typedef vector< uint32_t > _TyRgIndices;
    _TyRgIndices rgIndices;
    rgIndices.reserve( nIndices );

    // Make space for the count of unique vertices, and the count of indices - we can write the count of indices right now:
    pbyCur += 2 * sizeof( uint32_t ); // Also reserve a spot for the offset of the start of the indices.
    *(uint32_t*)pbyCur = nIndices;
    pbyCur += sizeof( uint32_t );

    typedef unordered_map< _TyVertex, uint32_t > _TyMapUniqueVert;
    _TyMapUniqueVert mapUniqueVertices;

    uint32_t nUniqueVertCur = 0; // As we write unique vertices to the file we increment this global index of vertices.
    t_TyIterShapes iterShapesCur = _iterShapesBegin;    
    for ( ; _iterShapesEnd != iterShapesCur; ++iterShapesCur )
    {
      for ( const auto& idxShapeCur : iterShapesCur->mesh.indices )
      {
        const tinyobj::shape_t & rspCur = *iterShapesCur;
        pair< _TyMapUniqueVert, bool > pib = mapUniqueVertices.emplace( piecewise_construct, forward_as_tuple( _toAttribs, idxShapeCur ), forward_as_tuple( nUniqueVertCur ) );
        if ( pid.second ) // If unique vertex.
        {
          rgIndices.push_back( nUniqueVertCur++ )
          // Finish import of vertex here - this saves time in the case where we have a duplicate vertex.
          const_cast< _TyVertex >( pib.first->first ).FinishImport( _toAttribs, idxShapeCur );
          // Write the vertex to the file.
          if ( pbyEnd - pbyCur < kstMaxVertexWriteSize )
          {
            // Then we have to grow the mapped writeable file:
            stCurrentMappedSize += kstGrowBySizeBytes;
            void * pvOldMapping = fmoFile.Pv();
            (void)fmoFile.Close();
            int iFileSetSize = FileSetSize( foFile.HFileGet(), stCurrentMappedSize );
            VerifyThrowSz( !iFileSetSize, "FileSetSize() failed for file [%s].", _pcFileOut );
            fmoFile.SetHMMFile( MapReadWriteHandle( m_foFile.HFileGet() ) );
            VerifyThrowSz( m_fmoFile.FIsOpen(), "Remapping the failed for file [%s].", _pcFileOut );
            pbyMappedEnd = (uint8_t*)fmoFile.Pv() + stCurrentMappedSize;
            pbyCur = (uint8_t*)fmoFile.Pv() + ( pbyCur - pvOldMapping );
          }
          pbyCur += pib.first->first->WriteMem( pbyCur );
        }
        else
          rgIndices.push_back( pib->second ); // existing vertex - just record the index.
    }
  }
  // Now write the count of vertices to the begining of the file:
  *(uint32_t*)fmoFile.Pv() = nUniqueVertCur;
  // Write the offset of the start of the indices, this allows variable length per-vertex data.
  ((uint32_t*)fmoFile.Pv())[1] = uint32_t( pbyCur - (uint8_t*)fmoFile.Pv() );
  Assert( rgIndices.size() == nIndices );
  
  // Now make sure we have enough room for the indices:
  size_t nbyIndices = rgIndices.size() * sizeof( uint32_t );
    if ( pbyEnd - pbyCur < kstMaxVertexWriteSize )
    {
    }
  }

};


__BIENUTIL_END_NAMESPACE
