#pragma once

// vma_raii.h
// Extends various vk::raii objects to support the VulkanMemoryAllocator implementation.
// dbien
// 07MAR2022

#include "bienutil.h"
#include "shared_obj.h"
#include "vulkan_raii.hpp"
#include "vk_mem_alloc.h"

__BIENUTIL_BEGIN_NAMESPACE

static constexpr uint32_t GetVulkanApiVersion()
{
#if VMA_VULKAN_VERSION == 1003000
    return VK_API_VERSION_1_3;
#elif VMA_VULKAN_VERSION == 1002000
    return VK_API_VERSION_1_2;
#elif VMA_VULKAN_VERSION == 1001000
    return VK_API_VERSION_1_1;
#elif VMA_VULKAN_VERSION == 1000000
    return VK_API_VERSION_1_0;
#else
#error Invalid VMA_VULKAN_VERSION.
    return UINT32_MAX;
#endif
}

// VulkanInstance:
class VulkanInstance : public vk::raii::Instance
{
  typedef vk::raii::Instance _TyBase;
  typedef VulkanInstance _TyThis;
public:
  using _TyBase::_TyBase;

};

struct _QueueFlagSupportIndex
{
  uint32_t m_nQueueIndex;
  vk::QueueFlags m_grfQueueFlags{0};
  bool m_fSupportPresent{false};
};

// VulkanPhysicalDevice:
class VulkanPhysicalDevice : public vk::raii::PhysicalDevice
{
  typedef vk::raii::PhysicalDevice _TyBase;
  typedef VulkanPhysicalDevice _TyThis;
public:
  VulkanPhysicalDevice & operator=( VulkanPhysicalDevice && _rr )
  {
    _TyBase::operator = ( _rr );
    m_PhysicalDeviceProperties = _rr.m_PhysicalDeviceProperties;
    m_PhysicalDeviceFeatures = _rr.m_PhysicalDeviceFeatures;
    m_PhysicalDeviceMemoryProperties = _rr.m_PhysicalDeviceMemoryProperties;
    m_rgqfpQueueProps = std::move( _rr.m_rgqfpQueueProps );
    m_rgqfsiQueueSupport = std::move( _rr.m_rgqfsiQueueSupport ); // Results of queue support;
    m_fQueuesHaveSupport = std::exchange( _rr.m_fQueuesHaveSupport, false };
    m_psrfSurface = std::exchange( _rr.m_psrfSurface, nullptr );
    m_rgFormats = std::move( _rr.m_rgFormats );
    m_SurfaceCapabilitiesKHR = _rr.m_SurfaceCapabilitiesKHR;
    m_rgFormats = std::move( _rr. m_rgFormats );
    m_rgPresentModeKHR = std::move( _rr.m_rgPresentModeKHR );
  }
  
  // We always construct this via move constructor and then we initialize it by getting all the properties, etc.
  // If a surface is not required then nullptr may be passed for it - i.e. for off-screen rendering.
  VulkanPhysicalDevice( vk::raii::PhysicalDevice && _rr, vk::QueueFlags _grfQueueFlags, vk::raii::SurfaceKHR const * _psrfSurface )
    : _TyBase( std::move( _rr ) ),
      m_psrfSurface( _psrfSurface ),
      m_grfQueueFlags( _grfQueueFlags ) // save for support check.
  {
    m_PhysicalDeviceProperties = getProperties();
    m_PhysicalDeviceFeatures = getFeatures();
    m_PhysicalDeviceMemoryProperties = getMemoryProperties();
    m_rgqfpQueueProps = getQueueFamilyProperties();
    // Now get surface-related info if needed:
    if ( m_psrfSurface )
    {
      m_SurfaceCapabilitiesKHR = getSurfaceCapabilitiesKHR( **m_psrfSurface );
      m_rgFormats = getSurfaceFormatsKHR( **m_psrfSurface );
      m_rgPresentModeKHR = getSurfacePresentModesKHR( **m_psrfSurface );
    }

    // Now check for support of all required queue types:
    bool fPresentFound = !m_psrfSurface;
    const vk::QueueFamilyProperties * pqfpCur = m_rgqfpQueueProps.data();
    const vk::QueueFamilyProperties * const pqfpEnd = pqfpCur + m_rgqfpQueueProps.size();
    for ( ; ( pqfpEnd != pqfpCur ) && !!_grfQueueFlags && !fPresentFound; ++pqfpCur )
    {
      uint32_t nCurIndex = uint32_t( pqfpCur - m_rgqfpQueueProps.data() );
      vk::QueueFlags grfqfSupported = _grfQueueFlags & pqfpCur->queueFlags;
      bool fPresentSupported = !fPresentFound && getSurfaceSupportKHR( nCurIndex, **m_psrfSurface );
      if ( grfqfSupported || fPresentSupported )
      {
        _QueueFlagSupportIndex qfsi = { nCurIndex, grfqfSupported, fPresentSupported };
        m_rgqfsiQueueSupport.push_back( qfsi );
        fPresentFound ||= fPresentFound;
        _grfQueueFlags &= ~grfqfSupported;
      }
    }
    m_fQueuesHaveSupport = !_grfQueueFlags && fPresentFound;
  }

  // After construction this checks that all requested queues are supported by this device.
  bool FQueuesHaveSupport() const
  {
    return m_fQueuesHaveSupport;
  }
  // Return all required queue families for the support requested.
  std::vector< uint32_t > RgGetQueueFamilies() const
  {
    Assert( m_fQueuesHaveSupport );
    std::vector< uint32_t > rgQueueIndices;
    rgQueueIndices.reserve( m_rgqfsiQueueSupport.size() );
    const _QueueFlagSupportIndex * pqfsiCur = m_rgqfsiQueueSupport.data();
    const _QueueFlagSupportIndex * const pqfsiEnd = pqfsiCur + m_rgqfsiQueueSupport.size();
    for ( ; pqfsiEnd != pqfsiCur; ++pqfsiCur )
      rgQueueIndices.push_back( pqfsiCur->m_nQueueIndex );
    return rgQueueIndices;
  }

  vk::PhysicalDeviceProperties m_PhysicalDeviceProperties;
  vk::PhysicalDeviceFeatures m_PhysicalDeviceFeatures;
  vk::PhysicalDeviceMemoryProperties m_PhysicalDeviceMemoryProperties;
  std::vector< vk::QueueFamilyProperties > m_rgqfpQueueProps;
  std::vector< _QueueFlagSupportIndex > m_rgqfsiQueueSupport; // Results of queue support;
  bool m_fQueuesHaveSupport{false};
// surface-related properties:
  vk::raii::SurfaceKHR const * m_psrfSurface{nullptr};
  std::vector< vk::SurfaceFormatKHR > m_rgFormats;
  vk::SurfaceCapabilitiesKHR m_SurfaceCapabilitiesKHR;
  std::vector< vk::SurfaceFormatKHR > m_rgFormats;
  std::vector< vk::PresentModeKHR > m_rgPresentModeKHR;
};

// VmaDevice: A vk::raii::Device that contains a VmaAllocator.
// Use protected inheritance to enure we don't do anything weird.
class VmaDevice : protected vk::raii::Device
{
  typedef vk::raii::Device _TyBase;
  typedef VmaDevice _TyThis;
public:

  VmaDevice(  VulkanPhysicalDevice const & _rpdPhysicalDevice,
              vk::raii::SurfaceKHR const & _psrfSurface, // Contains a reference to our instance.
              std::unordered_map<const char *, bool> _umExtentionsOpt )
    : m_rpdPhysicalDevice( _rpdPhysicalDevice ),
      m_psrfSurface( _psrfSurface )
  {
      vmaCreateAllocator
  }

  


  // maintain a reference to our physical device since our base doesn't.
  vk::raii::PhysicalDevice const & m_rpdPhysicalDevice;
  vk::raii::SurfaceKHR const & m_psrfSurface;
  // We maintain a reference to a VulkanInstance since our m_vmaAllocator has one that isn't ref-counted.
	VmaAllocator m_vmaAllocator{ nullptr };

};

__BIENUTIL_END_NAMESPACE
