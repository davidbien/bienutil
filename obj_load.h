#pragma once

// obj_load.h
// This module loads an optimized object by mapping the file. The data can be used directly using the accessors.
// dbien
// 25FEB2022

#include "bienutil.h"

__BIENUTIL_BEGIN_NAMESPACE

template < class t_TyVertex >
class OptimizedObjLoader
{
  typedef OptimizedObjLoader _TyThis;
public:
  typedef t_TyVertex _TyVertex;

  // We need only keep open the mapped file - we can close the file proper.
  FileMappingObj m_fmoFile;

  OptimizedObjLoader( const char * _pcFileName )
  {
    FileObj fileLocal( OpenReadOnlyFile(_szFilename) ); // We will close the file after we map it since that is fine and results in easier cleanup.
    if ( !fileLocal.FIsOpen() )
      THROWBADJSONSTREAMERRNO(GetLastErrNo(), "Unable to OpenReadOnlyFile() file [%s]", _szFilename);
    // Now get the size of the file and then map it.
    vtyHandleAttr attrFile;
    int iResult = GetHandleAttrs( fileLocal.HFileGet(), attrFile );
    if (-1 == iResult)
      THROWBADJSONSTREAMERRNO(GetLastErrNo(), "GetHandleAttrs() failed for [%s]", _szFilename);
    uint64_t u64Size = GetSize_HandleAttr( attrFile );
    if (0 == u64Size )
      THROWBADJSONSTREAM("File [%s] is empty - it contains no JSON value.", _szFilename);
    if (!!(u64Size % sizeof(t_tyPersistAsChar)))
      THROWBADJSONSTREAM("File [%s]'s size not multiple of char size u64Size[%llu].", _szFilename, u64Size);
    if ( !FIsRegularFile_HandleAttr( attrFile ) )
      THROWBADJSONSTREAM("File [%s] is not a regular file.", _szFilename);
    m_fmoMapping.SetHMMFile( MapReadOnlyHandle( fileLocal.HFileGet(), nullptr ) );
    if ( !m_fmoMapping.FIsOpen() )
      THROWBADJSONSTREAMERRNO(GetLastErrNo(), "MapReadOnlyHandle() failed to map [%s], size [%llu].", _szFilename, u64Size);
    m_pcpxBegin = m_pcpxCur = (const t_tyPersistAsChar *)m_fmoMapping.Pv();
    m_pcpxEnd = m_pcpxCur + (u64Size / sizeof(t_tyPersistAsChar));
    m_fHasLookahead = false;    // ensure that if we had previously been opened that we don't think we still have a lookahead.
    m_szFilename = _szFilename; // For error reporting and general debugging. Of course we don't need to store this.
  }

  size_t NVertices() const noexcept
  {
    return *(uint32_t*)m_fmoFile.Pv();
  }
  size_t NIndices() const noexcept
  {
    return ((uint32_t*)m_fmoFile.Pv())[1];
  }
  const _TyVertex * PVertexBegin() const noexcept
  {
    const size_t kstBegin = 2 * sizeof( uint32_t );
    const size_t kstAlignVertex = alignof( _TyVertex );
    const size_t kstVertexBegin = kstBegin + ( kstAlignVertex - ( kstAlignVertex % kstBegin ) );
    return (const _TyVertex *)( (uint8_t*)m_fmoFile.Pv() + kstVertexBegin );
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
