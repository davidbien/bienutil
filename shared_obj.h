#pragma once

// shared_obj.h
// dbien
// 15APR2021
// This implements a shared object base class to allow reference counted objects that only cost a single pointer
//	in the implementation.

__BIENUTIL_BEGIN_NAMESPACE

// We must specify the value of t_fDtorNoExcept since t_TyT is the derived class we are defining and it is an undefined type at this point.
template < bool t_fDtorNoExcept, bool t_fDtorAllowThrow = true >
class SharedObjectBase;

// CHasSharedObjectBase: Enforce no reference, a base class and the base class.
// Can't enforce is_nothrow_destructible_v< t_TyT > since t_TyT isn't a known class at this point.
template < class t_TyT >
concept CHasSharedObjectBase = !is_reference_v< t_TyT > &&  
                                ( is_base_of_v< SharedObjectBase< is_nothrow_destructible_v< remove_cv_t< t_TyT > >, false >, remove_cv_t< t_TyT > > ||
                                  is_base_of_v< SharedObjectBase< is_nothrow_destructible_v< remove_cv_t< t_TyT > >, true >, remove_cv_t< t_TyT > > );

// We glean everything from the t_TyT class since it has the appropriate base class.
// The const version of this object is templatized by "const t_TyT".
// The volative version of this object is templatized by "volatile t_TyT".
// And you can also use both const and volatile if that's yer goal.
template < CHasSharedObjectBase t_TyT >
class SharedPtr
{
  typedef SharedPtr _TyThis;
  template < CHasSharedObjectBase t_TyTOther > friend class SharedPtr;
public:
  typedef t_TyT _TyT;
  static constexpr bool s_fIsConstObject = is_const_v< t_TyT >;
  static constexpr bool s_fIsVolatileObject = is_volatile_v< t_TyT >;
  typedef remove_cv_t< _TyT > _TyTNonConstVolatile;
  static constexpr bool s_fDtorNoExcept = is_nothrow_destructible_v< _TyTNonConstVolatile >;
  static constexpr bool s_fDtorAllowThrow = _TyTNonConstVolatile::s_fDtorAllowThrow;
  static constexpr bool s_fIsNoThrowConstructible = is_nothrow_constructible_v< _TyTNonConstVolatile >;
  // Get the ultimate shared object base:
  typedef typename _TyTNonConstVolatile::_TySharedObjectBase _TySharedObjectBase;

  ~SharedPtr() noexcept( s_fDtorNoExcept || !s_fDtorAllowThrow )
  {
    if ( m_pt ) // We are assume that re-entering this method cannot occur even if m_pt->Release() ends up throwing in ~t_TyT().
      m_pt->Release();
  }
  SharedPtr() noexcept = default;
// Copy construct and assign:
  SharedPtr( SharedPtr const & _r ) noexcept
    : m_pt( _r.m_pt )
  {
    if ( m_pt )
      m_pt->AddRef();
  }
  // This may throw because we must release any existing referece.
  _TyThis & operator = ( SharedPtr const & _r ) noexcept( s_fDtorNoExcept || !s_fDtorAllowThrow )
  {
    Clear();
    if ( _r.m_pt )
    {
      m_pt = _r.m_pt;
      m_pt->AddRef();
    }
    return *this;
  }
  // We allow initialization from a less or equally cv-qualified object SharedPtr that has the base 
  //  class _TyT - as we understand that _TySharedObjectBase has a virtual destructor.
  template < class t_TyTOther >
  SharedPtr( SharedPtr< t_TyTOther > const & _rO ) noexcept
    requires( ( s_fIsConstObject >= is_const_v< t_TyTOther > ) && 
              ( s_fIsVolatileObject >= is_volatile_v< t_TyTOther > ) &&
              is_base_of_v< _TyTNonConstVolatile, remove_cv_t< t_TyTOther > > )
#if 0 // We should just be able to static_cast<> and things should work - also it's a good litmus for my requirements below - they should match.
    : m_pt( static_cast< _TyTNonConstVolatile * >( const_cast< remove_cv_t< t_TyTOther > * >( _rO.m_pt ) ) )
#else //1
    : m_pt( static_cast< _TyT * >( _rO.m_pt ) )
#endif //1
  {
    if ( m_pt )
      m_pt->AddRef();
  }
  // Similar as above for assignment - might throw due to Clear().
  template < class t_TyTOther>
  _TyThis & operator =( SharedPtr< t_TyTOther > const & _rO ) noexcept( s_fDtorNoExcept || !s_fDtorAllowThrow )
    requires( ( s_fIsConstObject >= is_const_v< t_TyTOther > ) && 
              ( s_fIsVolatileObject >= is_volatile_v< t_TyTOther > ) &&
              is_base_of_v< _TyTNonConstVolatile, remove_cv_t< t_TyTOther > > )
  {
    Clear();
    if ( _rO.m_pt )
    {
      m_pt = static_cast< _TyT * >( _rO.m_pt );
      m_pt->AddRef();
    }
    return *this;
  }
// Movement construct and assign:
// No call to AddRef or Release() is necessary for the move constructor which is nice.
  SharedPtr( SharedPtr && _rr ) noexcept
  {
    swap( _rr );
  }
  // This may throw because we must release any existing referece.
  _TyThis & operator = ( SharedPtr && _rr ) noexcept( s_fDtorNoExcept || !s_fDtorAllowThrow )
  {
    Clear();
    swap( _rr );
    return *this;
  }
  // As with copy construct and assign we allow move construct and assign from an object base on _TyT.
  template < class t_TyTOther >
  SharedPtr( SharedPtr< t_TyTOther > && _rrO ) noexcept
    requires( ( s_fIsConstObject >= is_const_v< t_TyTOther > ) && 
              ( s_fIsVolatileObject >= is_volatile_v< t_TyTOther > ) &&
              is_base_of_v< _TyTNonConstVolatile, remove_cv_t< t_TyTOther > > )
  {
    if ( _rrO.m_pt )
    {
      m_pt = static_cast< _TyT * >( _rrO.m_pt );
      _rrO.m_pt = nullptr;
    }
  }
  // Similar as above for assignment - might throw due to Clear().
  template < class t_TyTOther>
  _TyThis & operator =( SharedPtr< t_TyTOther > && _rrO ) noexcept( s_fDtorNoExcept || !s_fDtorAllowThrow )
    requires( ( s_fIsConstObject >= is_const_v< t_TyTOther > ) && 
              ( s_fIsVolatileObject >= is_volatile_v< t_TyTOther > ) &&
              is_base_of_v< _TyTNonConstVolatile, remove_cv_t< t_TyTOther > > )
  {
    Clear();
    if ( _rrO.m_pt )
    {
      m_pt = static_cast< _TyT * >( _rrO.m_pt );
      _rrO.m_pt = nullptr;
    }
    return *this;
  }
  void swap( _TyThis & _r ) noexcept
  {
    std::swap( m_pt, _r.m_pt );
  }
// In-place construction constructors:
  // In-place construction of the same type as the declared _TyT.
  template < class ... t_TysArgs >
  explicit SharedPtr( std::in_place_t, t_TysArgs && ... _args ) noexcept( s_fIsNoThrowConstructible )
  {
    m_pt = DBG_NEW _TyT( std::forward< t_TysArgs >( _args ) ... );
  }
  // In-place construction of a derived type. In this case we expect a non-qualified type in t_TyDerived, though if I had reason to change that...
  template < class t_TyDerived, class ... t_TysArgs >
  explicit SharedPtr( std::in_place_type_t< t_TyDerived >, t_TysArgs && ... _args ) noexcept( is_nothrow_constructible_v< t_TyDerived > )
  {
    static_assert( is_same_v< t_TyDerived, remove_cv_t< t_TyDerived > > ); // Have this here so that we participate in overload resolution and then fail.
    m_pt = DBG_NEW t_TyDerived( std::forward< t_TysArgs >( _args ) ... );
  }

// Emplacement: We allow creation of this object and derived objects.
  // We return the non-cv-qualified object reference - allowing full access to the object for the caller.
  // This is a design decision and should be considered in the future.
  // We will clear any existing object first. The side effect of this is that the SharedPtr will be empty if we throw
  //  in the existing reference's destructor or in the constructor of the created object.
  template < class ... t_TysArgs >
  _TyTNonConstVolatile & emplace( t_TysArgs &&... _args ) noexcept( s_fIsNoThrowConstructible && ( s_fDtorNoExcept || !s_fDtorAllowThrow ) )
  {
    Clear();
    _TyTNonConstVolatile * pt = DBG_NEW _TyTNonConstVolatile( std::forward< t_TysArgs >( _args ) ... );
    Assert( 1 == pt->NGetRefCount() );
    m_pt = pt;
    return *pt;
  }
  // Create a derived class and assign to m_pt.
  // In this case we enforce that the cv-qualified type given for t_TyDerived is compatible with the
  //  cv-qualification of _TyT. This ensures that callers "get what they expect". Now it could be useful to
  //  do it the other way as well - but this is my first idea on this.
  // We still return a reference to a non-cv-qualified object to allow the caller to further modify as necessary as the
  //  caller must be the only thread with access to this object at the time of creation.
  template < class t_TyDerived, class ... t_TysArgs >
  remove_cv_t< t_TyDerived > & emplaceDerived( t_TysArgs && ... _args ) noexcept( is_nothrow_constructible_v< t_TyDerived > && ( s_fDtorNoExcept || !s_fDtorAllowThrow ) )
  {
    typedef remove_cv_t< t_TyDerived > _TyDerivedNoCV;
    Clear();
    _TyDerivedNoCV * ptDerivedNoCV = DBG_NEW _TyDerivedNoCV( std::forward< t_TysArgs >( _args ) ... );
    Assert( 1 == ptDerivedNoCV->NGetRefCount() );
    m_pt = static_cast< _TyT * >( static_cast< t_TyDerived * >( ptDerivedNoCV ) );
    return *ptDerivedNoCV;
  }

// Accessors:
  _TyT * operator ->() const noexcept
  {
    return m_pt; // may be null...
  }
  _TyT & operator *() const noexcept
  {
    return *m_pt; // may be a null reference...
  }
// operations:
  void Clear() noexcept( s_fDtorNoExcept || !s_fDtorAllowThrow )
  {
    if ( m_pt )
    {
      _TyT * pt = m_pt;
      m_pt = 0; // throw safety.
      pt->Release();
    }
  }
protected:
  // Store the pointer const-correct:
  _TyT * m_pt{nullptr};
};

// SharedObjectBase:
// We templatize by whether the destructor of the most derived object can throw.
// The second template argument is whether we allow a throwing destructor to throw out of _DeleteSelf() or we catch locally.
template < bool t_fDtorNoExcept, bool t_fDtorAllowThrow >
class SharedObjectBase
{
	typedef SharedObjectBase _TyThis;
public:
	static constexpr bool s_fDtorNoExcept = t_fDtorNoExcept;
	static constexpr bool s_fDtorAllowThrow = t_fDtorAllowThrow;
  typedef _TyThis _TySharedObjectBase;

	// We'll use the supposed "fastest" integer here - since we know our pointer is 64bit.
	typedef int_fast32_t _TyRefValueType;
#if IS_MULTITHREADED_BUILD
	// Assume that if we are in the multithreaded build that we want a threadsafe increment/decrement.
	typedef atomic< _TyRefValueType > _TyRefMemberType;
#else //!IS_MULTITHREADED_BUILD
	typedef _TyRefValueType _TyRefMemberType;
#endif //!IS_MULTITHREADED_BUILD

	SharedObjectBase() noexcept
	{
	}
	// Trivial copy constructor allows object to be copy constructible.
  // We don't copy the ref count - leave it at one.
	SharedObjectBase( const SharedObjectBase & ) noexcept
	{
	}
  // We can't support swapping the reference count since there would be smart objects bound to those references on this object.
  // We can't support move operations for the same reason - though the derived class support those.
	_TyRefValueType	NGetRefCount() const volatile noexcept
	{
		return m_nRefCount;
	}
  // We have declared m_nRefCount mutable so that we can declare these const and volatile and then allow the code to be generally cv-correct.
  // This allows us to add-ref an object even though its element is const or volatile - which is what we want.
	_TyRefValueType AddRef() const volatile noexcept
	{
#if IS_MULTITHREADED_BUILD
		return ++m_nRefCount;
#else //!IS_MULTITHREADED_BUILD
    return ++const_cast< _TyRefValueType & >( m_nRefCount );
#endif //!IS_MULTITHREADED_BUILD
	}
  _TyRefValueType Release() const volatile noexcept( s_fDtorNoExcept || !s_fDtorAllowThrow )
	{
#if IS_MULTITHREADED_BUILD
		_TyRefValueType nRef = --m_nRefCount;
#else //!IS_MULTITHREADED_BUILD
		_TyRefValueType nRef = --const_cast< _TyRefValueType & >( m_nRefCount );
#endif //!IS_MULTITHREADED_BUILD
		if ( !nRef )
			_DeleteSelf();
		return nRef;
	}
protected:
	virtual ~SharedObjectBase() noexcept( s_fDtorNoExcept ) = default;
  // Need to declare const volatile since might get called by Release().
  // If we are the last reference to an object then we are the only thread accessing that object.
	void _DeleteSelf() const volatile noexcept( s_fDtorNoExcept || !s_fDtorAllowThrow )
		requires( s_fDtorNoExcept )
	{
		delete this; // destructor can't throw so nothing special to do here.
	}
	void _DeleteSelf() const volatile noexcept( s_fDtorNoExcept || !s_fDtorAllowThrow )
		requires( !s_fDtorNoExcept )
	{
    const bool kfInUnwinding = !s_fDtorAllowThrow || !!std::uncaught_exceptions();
    try
    {
			delete this;
    }
    catch ( std::exception const & )
    {
        if ( !kfInUnwinding )
          throw; // let someone else deal with this.
				// else just eat the exception silently - mmmm yummy!
    }
	}
	mutable _TyRefMemberType m_nRefCount{1}; // We make this mutable allowing AddRef() and Release() to be "const" methods.
};

__BIENUTIL_END_NAMESPACE
