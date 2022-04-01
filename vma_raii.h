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

// Return if an extension is supported. If it is builtin the the current version then return true and set *_pfIsBuiltin to true.
inline bool FIsVulkanExtensionSupported( const char * _pszExt, bool * _pfIsBuiltin, uint32_t _nApiVersion,
  vector< _ExtensionByVersion > const & _rrgebv, vector< vk::ExtensionProperties > const & _regep )
{
  // First check if we find it in the ExtensionByVersion array and if we are above the version found:
  if ( _pfIsBuiltin )
    *_pfIsBuiltin = false;
  bool fIsBuiltin = false;
  const _ExtensionByVersion * const pebvBegin = _rrgebv.data();
  const _ExtensionByVersion * const pebvEnd = pebvBegin + _rrgebv.size();
  const _ExtensionByVersion * pebvFound = std::find_if( pebvBegin, pebvEnd,
    [_pszExt,&fIsBuiltin]( _ExtensionByVersion const & _rebv )
    {
      if ( !strcmp( _rebv.m_szExtensionName, _pszExt ) )
      {
        fIsBuiltin = _nApiVersion >= _rebv.m_nVersionPromoted;
        return true;
      }
      return false;
    } );

  if ( ( pebvFound != pebvEnd ) && fIsBuiltin )
  {
    if ( _pfIsBuiltin )
      *_pfIsBuiltin = true;
    return true;
  }

  const vk::ExtensionProperties * const pepBegin = _regep.data();
  const vk::ExtensionProperties * const pepEnd = pepBegin + _regep.size();
  const vk::ExtensionProperties * const pepFound = std::find_if( pepBegin, pepEnd,
    [_pszExt]( vk::ExtensionProperties const & _rep ) 
    {
      return !strcmp( _rep.extensionName, _pszExt );
    });
  return pepFound != pepEnd;
}
inline bool FIsExtLayerEnabled( vector< const char * const > & _rgpszLayersEnabled, const char * _pszLayer )
{
  return find_if( _rgpszLayersEnabled.begin(), _rgpszLayersEnabled.end(), 
    [_pszLayer]( const char * const _pszTest )
    {
      return !strcmp( _pszLayer, _pszTest );
    } ) != _rgpszLayersEnabled.end();
}

// ExtensionFeaturesChain:
// This object maintains extension feature chains to be used for extension features for instance creation and logical device creation.
class ExtensionFeaturesChain
{
  typedef ExtensionFeaturesChain _TyThis;
public:
  typedef unordered_map< VkStructureType, unique_void_ptr > _TyMapExtFeatures;

  ExtensionFeaturesChain() = default;
  ExtensionFeaturesChain( ExtensionFeaturesChain const & ) = delete;
  ExtensionFeaturesChain( ExtensionFeaturesChain && _rr )
    : m_umapExtFeatures( std::move( _rr.m_umapExtFeatures ) ),
      m_pvFirst( exchange( _rr.m_pvFirst, nullptr ) )
  {
  }

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

  // This obtains the features structure from Vulkan but doesn't add it to the set of features.
  // If the feature has already been added then it is returned. Note that the return is by value.
  // If the feature, once examined, is desired to be added then a further call to AddDeviceFeatureSet() (the below version)
  //  is needed.
  template < class t_TyExtFeature >
  t_TyExtFeature CheckDeviceExtensionFeatures( vk::raii::PhysicalDevice const & _rpd, VkStructureType _st )
  {
    _TyMapExtFeatures::iterator it = m_umapExtFeatures.find( _st );
    if ( m_umapExtFeatures.end() != it )
      return *static_cast< t_TyExtFeature * >( it->second.get() );
    vk::StructureChain< vk::PhysicalDeviceFeatures2, t_TyExtFeature > scFeatures = _rpd.getFeatures2KHR();
    t_TyExtFeature & rtef = scFeatures.get< t_TyExtFeature >();
    Assert( rtef.sType == _st );
    return rtef;
  }
  // This adds a device extension feature that was already obtained via CheckDeviceExtensionFeatures() above.
  template < class t_TyExtFeature >
  t_TyExtFeature & AddDeviceExtensionFeature( t_TyExtFeature const & _rtef )
  {
    _TyMapExtFeatures::iterator it = m_umapExtFeatures.find( rtef.sType );
    if ( m_umapExtFeatures.end() != it )
      return *static_cast< t_TyExtFeature * >( it->second.get() );
    pair< _TyMapExtFeatures::iterator, bool > pib = m_umapExtFeatures.emplace( _rtefDefault.sType, make_unique_void_ptr< t_TyExtFeature >( _rtef ) );
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

  // Return if an extension is supported. If it is builtin the the current version then return true and set *_pfIsBuiltin to true.
  bool FIsExtensionSupported( const char * _pszExt, bool * _pfIsBuiltin = nullptr ) const noexcept
  {
    return FIsVulkanExtensionSupported( _pszExt, _pfIsBuiltin, m_nApiVersion, m_rgExtensionsByVersion, m_rgExtensionProperties );
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
  template < class t_TyExtFeature >
  t_TyExtFeature & AddInstanceExtensionFeature( t_TyExtFeature const & _rtefDefault )
  {
    return m_efcFeaturesChain.AddInstanceExtensionFeature( dumCreateInfo );
  }
  
  _TyVulkanInstance * PCreateInstance(  vk::ApplicationInfo const & _raiAppInfo,
                                        unordered_map<const char *, bool> _umLayersOpt, bool _fEnableValidationLayer,
                                        unordered_map<const char *, bool> _umExtentionsOpt, bool _fEnableDebugUtils,
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
    m_rgpszLayersEnabled.swap( rgpszLayers );
    m_rgpszExtensionsEnabled.swap( rgpszExtensions );
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
// Valid after successful call to PCreateInstance().
  _TyVulkanInstance & GetInstance() const
  {
    Assert( !!m_upviInstance );
    return *m_upviInstance;
  }
  bool FIsLayerEnabled( const char * _pszLayer ) const noexcept
  {
    return FIsExtLayerEnabled( m_rgpszLayersEnabled, _pszLayer );
  }
  bool FIsExtensionEnabled( const char * _pszExt ) const noexcept
  {
    return FIsExtLayerEnabled( m_rgpszExtensionsEnabled, _pszExt );
  }

// Properties initialized during creation:
  vector< vk::ExtensionProperties > m_rgExtensionProperties;
  vector< _ExtensionByVersion > m_rgExtensionsByVersion;
  vector< vk::LayerProperties > m_rgLayerProperties;
  ExtensionFeaturesChain m_efcFeaturesChain;
  uint32_t m_nApiVersion;
// Properties of the created instance, initialized in PCreateInstance():
  unique_ptr< _TyVulkanInstance > m_upviInstance;
  unique_ptr< vk::raii::DebugUtilsMessengerEXT > m_updumDebugMessenger; // store this here as well and forward to stored instance.
  vector< const char * const > m_rgpszLayersEnabled;
  vector< const char * const > m_rgpszExtensionsEnabled;
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
  _QueueCreateProps( _QueueFlagSupportIndex const & _rBase ) noexcept
    : _QueueFlagSupportIndex( _rBase )
  {
    m_dqciCreateInfo.queueFamilyIndex = m_nQueueIndex;
    m_dqciCreateInfo.queueCount = 1;
    m_dqciCreateInfo.pQueuePriorities = &m_flQueuePriority;
  }
  operator vk::DeviceQueueCreateInfo() const noexcept
  {
    return m_dqciCreateInfo;
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
  VulkanPhysicalDevice() = delete;
  VulkanPhysicalDevice( VulkanPhysicalDevice const & ) = delete;
  VulkanPhysicalDevice & operator = ( VulkanPhysicalDevice const & ) = delete;
  VulkanPhysicalDevice & operator = ( VulkanPhysicalDevice && ) = delete;
  ~VulkanPhysicalDevice() = default;

  VulkanPhysicalDevice( VulkanPhysicalDevice && _rr ) noexcept
    : m_rviInstance( _rr.m_rviInstance ),
      m_PhysicalDeviceProperties( _rr.m_PhysicalDeviceProperties ),
      m_PhysicalDeviceFeaturesHas( _rr.m_PhysicalDeviceFeaturesHas ),
      m_PhysicalDeviceMemoryProperties( _rr.m_PhysicalDeviceMemoryProperties ),
      m_rgqfpQueueProps( std::move( m_rgqfpQueueProps ) ),
      m_rgqfsiQueueSupport( std::move( m_rgqfsiQueueSupport ) ),
      m_rgExtensionProperties( std::move( m_rgExtensionProperties ) ),
      m_rgExtensionsByVersion( std::move( m_rgExtensionsByVersion ) ),
      m_efcFeaturesChain( std::move( _rr.m_efcFeaturesChain ) ),
      m_PhysicalDeviceFeaturesMutable( _rr.m_PhysicalDeviceFeaturesMutable ),
      m_nApiVersion( _rr.m_nApiVersion ),
      m_psrfSurface( _rr.m_psrfSurface ),
      m_rgFormats( std::move( _rr.m_rgFormats ) ),
      m_SurfaceCapabilitiesKHR( _rr.m_SurfaceCapabilitiesKHR ),
      m_rgFormats( std::move( m_rgFormats ) ),
      m_rgPresentModeKHR( std::move( m_rgPresentModeKHR ) ),
      m_rgpszExtensionsEnabled( std::move( m_rgpszExtensionsEnabled ) )
  {
  }
  
  // We always construct this via move constructor and then we initialize it by getting all the properties, etc.
  // If a surface is not required then nullptr may be passed for it - i.e. for off-screen rendering.
  // If <_fFindAllQueuesForFlags> then we will continue to find different queue families for <_grfQueueFlags> even though one was already found.
  VulkanPhysicalDevice( vk::raii::Context const & _rctxt, vk::raii::PhysicalDevice && _rr, vk::QueueFlags _grfQueueFlags, 
                        vk::raii::SurfaceKHR const * _psrfSurface, bool _fFindAllQueuesForFlags = true ) noexcept(false) // might throw due to OOM.
    : _TyBase( std::move( _rr ) ),
      m_psrfSurface( _psrfSurface ),
      m_nApiVersion( _rctxt.m_nApiVersion ),
      m_rviInstance( _rctxt.GetInstance() )
  {
    m_PhysicalDeviceProperties = getProperties();
    m_PhysicalDeviceFeaturesHas = getFeatures();
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
    m_rgExtensionsByVersion.push_back( _ExtensionByVersion( VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME, VK_API_VERSION_1_2 ) );
    m_rgExtensionsByVersion.push_back( _ExtensionByVersion( VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, VK_API_VERSION_1_2 ) );
    
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
  bool FQueuesAndSurfaceSupport() const
  {
    return m_fQueuesHaveSupport && ( !m_psrfSurface || ( !m_rgFormats.empty() && !m_rgPresentModeKHR.empty() ) );
  }
  // Returns the set of physical devices features present in the device.
  const VkPhysicalDeviceFeatures & RHasPhysicalDeviceFeatures() const
  {
    return m_HasPhysicalDeviceFeatures;
  }
  // Allows setting physical device features before creating the logical device.
  VkPhysicalDeviceFeatures & RSetPhysicalDeviceFeatures()
  {
    return m_PhysicalDeviceFeaturesMutable;
  }
  void ActivateAllDeviceFeatures()
  {
    m_PhysicalDeviceFeaturesMutable = m_HasPhysicalDeviceFeatures;
  }
  bool FIsExtensionSupported( const char * _pszExt, bool * _pfIsBuiltin = nullptr ) const noexcept
  {
    return FIsVulkanExtensionSupported( _pszExt, _pfIsBuiltin, m_nApiVersion, m_rgExtensionsByVersion, m_rgExtensionProperties );
  }
  template < typename t_TyIter >
  bool FCheckExtensionsSupported( t_TyIter _iterBegin, t_TyIter _iterEnd ) const noexcept
  {
    for ( t_TyIter iterCur = _iterBegin; _iterEnd != iterCur; ++iterCur )
      if ( !FIsExtensionSupported( *iterCur ) )
        return false;
    return true;
  }
  vk::SampleCountFlagBits GetMaxUsableSampleCount() const noexcept
  {
    vk::SampleCountFlags counts = m_PhysicalDeviceProperties.limits.framebufferColorSampleCounts & m_PhysicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if ( counts & vk::SampleCountFlagBits::e64 )
      return vk::SampleCountFlagBits::e64; 
    if ( counts & vk::SampleCountFlagBits::e32 ) 
      return vk::SampleCountFlagBits::e32;
    if ( counts & vk::SampleCountFlagBits::e16 ) 
      return vk::SampleCountFlagBits::e16;
    if ( counts & vk::SampleCountFlagBits::e8 ) 
      return vk::SampleCountFlagBits::e8;
    if ( counts & vk::SampleCountFlagBits::e4 ) 
      return vk::SampleCountFlagBits::e4;
    if ( counts & vk::SampleCountFlagBits::e2 ) 
      return vk::SampleCountFlagBits::e2;
    return vk::SampleCountFlagBits::e1;
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
  // Return default creation properties from discovered queues. The caller will modify as desired and send to CreateVmaDevice().
  vector< _QueueCreateProps > RgGetQueueCreateProps() const
  {
    vector< _QueueCreateProps > rgqcp;
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
  VmaDevice CreateVmaDevice(  unordered_map<const char *, bool> _umExtentionsOpt,
                              vector< _QueueCreateProps > const & _rrgqcp,
                              bool _fEnablePerfQueries = false ) noexcept(false)
  {
    // Transfer queue creation props:
    std::vector< vk::DeviceQueueCreateInfo > rgdqci;
    rgdqci.reserve( _rrgqcp.size() );
    for ( const _QueueCreateProps & rqcp : _rrgqcp )
      rgdqci.push_back( rqcp );

    VmaAllocatorCreateInfo vaciVmaCreateInfo{};
    bool fIsBuiltinUnused;
    bool fGetMemoryRequirements = FIsExtensionSupported( VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME, fIsBuiltinUnused );
    bool fDedicatedAllocation = FIsExtensionSupported( VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME, fIsBuiltinUnused );

    typedef unordered_map<const char *, bool>::value_type _TyValue;
    if ( fGetMemoryRequirements && fDedicatedAllocation )
    {
      vaciVmaCreateInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
      (void)_umExtentionsOpt.insert( _TyValue( VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME, true ) );
      (void)_umExtentionsOpt.insert( _TyValue( VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME, true ) );
    }
    if ( _fEnablePerfQueries )
    {
      bool fPerformanceQuery = FIsExtensionSupported( VK_KHR_PERFORMANCE_QUERY_EXTENSION_NAME, fIsBuiltinUnused );
      bool fHostQueryReset = FIsExtensionSupported( VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME, fIsBuiltinUnused );
      if ( fPerformanceQuery && fHostQueryReset )
      {
        // Before enabling these extensions we want to see if they have the support that we need for perf queries:
        vk::PhysicalDevicePerformanceQueryFeaturesKHR pqfFeatures = 
          m_efcFeaturesChain.CheckDeviceExtensionFeatures< vk::PhysicalDevicePerformanceQueryFeaturesKHR >( *this, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PERFORMANCE_QUERY_FEATURES_KHR );
        vk::PhysicalDeviceHostQueryResetFeatures hqrfFeatures = 
          m_efcFeaturesChain.CheckDeviceExtensionFeatures< vk::PhysicalDeviceHostQueryResetFeatures >( *this, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES );
  		  if ( pqfFeatures.performanceCounterQueryPools && hqrfFeatures.hostQueryReset )
        {
          (void)m_efcFeaturesChain.AddDeviceExtensionFeature( pqfFeatures );
          (void)m_efcFeaturesChain.AddDeviceExtensionFeature( hqrfFeatures );
          (void)_umExtentionsOpt.insert( _TyValue( VK_KHR_PERFORMANCE_QUERY_EXTENSION_NAME, true ) );
          (void)_umExtentionsOpt.insert( _TyValue( VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME, true ) );
        }
      }
    }

    // Now move through and create the extensions set:
    vector< const char * const > rgpszExtensions;
    rgpszExtensions.reserve( _umExtensionsOpt.size() );
    for( auto & rvtExt : _umExtensionsOpt )
    {
      bool fIsBuiltin;
      bool fIsSupported = FIsExtensionSupported( rvtExt.first, fIsBuiltin );
      VerifyThrowSz( fIsSupported || !rvtExt.second, "Required extension [%s] isn't supported.", rvtExt.first );
      if ( fIsSupported && !fIsBuiltin )
         rgpszExtensions.push_back( rvtExt.first );
    }
    vk::DeviceCreateInfo dci( {}, rgdqci, {}, rgpszExtensions, &m_PhysicalDeviceFeaturesMutable );
    dci.pNext = m_efcFeaturesChain.PGetFirstExtension();

    vk::raii::Device vd( *this, dci ); // throws on error creating.
    m_rgpszExtensionsEnabled.swap( rgpszExtensions );

    if ( FIsExtensionEnabled( VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME ) )
      vaciVmaCreateInfo.flags |= VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vaciVmaCreateInfo.physicalDevice = **this;
    vaciVmaCreateInfo.device = *vd;
    vaciVmaCreateInfo.instance = *m_rviInstance;
    VmaAllocator vmaAllocator{nullptr};
    VkResult result = vmaCreateAllocator( &vaciVmaCreateInfo, &vmaAllocator );
    VerifyThrowSz( VK_SUCCESS == result, "Error creating Vulkan Memory Allocator [%d].", result );

    // Now create a VmaDevice from vd and vmaAllocator, transfer this object into at the same time and then return the VmaAllocator to the caller.
    return VmaDevice( std::move( *this), std::move( vd ), vmaAllocator, _rrgqcp );
  }
  bool FIsExtensionEnabled( const char * _pszExt ) const noexcept
  {
    return FIsExtLayerEnabled( m_rgpszExtensionsEnabled, _pszExt );
  }

  VulkanInstance & m_rviInstance;
  vk::PhysicalDeviceProperties m_PhysicalDeviceProperties;
  vk::PhysicalDeviceFeatures m_PhysicalDeviceFeaturesHas;
  vk::PhysicalDeviceMemoryProperties m_PhysicalDeviceMemoryProperties;
  std::vector< vk::QueueFamilyProperties > m_rgqfpQueueProps;
  std::vector< _QueueFlagSupportIndex > m_rgqfsiQueueSupport; // Results of queue support;
  std::vector< vk::ExtensionProperties > m_rgExtensionProperties;
  std::vector< _ExtensionByVersion > m_rgExtensionsByVersion;
  ExtensionFeaturesChain m_efcFeaturesChain;
  vk::PhysicalDeviceFeatures m_PhysicalDeviceFeaturesMutable{};
  uint32_t m_nApiVersion{};
  bool m_fQueuesHaveSupport{false};
// surface-related properties - may not have a surface.
  vk::raii::SurfaceKHR const * m_psrfSurface{nullptr};
  std::vector< vk::SurfaceFormatKHR > m_rgFormats;
  vk::SurfaceCapabilitiesKHR m_SurfaceCapabilitiesKHR;
  std::vector< vk::SurfaceFormatKHR > m_rgFormats;
  std::vector< vk::PresentModeKHR > m_rgPresentModeKHR;
// initialized after a successful call to CreateVmaDevice().
  vector< const char * const > m_rgpszExtensionsEnabled;
};

// VmaDevice: A vk::raii::Device that contains a VmaAllocator.
// Use protected inheritance to enure we don't do anything weird.
class VmaDevice : protected vk::raii::Device
{
  typedef vk::raii::Device _TyBase;
  typedef VmaDevice _TyThis;
public:
  typedef pair< _QueueCreateProps, vector< vk::raii::Queue > > _TyPrQueueInfo;

  VmaDevice() = delete;
  VmaDevice( VmaDevice const & ) = delete;
  VmaDevice & operator = ( VmaDevice const & ) = delete;
  VmaDevice & operator = ( VmaDevice && ) = delete;
  ~VmaDevice() = default;

  VmaDevice( VmaDevice && _rr )
    : _TyBase( std::move( _rr ) ),
      m_vpdPhysicalDevice( std::move( _rr.m_vpdPhysicalDevice ) ),
      m_vmaAllocator( std::exchange( _rr.m_vmaAllocator, nullptr ) ),
      m_rgQueues( std::move( _rr.m_rgQueues ) )
      m_sfmtSurfaceFormat( std::exchange( _rr.m_sfmtSurfaceFormat, vk::Format::eUndefined ) ),
      m_optPresentMode( std::move( _rr.m_optPresentMode ) )
  {
  }
  VmaDevice(  VulkanPhysicalDevice && _rrpdPhysicalDevice,
              vk::raii::Device && _rrdDevice,
              VmaAllocator _vmaAllocator,
              vector< _QueueCreateProps > const & _rrgqcp ) noexcept
    : m_vpdPhysicalDevice( std::move( _rrpdPhysicalDevice ) ),
      _TyBase( std::move( _rrdDevice ) ),
      m_vmaAllocator( _vmaAllocator )
  {
    // Obtain our device queues:
    m_rgQueues.reserve( _rrgqcp.size() );
    for ( _QueueCreateProps const & rqcp : _rrgqcp )
    {
      _TyPrQueueInfo rpr = m_rgQueues.emplace_back( rqcp, {} );
      rpr.second.reserve( rqcp.m_dqciCreateInfo.queueCount );
      for ( uint32_t nQueue = 0; nQueue < rqcp.m_dqciCreateInfo.queueCount; ++nQueue )
      {
        vk::raii:Queue & rq = rpr.second.emplace_back( vk::raii:Queue( *this, rqcp.m_dqciCreateInfo.queueFamilyIndex, nQueue ) );
        VerifyThrowSz( *rq != VK_NULL_HANDLE, "Device ueue not found family[%u] index[%u].", rqcp.m_dqciCreateInfo.queueFamilyIndex, nQueue );
      }
    }
  }
  ~VmaDevice()
  {
    vmaDestroyAllocator( m_vmaAllocator );
  }

  bool FIsExtensionEnabled( const char * _pszExt ) const
  {
    return m_vpdPhysicalDevice.FIsExtensionEnabled( _pszExt );
  }
  const vector< _TyPrQueueInfo > & RgGetQueues() const
  {
    return m_rgQueues;
  }

  vk::SurfaceFormatKHR GetSwapSurfaceFormat()
  {
    if ( m_sfmtSurfaceFormat != vk::Format::eUndefined )
      return m_sfmtSurfaceFormat;
    
    for ( const vk::SurfaceFormatKHR & sfmt : m_vpdPhysicalDevice.m_rgFormats )
    {
      if ( ( sfmt.format == vk::Format::eB8G8R8A8Srgb ) && ( sfmt.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear ) )
      {
        m_sfmtSurfaceFormat = sfmt;
        return m_sfmtSurfaceFormat;
      }
    }
    m_sfmtSurfaceFormat = m_vpdPhysicalDevice.m_rgFormats[0];
    return m_sfmtSurfaceFormat;
  }
  vk::PresentModeKHR GetSwapPresentMode(  vk::PresentModeKHR _pmPreferred1st = vk::PresentModeKHR::eMailbox,
                                          vk::PresentModeKHR _pmPreferred2nd = vk::PresentModeKHR::eFifo  ) const
  {
    if ( m_optPresentMode.has_value() )
      return m_optPresentMode.value();
    // Move through the available presentation modes choosing the preferred if present:
    vk::PresentModeKHR * ppmPreferredCur = &_pmPreferred1st;
    for ( ; !m_optPresentMode.has_value(); ppmPreferredCur = &_pmPreferred2nd )
    {
      for ( const vk::PresentModeKHR & pmCur : m_vpdPhysicalDevice.m_rgPresentModeKHR )
      {
        if ( pmCur == *ppmPreferredCur )
        {
          m_optPresentMode = pmCur;
          return pmCur;
        }
      }
      if ( ppmPreferredCur == &_pmPreferred2nd )
        break;
    }
    m_optPresentMode = m_vpdPhysicalDevice.m_rgPresentModeKHR[0];
    return m_optPresentMode.value();
  }
  // We don't cache the calculation here as it is called every time the window is resized.
  vk::Extent2D GetSwapExtent( vk::Extent2D _re2dFrameBufferSize ) 
  {
    if ( m_vpdPhysicalDevice.m_SurfaceCapabilitiesKHR.currentExtent.width != std::numeric_limits<uint32_t>::max() ) 
      return m_vpdPhysicalDevice.m_SurfaceCapabilitiesKHR.currentExtent;
    else 
    {
      _re2dFrameBufferSize.width = std::clamp( _re2dFrameBufferSize.width, m_vpdPhysicalDevice.m_SurfaceCapabilitiesKHR.minImageExtent.width, m_vpdPhysicalDevice.m_SurfaceCapabilitiesKHR.maxImageExtent.width );
      actualExtent.height = std::clamp( _re2dFrameBufferSize.height, m_vpdPhysicalDevice.m_SurfaceCapabilitiesKHR.minImageExtent.height, m_vpdPhysicalDevice.m_SurfaceCapabilitiesKHR.maxImageExtent.height );
      return _re2dFrameBufferSize;
    }
  }
  uint32_t GetSwapImageCount( uint32_t _nMoreThanMinImageCount = 1 )
  {
    uint32_t nImageCount = swapChainSupport.capabilities.minImageCount + _nMoreThanMinImageCount;
    if ( m_vpdPhysicalDevice.m_SurfaceCapabilitiesKHR.maxImageCount > 0 && nImageCount > m_vpdPhysicalDevice.m_SurfaceCapabilitiesKHR.maxImageCount )
      nImageCount = m_vpdPhysicalDevice.m_SurfaceCapabilitiesKHR.maxImageCount;
  }

  VulkanSwapchain CreateSwapchain(  uint32_t _nMoreThanMinImageCount = 1, 
                                    vk::PresentModeKHR _pmPreferred1st = vk::PresentModeKHR::eMailbox,
                                    vk::PresentModeKHR _pmPreferred2nd = vk::PresentModeKHR::eFifo  )
  {
    
  }

  VulkanPhysicalDevice m_vpdPhysicalDevice; // The physical device is contained within our logical device for easy access at all times.
  // We maintain a reference to a VulkanInstance since our m_vmaAllocator has one that isn't ref-counted.
	VmaAllocator m_vmaAllocator{ nullptr };
  vk::SurfaceFormatKHR m_sfmtSurfaceFormat{ vk::Format::eUndefined };
  std::optional< vk::PresentModeKHR > m_optPresentMode;
  vector< _TyPrQueueInfo > m_rgQueues;
};

class VulkanSwapchain : public vk::raii::SwapchainKHR
{
  typedef vk::raii::SwapchainKHR _TyBase;
  typedef VulkanSwapchain _TyThis;
public:
  SwapchainKHR( VulkanDevice const & _vdDevice,
                vk::SwapchainCreateInfoKHR const & _rscciCreateInfo )
    : _TyBase( _vdDevice, _rscciCreateInfo ),
      m_scciCreateInfo( _rscciCreateInfo )
  {
  }



  // Save the swap chain create info to allow for easy recreation.
  vk::SwapchainCreateInfoKHR m_scciCreateInfo;
};


__BIENUTIL_END_NAMESPACE
