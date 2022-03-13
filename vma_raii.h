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

// _ExtensionByVersion:
// This codfies the idea of extensions being incorporated into Vulkan versions.
struct _ExtensionByVersion
{
  _ExtensionByVersion( const char * _szExtensionName, uint32_t _nVersionPromoted )
    : m_szExtensionName( _szExtensionName ),
      m_nVersionPromoted( _nVersionPromoted )
  {
  }
  _ExtensionByVersion( _ExtensionByVersion const & ) = default;
  _ExtensionByVersion & operator = ( _ExtensionByVersion const & ) = default;

  const char * m_szExtensionName; // The current/former extension name.
  uint32_t m_nVersionPromoted; // The version 
};

// VulkanInstance:
class VulkanInstance : public vk::raii::Instance
{
  typedef vk::raii::Instance _TyBase;
  typedef VulkanInstance _TyThis;
public:
  using _TyBase::_TyBase;

};

// Return if an extension is supported. If it is builtin the the current version then return true and set _rfIsBuiltin to true.
bool FIsVulkanExtensionSupported( const char * _pszExt, bool & _rfIsBuiltin, uint32_t _nApiVersion,
  vector< _ExtensionByVersion > const & _rrgebv, vector< vk::ExtensionProperties > const & _regep )
{
  // First check if we find it in the ExtensionByVersion array and if we are above the version found:
  _rfIsBuiltin = false;
  const _ExtensionByVersion * const pebvBegin = _rrgebv.data();
  const _ExtensionByVersion * const pebvEnd = pebvBegin + _rrgebv.size();
  const _ExtensionByVersion * pebvFound = std::find_if( pebvBegin, pebvEnd,
    [_pszExt,&_rfIsBuiltin]( _ExtensionByVersion const & _rebv )
    {
      if ( !strcmp( _rebv.m_szExtensionName, _pszExt ) )
      {
        _rfIsBuiltin = _nApiVersion >= _rebv.m_nVersionPromoted;
        return true;
      }
      return false;
    } );

  if ( ( pebvFound != pebvEnd ) && _rfIsBuiltin )
    return true;

  const vk::ExtensionProperties * const pepBegin = _regep.data();
  const vk::ExtensionProperties * const pepEnd = pepBegin + _regep.size();
  const vk::ExtensionProperties * const pepFound = std::find_if( pepBegin, pepEnd,
    [_pszExt]( vk::ExtensionProperties const & _rep ) 
    {
      return !strcmp( _rep.extensionName, _pszExt );
    });
  return pepFound != pepEnd;
}

class VulkanContext : public vk::raii::Context
{
  typedef vk::raii::Context _TyBase;
  typedef VulkanContext _TyThis;
public:
  VulkanContext()
  {
    m_rgExtensionProperties = enumerateInstanceExtensionProperties();
    m_rgExtensionsByVersion.push_back( _ExtensionByVersion( VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, VK_API_VERSION_1_1 ) );
    m_rgLayerProperties = enumerateInstanceLayerProperties();
    m_nApiVersion = enumerateInstanceVersion();
  }
  VulkanInstance CreateInstance(  unordered_map<const char *, bool> const & _umLayersOpt, 
                                  unordered_map<const char *, bool> const &_umExtentionsOpt ) const noexcept(false)
  {
    // Check for layer support:
    typedef unordered_map<const char *, bool>::value_type _TyValue;
    typedef unordered_map<const char *, bool>::const_iterator _TyConstIter;
    vector< const char * > rgpszLayers;
    { //B
      rgpszLayers.reserve( _umLayersOpt.size() );
      _TyConstIter pcitCur = _umLayersOpt.begin();
      _TyConstIter pcitEnd = _umLayersOpt.end();
      for( pcitEnd != pcitCur; ++pcitCur )
      {
        bool fIsSupported = FIsLayerSupported( pcitCur->first );
        VerifyThrowSz( fIsSupported || !pcitCur->second, "Required layer [%s] isn't supported.", pcitCur->first );
        if ( fIsSupported )
          rgpszLayers.push_back( pcitCur->first );
      }
    } //EB
    vector< const char * > rgpszExtensions;
    { //B
      rgpszExtensions.reserve( _umExtensionsOpt.size() );
      _TyConstIter pcitCur = _umExtensionsOpt.begin();
      _TyConstIter pcitEnd = _umExtensionsOpt.end();
      for( pcitEnd != pcitCur; ++pcitCur )
      {
        bool fIsBuiltin;
        bool fIsSupported = FIsExtensionSupported( rvExtension.first, fIsBuiltin );
        VerifyThrowSz( fIsSupported || !rvExtension.second, "Required layer [%s] isn't supported.", rvExtension.first );
        if ( fIsSupported && !fIsBuiltin )
          rgpszExtensions.push_back( pcitCur->first );
      }
    } //EB

    

  }

  // Return if an extension is supported. If it is builtin the the current version then return true and set _rfIsBuiltin to true.
  bool FIsExtensionSupported( const char * _pszExt, bool & _rfIsBuiltin ) const noexcept
  {
    return FIsVulkanExtensionSupported( _pszExt, _rfIsBuiltin, m_nApiVersion, m_rgExtensionsByVersion, m_rgExtensionProperties );
  }
  bool FIsLayerSupported( const char * _pszLayer ) const noexcept
  {
    const vk::LayerProperties * const plpBegin = _regep.data();
    const vk::LayerProperties * const plpEnd = plpBegin + _regep.size();
    const vk::LayerProperties * const plpFound = std::find_if( plpBegin, plpEnd,
      [_pszLayer]( vk::LayerProperties const & _rlp ) 
      {
        return !strcmp( _rlp.layerName, _pszLayer );
      });
    return plpFound != plpEnd;
  }

  std::vector< vk::ExtensionProperties > m_rgExtensionProperties;
  std::vector< _ExtensionByVersion > m_rgExtensionsByVersion;
  std::vector< vk::LayerProperties > m_rgLayerProperties;
  uint32_t m_nApiVersion;
};

struct _QueueFlagSupportIndex
{
  uint32_t m_nQueueIndex;
  vk::QueueFlags m_grfQueueFlags{0};
  bool m_fSupportPresent{false};
  uint32_t m_nQueuesSupported{0};
};
struct _QueueCreateProps : public _QueueFlagSupportIndex
{
  _QueueCreateProps( _QueueFlagSupportIndex const & _rBase )
    : _QueueFlagSupportIndex( _rBase )
  {
    m_dqciCreateInfo.queueFamilyIndex = m_nQueueIndex;
    m_dqciCreateInfo.queueCount = 1;
    m_dqciCreateInfo.pQueuePriorities = &m_flQueuePriority;
  }
  float m_flQueuePriority = {1.0f};
  vk::DeviceQueueCreateInfo m_dqciCreateInfo = {};
};

// VulkanPhysicalDevice:
class VulkanPhysicalDevice : public vk::raii::PhysicalDevice
{
  typedef vk::raii::PhysicalDevice _TyBase;
  typedef VulkanPhysicalDevice _TyThis;
public:
  VulkanPhysicalDevice & operator=( VulkanPhysicalDevice && _rr )
  {
    _TyBase::operator = ( std::move( _rr ) );
    m_PhysicalDeviceProperties = _rr.m_PhysicalDeviceProperties;
    m_PhysicalDeviceFeatures = _rr.m_PhysicalDeviceFeatures;
    m_PhysicalDeviceMemoryProperties = _rr.m_PhysicalDeviceMemoryProperties;
    m_rgqfpQueueProps = std::move( _rr.m_rgqfpQueueProps );
    m_rgqfsiQueueSupport = std::move( _rr.m_rgqfsiQueueSupport ); // Results of queue support;
    m_fQueuesHaveSupport = std::exchange( _rr.m_fQueuesHaveSupport, false );
    m_psrfSurface = std::exchange( _rr.m_psrfSurface, nullptr );
    m_rgFormats = std::move( _rr.m_rgFormats );
    m_SurfaceCapabilitiesKHR = _rr.m_SurfaceCapabilitiesKHR;
    m_rgFormats = std::move( _rr. m_rgFormats );
    m_rgPresentModeKHR = std::move( _rr.m_rgPresentModeKHR );
  }
  
  // We always construct this via move constructor and then we initialize it by getting all the properties, etc.
  // If a surface is not required then nullptr may be passed for it - i.e. for off-screen rendering.
  // If <_fFindAllQueuesForFlags> then we will continue to find different queue families for <_grfQueueFlags> even though one was already found.
  VulkanPhysicalDevice( vk::raii::PhysicalDevice && _rr, vk::QueueFlags _grfQueueFlags, vk::raii::SurfaceKHR const * _psrfSurface, bool _fFindAllQueuesForFlags = true )
    : _TyBase( std::move( _rr ) ),
      m_psrfSurface( _psrfSurface )
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

    m_rgExtensionProperties = enumerateDeviceExtensionProperties();
    m_rgExtensionsByVersion.push_back( _ExtensionByVersion( VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME, VK_API_VERSION_1_1 ) );
    m_rgExtensionsByVersion.push_back( _ExtensionByVersion( VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME, VK_API_VERSION_1_1 ) );

    // Now check for support of all required queue types:
    bool fPresentFoundOne = !m_psrfSurface;
    bool fPresentFoundAll = !m_psrfSurface;
    vk::QueueFlags grfQueueFlagsRemainging = _grfQueueFlags;
    vk::QueueFlags grfQueueFlagsFoundAll{};
    const vk::QueueFamilyProperties * pqfpCur = m_rgqfpQueueProps.data();
    const vk::QueueFamilyProperties * const pqfpEnd = pqfpCur + m_rgqfpQueueProps.size();
    for ( ; ( pqfpEnd != pqfpCur ) && !!grfQueueFlagsRemainging && !fPresentFoundOne; ++pqfpCur )
    {
      uint32_t nCurIndex = uint32_t( pqfpCur - m_rgqfpQueueProps.data() );
      vk::QueueFlags grfqfSupported = grfQueueFlagsRemainging & pqfpCur->queueFlags;
      bool fPresentSupported = !fPresentFoundAll && getSurfaceSupportKHR( nCurIndex, **m_psrfSurface );
      if ( grfqfSupported || fPresentSupported )
      {
        _QueueFlagSupportIndex qfsi = { nCurIndex, grfqfSupported, fPresentSupported, pqfpCur->queueCount };
        m_rgqfsiQueueSupport.push_back( qfsi );
        fPresentFoundOne = fPresentFoundOne || fPresentSupported;
        fPresentFoundAll = !_fFindAllQueuesForFlags && fPresentFoundOne;
        if ( !_fFindAllQueuesForFlags )
          grfQueueFlagsRemainging &= ~grfqfSupported;
        grfQueueFlagsFoundAll |= grfqfSupported;
      }
    }
    m_fQueuesHaveSupport = ( grfQueueFlagsFoundAll == _grfQueueFlags ) && fPresentFoundOne;
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
  // Return default creation properties from discovered queues:
  std::vector< _QueueCreateProps > RgGetQueueCreateProps() const
  {
    std::vector< _QueueCreateProps > rgqcp;
    rgqcp.reserve( m_rgqfsiQueueSupport.size() );
    const _QueueFlagSupportIndex * pqfsiCur = m_rgqfsiQueueSupport.data();
    const _QueueFlagSupportIndex * const pqfsiEnd = pqfsiCur + m_rgqfsiQueueSupport.size();
    for ( ; pqfsiEnd != pqfsiCur; ++pqfsiCur )
      rgqcp.emplace_back( *pqfsiCur );
    return rgqcp;
  }

  vk::PhysicalDeviceProperties m_PhysicalDeviceProperties;
  vk::PhysicalDeviceFeatures m_PhysicalDeviceFeatures;
  vk::PhysicalDeviceMemoryProperties m_PhysicalDeviceMemoryProperties;
  std::vector< vk::QueueFamilyProperties > m_rgqfpQueueProps;
  std::vector< _QueueFlagSupportIndex > m_rgqfsiQueueSupport; // Results of queue support;
  std::vector< vk::ExtensionProperties > m_rgExtensionProperties;
  std::vector< _ExtensionByVersion > m_rgExtensionsByVersion;
  bool m_fQueuesHaveSupport{false};
// surface-related properties - may not have a surface.
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

  VmaDevice(  VulkanPhysicalDevice const & _rpdPhysicalDevice, // Contains a reference to our instance and any surface.
              std::unordered_map<const char *, bool> _umExtentionsOpt,
              const _QueueCreateProps * _pqcpBegin, const _QueueCreateProps * _pqcpEnd )
    : m_rpdPhysicalDevice( _rpdPhysicalDevice ),
      m_psrfSurface( _psrfSurface )
  {
    
  }

  


  // maintain a reference to our physical device since our base doesn't.
  vk::raii::PhysicalDevice const & m_rpdPhysicalDevice;
  vk::raii::SurfaceKHR const & m_psrfSurface;
  // We maintain a reference to a VulkanInstance since our m_vmaAllocator has one that isn't ref-counted.
	VmaAllocator m_vmaAllocator{ nullptr };

};

__BIENUTIL_END_NAMESPACE
