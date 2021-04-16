#pragma once

// shared_obj.h
// dbien
// 15APR2021
// This implements a shared object base class to allow reference counted objects that only cost a single pointer
//	in the implementation.

__BIENUTIL_BEGIN_NAMESPACE

template < bool t_fDtorNoExcept, bool t_fDtorAllowThrow >
class _SharedObjectRef;
template < bool t_fDtorNoExcept, bool t_fDtorAllowThrow = true >
class SharedObjectBase;

// CHasSharedObjectBase: Enforce no reference, a base class, and the destruction semantics.
template < class t_TyT >
concept CHasSharedObjectBase = !is_reference_v< t_TyT > &&  
                                ( is_base_of_v< SharedObjectBase, is_nothrow_destructible_v< t_TyT >, true > || 
                                  is_base_of_v< SharedObjectBase, is_nothrow_destructible_v< t_TyT >, false > );

// A implementing class t_TyT can use this to obtain the correct base class:
template < CHasSharedObjectBase t_TyT, bool t_fDtorAllowThrow = true >
struct _TGetSharedObjectBase
{
  typedef SharedObjectBase< is_nothrow_destructible_v< t_TyT >, t_fDtorAllowThrow > type;
};
template < CHasSharedObjectBase t_TyT, bool t_fDtorAllowThrow = true >
using TGetSharedObjectBase_t = typename _TGetSharedObjectBase< t_TyT, t_fDtorAllowThrow >::type;

// We glean everything from the t_TyT class since it has the appropriate base class.
// The const version of this object is templatized by "const t_TyT".
// The volative version of this object is templatized by "volatile t_TyT".
// And you can also use both const and volatile if that's yer goal.
template < CHasSharedObjectBase t_TyT >
class SharedPtr
{
  typedef SharedPtr _TyThis;
public:
  typedef t_TyT _TyT;
  static constexpr bool s_fIsConstObject = is_const_v< t_TyT >;
  static constexpr bool s_fIsVolatileObject = is_volatile_v< t_TyT >;
  typedef remove_cv_t< _TyT > _TyTNonConstVolatile;
  static constexpr bool s_fDtorNoExcept = _TyTNonConstVolatile::s_fDtorNoExcept;
  static constexpr bool s_fDtorAllowThrow = _TyTNonConstVolatile::s_fDtorAllowThrow;
  typedef _SharedObjectRef< s_fDtorNoExcept, s_fDtorAllowThrow > _TySharedObjectRef;
  typedef SharedObjectBase< s_fDtorNoExcept, s_fDtorAllowThrow > _TySharedObjectBase;

  ~SharedPtr() noexcept( s_fDtorNoExcept || !s_fDtorAllowThrow )
  {
    if ( m_pt ) // We are assume that re-entering this method cannot occur even if m_pt->Release() ends up throwing in ~t_TyT().
      m_pt->Release();
  }
  SharedPtr() = default;
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
#if 0 // We should just be able to static_cast<> and things should work - also it's a good litmus for my requirements below - they should match.
    : m_pt( static_cast< _TyTNonConstVolatile * >( const_cast< remove_cv_t< t_TyTOther > * >( _rO.m_pt ) ) )
#else //1
    : m_pt( static_cast< _TyT * >( _rO.m_pt ) )
#endif //1
    requires( ( s_fIsConstObject >= is_const_v< t_TyOther > ) && 
              ( s_fIsVolatileObject >= is_volatile_v< t_TyOther > ) &&
              is_base_of_v< _TyTNonConstVolatile, remove_cv_t< t_TyTOther > > )
  {
    if ( m_pt )
      m_pt->AddRef();
  }
  // Similar as above for assignment - might throw due to Clear().
  template < class t_TyTOther>
  _TyThis & operator =( SharedPtr< t_TyTOther > const & _rO ) noexcept( s_fDtorNoExcept || !s_fDtorAllowThrow )
    requires( ( s_fIsConstObject >= is_const_v< t_TyOther > ) && 
              ( s_fIsVolatileObject >= is_volatile_v< t_TyOther > ) &&
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
    requires( ( s_fIsConstObject >= is_const_v< t_TyOther > ) && 
              ( s_fIsVolatileObject >= is_volatile_v< t_TyOther > ) &&
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
    requires( ( s_fIsConstObject >= is_const_v< t_TyOther > ) && 
              ( s_fIsVolatileObject >= is_volatile_v< t_TyOther > ) &&
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
	typedef _SharedObjectRef< s_fDtorNoExcept, s_fDtorAllowThrow > _TySharedObjectRef;

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
	SharedObjectBase( const SharedObjectBase & ) noexcept
	{
	}
	_TyRefValueType	NGetRefCount() const volatile noexcept
	{
		return m_nRefCount;
	}
  // We have declared m_nRefCount mutable so that we can declare these const and volatile and then allow the code to be generally cv-correct.
  // This allows us to add-ref an object even though its element is const or volatile - which is what we want.
	_TyRefValueType AddRef() const volatile noexcept
	{
		return ++m_nRefCount;
	}
  _TyRefValueType Release() const volatile noexcept( s_fDtorNoExcept || !s_fDtorAllowThrow )
	{
		_TyRefValueType nRef = --m_nRefCount;
		if ( !nRef )
			_DeleteSelf();
		return nRef;
	}

// To do this correctly we must override new and delete. Note that we do not support array new[].
	void * SharedObjectBase::operator new( size_t _nbySize, _TySharedObjectRef & _sor ) noexcept(false)
	{
		void * pvMem = ::operator new( _nbySize
	#if TRACK_SHARED_OBJECT_ALLOCATIONS
		// We are tracking shared object memory allocation but the caller didn't use SO_NEW().
		,__FILE__, __LINE__ 
	#endif //TRACK_SHARED_OBJECT_ALLOCATIONS
		);
		// Note that we are setting in a uninitialized object - just memory. However SharedObjectRef has no effective lifetime and is a trivial object so that doesn't matter.
		_sor.ResetSharedObjectBase( (SharedObjectBase*)pvMem );
		return pvMem;
	}
#if TRACK_SHARED_OBJECT_ALLOCATIONS
	// This allows us to associate the creating file/line with the allocation - by passing it to the eventual call to new().
	void* operator new( size_t _nbySize, _TySharedObjectRef & _sor, const char * _szFilename, int _nSourceLine ) noexcept(false)
	{
		void * pvMem = ::operator new( _nbySize, _szFilename, _nSourceLine );
		_sor.ResetSharedObjectBase( (SharedObjectBase*)pvMem );
		return pvMem;
	}
#endif //TRACK_SHARED_OBJECT_ALLOCATIONS
	// We can only enter this delete operator if we fail inside of the corresonding new operator.
	void operator delete( void * _pv, _TySharedObjectRef & _sor ) noexcept
	{
		_sor._ClearSharedObjectBase( (SharedObjectBase*)_pv ); // Clear any partially constructed object but only if it equals _pv;
		::delete( _pv );
	}
#if TRACK_SHARED_OBJECT_ALLOCATIONS
	// If we throw inside of the corresponding new operator then this will be called:
	void operator delete( void * _pv, _SharedObjectRef & _sor, const char * _szFilename, int _nSourceLine ) noexcept
	{
		_sor._ClearSharedObjectBase( (SharedObjectBase*)_pv ); // Clear any partially constructed object but only if it equals _pv;
		::delete( _pv );
	}
#endif //TRACK_SHARED_OBJECT_ALLOCATIONS
	// The normal way an obect's memory is deleted...
	void operator delete( void* _pv ) noexcept
	{	
		::delete( _pv );
	}
protected:
	mutable _TyRefMemberType m_nRefCount{1}; // We make this mutable allowing AddRef() and Release() to be "const" methods.
	virtual	~SharedObjectBase() noexcept( s_fDtorNoExcept )
	{
		// Base needs to be virtual so we can delete this with only the knowledge of a pointer to this.
		// Also must match the exception specification of any inheriting class.
	}
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
};

template < bool t_fDtorNoExcept, bool t_fDtorAllowThrow >
class _SharedObjectRef
{
	typedef _SharedObjectRef _TyThis;
public:
	static constexpr bool s_fDtorNoExcept = t_fDtorNoExcept;
	static constexpr bool s_fDtorAllowThrow = t_fDtorAllowThrow;
	typedef SharedObjectBase< s_fDtorNoExcept, s_fDtorAllowThrow > _TySharedObjectBase;
	_SharedObjectRef() = delete; // We only ever cast to this object - we never create it.
	_SharedObjectRef( _SharedObjectRef const & ) noexcept = default; // we are copyable.
protected:
	_TySharedObjectBase * PGetSharedObjectBase() const noexcept
	{
		return m_psob;
	}
	// This merely clears the *value* of m_psob. This is called when an exception is encountered during construction.
	// The indication is that we have an object here that is unowned - it is owned by C++ construction not have completed.
	void _ClearSharedObjectBase( _TySharedObjectBase * _psob ) noexcept
	{
		if ( _psob == m_psob )
			m_psob = nullptr;
	}
	void ResetSharedObjectBase( _TySharedObjectBase * _psob ) noexcept( s_fDtorNoExcept || !s_fDtorAllowThrow )
		requires( s_fDtorNoExcept || !s_fDtorAllowThrow )
	{
		// no throwing out on destruct so a bit simpler code:
		if ( m_psob )
			(void)m_psob->Release();
		m_psob = _psob;
	}
	void ResetSharedObjectBase( _TySharedObjectBase * _psob ) noexcept( s_fDtorNoExcept || !s_fDtorAllowThrow )
		requires( !s_fDtorNoExcept && s_fDtorAllowThrow )
	{
		// destructor may throw:
		if ( m_psob )
		{
			_TySharedObjectBase * psobRelease = m_psob;
			m_psob = nullptr; // throw-safety
			(void)psobRelease->Release();
		}
		m_psob = _psob;
	}
	_TySharedObjectBase * m_psob/* {nullptr} */; // No reason to initialize because we are never constructed.
};

__BIENUTIL_END_NAMESPACE
