#pragma once

// _glbuffer.h
// OpenGL buffer containing object.
// dbien
// 04FEB2022

#include "bienutil.h"
#include "glad/glad.h"

__BIENUTIL_BEGIN_NAMESPACE

// GLBufferContainerFixed:
// This manages a fixed set of buffers which are created and deleted en masse.
// There are other paragigms than this that would be useful but this is one useful paradigm.
template < GLsizei t_knBuffers >
class GLBufferContainerFixed
{
  typedef GLBufferContainerFixed _TyThis;
public:
  static_assert( t_knBuffers > 0 );
  typedef array< GLuint, t_knBuffers > _TyArray;

  GLBufferContainerFixed() noexcept = default;
  GLBufferContainerFixed( GLBufferContainerFixed const & ) = delete;
  GLBufferContainerFixed & operator=( GLBufferContainerFixed const & ) = delete;
  GLBufferContainerFixed( GLBufferContainerFixed && _rr ) noexcept
  {
    m_rgBuffers.swap( _rr.m_rgBuffers );
  }
  GLBufferContainerFixed & operator=( GLBufferContainerFixed && _rr ) noexcept
  {
    GLBufferContainerFixed acquire( std::move( _rr ) );
    swap( acquire );
    return *this;
  }
  void swap( _TyThis & _r ) noexcept
  {
    m_rgBuffers.swap( _r.m_rgBuffers );
  }

  // _fInit: Whether to initialize the buffers. If this initialization fails then we throw.
  GLBufferContainerFixed( bool _fInit ) noexcept(false)
  {
    if ( _fInit )
    {
      glGenBuffers( t_knBuffers, &m_rgBuffers[0] );
      VerifyThrowSz( FIsInited(), "glGenBuffers() failed." );
    }
  }
  ~GLBufferContainerFixed()
  {
    if ( FIsInited() )
      glDeleteBuffers( t_knBuffers, m_rgBuffers );
  }
  // Either all elements should be zero or all non-zero.
  void AssertValid() const
  {
#if ASSERTSENABLED
    const GLuint * puCur = &m_rgBuffers[0];
    const GLuint * puTail = puCur + t_knBuffers - 1;
    for ( ; puTail != puCur; ++puCur )
    {
      Assert( !*puCur == !puCur[1] );
    }
#endif //ASSERTSENABLED
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

  // Binds a single of the contained buffers to the given target.
  void BindOne( size_t _n, GLenum _eTarget ) noexcept(false)
  {
    Assert( FIsInited() );
    glBindBuffer( _eTarget, (*this)[ _n ] );
    Assert( FIsOneBound( _n, _eTarget ) );
  }
  // Check if the given buffer is bound to the given target.
  // This translates the target to the appropriate querying flag.
  bool FIsOneBound( size_t _n, GLenum _eTarget ) noexcept(false)
  {
    GLenum eBindingForTarget = EGetBindingFromTarget( _eTarget );
    VerifyThrowSz( !!eBindingForTarget, "Invalid _eTarget[0x%x]", _eTarget );
    GLuint uBound;
    glGetIntegerv( eBindingForTarget, (GLint*)&uBound );
    return uBound == (*this)[ _n ];
  }

  static GLenum EGetBindingFromTarget( GLenum _eTarget ) noexcept
  {
    switch( _eTarget )
    {
      case GL_ARRAY_BUFFER:
        return GL_ARRAY_BUFFER_BINDING;
      case GL_ATOMIC_COUNTER_BUFFER:
        return GL_ATOMIC_COUNTER_BUFFER_BINDING;
      case GL_COPY_READ_BUFFER:
        return GL_COPY_READ_BUFFER_BINDING;
      case GL_COPY_WRITE_BUFFER:
        return GL_COPY_WRITE_BUFFER_BINDING;
      case GL_DISPATCH_INDIRECT_BUFFER:
        return GL_DISPATCH_INDIRECT_BUFFER_BINDING;
      case GL_DRAW_INDIRECT_BUFFER:
        return GL_DRAW_INDIRECT_BUFFER_BINDING;
      case GL_ELEMENT_ARRAY_BUFFER:
        return GL_ELEMENT_ARRAY_BUFFER_BINDING;
      case GL_PIXEL_PACK_BUFFER:
        return GL_PIXEL_PACK_BUFFER_BINDING;
      case GL_PIXEL_UNPACK_BUFFER:
        return GL_PIXEL_UNPACK_BUFFER_BINDING;
      case GL_QUERY_BUFFER:
        return GL_QUERY_BUFFER_BINDING;
      case GL_SHADER_STORAGE_BUFFER:
        return GL_SHADER_STORAGE_BUFFER_BINDING;
      case GL_TEXTURE_BUFFER:
        return GL_TEXTURE_BUFFER_BINDING;
      case GL_TRANSFORM_FEEDBACK_BUFFER:
        return GL_TRANSFORM_FEEDBACK_BUFFER_BINDING;
      case GL_UNIFORM_BUFFER:
        return GL_UNIFORM_BUFFER_BINDING;
      default:
        Assert( false );
        return 0;       
    }
  }

protected:
  _TyArray m_rgBuffers = { 0 };
};

__BIENUTIL_END_NAMESPACE
