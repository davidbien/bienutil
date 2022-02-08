#pragma once

// stb_image_util.h
// C++ utility objects for the stb_image "library".
// dbien
// 07FEB2022

#include "bienutil.h"
#include "stb_image.h"

__BIENUTIL_BEGIN_NAMESPACE

// StbImageSmartPtr:
// Smart pointer for containing an image loaded from the stb_image library.
class StbImageSmartPtr
{
  typedef StbImageSmartPtr _TyThis;
public:
  StbImageSmartPtr() noexcept = default;
  StbImageSmartPtr( StbImageSmartPtr const & ) = delete;
  StbImageSmartPtr & operator=( StbImageSmartPtr const & ) = delete;
  StbImageSmartPtr( StbImageSmartPtr && _rr ) noexcept
  {
    std::swap( m_pcImageData, _rr.m_pcImageData );
  }
  StbImageSmartPtr & operator=( StbImageSmartPtr && _rr ) noexcept
  {
    StbImageSmartPtr acquire( std::move( _rr ) );
    swap( acquire );
    return *this;
  }
  void swap( _TyThis & _r ) noexcept
  {
    std::swap( m_pcImageData, _r.m_pcImageData );
  }

  StbImageSmartPtr( stbi_uc * _pcImageData ) noexcept
    : m_pcImageData( _pcImageData )
  {
  }
  void Init( stbi_uc * _pcImageData ) noexcept
  {
    Release();
    m_pcImageData = _pcImageData;
  }
  ~StbImageSmartPtr()
  {
    if ( m_pcImageData )
      stbi_image_free( m_pcImageData );
  }
  void Release() noexcept
  {
    if ( m_pcImageData )
    {
      stbi_uc * pcImageData = m_pcImageData;
      m_pcImageData = nullptr;
      stbi_image_free( pcImageData );
    }
  }
  bool operator ! () const noexcept
  {
    return !m_pcImageData;
  }
  stbi_uc * PcGetImageData() noexcept
  {
    return m_pcImageData;
  }
  const stbi_uc * PcGetImageData() const noexcept
  {
    return m_pcImageData;
  }
protected:
  stbi_uc * m_pcImageData{ nullptr };
};

__BIENUTIL_END_NAMESPACE
