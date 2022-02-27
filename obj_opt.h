#pragma once

// obj_opt.h
// This module optimizes an object loaded with tiny_obj_loader and stores it in a binary format for fast loading.
// dbien
// 24FEB2022

#include "bienutil.h"
#include <limits>
#include <unordered_map>

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
  { t_TyVertex::GetWriteLength() } -> std::convertible_to<size_t>;
  { _v.ReadMem( _pv, _stLength ) } -> std::convertible_to<size_t>;
  { _v.WriteMem( ( _pv ) ) } -> std::convertible_to<size_t>;
};

struct _OO_MappedWriteableFile
{
  typedef _OO_MappedWriteableFile _TyThis;
public:
  FileObj m_foFile;
  FileMappingObj m_fmoFile;
  const size_t m_kstVertexWriteSize;
  static const size_t s_kstVertexGrowBy = 8192;
  size_t m_stCurrentMappedSize;
  uint8_t * m_pbyCur{ nullptr };
  uint8_t * m_pbyEnd{ nullptr };
  string m_strFileOut;

  _OO_MappedWriteableFile( const char * _pcFileOut, size_t _kstVertexWriteSize ) noexcept( false )
    : m_strFileOut( _pcFileOut ),
      m_foFile( CreateReadWriteFile( _pcFileOut ) ),
      m_kstVertexWriteSize( _kstVertexWriteSize ),
      m_stCurrentMappedSize( _kstVertexWriteSize * s_kstVertexGrowBy )
  {
    VerifyThrowSz( m_foFile.FIsOpen(), "Unable to open [%s] for reading and writing.", &m_strFileOut[0] );
    int iResult = FileSetSize( m_foFile.HFileGet(), m_stCurrentMappedSize ); // Set initial size.
    VerifyThrowSz( !iResult, "Failed to set file size on [%s], errno[%d].", &m_strFileOut[0], GetLastErrNo() );
    m_fmoFile.SetHMMFile( MapReadWriteHandle( m_foFile.HFileGet() ) );
    VerifyThrowSz( m_fmoFile.FIsOpen(), "Mapping failed for [%s].", &m_strFileOut[0] );
    m_pbyCur = (uint8_t*)m_fmoFile.Pv();
    m_pbyEnd = m_pbyCur + m_stCurrentMappedSize;
  }
  template < class t_TyVertex >
  void WriteVertex( t_TyVertex const & _rvtx )
  {
    if ( size_t( m_pbyEnd - m_pbyCur ) < m_kstVertexWriteSize )
      _SetMappedSize( m_stCurrentMappedSize + ( m_kstVertexWriteSize * s_kstVertexGrowBy ) );
    m_pbyCur += _rvtx.WriteMem( m_pbyCur );
  }
  // Reset the file to the final needed size and then write the indices.
  template < class t_TyIndex >
  void WriteIndices( const t_TyIndex * _pidxBegin, const t_TyIndex * _pidxEnd )
  {
    size_t stRemaining = ( m_pbyEnd - m_pbyCur );
    size_t stNeeded = ( _pidxEnd - _pidxBegin ) * sizeof (*_pidxEnd);
    if ( stNeeded != stRemaining )
      _SetMappedSize( m_stCurrentMappedSize + ( stNeeded - stRemaining ) );
    memcpy( m_pbyCur, _pidxBegin, stNeeded );
    m_pbyCur = m_pbyEnd;
  }
private:
  // Grow or shrink the mapped file.
  void _SetMappedSize( const size_t _kstNewSize )
  {
    m_stCurrentMappedSize = _kstNewSize;
    uint8_t * pbyOldMapping = (uint8_t*)m_fmoFile.Pv();
    (void)m_fmoFile.Close();
    int iFileSetSize = FileSetSize( m_foFile.HFileGet(), m_stCurrentMappedSize );
    VerifyThrowSz( !iFileSetSize, "FileSetSize() failed for file [%s].", &m_strFileOut[0] );
    m_fmoFile.SetHMMFile( MapReadWriteHandle( m_foFile.HFileGet() ) );
    VerifyThrowSz( m_fmoFile.FIsOpen(), "Remapping the failed for file [%s].", &m_strFileOut[0] );
    m_pbyEnd = (uint8_t*)m_fmoFile.Pv() + m_stCurrentMappedSize;
    m_pbyCur = (uint8_t*)m_fmoFile.Pv() + ( m_pbyCur - pbyOldMapping );
  }
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
  static void OptimizeTinyObjShapes( tinyobj::attrib_t const & _toAttribs, t_TyIterShapes _iterShapesBegin, t_TyIterShapes _iterShapesEnd, const char * _pcFileOut )
  {
    // We want to be able to map the resultant file and use that as the store for vertices that we then pass to the GPU directly to avoid any extra copying.
    VerifyThrowSz( _TyVertex::GetWriteLength() == sizeof( _TyVertex ), "We only (currently) support a write length that is the sizeof the vertex struct itself.");
    _OO_MappedWriteableFile mwf( _pcFileOut, _TyVertex::GetWriteLength() );

    // First move through and count the vertices so that we can reserve space in the index array to avoid reallocation during loading. (speed)
    size_t nIndices = 0;
    t_TyIterShapes iterShapesCur = _iterShapesBegin;
    for ( ; _iterShapesEnd != iterShapesCur; ++iterShapesCur )
      nIndices += iterShapesCur->mesh.indices.size();
    VerifyThrowSz( nIndices < (numeric_limits< uint32_t >::max)(), "nIndices is larger than UINT32_MAX." );
    typedef vector< uint32_t > _TyRgIndices;
    _TyRgIndices rgIndices;
    rgIndices.reserve( nIndices );

    // Make space for the count of unique vertices, and the count of indices - we can write the count of indices right now:
    mwf.m_pbyCur += sizeof( uint32_t );
    *(uint32_t*)mwf.m_pbyCur = (uint32_t)nIndices;
    mwf.m_pbyCur += sizeof( uint32_t );
    const size_t stAlignOfVertex = alignof( _TyVertex );
    static_assert( stAlignOfVertex >= alignof( uint32_t ) );
    // Align to the vertex alignment:
    const size_t kstBegin = ( mwf.m_pbyCur - (uint8_t*)mwf.m_fmoFile.Pv() );
    mwf.m_pbyCur += ( kstBegin % stAlignOfVertex ) ? ( stAlignOfVertex - ( kstBegin % stAlignOfVertex ) ) : 0;

    typedef unordered_map< _TyVertex, uint32_t > _TyMapUniqueVert;
    _TyMapUniqueVert mapUniqueVertices;

    uint32_t nUniqueVertCur = 0; // As we write unique vertices to the file we increment this global index of vertices.
    iterShapesCur = _iterShapesBegin;    
    for ( ; _iterShapesEnd != iterShapesCur; ++iterShapesCur )
    {
      for ( const tinyobj::index_t & ridxShapeCur : iterShapesCur->mesh.indices )
      {
        const tinyobj::shape_t & rspCur = *iterShapesCur;
        pair< _TyMapUniqueVert::const_iterator, bool > pib = mapUniqueVertices.emplace( piecewise_construct, forward_as_tuple( _toAttribs, ridxShapeCur ), forward_as_tuple( nUniqueVertCur ) );
        if ( pib.second ) // If unique vertex.
        {
          rgIndices.push_back( nUniqueVertCur++ );
          // Finish import of vertex here - this saves time in the case where we have a duplicate vertex.
          const_cast< _TyVertex & >( pib.first->first ).FinishImport( _toAttribs, ridxShapeCur );
          // Write the vertex to the file.
          mwf.WriteVertex( pib.first->first );
          // We should always be aligned to a vertex after a write.
          VerifyThrowSz( !( ( mwf.m_pbyCur - (uint8_t*)mwf.m_fmoFile.Pv() ) % stAlignOfVertex ), 
            "Not aligned to a vertex after write. nUniqueVertCur[%u].", nUniqueVertCur );
        }
        else
        {
#ifndef NDEBUG
          // All elements of the vertex should match.
          _TyVertex dbg_vtx( _toAttribs, ridxShapeCur );
          dbg_vtx.FinishImport( _toAttribs, ridxShapeCur );
          Assert( dbg_vtx.FCompare( pib.first->first, false ) );
#endif //!NDEBUG
          rgIndices.push_back( pib.first->second ); // existing vertex - just record the index.
        }
      }
    }
    // Now write the count of vertices to the begining of the file:
    ((uint32_t*)mwf.m_fmoFile.Pv())[0] = nUniqueVertCur;
    Assert( rgIndices.size() == nIndices );
    mwf.WriteIndices( &rgIndices[0], &rgIndices[0] + nIndices );
  }
};


__BIENUTIL_END_NAMESPACE
