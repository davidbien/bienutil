#pragma once

// _glbuffer.h
// OpenGL buffer containing object.
// dbien
// 04FEB2022

#include "bienutil.h"

__BIENUTIL_BEGIN_NAMESPACE

template < GLsizei t_knBuffers >
class GLBufferConainerFixed
{
  typedef GLBufferConainerFixed _TyThis;
public:
  typedef array< GLuint, t_knBuffers > _TyArray;

  GLBufferConainerFixed() noexcept = default;
  GLBufferConainerFixed( GLBufferConainerFixed const & ) = delete;
  GLBufferConainerFixed & operator=( GLBufferConainerFixed const & ) = delete;
  GLBufferConainerFixed( GLBufferConainerFixed && _rr ) noexcept
  {
    m_rgBuffers.swap( _rr.m_rgBuffers );
  }
  GLBufferConainerFixed & operator=( GLBufferConainerFixed && _rr ) noexcept
  {
    GLBufferConainerFixed acquire( std::move( _rr ) );
    swap( acquire );
    return *this;
  }
  void swap( _TyThis & _r ) noexcept
  {
    m_rgBuffers.swap( _r.m_rgBuffers );
  }

  // _fInit: Whether to initialize the buffers. If this initialization fails then we throw.
  GLBufferConainerFixed( bool _fInit ) noexcept(false)
  {
    if ( _fInit )
    {
      glGenBuffers( t_knBuffers, &m_rgBuffers[0] );
      VerifyThrowSz( FIsInited(), "glGenBuffers() failed to generate buffers." );
    }
  }
  ~GLBufferConainerFixed()
  {
    if ( FIsInited() )
      glDeleteBuffers( t_knBuffers, m_rgBuffers );
  }
  // Either all elements should be zero or all non-zero.
  void AssertValid() const
  {
    const GLuint * puCur = &m_rgBuffers[0];
    const GLuint * puTail = puCur + t_knBuffers - 1;
    for ( ; puTail != puCur; ++puCur )
    {
      Assert( !*puCur == !puCur[1] );
    }
  }
  bool FIsInited() const noexcept
  {
    AssertValid();
    return !!m_rgBuffers[0];
  }
  void Release() noexcept
  {
    if ( FIsInited() )
    {
      _TyArray rgBuffers = { 0 };
      m_rgBuffers.swap( rgBuffers );
      glDeleteBuffers( t_knBuffers, rgBuffers );
      AssertValid();
    }
  }
  GLuint operator []( size_t _n ) const noexcept(false)
  {
    Assert( FIsInited() );
    return m_rgBuffers.at( _n );// at performs bounds checking.
  }

protected:
  _TyArray m_rgBuffers = { 0 };
};

__BIENUTIL_END_NAMESPACE
