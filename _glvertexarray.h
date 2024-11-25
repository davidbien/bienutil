#pragma once

// _glvertexarray.h
// Utility object for maintaining sets of vertex arrays.
// dbien
// 05FEB2022

#include <cstdarg>
#include <array>
#include "glad/glad.h"
#include "bienutil.h"
#include "bien_assert.h"

__BIENUTIL_BEGIN_NAMESPACE

// GLVertexArrayFixed:
// This manages a fixed set of vertext arrays which are created and deleted en masse.
// There are other paragigms than this that would be useful but this is one useful paradigm.
template < GLsizei t_knArrays >
class GLVertexArrayFixed
{
  typedef GLVertexArrayFixed _TyThis;
public:
  static_assert( t_knArrays > 0 );
  typedef array< GLuint, t_knArrays > _TyArray;

  GLVertexArrayFixed() noexcept = default;
  GLVertexArrayFixed( GLVertexArrayFixed const & ) = delete;
  GLVertexArrayFixed & operator=( GLVertexArrayFixed const & ) = delete;
  GLVertexArrayFixed( GLVertexArrayFixed && _rr ) noexcept
  {
    m_rgArrays.swap( _rr.m_rgArrays );
  }
  GLVertexArrayFixed & operator=( GLVertexArrayFixed && _rr ) noexcept
  {
    GLVertexArrayFixed acquire( std::move( _rr ) );
    swap( acquire );
    return *this;
  }
  void swap( _TyThis & _r ) noexcept
  {
    m_rgArrays.swap( _r.m_rgArrays );
  }

  // _fInit: Whether to initialize the buffers. If this initialization fails then we throw.
  GLVertexArrayFixed( bool _fInit ) noexcept(false)
  {
    if ( _fInit )
    {
      glGenVertexArrays( t_knArrays, &m_rgArrays[0] );
      VerifyThrowSz( FIsInited(), "glGenVertexArrays() failed." );
    }
  }
  ~GLVertexArrayFixed()
  {
    if ( FIsInited() )
      glDeleteVertexArrays( t_knArrays, &m_rgArrays[0] );
  }
  // Either all elements should be zero or all non-zero.
  void AssertValid() const
  {
#if ASSERTSENABLED
    const GLuint * puCur = &m_rgArrays[0];
    const GLuint * puTail = puCur + t_knArrays - 1;
    for ( ; puTail != puCur; ++puCur )
    {
      Assert( !*puCur == !puCur[1] );
    }
#endif //ASSERTSENABLED
  }
  bool FIsInited() const noexcept
  {
    AssertValid();
    return !!m_rgArrays[0];
  }
  void Release() noexcept
  {
    if ( FIsInited() )
    {
      _TyArray rgArrays = { 0 };
      m_rgArrays.swap( rgArrays );
      glDeleteBuffers( t_knArrays, rgArrays );
      AssertValid();
    }
  }
  GLuint operator []( size_t _n ) const noexcept(false)
  {
    Assert( FIsInited() );
    return m_rgArrays.at( _n );// at performs bounds checking.
  }
  // When there is just a single vertex array then these methods are enabled:
  void Bind() const noexcept(false)
    requires ( 1 == t_knArrays )
  {
    BindOne( 0 );
  }
  bool FIsBound() const noexcept(false)
    requires ( 1 == t_knArrays )
  {
    return FIsOneBound( 0 );
  }
  // Binds a single of the contained buffers to the given target.
  void BindOne( size_t _n ) const noexcept(false)
  {
    glBindVertexArray( (*this)[ _n ] );
    Assert( FIsOneBound( _n ) );
  }
  // Check if the given buffer is bound to the given target.
  // This translates the target to the appropriate querying flag.
  bool FIsOneBound( size_t _n ) const noexcept(false)
  {
    return !!glIsVertexArray( (*this)[ _n ] );
  }
protected:
  _TyArray m_rgArrays = {0};
};

__BIENUTIL_END_NAMESPACE
