#pragma once

// _gltexture.h
// OpenGL texture containing object.
// dbien
// 07FEB2022

#include "bienutil.h"
#include "glad/glad.h"

__BIENUTIL_BEGIN_NAMESPACE

// GLTextureContainerFixed:
// This manages a fixed set of textures which are created and deleted en masse.
// There are other paragigms than this that would be useful but this is one useful paradigm.
template < GLsizei t_knTextures >
class GLTextureContainerFixed
{
  typedef GLTextureContainerFixed _TyThis;
public:
  static_assert( t_knTextures > 0 );
  typedef array< GLuint, t_knTextures > _TyArray;

  GLTextureContainerFixed() noexcept = default;
  GLTextureContainerFixed( GLTextureContainerFixed const & ) = delete;
  GLTextureContainerFixed & operator=( GLTextureContainerFixed const & ) = delete;
  GLTextureContainerFixed( GLTextureContainerFixed && _rr ) noexcept
  {
    m_rgTextures.swap( _rr.m_rgTextures );
  }
  GLTextureContainerFixed & operator=( GLTextureContainerFixed && _rr ) noexcept
  {
    GLTextureContainerFixed acquire( std::move( _rr ) );
    swap( acquire );
    return *this;
  }
  void swap( _TyThis & _r ) noexcept
  {
    m_rgTextures.swap( _r.m_rgTextures );
  }

  // _fInit: Whether to initialize the textures. If this initialization fails then we throw.
  GLTextureContainerFixed( bool _fInit ) noexcept(false)
  {
    if ( _fInit )
    {
      glGenTextures( t_knTextures, &m_rgTextures[0] );
      VerifyThrowSz( FIsInited(), "glGenTextures() failed." );
    }
  }
  ~GLTextureContainerFixed()
  {
    if ( FIsInited() )
      glDeleteTextures( t_knTextures, &m_rgTextures[0] );
  }
  // Either all elements should be zero or all non-zero.
  void AssertValid() const
  {
#if ASSERTSENABLED
    const GLuint * puCur = &m_rgTextures[0];
    const GLuint * puTail = puCur + t_knTextures - 1;
    for ( ; puTail != puCur; ++puCur )
    {
      Assert( !*puCur == !puCur[1] );
    }
#endif //ASSERTSENABLED
  }
  bool FIsInited() const noexcept
  {
    AssertValid();
    return !!m_rgTextures[0];
  }
  void Release() noexcept
  {
    if ( FIsInited() )
    {
      _TyArray rgTextures = { 0 };
      m_rgTextures.swap( rgTextures );
      glDeleteTextures( t_knTextures, rgTextures );
      AssertValid();
    }
  }
  GLuint operator []( size_t _n ) const noexcept(false)
  {
    Assert( FIsInited() );
    return m_rgTextures.at( _n );// at performs bounds checking.
  }
  // When there is just a texture then these methods are enabled:
  void Bind( GLenum _eTarget ) const noexcept(false)
    requires ( 1 == t_knTextures )
  {
    BindOne( 0, _eTarget );
  }
  bool FIsBound( GLenum _eTarget ) const noexcept(false)
    requires ( 1 == t_knTextures )
  {
    return FIsOneBound( 0, _eTarget );
  }
  // Binds a single of the contained textures to the given target.
  void BindOne( size_t _n, GLenum _eTarget ) const noexcept(false)
  {
    Assert( FIsInited() );
    glBindTexture( _eTarget, (*this)[ _n ] );
    Assert( FIsOneBound( _n, _eTarget ) );
  }
  // Check if the given texture is bound to the given target.
  // This translates the target to the appropriate querying flag.
  bool FIsOneBound( size_t _n, GLenum _eTarget ) const noexcept(false)
  {
    GLenum eBindingForTarget = EGetBindingFromTarget( _eTarget );
    VerifyThrowSz( !!eBindingForTarget, "Invalid texture _eTarget[0x%x]", _eTarget );
    GLuint uBound;
    glGetIntegerv( eBindingForTarget, (GLint*)&uBound );
    return uBound == (*this)[ _n ];
  }

  static GLenum EGetBindingFromTarget( GLenum _eTarget ) noexcept
  {
    switch( _eTarget )
    {
      case GL_TEXTURE_1D:
        return GL_TEXTURE_BINDING_1D;
      case GL_TEXTURE_2D:
        return GL_TEXTURE_BINDING_2D;
      case GL_TEXTURE_3D:
        return GL_TEXTURE_BINDING_3D;
      case GL_TEXTURE_1D_ARRAY:
        return GL_TEXTURE_BINDING_1D_ARRAY;
      case GL_TEXTURE_2D_ARRAY:
        return GL_TEXTURE_BINDING_2D_ARRAY;
      case GL_TEXTURE_RECTANGLE:
        return GL_TEXTURE_BINDING_RECTANGLE;
      case GL_TEXTURE_CUBE_MAP:
        return GL_TEXTURE_BINDING_CUBE_MAP;
#if 0
      case GL_TEXTURE_CUBE_MAP_ARRAY:
        return GL_TEXTURE_BINDING_CUBE_MAP_ARRAY;
#endif //0
      case GL_TEXTURE_BUFFER:
        return GL_TEXTURE_BINDING_BUFFER;
      case GL_TEXTURE_2D_MULTISAMPLE:
        return GL_TEXTURE_BINDING_2D_MULTISAMPLE;
      case GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
        return GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY;
      default:
        Assert( false );
        return 0;       
    }
  }

protected:
  _TyArray m_rgTextures = { 0 };
};

// A texture container that takes the names of the targets as template parameters.
template < GLenum ... t_knsTargets >
class GLTextureContainerTargets : public GLTextureContainerFixed< sizeof...( t_knsTargets ) >
{
  typedef GLTextureContainerTargets _TyThis;
  typedef GLTextureContainerFixed< sizeof...( t_knsTargets ) > _TyBase;
public:
  // We want same semantics as base class.
  using GLTextureContainerTargets::GLTextureContainerTargets;
  // We don't define any members so swaping with a base is reasonable.
  using _TyBase::swap;
  using _TyBase::FIsInited;
  using _TyBase::operator [];

  // We bind all textures in the order of the declaration or the targets.
  void BindAll()
  {
    _Bind( 0, t_knsTargets... );
  }
protected:
  void _Bind( size_t _n ) { } // base case.
  template < typename ... t_knsRemainingTargets >
  void _Bind( size_t _n, GLenum _eTarget, t_knsRemainingTargets ... _esTargetsRemaining )
  {
    _TyBase::BindOne( _n, _eTarget );
    _Bind( _n + 1, _esTargetsRemaining... );
  }
};

__BIENUTIL_END_NAMESPACE
