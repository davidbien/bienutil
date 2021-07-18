#pragma once

// _shwkptr.h
// Shared/weak pointer impl using single pointer shared/weak reference containers and a piece of memory for the object with references and allocator.
// dbien
// 07JUL2021

// Design goals:
// 1) Single pointer for both shared and weak pointers.
// 2) Single memory object holding the memory of the contained T.
// 3) The object may or may not be constructed.
// 4) Separate weak and object reference counts.
// 5) Const and volatile correctness.
// 6) No requirement to base the object on a given class - works like std::optional<>.

__BIENUTIL_BEGIN_NAMESPACE

// STRUCT _bienutil_Nontrivial_dummy_type
struct _bienutil_Nontrivial_dummy_type {
    constexpr _bienutil_Nontrivial_dummy_type() noexcept {
        // This default constructor is user-provided to avoid zero-initialization when objects are value-initialized.
    }
};
static_assert(!is_trivially_default_constructible_v<_bienutil_Nontrivial_dummy_type>);

// Predeclare:
template < class t_TyT, class t_TyAllocator, class t_TyRef, bool t_fReleaseAllowThrow >
class _SharedWeakPtrContainer;
template < class t_TyT, class t_TyAllocator = allocator< t_TyT >, class t_TyRef = size_t, bool t_fReleaseAllowThrow = is_nothrow_destructible_v< t_TyT > >
class SharedStrongPtr;
template < class t_TyT, class t_TyAllocator = allocator< t_TyT >, class t_TyRef = size_t, bool t_fReleaseAllowThrow = is_nothrow_destructible_v< t_TyT > >
class SharedWeakPtr;

// This exception will get thrown if the user SharedStrongPtr tries to .
class json_objects_bad_usage_exception : public _t__Named_exception<__JSONSTRM_DEFAULT_ALLOCATOR>
{
  typedef json_objects_bad_usage_exception _TyThis;
  typedef _t__Named_exception<__JSONSTRM_DEFAULT_ALLOCATOR> _TyBase;
public:
  json_objects_bad_usage_exception( const char * _pc ) 
      : _TyBase( _pc ) 
  {
  }
  json_objects_bad_usage_exception(const string_type &__s)
      : _TyBase(__s)
  {
  }
  json_objects_bad_usage_exception(const char *_pcFmt, va_list _args)
      : _TyBase(_pcFmt, _args)
  {
  }
};
// By default we will always add the __FILE__, __LINE__ even in retail for debugging purposes.
#define THROWJSONBADUSAGE(MESG, ...) ExceptionUsage<json_objects_bad_usage_exception>::ThrowFileLineFunc(__FILE__, __LINE__, FUNCTION_PRETTY_NAME, MESG, ##__VA_ARGS__)


// SharedStrongPtr: Unique name for this class since this is yaspc (yet another shared pointer class).
// This smart pointer locks the object itself, it also obtains a weak 
template < class t_TyT, class t_TyAllocator = allocator< t_TyT >, class t_TyRef = size_t, bool t_fReleaseAllowThrow = is_nothrow_destructible_v< t_TyT > >
class SharedStrongPtr
{
  typedef SharedStrongPtr _TyThis;
  typedef _SharedWeakPtrContainer< t_TyT, t_TyAllocator, t_TyRef, t_fReleaseAllowThrow > _TyContainer;
public:
  typedef t_TyT _TyT;
  typedef remove_cv_t< _TyT > _TyTNonConstVolatile;
  typedef t_TyAllocator _TyAllocatorAsPassed;
  typedef t_TyRef _TyRef;
  typedef _TyThis _TyStrongPtr;
  typedef SharedWeakPtr< t_TyT, t_TyAllocator, t_TyRef, t_fReleaseAllowThrow > _TyWeakPtr;
  static constexpr bool s_kfReleaseAllowThrow = t_fReleaseAllowThrow;
  static constexpr bool s_kfIsNoThrowDestructible = is_nothrow_destructible_v< _TyT >;
  static constexpr bool s_kfDtorNoThrow = s_kfIsNoThrowDestructible || !s_kfReleaseAllowThrow;
  ~SharedStrongPtr() noexcept( s_kfDtorNoThrow )
  {
    if ( m_pc )
      m_pc->_ReleaseStrong(); // no reentry guaranteed (even upon throw?).
  }
  SharedStrongPtr() noexcept = default;
  SharedStrongPtr( SharedStrongPtr const & _r ) noexcept
    : m_pc( _r.m_pc )
  {
    if ( m_pc )
      m_pc->_AddRefStrongNoThrow();
  }
  SharedStrongPtr( SharedStrongPtr && _rr ) noexcept
    : m_pc( _rr.m_pc )
  {
    _rr.m_pc = nullptr;
  }
  // In-place construction from any set of arguments - instanceless allocator version:
  template < class ... t_TysArgs >
  SharedStrongPtr( std::in_place_t, t_TysArgs && ... _args ) noexcept( false ) // at the least we may throw because of allocation.
  {
    m_pc = _TyContainer::PCreate( _TyAllocatorContainer(), std::in_place_t(), std::forward( _args ) ...  ); // If this fails to compile then the allocator has no default allocator.
    Assert( m_pc->_NGetStrongRef() == 1 );
  }
  // In-place construction from any set of arguments - instanceless allocator version:
  template < class ... t_TysArgs >
  SharedStrongPtr( std::in_place_t, t_TysArgs && ... _args ) noexcept( false ) // at the least we may throw because of allocation.
  {
    m_pc = _TyContainer::PCreate( _TyAllocatorContainer(), std::in_place_t(), std::forward( _args ) ...  ); // If this fails to compile then the allocator has no default allocator.
    Assert( m_pc->_NGetStrongRef() == 1 );
  }
  
  _TyThis & operator = ( _TyThis const & _r ) noexcept( s_kfDtorNoThrow )
  {
    AssertValid();
    _r.AssertValid();
    // If the release throws then the assignment fails:
    reset();
    if ( _r.m_pc )
    {
      m_pc = _r.m_pc;
      m_pc->_AddRefStrongNoThrow(); // Since we received a valid strong reference we shouldn't throw here - and so we don't even check for it.
    }
  }

  void AssertValid() const
  {
#if ASSERTSENABLED
    if ( m_pc )
      m_pc->AssertValid( true );
#endif //ASSERTSENABLED
  }

  void reset() noexcept( s_kfDtorNoThrow )
  {
    if ( m_pc )
    {
      _TyContainer * pc = m_pc;
      m_pc = nullptr;
      pc->_ReleaseStrong();
    }
  }
  void reset() noexcept( s_kfDtorNoThrow )
  {
    if ( m_pc )
    {
      _TyContainer * pc = m_pc;
      m_pc = nullptr;
      pc->_ReleaseStrong();
    }
  }

protected:
  _TyContainer * m_pc{ nullptr };
};


// _SharedWeakPtrContainer: Contains the object1, allocator, and type of the reference counters.
template < class t_TyT, class t_TyAllocator, class t_TyRef, bool t_fReleaseAllowThrow >
class _SharedWeakPtrContainer
{
  typedef _SharedWeakPtrContainer _TyThis;
  friend SharedStrongPtr< t_TyT,t_TyAllocator, t_TyRef, t_fReleaseAllowThrow >;
  friend SharedWeakPtr< t_TyT,t_TyAllocator, t_TyRef, t_fReleaseAllowThrow >;
public:
  typedef t_TyT _TyT;
  typedef remove_cv_t< _TyT > _TyTNonConstVolatile;
  typedef t_TyAllocator _TyAllocatorAsPassed;
  typedef t_TyRef _TyRef;
#if IS_MULTITHREADED_BUILD
	// Assume that if we are in the multithreaded build that we want a threadsafe increment/decrement.
	typedef atomic< _TyRef > _TyRefMemberType;
#else //!IS_MULTITHREADED_BUILD
	typedef _TyRef _TyRefMemberType;
#endif //!IS_MULTITHREADED_BUILD

  static const size_t s_nbyThis = sizeof( _TyThis );
  static constexpr bool s_kfIsNoThrowDestructible = is_nothrow_destructible_v< _TyT >;
  static constexpr bool s_kfIsAllocatorNoThrowDestructible = is_nothrow_destructible_v< _TyAllocatorThis >;
  static constexpr bool s_kfReleaseAllowThrow = t_fReleaseAllowThrow;
  
  // We need an allocator for this piece of memory since when the final weak reference count is release we will deallocate ourselves.
  typedef typename allocator_traits< _TyAllocatorAsPassed >::template rebind_traits< _TyThis > _TyAllocTraitsThis;
  typedef typename _TyAllocTraitsThis::allocator_type _TyAllocatorThis;
  static bool s_kfIsAllocatorNoThrowMoveConstructible = std::is_nothrow_move_constructible_v< _TyAllocatorThis >();
  // Determine if we can throw when cross-constructing our required allocator from _TyAllocatorAsPassed:
  // static bool s_kfIsAllocatorNoThrowCrossConstructible = std::is_nothrow_constructible_v<_TyAllocatorThis, typename std::add_lvalue_reference<
  //                 typename std::add_const<_TyAllocatorAsPassed>::type>::type>;

protected:
// These create just weakly held _SharedWeakPtrContainer which contain no object.
  static _SharedWeakPtrContainer * PCreate( _TyAllocatorThis && _rralloc ) noexcept( false )
    requires( s_kfIsAllocatorNoThrowMoveConstructible ) // The move construction of the allocator won't throw.
  {
    _TyThis * pThis = _TyAllocTraitsThis::allocate( _rralloc, 1 ); // This will throw rather than return nullptr.
    return new( pThis ) _SharedWeakPtrContainer( std::move( _rralloc ) ); // shouldn't throw.
  }
  static _SharedWeakPtrContainer * PCreate( _TyAllocatorThis && _rralloc ) noexcept( false )
    requires( !s_kfIsAllocatorNoThrowMoveConstructible ) // The move construction of the allocator might throw.
  {
    _TyThis * pThis = _TyAllocTraitsThis::allocate( _rralloc, 1 ); // This will throw rather than return nullptr.
    _BIEN_TRY
    {
      // Hmmm... we moved the allocator into the object - but it threw. We rely on the implementation of the allocator's move constructor to not modify the source upon throw to allow correct operation.
      return new( pThis ) _SharedWeakPtrContainer( std::move( _rralloc ) ); // might throw.
    }
    _BIEN_UNWIND( _TyAllocTraitsThis::deallocate( _rralloc, pThis, 1 ) ); // Still deallocate the memory then rethrow.
  }
  static _SharedWeakPtrContainer * PCreate( _TyAllocatorAsPassed const & _ralloc ) noexcept( false )
  {
    _TyAllocatorThis alloc( _ralloc );
    return _SharedWeakPtrContainer( std::move( alloc ) );
  }
// These create strongly held _SharedWeakPtrContainer containing an object.
  template < class ... t_TysArgs >
  static _SharedWeakPtrContainer * PCreate( _TyAllocatorThis && _rralloc, std::in_place_t, t_TysArgs && ... _args ) noexcept( false )
    requires( s_kfIsAllocatorNoThrowMoveConstructible && std::is_nothrow_constructible_v< _TyT, t_TysArgs ... > ) // This method cannot throw except during allocation.
  {
    _TyThis * pThis = _TyAllocTraitsThis::allocate( _rralloc, 1 ); // This will throw rather than return nullptr.
    return new( pThis ) _SharedWeakPtrContainer( std::move( _rralloc ), std::in_place_t(), std::forward( _args ) ... ); // shouldn't throw.
  }
  template < class ... t_TysArgs >
  static _SharedWeakPtrContainer * PCreate( _TyAllocatorThis && _rralloc, std::in_place_t, t_TysArgs && ... _args ) noexcept( false )
    requires( !s_kfIsAllocatorNoThrowMoveConstructible || !std::is_nothrow_constructible_v< _TyT, t_TysArgs ... > ) // This method might throw in a few scenarios.
  {
    _TyThis * pThis = _TyAllocTraitsThis::allocate( _rralloc, 1 ); // This will throw rather than return nullptr.
    _BIEN_TRY
    {
      // Hmmm... we moved the allocator into the object - but it threw. We rely on the implementation of the allocator's move constructor to not modify the source upon throw to allow correct operation.
      return new( pThis ) _SharedWeakPtrContainer( std::move( _rralloc ), std::in_place_t(), std::forward( _args ) ... ); // might throw.
    }
    _BIEN_UNWIND( _TyAllocTraitsThis::deallocate( _rralloc, pThis, 1 ) ); // Still deallocate the memory then rethrow.
  }
  template < class ... t_TysArgs >
  static _SharedWeakPtrContainer * PCreate( _TyAllocatorAsPassed const & _ralloc, std::in_place_t, t_TysArgs && ... _args ) noexcept( false )
  {
    _TyAllocatorThis alloc( _ralloc );
    return _SharedWeakPtrContainer( std::move( alloc ), std::in_place_t(), std::forward( _args ) ... );
  }

// We need only provide constructors taking rvalue-refs to an allocator for this since all creations go through PCreate().
  // Construct the allocator and set weak reference count to 1 - the object is not constructed.
  _SharedWeakPtrContainer( _TyAllocatorThis && _rralloc )
    : m_alloc( std::move( _rralloc ) )
  {
  }
  // Construct the allocator and the object - set both weak and object reference counts to 1.
  // REVIEW:<dbien>: For throw safety, in the case of move-construction of the allocator, we must construct m_tyT first
  //  as that way we allocator hasn't been moved into the contained allocator. If the move-constructor for the allocator
  //  can't throw then this is still correct.
  template < class ... t_TysArgs >
  _SharedWeakPtrContainer( _TyAllocatorThis && _rralloc, std::in_place_t, t_TysArgs && ... _args )
    : m_tyT( std::forward< t_TysArgs >( _args ) ... ),
      m_alloc( std::move( _rralloc ) ),
      m_nRefObj( 1 )
  {
  }
  // No reason and no point in overriding the default destructor.
  ~_SharedWeakPtrContainer() = default;

  void AssertValid( bool _fStrongRef ) const volatile noexcept
  {
#if ASSERTSENABLED
    Assert( !_fStrongRef || _NGetStrongRef() );
    Assert( _NGetWeakRef() && ( _NGetStrongRef() >= _NGetWeakRef() ) );
#endif //ASSERTSENABLED    
  }

  _TyRef _NGetStrongRef() const volatile noexcept
  {
#if IS_MULTITHREADED_BUILD
    return m_nRefObj.load();
#else //!IS_MULTITHREADED_BUILD
    return m_nRefObj;
#endif //!IS_MULTITHREADED_BUILD
  }
  _TyRef _NGetWeakRef() const volatile noexcept
  {
#if IS_MULTITHREADED_BUILD
    return m_nRefWeak.load();
#else //!IS_MULTITHREADED_BUILD
    return m_nRefWeak;
#endif //!IS_MULTITHREADED_BUILD
  }

// All AddRef and Relaase methods are all both const and volatile.
  void _AddRefStrongNoThrow() const volatile noexcept
	{
#if IS_MULTITHREADED_BUILD
    ++m_nRefWeak;
		++m_nRefObj;
#else //!IS_MULTITHREADED_BUILD
    ++const_cast< _TyRef & >( m_nRefWeak );
    ++const_cast< _TyRef & >( m_nRefObj );
#endif //!IS_MULTITHREADED_BUILD
	}
  static constexpr bool s_kfIsNoThrowDestructible = is_nothrow_destructible_v< _TyT >;
  static constexpr bool s_kfIsAllocatorNoThrowDestructible = is_nothrow_destructible_v< _TyAllocatorThis >;
  static constexpr bool s_kfReleaseAllowThrow = t_fReleaseAllowThrow;

// Produce some flavors of the release depending on what might or might not throw - allows better codegen when destructors don't throw (as is usual).
  void _ReleaseStrong() const volatile noexcept( true )
    requires( s_kfIsNoThrowDestructible && s_kfIsAllocatorNoThrowDestructible && s_kfIsAllocatorNoThrowMoveConstructible ) // nothing can throw.
  {
    AssertValid( true );
#if IS_MULTITHREADED_BUILD
    bool fDeleteT = !--m_nRefObj;
#else //!IS_MULTITHREADED_BUILD
    bool fDeleteT = !--const_cast< _TyRef & >( m_nRefObj );
#endif //!IS_MULTITHREADED_BUILD
    if ( fDeleteT )
      m_tyT.~_TyT();
  // Don't decrement the weak count until after destructing T in case it had a weak pointer to this.
#if IS_MULTITHREADED_BUILD
    const bool fDeallocateThis = !--m_nRefWeak;
#else //!IS_MULTITHREADED_BUILD
    const bool fDeallocateThis = !--const_cast< _TyRef & >( m_nRefWeak );
#endif //!IS_MULTITHREADED_BUILD
    if ( fDeallocateThis )
    {
      _TyAllocatorThis alloc( std::move( m_alloc ) );
      _TyAllocTraitsThis::deallocate( alloc, this, 1 );
    }
  }
  void _ReleaseStrong() const volatile noexcept( !s_kfReleaseAllowThrow )
    requires( !s_kfIsNoThrowDestructible && s_kfIsAllocatorNoThrowDestructible && s_kfIsAllocatorNoThrowMoveConstructible ) // ~_TyT() can throw.
  {
    AssertValid( true );
#if IS_MULTITHREADED_BUILD
    bool fDeleteT = !--m_nRefObj;
#else //!IS_MULTITHREADED_BUILD
    bool fDeleteT = !--const_cast< _TyRef & >( m_nRefObj );
#endif //!IS_MULTITHREADED_BUILD
    std::exception_ptr eptrDeleteT = nullptr;
    bool fInUnwinding = !!std::uncaught_exceptions();
    if ( fDeleteT )
    {
      try
      {
        m_tyT.~_TyT();
      }
      catch( ... )
      {
        if ( s_kfReleaseAllowThrow && !fInUnwinding ) // Don't rethrow if we are currently within an unwinding.
          eptrDeleteT = std::current_exception();
      }
    }
  // Don't decrement the weak count until after destructing T in case it had a weak pointer to this.
#if IS_MULTITHREADED_BUILD
    const bool fDeallocateThis = !--m_nRefWeak;
#else //!IS_MULTITHREADED_BUILD
    const bool fDeallocateThis = !--const_cast< _TyRef & >( m_nRefWeak );
#endif //!IS_MULTITHREADED_BUILD
    if ( fDeallocateThis )
    {
      _TyAllocatorThis alloc( std::move( m_alloc ) );
      _TyAllocTraitsThis::deallocate( alloc, this, 1 );
    }
    if ( s_kfReleaseAllowThrow && ( nullptr != eptrDeleteT ) )
      std::rethrow_exception( eptrDeleteT );
  }
  void _ReleaseStrong() const volatile noexcept( !s_kfReleaseAllowThrow )
    requires( !s_kfIsNoThrowDestructible && ( !s_kfIsAllocatorNoThrowDestructible || !s_kfIsAllocatorNoThrowMoveConstructible ) ) // ~_TyT() can throw and the allocator might throw.
  {
    AssertValid( true );
#if IS_MULTITHREADED_BUILD
    bool fDeleteT = !--m_nRefObj;
#else //!IS_MULTITHREADED_BUILD
    bool fDeleteT = !--const_cast< _TyRef & >( m_nRefObj );
#endif //!IS_MULTITHREADED_BUILD
    std::exception_ptr eptrDeleteT = nullptr;
    bool fInUnwinding = !!std::uncaught_exceptions();
    if ( fDeleteT )
    {
      try
      {
        m_tyT.~_TyT();
      }
      catch( ... )
      {
        if ( s_kfReleaseAllowThrow && !fInUnwinding ) // Don't rethrow if we are currently within an unwinding.
          eptrDeleteT = std::current_exception();
      }
    }
  // Don't decrement the weak count until after destructing T in case it had a weak pointer to this.
#if IS_MULTITHREADED_BUILD
    const bool fDeallocateThis = !--m_nRefWeak;
#else //!IS_MULTITHREADED_BUILD
    const bool fDeallocateThis = !--const_cast< _TyRef & >( m_nRefWeak );
#endif //!IS_MULTITHREADED_BUILD
    if ( fDeallocateThis )
    {
      try
      {
        _TyAllocatorThis alloc( std::move( m_alloc ) ); // Not much to do if the move constructor for the allocator throws. If the destructor throws then we don't really case too much.
        _TyAllocTraitsThis::deallocate( alloc, this, 1 );
      }
      catch( ... )
      {
        if ( s_kfReleaseAllowThrow && !fInUnwinding && ( nullptr == eptrDeleteT ) )
          eptrDeleteT = std::current_exception();
      }
    }
    if ( s_kfReleaseAllowThrow && ( nullptr != eptrDeleteT ) )
      std::rethrow_exception( eptrDeleteT );
  }
  void _ReleaseStrong() const volatile noexcept( !s_kfReleaseAllowThrow )
    requires( s_kfIsNoThrowDestructible && ( !s_kfIsAllocatorNoThrowDestructible || !s_kfIsAllocatorNoThrowMoveConstructible ) ) // only the allocator might throw.
  {
    AssertValid( true );
#if IS_MULTITHREADED_BUILD
    bool fDeleteT = !--m_nRefObj;
#else //!IS_MULTITHREADED_BUILD
    bool fDeleteT = !--const_cast< _TyRef & >( m_nRefObj );
#endif //!IS_MULTITHREADED_BUILD
    if ( fDeleteT )
      m_tyT.~_TyT();
  // Don't decrement the weak count until after destructing T in case it had a weak pointer to this.
#if IS_MULTITHREADED_BUILD
    const bool fDeallocateThis = !--m_nRefWeak;
#else //!IS_MULTITHREADED_BUILD
    const bool fDeallocateThis = !--const_cast< _TyRef & >( m_nRefWeak );
#endif //!IS_MULTITHREADED_BUILD
    if ( fDeallocateThis )
    {
      bool fInUnwinding = !!std::uncaught_exceptions();
      try
      {
        _TyAllocatorThis alloc( std::move( m_alloc ) ); // Not much to do if the move constructor for the allocator throws. If the destructor throws then we don't really case too much.
        _TyAllocTraitsThis::deallocate( alloc, this, 1 );
      }
      catch( ... )
      {
        if ( s_kfReleaseAllowThrow && !fInUnwinding )
          throw; // merely rethrow - nothing we can do about the failure and the caller wants to know about it.
      }
    }
  }

  void _ReleaseWeak() const volatile noexcept( true )
    requires( s_kfIsAllocatorNoThrowDestructible && s_kfIsAllocatorNoThrowMoveConstructible )
  {
    AssertValid( false );
#if IS_MULTITHREADED_BUILD
    const bool fDeallocateThis = !--m_nRefWeak;
#else //!IS_MULTITHREADED_BUILD
    const bool fDeallocateThis = !--const_cast< _TyRef & >( m_nRefWeak );
#endif //!IS_MULTITHREADED_BUILD
    if ( fDeallocateThis )
    {
      _TyAllocatorThis alloc( std::move( m_alloc ) );
      _TyAllocTraitsThis::deallocate( alloc, this, 1 );
    }
  }
  void _ReleaseWeak() const volatile noexcept( !s_kfReleaseAllowThrow )
    requires( !s_kfIsAllocatorNoThrowDestructible || !s_kfIsAllocatorNoThrowMoveConstructible ) // only the allocator might throw.
  {
    AssertValid( false );
#if IS_MULTITHREADED_BUILD
    const bool fDeallocateThis = !--m_nRefWeak;
#else //!IS_MULTITHREADED_BUILD
    const bool fDeallocateThis = !--const_cast< _TyRef & >( m_nRefWeak );
#endif //!IS_MULTITHREADED_BUILD
    if ( fDeallocateThis )
    {
      bool fInUnwinding = !!std::uncaught_exceptions();
      try
      {
        _TyAllocatorThis alloc( std::move( m_alloc ) ); // Not much to do if the move constructor for the allocator throws. If the destructor throws then we don't really case too much.
        _TyAllocTraitsThis::deallocate( alloc, this, 1 );
      }
      catch( ... )
      {
        if ( s_kfReleaseAllowThrow && !fInUnwinding )
          throw; // merely rethrow - nothing we can do about the failure and the caller wants to know about it.
      }
    }
  }

  union 
  {
      _bienutil_Nontrivial_dummy_type m_Dummy;
      _TyTNonConstVolatile m_tyT; // This is constructed if m_nRefObj > 0.
  };
  [[no_unique_address]] _TyAllocatorThis m_alloc; // Allow instanceless allocator to use no memory.
  mutable _TyRefMemberType m_nRefWeak{ 1 }; // The reference on this piece of memory.
  mutable _TyRefMemberType m_nRefObj{ 0 }; // The reference on the _TyT object contained in m_rgbyBufT;
};


__BIENUTIL_END_NAMESPACE
