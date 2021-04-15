#pragma once

// shared_obj.h
// dbien
// 15APR2021
// This implements a shared object base class to allow reference counted objects that only cost a single pointer
//	in the implementation.

__BIENUTIL_BEGIN_NAMESPACE

template < bool t_fDtorNoExcept, bool t_fDtorAllowThrow = true >
class SharedObjectBase;

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
	SharedObjectBase( const SharedObjectBase& brco ) noexcept
	{
	}
	_TyRefValueType	NGetRefCount() const noexcept
	{
		return m_nRefCount;
	}
	_TyRefValueType AddRef() noexcept
	{
		return ++m_nRefCount;
	}
  _TyRefValueType Release() noexcept( s_fDtorNoExcept || !s_fDtorAllowThrow )
	{
		_TyRefValueType nRef = --m_nRefCount;
		if ( !nRef )
			_DeleteSelf();
		return nRef;
	}

// To do this correctly we must override new and delete. Note that we do not support array new[].
	void * SharedObjectBase::operator new( size_t _nbySize, _TySharedObjectRef & _sor )
	{
		// We are tracking shared object memory allocation but the caller didn't use SO_NEW().
		void * pvMem = ::operator new( _nbySize
	#if TRACK_SHARED_OBJECT_ALLOCATIONS
		,__FILE__, __LINE__ 
	#endif //TRACK_SHARED_OBJECT_ALLOCATIONS
		);
		// Note that we are setting in a uninitialized object - just memory. However SharedObjectRef has no effective lifetime and is a trivial object so that doesn't matter.
		_sor.ResetSharedObjectBase( (SharedObjectBase*)pvMem );
		return pvMem;
	}
#if TRACK_SHARED_OBJECT_ALLOCATIONS
	// This allows us to associate the creating file/line with the allocation - by passing it to the eventual call to new().
	void* operator new( size_t _nbySize, _TySharedObjectRef & _sor, const char * _szFilename, int _nSourceLine )
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
	void operator delete( size_t _nbySize, _SharedObjectRef & _sor, const char * _szFilename, int _nSourceLine ) noexcept
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
	_TyRefMemberType m_nRefCount{1};
	virtual	~SharedObjectBase() noexcept( s_fDtorNoExcept )
	{
		// Base needs to be virtual so we can delete this with only the knowledge of a pointer to this.
		// Also must match the exception specification of any inheriting class.
	}
	void _DeleteSelf() noexcept( s_fDtorNoExcept || !s_fDtorAllowThrow )
		requires( s_fDtorNoExcept )
	{
		delete this; // destructor can't throw so nothing special to do here.
	}
	void _DeleteSelf() noexcept( s_fDtorNoExcept || !s_fDtorAllowThrow )
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

__BIENUTIL_END_NAMESPACE
