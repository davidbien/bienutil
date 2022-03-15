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

  // We call this from the VulkanContext object - which stores a pointer to the created VulkanInstance allowing it to forward the message.
  virtual void VulkanDebugCallback( VkDebugUtilsMessageSeverityFlagBitsEXT _grfMessageSeverity, 
    VkDebugUtilsMessageTypeFlagsEXT _grfMessageType, const VkDebugUtilsMessengerCallbackDataEXT* _pCallbackData, void* _pUserData )
  {
    StaticVulkanDebugCallback( _grfMessageSeverity, _grfMessageType, _pCallbackData, _pUserData );
  }
  static void StaticVulkanDebugCallback( VkDebugUtilsMessageSeverityFlagBitsEXT _grfMessageSeverity, 
    VkDebugUtilsMessageTypeFlagsEXT _grfMessageType, const VkDebugUtilsMessengerCallbackDataEXT* _pCallbackData, void* _pUserData )
  {
    ESysLogMessageType eslmtCur;
    if ( VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT & _grfMessageSeverity )
      eslmtCur = eslmtError;
    else
    if ( VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT & _grfMessageSeverity )
      eslmtCur = eslmtWarning;
    else
      eslmtCur = eslmtInfo;
    const char * pszMessageType;
    if ( VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT & _grfMessageType )
      pszMessageType = "General";
    else
    if ( VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT & _grfMessageType )
      pszMessageType = "Validation";
    else
      pszMessageType = "Performance";
    LOGSYSLOG( eslmtCur, "VulkanDebugCallback: T[%s]: id[%s]: %s", pszMessageType, _pCallbackData->pMessageIdName, _pCallbackData->pMessage );
    return VK_FALSE;
  }
};

// Return if an extension is supported. If it is builtin the the current version then return true and set _rfIsBuiltin to true.
inline bool FIsVulkanExtensionSupported( const char * _pszExt, bool & _rfIsBuiltin, uint32_t _nApiVersion,
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

// ExtensionFeaturesChain:
// This object maintains extension feature chains to be used for extension features for instance creation and logical device creation.
class ExtensionFeaturesChain
{
  typedef ExtensionFeaturesChain _TyThis;
public:
  typedef unordered_map< VkStructureType, unique_void_ptr > _TyMapExtFeatures;

  void * PGetFirstExtension() const
  {
    return m_pvFirst;
  }

  // AddInstanceExtensionFeature: Adds a new extension feature object for a Vulkan Instance - this doesn't obtain the feature first since there is no instance yet.
  // If already added then returns the current one.
  template < class t_TyExtFeature >
  t_TyExtFeature & AddInstanceExtensionFeature( t_TyExtFeature const & _rtefDefault )
  {
    _TyMapExtFeatures::iterator it = m_umapExtFeatures.find( _rtefDefault.sType );
    if ( m_umapExtFeatures.end() != it )
      return *static_cast< t_TyExtFeature * >( it->second.get() );
    pair< _TyMapExtFeatures::iterator, bool > pib = m_umapExtFeatures.emplace( _rtefDefault.sType, make_unique_void_ptr< t_TyExtFeature >( _rtefDefault ) );
    Assert( pib.second );
    t_TyExtFeature & rtef = *static_cast< t_TyExtFeature * >( pib.first->second.get() );
    rtef.pNext = m_pvFirst;
    m_pvFirst = &rtef;
    return rtef;
  }

  template < class t_TyExtFeature >
  t_TyExtFeature & AddDeviceExtensionFeature( vk::raii::PhysicalDevice const & _rpd, VkStructureType _st )
  {
    _TyMapExtFeatures::iterator it = m_umapExtFeatures.find( _st );
    if ( m_umapExtFeatures.end() != it )
      return *static_cast< t_TyExtFeature * >( it->second.get() );
    // In this case we get the default extension feature settings from the physical device:
    vk::StructureChain< vk::PhysicalDeviceFeatures2, t_TyExtFeature > scFeatures = _rpd.getFeatures2KHR();
    t_TyExtFeature & rtef = scFeatures.get< t_TyExtFeature >();
    Assert( rtef.sType == _st );
    pair< _TyMapExtFeatures::iterator, bool > pib = m_umapExtFeatures.emplace( _rtefDefault.sType, make_unique_void_ptr< t_TyExtFeature >( rtef ) );
    Assert( pib.second );
    t_TyExtFeature & rtef = *static_cast< t_TyExtFeature * >( pib.first->second.get() );
    rtef.pNext = m_pvFirst;
    m_pvFirst = &rtef;
    return rtef;
  }

  _TyMapExtFeatures m_umapExtFeatures;
  void * m_pvFirst{nullptr}; // This points to the chain if it exists at all. New features are pushed in front of it.
};

// VulkanContext:
// This holds its associated t_TyVulkanInstance as a member and maintains its lifetime.
// This allows forwarding of 
template < class t_TyVulkanInstance >
class VulkanContext : public vk::raii::Context
{
  typedef vk::raii::Context _TyBase;
  typedef VulkanContext _TyThis;
public:
  typedef t_TyVulkanInstance _TyVulkanInstance;

  VulkanContext()
  {
    m_rgExtensionProperties = enumerateInstanceExtensionProperties();
    m_rgExtensionsByVersion.push_back( _ExtensionByVersion( VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, VK_API_VERSION_1_1 ) );
    m_rgLayerProperties = enumerateInstanceLayerProperties();
    m_nApiVersion = enumerateInstanceVersion();
  }

  template < class t_TyExtFeature >
  t_TyExtFeature & AddInstanceExtensionFeature( t_TyExtFeature const & _rtefDefault )
  {
    return m_efcFeaturesChain.AddInstanceExtensionFeature( dumCreateInfo );
  }
  
  _TyVulkanInstance * PCreateInstance(  vk::ApplicationInfo const & _raiAppInfo,
                                        unordered_map<const char *, bool> const & _umLayersOpt, bool _fEnableValidationLayer,
                                        unordered_map<const char *, bool> const & _umExtentionsOpt, bool _fEnableDebugUtils,
                                        vk::DebugUtilsMessengerCreateInfoEXT const * _pdumCreateInfo = nullptr ) const noexcept(false)
  {
    VerifyThwowSz( !m_upviInstance, "Instance already created." );
    typedef unordered_map<const char *, bool>::value_type _TyValue;
    if ( _fEnableValidationLayer )
      (void)_umLayersOpt.insert( _TyValue( "VK_LAYER_KHRONOS_validation", true ) ); // May already be there - and may be optional.
    if ( _fEnableDebugUtils )
      (void)_umExtentionsOpt.insert( _TyValue( VK_EXT_DEBUG_UTILS_EXTENSION_NAME, true ) ); // May already be there - and may be optional.
    // Check for layer support:
    typedef unordered_map<const char *, bool>::value_type _TyValue;
    typedef unordered_map<const char *, bool>::const_iterator _TyConstIter;
    vector< const char * const > rgpszLayers;
    rgpszLayers.reserve( _umLayersOpt.size() );
    for( auto & rvtLayer : _umLayersOpt )
    {
      bool fIsSupported = FIsLayerSupported( rvtLayer.first );
      VerifyThrowSz( fIsSupported || !rvtLayer.second, "Required layer [%s] isn't supported.", rvtLayer.first );
      if ( fIsSupported )
        rgpszLayers.push_back( rvtLayer.first );
    }
    bool fDebugUtils = false;
    vector< const char * const > rgpszExtensions;
    rgpszExtensions.reserve( _umExtensionsOpt.size() );
    for( auto & rvtExt : _umExtensionsOpt )
    {
      bool fIsBuiltin;
      bool fIsSupported = FIsExtensionSupported( rvtExt.first, fIsBuiltin );
      VerifyThrowSz( fIsSupported || !rvtExt.second, "Required extension [%s] isn't supported.", rvtExt.first );
      if ( fIsSupported )
      {
        if ( !strcmp( rvtExt.first, VK_EXT_DEBUG_UTILS_EXTENSION_NAME ) )
          fDebugUtils = true;
        if ( !fIsBuiltin )
         rgpszExtensions.push_back( rvtExt.first );
      }
    }

    if ( fDebugUtils )
    {
      vk::DebugUtilsMessengerCreateInfoEXT dumCreateInfo;
      if ( _pdumCreateInfo )
      {
        dumCreateInfo = *_pdumCreateInfo;
        dumCreateInfo.pfnUserCallback = _TyThis::VulkanDebugCallback;
        dumCreateInfo.pUserData = this;
      }
      else
      {
        dumCreateInfo = vk::DebugUtilsMessengerCreateInfoEXT( {},
          vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
          vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
          _TyThis::VulkanDebugCallback, this );
      }
      // Add this to the extensions features chain. If it is already in there then the caller will have overridden our options entirely.
      (void)m_efcFeaturesChain.AddInstanceExtensionFeature( dumCreateInfo );
    }

    vk::InstanceCreateInfo iciCreateInfo( {}, &_raiAppInfo, rgpszLayers, rgpszExtensions );
    iciCreateInfo.pNext = m_efcFeaturesChain.PGetFirstExtension();
    m_upviInstance = make_unique< vk::raii::Instance >( g_vkcContext, iciCreateInfo );
    m_updumDebugMessenger = make_unique< vk::raii::DebugUtilsMessengerEXT >( *m_upviInstance, dumCreateInfo );
    return &*m_upviInstance;
  }

  static VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugCallback( VkDebugUtilsMessageSeverityFlagBitsEXT _grfMessageSeverity, 
    VkDebugUtilsMessageTypeFlagsEXT _grfMessageType, const VkDebugUtilsMessengerCallbackDataEXT* _pCallbackData, void* _pUserData )
  {
    Assert( _pUserData );
    VulkanContext * pvcThis = (VulkanContext *)_pUserData;
    Assert( !!pvcThis->m_upviInstance );
    if ( !pvcThis->m_upviInstance )
      return VulkanInstance::StaticVulkanDebugCallback( _grfMessageSeverity, _grfMessageType, _pCallbackData, _pUserData );
    else
      return pvcThis->m_upviInstance->VulkanDebugCallback( _grfMessageSeverity, _grfMessageType, _pCallbackData, _pUserData );
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

  unique_ptr< _TyVulkanInstance > m_upviInstance;
  unique_ptr< vk::raii::DebugUtilsMessengerEXT > m_updumDebugMessenger; // store this here as well and forward to stored instance.
  vector< vk::ExtensionProperties > m_rgExtensionProperties;
  vector< _ExtensionByVersion > m_rgExtensionsByVersion;
  vector< vk::LayerProperties > m_rgLayerProperties;
  ExtensionFeaturesChain m_efcFeaturesChain;
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

  template < class t_TyExtFeature >
  t_TyExtFeature & AddDeviceExtensionFeature( VkStructureType _st )
  {
    return m_efcFeaturesChain.AddDeviceExtensionFeature< t_TyExtFeature >( *this, _st );
  }

  // Create a VmaDevice - a logical device that incorporates the Vulkan Memory Allocator for device allocation.
  // Upon success *this will be moved assigned into the VmaDevice::m_vpdPhysicalDevice member. This is fine because
  //  the physical device's lifetime is that of the vulkan instance itself.
  template < class t_TyQueueCreatePropsIter >
  VmaDevice CreateVmaDevice(  std::unordered_map<const char *, bool> _umExtentionsOpt,
                              t_TyQueueCreatePropsIter _iterqcpBegin, t_TyQueueCreatePropsIter _iterqcpEnd )
  {
    
  }

  vk::PhysicalDeviceProperties m_PhysicalDeviceProperties;
  vk::PhysicalDeviceFeatures m_PhysicalDeviceFeatures;
  vk::PhysicalDeviceMemoryProperties m_PhysicalDeviceMemoryProperties;
  std::vector< vk::QueueFamilyProperties > m_rgqfpQueueProps;
  std::vector< _QueueFlagSupportIndex > m_rgqfsiQueueSupport; // Results of queue support;
  std::vector< vk::ExtensionProperties > m_rgExtensionProperties;
  std::vector< _ExtensionByVersion > m_rgExtensionsByVersion;
  ExtensionFeaturesChain m_efcFeaturesChain;
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

  


  VulkanPhysicalDevice m_vpdPhysicalDevice; // The physical device is contained within our logical device for easy access at all times.
  vk::raii::SurfaceKHR const & m_psrfSurface;
  // We maintain a reference to a VulkanInstance since our m_vmaAllocator has one that isn't ref-counted.
	VmaAllocator m_vmaAllocator{ nullptr };

};

__BIENUTIL_END_NAMESPACE
