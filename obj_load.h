#pragma once

// obj_load.h
// This module loads an optimized object by mapping the file. The data can be used directly using the accessors.
// dbien
// 25FEB2022

#include "bienutil.h"
#include "obj_opt.h"

__BIENUTIL_BEGIN_NAMESPACE

template < class t_TyVertex >
class OptimizedObjLoader
{
  typedef OptimizedObjLoader _TyThis;
public:
  typedef t_TyVertex _TyVertex;

  // We need only keep open the mapped file - we can close the file proper.
  string m_strFileName;
  FileMappingObj m_fmoMapping;

  OptimizedObjLoader( const char * _pcFileName ) noexcept(false)
    : m_strFileName( _pcFileName )
  {
    FileObj fileLocal( OpenReadOnlyFile(_pcFileName) ); // We will close the file after we map it since that is fine and results in easier cleanup.
    VerifyThrowSz( fileLocal.FIsOpen(), "Unable to OpenReadOnlyFile() file [%s]", &m_strFileName[0] );
    vtyHandleAttr attrFile;
    int iResult = GetHandleAttrs( fileLocal.HFileGet(), attrFile );
    VerifyThrowSz( !iResult, "GetHandleAttrs() failed for [%s] errno[%u]", &m_strFileName[0], GetLastErrNo() );
    uint64_t u64Size = GetSize_HandleAttr( attrFile );
    VerifyThrowSz( !!u64Size, "File [%s] is empty.", &m_strFileName[0] );
    VerifyThrowSz( FIsRegularFile_HandleAttr( attrFile ), "File [%s] is not a regular file.", &m_strFileName[0] );
    m_fmoMapping.SetHMMFile( MapReadOnlyHandle( fileLocal.HFileGet(), nullptr ) );
    VerifyThrowSz( m_fmoMapping.FIsOpen(), "MapReadOnlyHandle() failed to map [%s], size [%llu].", &m_strFileName[0], u64Size );
    // Now check validity of file as much as we can - really the only thing to check is that the end of the indices corresponds to the end
    //  of the file itself.
    VerifyThrowSz( (size_t)u64Size == ( (uint8_t*)PIndexEnd() - (uint8_t*)m_fmoMapping.Pv() ), "Invalid or corrupt file [%s].", &m_strFileName[0] );
  }

  const OptObjAttribs * PAttribs() const noexcept
  {
    return (OptObjAttribs*)m_fmoMapping.Pv();
  }

  size_t NVertices() const noexcept
  {
    return PAttribs()->m_nVertices;
  }
  size_t NIndices() const noexcept
  {
    return PAttribs()->m_nIndices;
  }
  float FlRadius() const noexcept
  {
    return PAttribs()->m_flMaxDistance;
  }
  const _TyVertex * PVertexBegin() const noexcept
  {
    const size_t kstBegin = sizeof( OptObjAttribs );
    const size_t kstAlignVertex = alignof( _TyVertex );
    const size_t kstVertexBegin = kstBegin + ( ( kstBegin % kstAlignVertex ) ? ( kstAlignVertex - ( kstBegin % kstAlignVertex ) ) : 0 );
    return (const _TyVertex *)( (uint8_t*)m_fmoMapping.Pv() + kstVertexBegin );
  }
  const _TyVertex * PVertexEnd() const noexcept
  {
    return PVertexBegin() + NVertices();
  }
  const uint32_t * PIndexBegin() const noexcept
  {
    return (const uint32_t*)PVertexEnd();
  }
  const uint32_t * PIndexEnd() const noexcept
  {
    return PIndexBegin() + NIndices();
  }
};

__BIENUTIL_END_NAMESPACE
