#pragma once

// _shwkptr.h
// Shared/weak pointer impl using single pointer shared/weak reference containers and a piece of memory for the object with references and allocator.
// dbien
// 07JUL2021

// Design goals:
// 1) Single pointer for both shared and weak pointers.
// 2) Single memory object holding the memory of the contained T.
// 3) The object may or may not be constructed - but there is no mechanism for constructing an unconstructed object.
// 4) Separate weak and object reference counts.
// 5) Const and volatile correctness. To use: cv-qualify the t_TyT.
// 6) No requirement to base the object on a given class - works like std::optional<>.
// The paradigm is that the object may only be created via emplacement or construction (which emplaces the control block).
// This paradigm elides some thread safety issues - it is guaranteed that a single thread is in control for construction and
//  destruction and that a thread cannot obtain a partially- or non-constructed object since once the object(strong) reference
//  count hits zero the object is destructed and cannot be constructed again.

__BIENUTIL_BEGIN_NAMESPACE

// STRUCT _bienutil_Nontrivial_dummy_type
struct _bienutil_Nontrivial_dummy_type {
    constexpr _bienutil_Nontrivial_dummy_type() noexcept {
        // This default constructor is user-provided to avoid zero-initialization when objects are value-initialized.
    }
};
static_assert(!is_trivially_default_constructible_v<_bienutil_Nontrivial_dummy_type>);

// Predeclare:
template < class t_TyT, class t_TyAllocator, class t_TyRef, bool t_kfReleaseAllowThrow >
class _SharedWeakPtrContainer;
template < class t_TyT, class t_TyAllocator = allocator< t_TyT >, class t_TyRef = size_t, bool t_kfReleaseAllowThrow = is_nothrow_destructible_v< t_TyT > >
class SharedStrongPtr;
template < class t_TyT, class t_TyAllocator = allocator< t_TyT >, class t_TyRef = size_t, bool t_kfReleaseAllowThrow = is_nothrow_destructible_v< t_TyT > >
class SharedWeakPtr;

// This exception will get thrown if the user SharedStrongPtr tries to .
class _SharedWeakPtr_no_object_present_exception : public _t__Named_exception< __STD::allocator<char> >
{
  typedef _SharedWeakPtr_no_object_present_exception _TyThis;
  typedef _t__Named_exception< __STD::allocator<char> > _TyBase;
public:
  _SharedWeakPtr_no_object_present_exception( const char * _pc ) 
      : _TyBase( _pc ) 
  {
  }
  _SharedWeakPtr_no_object_present_exception(const string_type &__s)
      : _TyBase(__s)
  {
  }
  _SharedWeakPtr_no_object_present_exception(const char *_pcFmt, va_list _args)
      : _TyBase(_pcFmt, _args)
  {
  }
};
// By default we will always add the __FILE__, __LINE__ even in retail for debugging purposes.
#define THROWSHAREDWEAK_NOOBJECTPRESENT(MESG, ...) ExceptionUsage<_SharedWeakPtr_no_object_present_exception>::ThrowFileLineFunc(__FILE__, __LINE__, FUNCTION_PRETTY_NAME, MESG, ##__VA_ARGS__)

// SharedStrongPtr: Unique name for this class since this is yaspc (yet another shared pointer class).
// This smart pointer locks the object itself, it also obtains a weak 
template <  class t_TyT, class t_TyAllocator = allocator< remove_cv_t< _TyT > >, 
            class t_TyRef = size_t, bool t_kfReleaseAllowThrow = is_nothrow_destructible_v< t_TyT > >
class SharedStrongPtr
{
  typedef SharedStrongPtr _TyThis;
  typedef _SharedWeakPtrContainer< t_TyT, t_TyAllocator, t_TyRef, t_kfReleaseAllowThrow > _TyContainer;
  typedef SharedWeakPtr< t_TyT, t_TyAllocator, t_TyRef, t_kfReleaseAllowThrow > _TySharedWeakPtr;
public:
  typedef t_TyT _TyT;
  typedef remove_cv_t< _TyT > _TyTNonConstVolatile;
  static constexpr bool s_kfIsConstObject = is_const_v< _TyT >;
  static constexpr bool s_kfIsVolatileObject = is_volatile_v< _TyT >;
  typedef t_TyAllocator _TyAllocatorAsPassed;
  typedef typename _TyContainer::_TyAllocatorThis _TyAllocatorContainer;
  typedef _TyContainer value_type;
  typedef t_TyRef _TyRef;
  typedef _TyThis _TyStrongPtr;
  typedef SharedWeakPtr< t_TyT, t_TyAllocator, t_TyRef, t_kfReleaseAllowThrow > _TyWeakPtr;
  static constexpr bool s_kfReleaseAllowThrow = t_kfReleaseAllowThrow;
  static constexpr bool s_kfIsNoThrowDestructible = is_nothrow_destructible_v< _TyT >;
  static constexpr bool s_kfDtorNoThrow = s_kfIsNoThrowDestructible || !s_kfReleaseAllowThrow;
  static constexpr bool s_kfStrongReleaseNoThrow = _TyContainer::s_kfStrongReleaseNoThrow;
  static constexpr bool s_kfWeakReleaseNoThrow = _TyContainer::s_kfWeakReleaseNoThrow;

  ~SharedStrongPtr() noexcept( s_kfStrongReleaseNoThrow )
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
  // We allow initialization from a less or equally cv-qualified SharedStrongPtr with same _TyT.
  template < class t_TyTCVQ >
  SharedStrongPtr( SharedStrongPtr< t_TyTCVQ, t_TyAllocator, t_TyRef, t_kfReleaseAllowThrow > const & _r ) noexcept
    requires( ( s_kfIsConstObject >= is_const_v< t_TyTCVQ > ) && 
              ( s_kfIsVolatileObject >= is_volatile_v< t_TyTCVQ > ) &&
              is_same_v< _TyTNonConstVolatile, remove_cv_t< t_TyTCVQ > > )
    : m_pc( _r.m_pc )
  {
    if ( m_pc )
      m_pc->_AddRefStrongNoThrow();
  }
  template < class t_TyTCVQ >
  SharedStrongPtr( SharedStrongPtr< t_TyTCVQ, t_TyAllocator, t_TyRef, t_kfReleaseAllowThrow > && _rr ) noexcept
    requires( ( s_kfIsConstObject >= is_const_v< t_TyTCVQ > ) && 
              ( s_kfIsVolatileObject >= is_volatile_v< t_TyTCVQ > ) &&
              is_same_v< _TyTNonConstVolatile, remove_cv_t< t_TyTCVQ > > )
    : m_pc( _rr.m_pc )
  {
    _rr.m_pc = nullptr;
  }
  // We allow initialization from a less or equally cv-qualified SharedWeakPtr with same _TyT.
  // May throw if no object is present - i.e. the strong count has hit zero already.
  template < class t_TyTCVQ >
  SharedStrongPtr( SharedWeakPtr< t_TyTCVQ, t_TyAllocator, t_TyRef, t_kfReleaseAllowThrow > const & _r ) noexcept( false )
    requires( ( s_kfIsConstObject >= is_const_v< t_TyTCVQ > ) && 
              ( s_kfIsVolatileObject >= is_volatile_v< t_TyTCVQ > ) &&
              is_same_v< _TyTNonConstVolatile, remove_cv_t< t_TyTCVQ > > )
  {
    if ( _r.m_pc )
    {
      _r.m_pc->_AddRefStrongOnly(); // This might throw if the strong count is currently zero.
      _r.m_pc->_AddRefWeakOnlyNoThrow();
      m_pc = _r.m_pc;
    }
  }
  template < class t_TyTCVQ >
  SharedStrongPtr( SharedWeakPtr< t_TyTCVQ, t_TyAllocator, t_TyRef, t_kfReleaseAllowThrow > && _rr ) noexcept
    requires( ( s_kfIsConstObject >= is_const_v< t_TyTCVQ > ) && 
              ( s_kfIsVolatileObject >= is_volatile_v< t_TyTCVQ > ) &&
              is_same_v< _TyTNonConstVolatile, remove_cv_t< t_TyTCVQ > > )
  {
    if ( _rr.m_pc )
    {
      _r.m_pc->_AddRefStrongOnly(); // This might throw if the strong count is currently zero.
      m_pc = _rr.m_pc;
      _rr.m_pc = nullptr;
    }
  }

  // In-place construction from any set of arguments - instanceless allocator version:
  template < class ... t_TysArgs >
  SharedStrongPtr( std::in_place_t, t_TysArgs && ... _args ) noexcept( false ) // at the least we may throw because of allocation.
  {
    m_pc = _TyContainer::PCreate( _TyAllocatorContainer(), std::in_place_t(), std::forward( _args ) ...  ); // If this fails to compile then the allocator has no default allocator.
    Assert( m_pc->_NGetStrongRef() == 1 );
  }
  // In-place construction from any set of arguments - instanced allocator version:
  template < class ... t_TysArgs >
  SharedStrongPtr( _TyAllocatorAsPassed const & _ralloc, std::in_place_t, t_TysArgs && ... _args ) noexcept( false ) // at the least we may throw because of allocation.
  {
    m_pc = _TyContainer::PCreate( _ralloc, std::in_place_t(), std::forward( _args ) ...  ); // If this fails to compile then the allocator has no default allocator.
    Assert( m_pc->_NGetStrongRef() == 1 );
  }
  void AssertValid() const
  {
#if ASSERTSENABLED
    if ( m_pc )
      m_pc->AssertValid( true );
#endif //ASSERTSENABLED
  }
// Assignment:
// To another less qualified strong pointer:
  template < class t_TyTCVQ >
  _TyThis & operator = ( SharedStrongPtr< t_TyTCVQ, t_TyAllocator, t_TyRef, t_kfReleaseAllowThrow > const & _r ) noexcept( s_kfStrongReleaseNoThrow )
    requires( ( s_kfIsConstObject >= is_const_v< t_TyTCVQ > ) && 
              ( s_kfIsVolatileObject >= is_volatile_v< t_TyTCVQ > ) &&
              is_same_v< _TyTNonConstVolatile, remove_cv_t< t_TyTCVQ > > )
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
    return *this;
  }
  // This leaves _rr empty.
  template < class t_TyTCVQ >
  _TyThis & operator = ( SharedStrongPtr< t_TyTCVQ, t_TyAllocator, t_TyRef, t_kfReleaseAllowThrow > && _rr ) noexcept( s_kfStrongReleaseNoThrow )
    requires( ( s_kfIsConstObject >= is_const_v< t_TyTCVQ > ) && 
              ( s_kfIsVolatileObject >= is_volatile_v< t_TyTCVQ > ) &&
              is_same_v< _TyTNonConstVolatile, remove_cv_t< t_TyTCVQ > > )
  {
    AssertValid();
    _rr.AssertValid();
    // If the release throws then the assignment fails:
    reset();
    m_pc = _rr.m_pc;
    _rr.m_pc = nullptr;
    return *this;
  }
  // We allow assignment to a less qualified weak pointer but if no object is present then we throw an exception:
  _TyThis & operator = ( _TySharedWeakPtr const & _r ) noexcept( false )
  {
    AssertValid();
    _r.AssertValid();
    // If the release throws then the assignment fails:
    reset();
    if ( _r.m_pc )
    {
      _r.m_pc->_AddRefStrongOnly(); // This might throw if the strong count is currently zero.
      _r.m_pc->_AddRefWeakOnlyNoThrow();
      m_pc = _r.m_pc;
    }
    return *this;
  }
  // This leaves _rr empty.
  _TyThis & operator = ( _TySharedWeakPtr && _rr ) noexcept( false )
  {
    AssertValid();
    _rr.AssertValid();
    // If the release throws then the assignment fails:
    reset();
    if ( _rr.m_pc )
    {
      _rr.m_pc->_AddRefStrongOnly(); // This might throw if the strong count is currently zero.
      // The weak reference is inherited from the weak pointer.
      std::swap( _rr.m_pc, m_pc );
    }
    return *this;
  }
  void swap( _TyThis & _r )
  {
    std::swap( m_pc, _r.m_pc );
  }
  void reset() noexcept( s_kfStrongReleaseNoThrow )
  {
    if ( m_pc )
    {
      _TyContainer * pc = m_pc;
      m_pc = nullptr;
      pc->_ReleaseStrong();
    }
  }
// emplacement: Since we allow the pasing in of the allocator (or not for non-instanced allocators like the global std::allocator<>) we use the in_place_t to mark the boundary between
//  any passed allocator and construction arguments for the contained object. Also we return the non-qualified object to allow the caller to modify as desired.
  template < class ... t_TysArgs >
  _TyTNonConstVolatile & emplace( std::in_place_t, t_TysArgs && ... _args ) noexcept( false ) // at the least we may throw because of allocation.
  {
    reset(); // If we allow release to throw then we may throw here before construction 
    m_pc = _TyContainer::PCreate( _TyAllocatorContainer(), std::in_place_t(), std::forward( _args ) ...  ); // If this fails to compile then the allocator has no default allocator.
    Assert( m_pc->_NGetStrongRef() == 1 );
    return m_pc->TGet();
  }
  // In-place construction from any set of arguments - instanced allocator version:
  template < class ... t_TysArgs >
  _TyTNonConstVolatile & emplace( _TyAllocatorAsPassed const & _ralloc, std::in_place_t, t_TysArgs && ... _args ) noexcept( false ) // at the least we may throw because of allocation.
  {
    reset(); // If we allow release to throw then we may throw here before construction 
    m_pc = _TyContainer::PCreate( _ralloc, std::in_place_t(), std::forward( _args ) ...  ); // If this fails to compile then the allocator has no default allocator.
    Assert( m_pc->_NGetStrongRef() == 1 );
    return m_pc->TGet();
  }
  
// Accessors:
  bool operator !() const noexcept
  {
    return !m_pc;
  }
  _TyT * operator ->() const noexcept
  {
    return &m_pt->TGet(); // may be null...
  }
  _TyT & operator *() const noexcept
  {
    return m_pt->TGet(); // may be null...
  }
protected:
  _TyContainer * m_pc{ nullptr };
};


// _SharedWeakPtrContainer: Contains the object1, allocator, and type of the reference counters.
template < class t_TyT, class t_TyAllocator, class t_TyRef, bool t_kfReleaseAllowThrow >
class _SharedWeakPtrContainer
{
  typedef _SharedWeakPtrContainer _TyThis;
  friend SharedStrongPtr< t_TyT,t_TyAllocator, t_TyRef, t_kfReleaseAllowThrow >;
  friend SharedWeakPtr< t_TyT,t_TyAllocator, t_TyRef, t_kfReleaseAllowThrow >;
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
  static constexpr bool s_kfReleaseAllowThrow = t_kfReleaseAllowThrow;
  
  // We need an allocator for this piece of memory since when the final weak reference count is release we will deallocate ourselves.
  typedef typename allocator_traits< _TyAllocatorAsPassed >::template rebind_traits< _TyThis > _TyAllocTraitsThis;
  typedef typename _TyAllocTraitsThis::allocator_type _TyAllocatorThis; // Make this public so that 
  static constexpr bool s_kfIsAllocatorNoThrowMoveConstructible = std::is_nothrow_move_constructible_v< _TyAllocatorThis >();

  // We publish whether or not the Release() won't throw - for both strong and weak releases:
  static constexpr bool s_kfStrongReleaseNoThrow = !t_kfReleaseAllowThrow || ( s_kfIsNoThrowDestructible && s_kfIsAllocatorNoThrowDestructible && s_kfIsAllocatorNoThrowMoveConstructible );
  static constexpr bool s_kfWeakReleaseNoThrow = !t_kfReleaseAllowThrow || ( s_kfIsAllocatorNoThrowDestructible && s_kfIsAllocatorNoThrowMoveConstructible );

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
  // We may not have a current object in which case we throw.
  void _AddRefStrongOnly() const volatile noexcept( false )
	{
    AssertValid( true ); // This will bark if we don't have a current strong ref.
#if IS_MULTITHREADED_BUILD
		bool fAddToZeroSuccess = FAtomicAddNotEqual( m_nRefObj, 0, 1 ); // Only add one if we aren't equal to zero.
#else //!IS_MULTITHREADED_BUILD
    bool fAddToZeroSuccess = !m_nRefObj ? false : ( ++m_nRefObj, true );
#endif //!IS_MULTITHREADED_BUILD
    // We must increment the strong reference count iff it is non-zero.
    if ( !fAddToZeroSuccess )
      THROWSHAREDWEAK_NOOBJECTPRESENT( "_AddRefStrongOnly(): No object present to obtain strong reference." );
	}
  void _AddRefWeakOnlyNoThrow() const volatile noexcept
  {
#if IS_MULTITHREADED_BUILD
    ++m_nRefWeak;
#else //!IS_MULTITHREADED_BUILD
    ++const_cast< _TyRef & >( m_nRefWeak );
#endif //!IS_MULTITHREADED_BUILD
  }
  static constexpr bool s_kfIsNoThrowDestructible = is_nothrow_destructible_v< _TyT >;
  static constexpr bool s_kfIsAllocatorNoThrowDestructible = is_nothrow_destructible_v< _TyAllocatorThis >;
  static constexpr bool s_kfReleaseAllowThrow = t_kfReleaseAllowThrow;

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
