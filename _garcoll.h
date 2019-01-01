#ifndef __GARCOLL_H
#define __GARCOLL_H

// _garcoll.h

// This module implements templates necessary for garbage collection.

__BIENUTIL_BEGIN_NAMESPACE

typedef int	_TyRef;

// utility template for dispatch of comparison:
template < class t_TyCompare, class t_TyEl, bool t_fUseElCompare >
struct _gco_dispatch_compare
{
  static inline bool  equals( t_TyCompare const & _rl, t_TyCompare const & _rr )
  {
    return static_cast< t_TyEl const & >( _rl ) == static_cast< t_TyEl const & >( _rr );
  }
  static inline bool  notequals( t_TyCompare const & _rl, t_TyCompare const & _rr )
  {
    return static_cast< t_TyEl const & >( _rl ) != static_cast< t_TyEl const & >( _rr );
  }
  static inline bool  less( t_TyCompare const & _rl, t_TyCompare const & _rr )
  {
    return static_cast< t_TyEl const & >( _rl ) < static_cast< t_TyEl const & >( _rr );
  }
  static inline bool  greater( t_TyCompare const & _rl, t_TyCompare const & _rr )
  {
    return static_cast< t_TyEl const & >( _rr ) < static_cast< t_TyEl const & >( _rl );
  }
};

template < class t_TyCompare, class t_TyEl >
struct _gco_dispatch_compare< t_TyCompare, t_TyEl, false >
{
  static inline bool  equals( t_TyCompare const & _rl, t_TyCompare const & _rr )
  {
    return &_rl == &_rr;
  }
  static inline bool  notequals( t_TyCompare const & _rl, t_TyCompare const & _rr )
  {
    return &_rl != &_rr;
  }
  static inline bool  less( t_TyCompare const & _rl, t_TyCompare const & _rr )
  {
    return &_rl < &_rr;
  }
  static inline bool  greater( t_TyCompare const & _rl, t_TyCompare const & _rr )
  {
    return &_rr < &_rl;
  }
};

// Non-polymorphic object support:

// _gco
// _gco with instanced allocator, element is member ( note this limits construction ):
template <	class t_TyEl,
						class t_TyAllocator = allocator< char >,
      bool t_fUseElCompare = false,
      bool t_fElIsInEmbeddedStore = false,
      bool t_fAllocatorIsStatic = _Alloc_traits< t_TyEl, t_TyAllocator >::_S_instanceless >
class _gco
{
	typedef _gco< t_TyEl, t_TyAllocator, t_fUseElCompare, 
                t_fElIsInEmbeddedStore, t_fAllocatorIsStatic >  _TyThis;

	typedef typename _Alloc_traits< _TyThis, t_TyAllocator >::allocator_type  _TyAllocatorThis;

  // Needed for external access:
	typedef t_TyAllocator		_TyAllocator;	
  typedef t_TyEl _TyEl;

  // Only allow _gcr, _gcp, and _gco_dispatch_compare access to this class:
	template <	class t__TyEl, 
							class t__TyGCO,
							bool t__fCopyOnWrite >
	friend class _gcr;

	template <	class t__TyEl, 
							class t__TyGCO,
							bool t__fCopyOnWrite >
	friend class _gcp;

  template <  class t__TyThis,
              class t__TyEl, 
              bool t__fUseElCompare >
  friend struct _gco_dispatch_compare;

  template < class t_TyGcr, bool t__fCopyOnWrite >
  friend struct _gcr_dispatch_el_access;

  template < class t_TyGcr, bool t__fCopyOnWrite >
  friend struct _gcp_dispatch_el_access;

  _TyAllocatorThis  m_alloc;
	_TyRef	          m_ref;
	t_TyEl	          m_tEl;

	explicit _gco( t_TyAllocator const & _rAlloc )
		: m_alloc( _rAlloc ),
      m_ref( 1 )
	{
	}
	explicit _gco( _TyThis const & _r )
		: m_alloc( _r.m_alloc ),
      m_ref( 1 ),
      m_tEl( _r.m_tEl )
	{
	}

  // Utility templatized constructors, unfortunately, these
  //  do not allow the passing of reference parameters:
	template < class t_Ty1 >
	_gco( t_Ty1 _r1, t_TyAllocator const & _rAlloc )
		: m_alloc( _rAlloc ),
      m_ref( 1 ),
      m_tEl( _r1 )
	{
	}
	template < class t_Ty1, class t_Ty2 >
	_gco( t_Ty1 _r1, t_Ty2 _r2, t_TyAllocator const & _rAlloc )
		: m_alloc( _rAlloc ),
      m_ref( 1 ),
      m_tEl( _r1, _r2 )
	{
	}

#ifndef NDEBUG
	~_gco()
	{
		assert( !m_ref );	// This should be the case.
	}
#endif //!NDEBUG

// access:
	operator t_TyEl const & () const
	{
		return m_tEl;
	}
	operator t_TyEl & ()
	{
		return m_tEl;
	}

// reference counting:
	_TyRef	AddRef() _BIEN_NOTHROW
	{
		return ++m_ref;
	}
	_TyRef	Release() _BIEN_NOTHROW
	{
		return --m_ref;
	}

	bool operator == ( _TyThis const & _r ) const	
  {
    return _gco_dispatch_compare< _TyThis, t_TyEl, t_fUseElCompare >::equals( *this, _r );
  }
	bool operator != ( _TyThis const & _r ) const
  { 
    return _gco_dispatch_compare< _TyThis, t_TyEl, t_fUseElCompare >::notequals( *this, _r );
  }
	bool operator < ( _TyThis const & _r) const		
  { 
    return _gco_dispatch_compare< _TyThis, t_TyEl, t_fUseElCompare >::less( *this, _r );
  }
	bool operator > ( _TyThis const & _r) const
  { 
    return _gco_dispatch_compare< _TyThis, t_TyEl, t_fUseElCompare >::greater( *this, _r );
  }

	_TyThis *	copy() const
	{
		_TyThis *	p = m_alloc.allocate( 1 );
		_BIEN_TRY
		{
			new ( p ) _TyThis( *this );
		}
		_BIEN_UNWIND( m_alloc.deallocate( p, 1 ) );
		return p;
	}

// static creation utilities:
	static _TyThis *	PGCOCreate( t_TyAllocator const & _rAlloc = t_TyAllocator() )
	{
		_TyAllocatorThis		localAllocator( _rAlloc );
    _TyThis *	p = localAllocator.allocate( 1 );
		_BIEN_TRY
		{
			new ( p ) _TyThis( _rAlloc );
		}
		_BIEN_UNWIND( localAllocator.deallocate( p, 1 ) );
    return p;
	}

	template < class t_Ty1 >
	static _TyThis *	PGCOCreate1(  t_Ty1 _r1, 
													        t_TyAllocator const & _rAlloc = t_TyAllocator() )
	{
		_TyAllocatorThis		localAllocator( _rAlloc );
    _TyThis *	p = localAllocator.allocate( 1 );
		_BIEN_TRY
		{
			new ( p ) _TyThis( _r1, _rAlloc );
		}
		_BIEN_UNWIND( localAllocator.deallocate( p, 1 ) );
    return p;
	}

	template < class t_Ty1, class t_Ty2 >
	static _TyThis *	PGCOCreate2(  t_Ty1 _r1, t_Ty2 _r2,
													        t_TyAllocator const & _rAlloc = t_TyAllocator() )
	{
		_TyAllocatorThis		localAllocator( _rAlloc );
    _TyThis *	p = localAllocator.allocate( 1 );
		_BIEN_TRY
		{
			new ( p ) _TyThis( _r1, _r2, _rAlloc );
		}
		_BIEN_UNWIND( localAllocator.deallocate( p, 1 ) );
    return p;
	}

	void			destroy( )
	{
		_TyAllocatorThis		localAllocator( m_alloc ); // since we are deallocating this.
		this->~_TyThis();
		localAllocator.deallocate( this, 1 );
	}
};

// _gco with instanceless allocator, element is member:
template <	class t_TyEl,
						class t_TyAllocator,
            bool t_fUseElCompare,
            bool t_fElIsInEmbeddedStore >
class _gco< t_TyEl, t_TyAllocator, t_fUseElCompare, t_fElIsInEmbeddedStore, true >
{
	typedef _gco< t_TyEl, t_TyAllocator, t_fUseElCompare, 
                t_fElIsInEmbeddedStore, true >                _TyThis;

  // The type for the static allocator:
	typedef typename _Alloc_traits<_TyThis, t_TyAllocator>::_Alloc_type	  _TyAllocThis;

  // Needed for external access:
	typedef t_TyAllocator		_TyAllocator;	
  typedef t_TyEl _TyEl;

  // Only allow _gcr, _gcp, and _gco_dispatch_compare access to this class:
	template <	class t__TyEl, 
							class t__TyGCO,
							bool t__fCopyOnWrite >
	friend class _gcr;

	template <	class t__TyEl, 
							class t__TyGCO,
							bool t__fCopyOnWrite >
	friend class _gcp;

  template <  class t__TyThis,
              class t__TyEl, 
              bool t__fUseElCompare >
  friend struct _gco_dispatch_compare;

  template < class t_TyGcr, bool t__fCopyOnWrite >
  friend struct _gcr_dispatch_el_access;

  template < class t_TyGcr, bool t__fCopyOnWrite >
  friend struct _gcp_dispatch_el_access;

	_TyRef	          m_ref;
	t_TyEl	          m_tEl;

	explicit _gco()
		: m_ref( 1 )
	{
	}
	explicit _gco( _TyThis const & _r )
		: m_ref( 1 ),
      m_tEl( _r.m_tEl )
	{
	}

  // Utility templatized constructors, unfortunately, these
  //  do not allow the passing of reference parameters:
	template < class t_Ty1 >
	_gco( t_Ty1 _r1 )
		: m_ref( 1 ),
      m_tEl( _r1 )
	{
	}
	template < class t_Ty1, class t_Ty2 >
	_gco( t_Ty1 _r1, t_Ty2 _r2 )
		: m_ref( 1 ),
      m_tEl( _r1, _r2 )
	{
	}

#ifndef NDEBUG
	~_gco()
	{
		assert( !m_ref );	// This should be the case.
	}
#endif //!NDEBUG

// access:
	operator t_TyEl const & () const
	{
		return m_tEl;
	}
	operator t_TyEl & ()
	{
		return m_tEl;
	}

// reference counting:
	_TyRef	AddRef() _BIEN_NOTHROW
	{
		return ++m_ref;
	}
	_TyRef	Release() _BIEN_NOTHROW
	{
		return --m_ref;
	}

	bool operator == ( _TyThis const & _r ) const	
  {
    return _gco_dispatch_compare< _TyThis, t_TyEl, t_fUseElCompare >::equals( *this, _r );
  }
	bool operator != ( _TyThis const & _r ) const
  { 
    return _gco_dispatch_compare< _TyThis, t_TyEl, t_fUseElCompare >::notequals( *this, _r );
  }
	bool operator < ( _TyThis const & _r) const		
  { 
    return _gco_dispatch_compare< _TyThis, t_TyEl, t_fUseElCompare >::less( *this, _r );
  }
	bool operator > ( _TyThis const & _r) const
  { 
    return _gco_dispatch_compare< _TyThis, t_TyEl, t_fUseElCompare >::greater( *this, _r );
  }

	// Return a copy of this:
	_TyThis *	copy() const
	{
		_TyThis *	p = _TyAllocThis::allocate( 1 );
		_BIEN_TRY
		{
			new ( p ) _TyThis( *this );
		}
		_BIEN_UNWIND( _TyAllocThis::deallocate( p, 1 ) );
		return p;
	}

// Public static creation utilities:
	static _TyThis * PGCOCreate( t_TyAllocator const & )
	{
		_TyThis * p = _TyAllocThis::allocate( 1 );
		_BIEN_TRY
		{
			new ( p ) _TyThis( );
		}
		_BIEN_UNWIND( _TyAllocThis::deallocate( p, 1 ) );
    return p;
	}

	template < class t_Ty1 >
	static _TyThis * PGCOCreate1( t_Ty1 _r1, 
												  	    t_TyAllocator const & )
	{
		_TyThis * p = _TyAllocThis::allocate( 1 );
		_BIEN_TRY
		{
			new ( p ) _TyThis( _r1 );
		}
		_BIEN_UNWIND( _TyAllocThis::deallocate( p, 1 ) );
    return p;
	}

	template < class t_Ty1, class t_Ty2 >
	static _TyThis * PGCOCreate2( t_Ty1 _r1, t_Ty2 _r2,
									    			  	t_TyAllocator const & )
	{
		_TyThis * p = _TyAllocThis::allocate( 1 );
		_BIEN_TRY
		{
			new ( p ) _TyThis( _r1, _r2 );
		}
		_BIEN_UNWIND( _TyAllocThis::deallocate( p, 1 ) );
    return pgco;
	}

	void		destroy( )
	{
		this->~_TyThis();
		_TyAllocThis::deallocate( this, 1 );
	}
};

// _gco with instanced allocator, element is contained in member buffer -
//  this enables construction of contained element with exact params
//  ( no template argument dedection occurs when ctor is called ):
template <	class t_TyEl,
						class t_TyAllocator,
            bool t_fUseElCompare >
class _gco< t_TyEl, t_TyAllocator, t_fUseElCompare, true, false >
{
	typedef _gco< t_TyEl, t_TyAllocator, t_fUseElCompare, true, false >     _TyThis;
	typedef typename _Alloc_traits<_TyThis, t_TyAllocator>::allocator_type	_TyAllocatorThis;

  // Needed for external access:
	typedef t_TyAllocator		_TyAllocator;	
  typedef t_TyEl _TyEl;

  // Only allow _gcr, _gcp, and _gco_dispatch_compare access to this class:
	template <	class t__TyEl, 
							class t__TyGCO,
							bool t__fCopyOnWrite >
	friend class _gcr;

	template <	class t__TyEl, 
							class t__TyGCO,
							bool t__fCopyOnWrite >
	friend class _gcp;

  template <  class t__TyThis,
              class t__TyEl, 
              bool t__fUseElCompare >
  friend struct _gco_dispatch_compare;

  template < class t_TyGcr, bool t__fCopyOnWrite >
  friend struct _gcr_dispatch_el_access;

  template < class t_TyGcr, bool t__fCopyOnWrite >
  friend struct _gcp_dispatch_el_access;

  _TyAllocatorThis  m_alloc;
	_TyRef	          m_ref;
  char              m_cpEl[ sizeof( _TyEl ) ];

	explicit _gco( t_TyAllocator const & _rAlloc )
		: m_alloc( _rAlloc ),
      m_ref( 1 )
	{
	}
	explicit _gco( _TyThis const & _r )
		: m_alloc( _r.m_alloc ),
      m_ref( 1 )
	{
    // Call copy constructor explicitly:
    new ( m_cpEl ) _TyEl( static_cast< _TyEl const & >( _r ) );
	}

#ifndef NDEBUG
	~_gco()
	{
		assert( !m_ref );	// This should be the case.
	}
#endif //!NDEBUG

// access:
	operator t_TyEl const & () const
	{
		return *reinterpret_cast< t_TyEl const * >( m_cpEl );
	}
	operator t_TyEl & ()
	{
		return *reinterpret_cast< t_TyEl * >( m_cpEl );
	}

// reference counting:
	_TyRef	AddRef() _BIEN_NOTHROW
	{
		return ++m_ref;
	}
	_TyRef	Release() _BIEN_NOTHROW
	{
		return --m_ref;
	}

	bool operator == ( _TyThis const & _r ) const	
  {
    return _gco_dispatch_compare< _TyThis, t_TyEl, t_fUseElCompare >::equals( *this, _r );
  }
	bool operator != ( _TyThis const & _r ) const
  { 
    return _gco_dispatch_compare< _TyThis, t_TyEl, t_fUseElCompare >::notequals( *this, _r );
  }
	bool operator < ( _TyThis const & _r) const		
  { 
    return _gco_dispatch_compare< _TyThis, t_TyEl, t_fUseElCompare >::less( *this, _r );
  }
	bool operator > ( _TyThis const & _r) const
  { 
    return _gco_dispatch_compare< _TyThis, t_TyEl, t_fUseElCompare >::greater( *this, _r );
  }

	_TyThis *	copy() const
	{
		_TyThis *	p = m_alloc.allocate( 1 );
		_BIEN_TRY
		{
			new ( p ) _TyThis( *this );
		}
		_BIEN_UNWIND( m_alloc.deallocate( p, 1 ) );
		return p;
	}

// static creation utilities:
	static _TyThis *	PGCOCreate( t_TyAllocator const & _rAlloc = t_TyAllocator() )
	{
		_TyAllocatorThis		localAllocator( _rAlloc );
    _TyThis *	p = localAllocator.allocate( 1 );
		_BIEN_TRY
		{
			new ( p ) _TyThis( _rAlloc );
      new ( &static_cast< _TyEl & >( *p ) ) _TyEl();
		}
		_BIEN_UNWIND( localAllocator.deallocate( p, 1 ) );
    return p;
	}

	template < class t_Ty1 >
	static _TyThis *	PGCOCreate1(  t_Ty1 _r1, 
													      t_TyAllocator const & _rAlloc = t_TyAllocator() )
	{
		_TyAllocatorThis		localAllocator( _rAlloc );
    _TyThis *	p = localAllocator.allocate( 1 );
		_BIEN_TRY
		{
			new ( p ) _TyThis( _rAlloc );
      new ( &static_cast< _TyEl & >( *p ) ) _TyEl( _r1 );
 		}
		_BIEN_UNWIND( localAllocator.deallocate( p, 1 ) );
    return p;
	}

	template < class t_Ty1, class t_Ty2 >
	static _TyThis *	PGCOCreate2(  t_Ty1 _r1, t_Ty2 _r2,
													      t_TyAllocator const & _rAlloc = t_TyAllocator() )
	{
		_TyAllocatorThis		localAllocator( _rAlloc );
    _TyThis *	p = localAllocator.allocate( 1 );
		_BIEN_TRY
		{
			new ( p ) _TyThis( _rAlloc );
      new ( &static_cast< _TyEl & >( *p ) ) _TyEl( _r1, _r2 );
		}
		_BIEN_UNWIND( localAllocator.deallocate( p, 1 ) );
    return p;
	}

	void			destroy( )
	{
		_TyAllocatorThis		localAllocator( m_alloc ); // since we are deallocating this.
    static_cast< _TyEl & >( *this ).~_TyEl(); // destruct contained object.
		this->~_TyThis();
		localAllocator.deallocate( this, 1 );
	}
};

// _gco with instanceless allocator, element is contained in member buffer -
//  this enables construction of contained element with exact params
//  ( no template argument dedection occurs when ctor is called ):
template <	class t_TyEl,
						class t_TyAllocator,
            bool t_fUseElCompare >
class _gco< t_TyEl, t_TyAllocator, t_fUseElCompare, true, true >
{
	typedef _gco< t_TyEl, t_TyAllocator, t_fUseElCompare, true, true >     _TyThis;

  // The type for the static allocator:
	typedef typename _Alloc_traits<_TyThis, t_TyAllocator>::allocator_type _TyAllocThis;

  // Needed for external access:
	typedef t_TyAllocator		_TyAllocator;	
  typedef t_TyEl _TyEl;

  // Only allow _gcr, _gcp, and _gco_dispatch_compare access to this class:
	template <	class t__TyEl, 
							class t__TyGCO,
							bool t__fCopyOnWrite >
	friend class _gcr;

	template <	class t__TyEl, 
							class t__TyGCO,
							bool t__fCopyOnWrite >
	friend class _gcp;

  template <  class t__TyThis,
              class t__TyEl, 
              bool t__fUseElCompare >
  friend struct _gco_dispatch_compare;

  template < class t_TyGcr, bool t__fCopyOnWrite >
  friend struct _gcr_dispatch_el_access;

  template < class t_TyGcr, bool t__fCopyOnWrite >
  friend struct _gcp_dispatch_el_access;

	_TyRef	          m_ref;
  char              m_cpEl[ sizeof( _TyEl ) ];

	_gco()
		: m_ref( 1 )
	{
	}

	explicit _gco( _TyThis const & _r )
		: m_ref( 1 )
	{
    // Call copy constructor explicitly:
    new ( m_cpEl ) _TyEl( static_cast< _TyEl const & >( _r ) );
	}

#ifndef NDEBUG
	~_gco()
	{
		assert( !m_ref );	// This should be the case.
	}
#endif //!NDEBUG

// access:
	operator t_TyEl const & () const
	{
		return *reinterpret_cast< t_TyEl const * >( m_cpEl );
	}
	operator t_TyEl & ()
	{
		return *reinterpret_cast< t_TyEl * >( m_cpEl );
	}

// reference counting:
	_TyRef	AddRef() _BIEN_NOTHROW
	{
		return ++m_ref;
	}
	_TyRef	Release() _BIEN_NOTHROW
	{
		return --m_ref;
	}

	bool operator == ( _TyThis const & _r ) const	
  {
    return _gco_dispatch_compare< _TyThis, t_TyEl, t_fUseElCompare >::equals( *this, _r );
  }
	bool operator != ( _TyThis const & _r ) const
  { 
    return _gco_dispatch_compare< _TyThis, t_TyEl, t_fUseElCompare >::notequals( *this, _r );
  }
	bool operator < ( _TyThis const & _r) const		
  { 
    return _gco_dispatch_compare< _TyThis, t_TyEl, t_fUseElCompare >::less( *this, _r );
  }
	bool operator > ( _TyThis const & _r) const
  { 
    return _gco_dispatch_compare< _TyThis, t_TyEl, t_fUseElCompare >::greater( *this, _r );
  }

	// Return a copy of this:
	_TyThis *	copy() const
	{
		_TyThis *	p = _TyAllocThis::allocate( 1 );
		_BIEN_TRY
		{
			new ( p ) _TyThis( *this );
		}
		_BIEN_UNWIND( _TyAllocThis::deallocate( p, 1 ) );
		return p;
	}

// Public static creation utilities:
	static _TyThis * PGCOCreate( t_TyAllocator const & )
	{
		_TyThis * p = _TyAllocThis::allocate( 1 );
		_BIEN_TRY
		{
			new ( p ) _TyThis( );
      new ( &static_cast< _TyEl & >( *p ) ) _TyEl();
		}
		_BIEN_UNWIND( _TyAllocThis::deallocate( p, 1 ) );
    return p;
	}

	template < class t_Ty1 >
	static _TyThis * PGCOCreate1( t_Ty1 _r1, 
												  	    t_TyAllocator const & )
	{
		_TyThis * p = _TyAllocThis::allocate( 1 );
		_BIEN_TRY
		{
			new ( p ) _TyThis( );
      new ( &static_cast< _TyEl & >( *p ) ) _TyEl( _r1 );
		}
		_BIEN_UNWIND( _TyAllocThis::deallocate( p, 1 ) );
    return p;
	}

	template < class t_Ty1, class t_Ty2 >
	static _TyThis * PGCOCreate2( t_Ty1 _r1, t_Ty2 _r2,
									    			  	t_TyAllocator const & )
	{
		_TyThis * p = _TyAllocThis::allocate( 1 );
		_BIEN_TRY
		{
			new ( p ) _TyThis( );
      new ( &static_cast< _TyEl & >( *p ) ) _TyEl( _r1, _r2 );
		}
		_BIEN_UNWIND( _TyAllocThis::deallocate( p, 1 ) );
    return pgco;
	}

	void		destroy( )
	{
    static_cast< _TyEl & >( *this ).~_TyEl(); // destruct contained object.
		this->~_TyThis();
		_TyAllocThis::deallocate( this, 1 );
	}
};

// Polymorphic object support:

template < class t_TyBase, bool t_fUseElCompare = false >
class _gcop_base
{
  typedef _gcop_base< t_TyBase, t_fUseElCompare > _TyThis;
  _gcop_base( _TyThis const & ); // don't define this.

protected:

  typedef t_TyBase  _TyEl;

  // We need to define a default allocator type:
  typedef allocator< char > _TyAllocator;

  // Only _gcp and _gcr have access to members of this class.
	template <	class t__TyEl, 
							class t__TyGCO,
							bool t__fCopyOnWrite >
	friend class _gcr;
  template <	class t_TyGCODerived,
              class t_TyGcrBase >
  friend class _gcr_create;

	template <	class t__TyEl, 
							class t__TyGCO,
							bool t__fCopyOnWrite >
	friend class _gcp;

  template <  class t__TyThis,
              class t__TyEl, 
              bool t__fUseElCompare >
  friend struct _gco_dispatch_compare;

  template < class t_TyGcr, bool t__fCopyOnWrite >
  friend struct _gcr_dispatch_el_access;

  template < class t_TyGcr, bool t__fCopyOnWrite >
  friend struct _gcp_dispatch_el_access;

  _TyRef               m_ref;
  // To avoid virtuals in this object we store the following:
  typedef void ( _TyThis:: * _TyPMFnDestroy )();
  _TyPMFnDestroy    m_pmfnDestroy;
  t_TyBase *        m_pBase; // Modifiable reference to derived's contained object.

  _gcop_base( t_TyBase * _pBase,
              _TyPMFnDestroy _pmfnDestroy )
    : m_ref( 1 ),
      m_pBase( _pBase ),
      m_pmfnDestroy( _pmfnDestroy )
  { }

  void  destroy()
  {
    // Delegate to derived container - it knows about the derived object:
    (this->*m_pmfnDestroy)();
  }

	_TyRef	AddRef() _BIEN_NOTHROW
	{
		return ++m_ref;
	}
	_TyRef	Release() _BIEN_NOTHROW
	{
		return --m_ref;
	}

  // Since semantically we have the object stored with us,
  //  we assume that const-ness is same.
  operator const t_TyBase & () const
  {
    return const_cast< const t_TyBase & >( *m_pBase );
  }
  operator t_TyBase & ()
  {
    return *m_pBase;
  }

	bool operator == ( _TyThis const & _r ) const	
  {
    return _gco_dispatch_compare< _TyThis, _TyEl, t_fUseElCompare >::equals( *this, _r );
  }
	bool operator != ( _TyThis const & _r ) const
  { 
    return _gco_dispatch_compare< _TyThis, _TyEl, t_fUseElCompare >::notequals( *this, _r );
  }
	bool operator < ( _TyThis const & _r) const		
  { 
    return _gco_dispatch_compare< _TyThis, _TyEl, t_fUseElCompare >::less( *this, _r );
  }
	bool operator > ( _TyThis const & _r) const
  { 
    return _gco_dispatch_compare< _TyThis, _TyEl, t_fUseElCompare >::greater( *this, _r );
  }
};

// An instance of a derived garbage collected polymorphic object.

// Instanced allocator:
template <  class t_TyDerived, class t_TyBase, class t_TyAllocator = allocator< char >,
            class t_TyBaseClass = _gcop_base< t_TyBase >,
            bool t_fInstanceless = _Alloc_traits< t_TyDerived, t_TyAllocator >::_S_instanceless >
class _gcop_derived : public t_TyBaseClass
{
  typedef _gcop_derived< t_TyDerived, t_TyBase, t_TyAllocator, t_TyBaseClass, t_fInstanceless > _TyThis;
  typedef t_TyBaseClass _TyBase;

  typedef typename _Alloc_traits< _TyThis, t_TyAllocator >::allocator_type _TyAllocatorThis;

  typedef t_TyAllocator _TyAllocator;
  typedef t_TyDerived   _TyDerived;
	typedef typename _TyBase::_TyPMFnDestroy _TyPMFnDestroy;

  // Only _gcp and _gcr have access to members of this class.
	template <	class t__TyEl, 
							class t__TyGCO,
							bool t__fCopyOnWrite >
	friend class _gcr;
  template <	class t_TyGCODerived,
              class t_TyGcrBase >
  friend class _gcr_create;

	template <	class t__TyEl, 
							class t__TyGCO,
							bool t__fCopyOnWrite >
	friend class _gcp;

  template < class t_TyGcr, bool t__fCopyOnWrite >
  friend struct _gcr_dispatch_el_access;

  template < class t_TyGcr, bool t__fCopyOnWrite >
  friend struct _gcp_dispatch_el_access;

public:

// Public static creation utilities - this is how a _gcop_derived object is created:
  template < class t_TyGcp >
  static void CreateGct( t_TyGcp & _rgcp, t_TyAllocator const & _rAlloc = t_TyAllocator() )
  {
    t_TyGcp::template create< _TyThis >::Create( _rgcp, _rAlloc );
  }

  template < class t_TyGcp, class t_Ty1 >
  static void CreateGct1( t_TyGcp & _rgcp, t_Ty1 _r1, 
                          t_TyAllocator const & _rAlloc = t_TyAllocator() )
  {
    t_TyGcp::template create< _TyThis >::template Create1< t_Ty1 >( _rgcp, _r1, _rAlloc );
  }

  template < class t_TyGcp, class t_Ty1, class t_Ty2 >
  static void CreateGct2( t_TyGcp & _rgcp, t_Ty1 _r1, t_Ty2 _r2,
                          t_TyAllocator const & _rAlloc = t_TyAllocator() )
  {
    t_TyGcp::template create< _TyThis >::template Create2< t_Ty1, t_Ty2 >( _rgcp, _r1, _r2, _rAlloc );
  }

private:

  _TyAllocatorThis  m_alloc;
  t_TyDerived       m_tEl;  // The derived element.

  explicit _gcop_derived( _TyThis const & _r )
    : _TyBase( (t_TyBase*)&m_tEl, static_cast< _TyPMFnDestroy >( &_TyThis::destroy ) ),
      m_alloc( _r.m_alloc ),
      m_tEl( _r.m_tEl )
  { 
  }

  explicit _gcop_derived( t_TyAllocator const & _rAlloc )
    : _TyBase( (t_TyBase*)&m_tEl, static_cast< _TyPMFnDestroy >( &_TyThis::destroy ) ),
      m_alloc( _rAlloc )
  {
  }

	template < class t_Ty1 >
  explicit _gcop_derived( t_Ty1 _r1, t_TyAllocator const & _rAlloc )
    : _TyBase( (t_TyBase*)&m_tEl, static_cast< _TyPMFnDestroy >( &_TyThis::destroy ) ),
      m_alloc( _rAlloc ),
      m_tEl( _r1 )
  {
  }

	template < class t_Ty1, class t_Ty2 >
  explicit _gcop_derived( t_Ty1 _r1, t_Ty2 _r2, t_TyAllocator const & _rAlloc )
    : _TyBase( (t_TyBase*)&m_tEl, static_cast< _TyPMFnDestroy >( &_TyThis::destroy ) ),
      m_alloc( _rAlloc ),
      m_tEl( _r1, _r2 )
  {
  }

	_TyThis *	copy() const
	{
		_TyThis *	pgco = m_alloc.allocate( 1 );
		_BIEN_TRY
		{
			new ( pgco ) _TyThis( *this );
		}
		_BIEN_UNWIND( m_alloc.deallocate( pgco, 1 ) );
		return pgco;
	}

	static _TyThis *	PGCOCreate( t_TyAllocator const & _rAlloc = t_TyAllocator() )
	{
		_TyAllocatorThis		localAllocator( _rAlloc );
    _TyThis *	pgco = localAllocator.allocate( 1 );
		_BIEN_TRY
		{
			new ( pgco ) _TyThis( _rAlloc );
		}
		_BIEN_UNWIND( localAllocator.deallocate( pgco, 1 ) );
    return pgco;
	}

	template < class t_Ty1 >
	static _TyThis *	PGCOCreate1(  t_Ty1 _r1, 
						  							      t_TyAllocator const & _rAlloc = t_TyAllocator() )
	{
		_TyAllocatorThis		localAllocator( _rAlloc );
    _TyThis *	pgco = localAllocator.allocate( 1 );
		_BIEN_TRY
		{
			new ( pgco ) _TyThis( _r1, _rAlloc );
		}
		_BIEN_UNWIND( localAllocator.deallocate( pgco, 1 ) );
    return pgco;
	}

	template < class t_Ty1, class t_Ty2 >
	static _TyThis *	PGCOCreate2(  t_Ty1 _r1, t_Ty2 _r2,
					  								      t_TyAllocator const & _rAlloc = t_TyAllocator() )
	{
		_TyAllocatorThis		localAllocator( _rAlloc );
    _TyThis *	pgco = localAllocator.allocate( 1 );
		_BIEN_TRY
		{
			new ( pgco ) _TyThis( _r1, _r2, _rAlloc );
		}
		_BIEN_UNWIND( localAllocator.deallocate( pgco, 1 ) );
    return pgco;
	}

  void  destroy()
  {
    _TyAllocatorThis local( m_alloc );
    this->~_TyThis();
    local.deallocate( this, 1 );
  }
};

// Instanceless allocator:
template <  class t_TyDerived, class t_TyBase, class t_TyAllocator,
            class t_TyBaseClass >
class _gcop_derived< t_TyDerived, t_TyBase, t_TyAllocator, t_TyBaseClass, true >
  : public t_TyBaseClass
{
  typedef _gcop_derived< t_TyDerived, t_TyBase, t_TyAllocator, t_TyBaseClass, true > _TyThis;
  typedef t_TyBaseClass _TyBase;

	typedef typename _Alloc_traits< _TyThis, t_TyAllocator >::_Alloc_type  _TyAllocThis;

  typedef t_TyAllocator _TyAllocator;
  typedef t_TyDerived   _TyDerived;
	typedef typename _TyBase::_TyPMFnDestroy _TyPMFnDestroy;

  // Only _gcp and _gcr have access to members of this class.
	template <	class t__TyEl, 
							class t__TyGCO,
							bool t__fCopyOnWrite >
	friend class _gcr;
  template <	class t_TyGCODerived,
              class t_TyGcrBase >
  friend class _gcr_create;

	template <	class t__TyEl, 
							class t__TyGCO,
							bool t__fCopyOnWrite >
	friend class _gcp;

  template < class t_TyGcr, bool t__fCopyOnWrite >
  friend struct _gcr_dispatch_el_access;

  template < class t_TyGcr, bool t__fCopyOnWrite >
  friend struct _gcp_dispatch_el_access;

public:
// Public static creation utilities - this is how a _gcop_derived object is created:
  template < class t_TyGcp >
  static void CreateGct( t_TyGcp & _rgcp, t_TyAllocator const & _rAlloc = t_TyAllocator() )
  {
    t_TyGcp::template create< _TyThis >::Create( _rgcp, _rAlloc );
  }

  template < class t_TyGcp, class t_Ty1 >
  static void CreateGct1( t_TyGcp & _rgcp, t_Ty1 _r1, 
                          t_TyAllocator const & _rAlloc = t_TyAllocator() )
  {
    t_TyGcp::template create< _TyThis >::Create1( _rgcp, _r1, _rAlloc );
  }

  template < class t_TyGcp, class t_Ty1, class t_Ty2 >
  static void CreateGct2( t_TyGcp & _rgcp, t_Ty1 _r1, t_Ty2 _r2,
                          t_TyAllocator const & _rAlloc = t_TyAllocator() )
  {
    t_TyGcp::template create< _TyThis >::Create1( _rgcp, _r1, _r2, _rAlloc );
  }

private:

  t_TyDerived       m_tEl;  // The derived element.

  explicit _gcop_derived( _TyThis const & _r )
    : _TyBase( (t_TyBase*)&m_tEl, static_cast< _TyPMFnDestroy >( &_TyThis::destroy ) ),
      m_tEl( _r.m_tEl )
  { 
  }

  explicit _gcop_derived( )
    : _TyBase( (t_TyBase*)&m_tEl, static_cast< _TyPMFnDestroy >( &_TyThis::destroy ) )
  {
  }

	template < class t_Ty1 >
  explicit _gcop_derived( t_Ty1 _r1 )
    : _TyBase( (t_TyBase*)&m_tEl, static_cast< _TyPMFnDestroy >( &_TyThis::destroy ) ),
      m_tEl( _r1 )
  {
  }

	template < class t_Ty1, class t_Ty2 >
  explicit _gcop_derived( t_Ty1 _r1, t_Ty2 _r2 )
    : _TyBase( (t_TyBase*)&m_tEl, static_cast< _TyPMFnDestroy >( &_TyThis::destroy ) ),
      m_tEl( _r1, _r2 )
  {
  }

	_TyThis *	copy() const
	{
		_TyThis *	pgco = _TyAllocThis::allocate( 1 );
		_BIEN_TRY
		{
			new ( pgco ) _TyThis( *this );
		}
		_BIEN_UNWIND( _TyAllocThis::deallocate( pgco, 1 ) );
		return pgco;
	}

	static _TyThis *	PGCOCreate( t_TyAllocator const &  )
	{
    _TyThis *	pgco = _TyAllocThis::allocate( 1 );
		_BIEN_TRY
		{
			new ( pgco ) _TyThis( );
		}
		_BIEN_UNWIND( _TyAllocThis::deallocate( pgco, 1 ) );
    return pgco;
	}

	template < class t_Ty1 >
	static _TyThis *	PGCOCreate1(  t_Ty1 _r1, 
						  							      t_TyAllocator const & )
	{
    _TyThis *	pgco = _TyAllocThis::allocate( 1 );
		_BIEN_TRY
		{
			new ( pgco ) _TyThis( _r1 );
		}
		_BIEN_UNWIND( _TyAllocThis::deallocate( pgco, 1 ) );
    return pgco;
	}

	template < class t_Ty1, class t_Ty2 >
	static _TyThis *	PGCOCreate2(  t_Ty1 _r1, t_Ty2 _r2,
					  								      t_TyAllocator const & )
	{
    _TyThis *	pgco = _TyAllocThis::allocate( 1 );
		_BIEN_TRY
		{
			new ( pgco ) _TyThis( _r1, _r2 );
		}
		_BIEN_UNWIND( _TyAllocThis::deallocate( pgco, 1 ) );
    return pgco;
	}

  void  destroy()
  {
    this->~_TyThis();
    _TyAllocThis::deallocate( this, 1 );
  }
};

// Base with virtuals:
template < class t_TyBase, bool t_fUseElCompare = false >
class _gcop_basev
{
  typedef _gcop_basev< t_TyBase, t_fUseElCompare > _TyThis;
  _gcop_basev( _TyThis const & ); // don't define this.

protected:

  typedef t_TyBase  _TyEl;

  // We need to define a default allocator type:
  typedef allocator< char > _TyAllocator;

  // Only _gcp and _gcr have access to members of this class.
	template <	class t__TyEl, 
							class t__TyGCO,
							bool t__fCopyOnWrite >
	friend class _gcr;
  template <	class t_TyGCODerived,
              class t_TyGcrBase >
  friend class _gcr_create;

	template <	class t__TyEl, 
							class t__TyGCO,
							bool t__fCopyOnWrite >
	friend class _gcp;

  template <  class t__TyThis,
              class t__TyEl, 
              bool t__fUseElCompare >
  friend struct _gco_dispatch_compare;

  template < class t_TyGcr, bool t__fCopyOnWrite >
  friend struct _gcr_dispatch_el_access;

  template < class t_TyGcr, bool t__fCopyOnWrite >
  friend struct _gcp_dispatch_el_access;

  _TyRef      m_ref;
  t_TyBase *  m_pBase; // Modifiable reference to derived's contained object.

  _gcop_basev( t_TyBase * _pBase )
    : m_ref( 1 ),
      m_pBase( _pBase )
  { }

#ifndef NDEBUG
  ~_gcop_basev()
  {
    assert( !m_ref ); // this should be the case.
  }
#endif //!NDEBUG

  virtual _TyThis * copy() const = 0;
  virtual void  destroy() = 0;

	_TyRef	AddRef() _BIEN_NOTHROW
	{
		return ++m_ref;
	}
	_TyRef	Release() _BIEN_NOTHROW
	{
		return --m_ref;
	}

  // Since semantically we have the object stored with us,
  //  we assume that const-ness is same.
  operator const t_TyBase & () const
  {
    return const_cast< const t_TyBase & >( *m_pBase );
  }
  operator t_TyBase & ()
  {
    return *m_pBase;
  }

	bool operator == ( _TyThis const & _r ) const	
  {
    return _gco_dispatch_compare< _TyThis, _TyEl, t_fUseElCompare >::equals( *this, _r );
  }
	bool operator != ( _TyThis const & _r ) const
  { 
    return _gco_dispatch_compare< _TyThis, _TyEl, t_fUseElCompare >::notequals( *this, _r );
  }
	bool operator < ( _TyThis const & _r) const		
  { 
    return _gco_dispatch_compare< _TyThis, _TyEl, t_fUseElCompare >::less( *this, _r );
  }
	bool operator > ( _TyThis const & _r) const
  { 
    return _gco_dispatch_compare< _TyThis, _TyEl, t_fUseElCompare >::greater( *this, _r );
  }
};

// An instance of a derived garbage collected polymorphic object.

// Instanced allocator:
template <  class t_TyDerived, class t_TyBase, class t_TyAllocator = allocator< char >,
            class t_TyBaseClass = _gcop_basev< t_TyBase >,
            bool t_fInstanceless = _Alloc_traits< t_TyDerived, t_TyAllocator >::_S_instanceless >
class _gcop_derivedv : public t_TyBaseClass
{
  typedef _gcop_derivedv< t_TyDerived, t_TyBase, t_TyAllocator, t_TyBaseClass, t_fInstanceless > _TyThis;
  typedef t_TyBaseClass _TyBase;

  typedef typename _Alloc_traits< _TyThis, t_TyAllocator >::allocator_type _TyAllocatorThis;

  typedef t_TyAllocator _TyAllocator;
  typedef t_TyDerived   _TyDerived;

  // Only _gcp and _gcr have access to members of this class.
	template <	class t__TyEl, 
							class t__TyGCO,
							bool t__fCopyOnWrite >
	friend class _gcr;
  template <	class t_TyGCODerived,
              class t_TyGcrBase >
  friend class _gcr_create;

	template <	class t__TyEl, 
							class t__TyGCO,
							bool t__fCopyOnWrite >
	friend class _gcp;

public:

// Public static creation utilities - this is how a _gcop_derivedv object is created:
  template < class t_TyGcp >
  static void CreateGct( t_TyGcp & _rgcp, t_TyAllocator const & _rAlloc = t_TyAllocator() )
  {
    t_TyGcp::template create< _TyThis >::Create( _rgcp, _rAlloc );
  }

  template < class t_TyGcp, class t_Ty1 >
  static void CreateGct1( t_TyGcp & _rgcp, t_Ty1 _r1, 
                          t_TyAllocator const & _rAlloc = t_TyAllocator() )
  {
    t_TyGcp::template create< _TyThis >::template Create1< t_Ty1 >( _rgcp, _r1, _rAlloc );
  }

  template < class t_TyGcp, class t_Ty1, class t_Ty2 >
  static void CreateGct2( t_TyGcp & _rgcp, t_Ty1 _r1, t_Ty2 _r2,
                          t_TyAllocator const & _rAlloc = t_TyAllocator() )
  {
    t_TyGcp::template create< _TyThis >::template Create2< t_Ty1, t_Ty2 >( _rgcp, _r1, _r2, _rAlloc );
  }

private:

  _TyAllocatorThis  m_alloc;
  t_TyDerived       m_tEl;  // The derived element.

  explicit _gcop_derivedv( _TyThis const & _r )
    : _TyBase( static_cast< t_TyBase* >( &m_tEl ) ),
      m_alloc( _r.m_alloc ),
      m_tEl( _r.m_tEl )
  { 
  }

  explicit _gcop_derivedv( t_TyAllocator const & _rAlloc )
    : _TyBase( static_cast< t_TyBase* >( &m_tEl ) ),
      m_alloc( _rAlloc )
  {
  }

	template < class t_Ty1 >
  explicit _gcop_derivedv( t_Ty1 _r1, t_TyAllocator const & _rAlloc )
    : _TyBase( static_cast< t_TyBase* >( &m_tEl ) ),
      m_alloc( _rAlloc ),
      m_tEl( _r1 )
  {
  }

	template < class t_Ty1, class t_Ty2 >
  explicit _gcop_derivedv( t_Ty1 _r1, t_Ty2 _r2, t_TyAllocator const & _rAlloc )
    : _TyBase( static_cast< t_TyBase* >( &m_tEl ) ),
      m_alloc( _rAlloc ),
      m_tEl( _r1, _r2 )
  {
  }

  // Bug in Intel compiler prevents returning covariant type:
	_TyBase *	copy() const
	{
		_TyThis *	pgco = m_alloc.allocate( 1 );
		_BIEN_TRY
		{
			new ( pgco ) _TyThis( *this );
		}
		_BIEN_UNWIND( m_alloc.deallocate( pgco, 1 ) );
		return pgco;
	}

	static _TyThis *	PGCOCreate( t_TyAllocator const & _rAlloc = t_TyAllocator() )
	{
		_TyAllocatorThis		localAllocator( _rAlloc );
    _TyThis *	pgco = localAllocator.allocate( 1 );
		_BIEN_TRY
		{
			new ( pgco ) _TyThis( _rAlloc );
		}
		_BIEN_UNWIND( localAllocator.deallocate( pgco, 1 ) );
    return pgco;
	}

	template < class t_Ty1 >
	static _TyThis *	PGCOCreate1(  t_Ty1 _r1, 
						  							      t_TyAllocator const & _rAlloc = t_TyAllocator() )
	{
		_TyAllocatorThis		localAllocator( _rAlloc );
    _TyThis *	pgco = localAllocator.allocate( 1 );
		_BIEN_TRY
		{
			new ( pgco ) _TyThis( _r1, _rAlloc );
		}
		_BIEN_UNWIND( localAllocator.deallocate( pgco, 1 ) );
    return pgco;
	}

	template < class t_Ty1, class t_Ty2 >
	static _TyThis *	PGCOCreate2(  t_Ty1 _r1, t_Ty2 _r2,
					  								      t_TyAllocator const & _rAlloc = t_TyAllocator() )
	{
		_TyAllocatorThis		localAllocator( _rAlloc );
    _TyThis *	pgco = localAllocator.allocate( 1 );
		_BIEN_TRY
		{
			new ( pgco ) _TyThis( _r1, _r2, _rAlloc );
		}
		_BIEN_UNWIND( localAllocator.deallocate( pgco, 1 ) );
    return pgco;
	}

  void  destroy()
  {
    _TyAllocatorThis local( m_alloc );
    this->~_TyThis();
    local.deallocate( this, 1 );
  }
};

// Instanceless allocator:
template <  class t_TyDerived, class t_TyBase, class t_TyAllocator,
            class t_TyBaseClass >
class _gcop_derivedv< t_TyDerived, t_TyBase, t_TyAllocator, t_TyBaseClass, true >
  : public t_TyBaseClass
{
  typedef _gcop_derivedv< t_TyDerived, t_TyBase, t_TyAllocator, t_TyBaseClass, true > _TyThis;
  typedef t_TyBaseClass _TyBase;

	typedef typename _Alloc_traits< _TyThis, t_TyAllocator >::_Alloc_type  _TyAllocThis;

  typedef t_TyAllocator _TyAllocator;
  typedef t_TyDerived   _TyDerived;

  // Only _gcp and _gcr have access to members of this class.
	template <	class t__TyEl, 
							class t__TyGCO,
							bool t__fCopyOnWrite >
	friend class _gcr;
  template <	class t_TyGCODerived,
              class t_TyGcrBase >
  friend class _gcr_create;

	template <	class t__TyEl, 
							class t__TyGCO,
							bool t__fCopyOnWrite >
	friend class _gcp;

public:
// Public static creation utilities - this is how a _gcop_derivedv object is created:
  template < class t_TyGcp >
  static void CreateGct( t_TyGcp & _rgcp, t_TyAllocator const & _rAlloc = t_TyAllocator() )
  {
    t_TyGcp::template create< _TyThis >::Create( _rgcp, _rAlloc );
  }

  template < class t_TyGcp, class t_Ty1 >
  static void CreateGct1( t_TyGcp & _rgcp, t_Ty1 _r1, 
                          t_TyAllocator const & _rAlloc = t_TyAllocator() )
  {
    t_TyGcp::template create< _TyThis >::Create1( _rgcp, _r1, _rAlloc );
  }

  template < class t_TyGcp, class t_Ty1, class t_Ty2 >
  static void CreateGct2( t_TyGcp & _rgcp, t_Ty1 _r1, t_Ty2 _r2,
                          t_TyAllocator const & _rAlloc = t_TyAllocator() )
  {
    t_TyGcp::template create< _TyThis >::Create1( _rgcp, _r1, _r2, _rAlloc );
  }

private:

  t_TyDerived       m_tEl;  // The derived element.

  explicit _gcop_derivedv( _TyThis const & _r )
    : _TyBase( static_cast< t_TyBase* >( &m_tEl ) ),
      m_tEl( _r.m_tEl )
  { 
  }

  explicit _gcop_derivedv( )
    : _TyBase( static_cast< t_TyBase* >( &m_tEl ) )
  {
  }

	template < class t_Ty1 >
  explicit _gcop_derivedv( t_Ty1 _r1 )
    : _TyBase( static_cast< t_TyBase* >( &m_tEl ) ),
      m_tEl( _r1 )
  {
  }

	template < class t_Ty1, class t_Ty2 >
  explicit _gcop_derivedv( t_Ty1 _r1, t_Ty2 _r2 )
    : _TyBase( static_cast< t_TyBase* >( &m_tEl ) ),
      m_tEl( _r1, _r2 )
  {
  }

  // Bug in Intel compiler prevents returning covariant type:
	_TyBase *	copy() const
	{
		_TyThis *	pgco = _TyAllocThis::allocate( 1 );
		_BIEN_TRY
		{
			new ( pgco ) _TyThis( *this );
		}
		_BIEN_UNWIND( _TyAllocThis::deallocate( pgco, 1 ) );
		return pgco;
	}

	static _TyThis *	PGCOCreate( t_TyAllocator const &  )
	{
    _TyThis *	pgco = _TyAllocThis::allocate( 1 );
		_BIEN_TRY
		{
			new ( pgco ) _TyThis( );
		}
		_BIEN_UNWIND( _TyAllocThis::deallocate( pgco, 1 ) );
    return pgco;
	}

	template < class t_Ty1 >
	static _TyThis *	PGCOCreate1(  t_Ty1 _r1, 
						  							      t_TyAllocator const & )
	{
    _TyThis *	pgco = _TyAllocThis::allocate( 1 );
		_BIEN_TRY
		{
			new ( pgco ) _TyThis( _r1 );
		}
		_BIEN_UNWIND( _TyAllocThis::deallocate( pgco, 1 ) );
    return pgco;
	}

	template < class t_Ty1, class t_Ty2 >
	static _TyThis *	PGCOCreate2(  t_Ty1 _r1, t_Ty2 _r2,
					  								      t_TyAllocator const & )
	{
    _TyThis *	pgco = _TyAllocThis::allocate( 1 );
		_BIEN_TRY
		{
			new ( pgco ) _TyThis( _r1, _r2 );
		}
		_BIEN_UNWIND( _TyAllocThis::deallocate( pgco, 1 ) );
    return pgco;
	}

  void  destroy()
  {
    this->~_TyThis();
    _TyAllocThis::deallocate( this, 1 );
  }
};

template <	class t__TyEl, 
						class t__TyGCO = _gco< t__TyEl >,
						bool t__fCopyOnWrite = false >
class _gcr;

template < class t_TyGcp, bool t__fCopyOnWrite >
struct _gcp_dispatch_el_access
{
  static inline typename t_TyGcp::_TyEl * PElNonConst( t_TyGcp & _r )
  {
    return _r._CopyOnWrite();
  }
};

template < class t_TyGcp >
struct _gcp_dispatch_el_access< t_TyGcp, false >
{
  static inline typename t_TyGcp::_TyEl * PElNonConst( t_TyGcp & _r )
  {
		return _r.m_pgco ? &static_cast< typename t_TyGcp::_TyEl & >( *_r.m_pgco ) : 0;
  }
};

// Pointer to reference container.
template <	class t_TyEl, 
						class t_TyGCO = _gco< t_TyEl >,
						bool t_fCopyOnWrite = false >
class _gcp
{
private:

	typedef	_gcp< t_TyEl, t_TyGCO, t_fCopyOnWrite >	_TyThis;
	typedef	typename t_TyGCO::_TyAllocator					_TyAllocator;

	template <	class t__TyEl, 
							class t__TyGCO,
							bool t__fCopyOnWrite >
	friend class _gcr;

  template < class t_TyGcp, bool t__fCopyOnWrite >
  friend struct _gcp_dispatch_el_access;

protected:

	t_TyGCO *	m_pgco;		// Pointer to garbage collected object.

	explicit _gcp( t_TyGCO & _rgco )
		: m_pgco( &_rgco )
	{
		m_pgco->AddRef();
	}

	explicit _gcp( t_TyGCO * _pgco )
		: m_pgco( _pgco )
	{
		if ( m_pgco )
		{
			m_pgco->AddRef();
		}
	}

public:

  typedef t_TyEl  _TyEl;
	typedef t_TyGCO	_TyGCO;
	typedef _gcr< t_TyEl, t_TyGCO, t_fCopyOnWrite >	_TyGcr;

// ctor, dtor:
	_gcp()
		: m_pgco( 0 )
	{
	}

	explicit _gcp( _TyThis const & _rgcp )
		: m_pgco( _rgcp.m_pgco )
	{
		if ( m_pgco )
		{
			m_pgco->AddRef();
		}
	}

  // Create from a reference to a contained element:
  // REVIEW: explicit or not.
  _gcp(  t_TyEl const & _rEl,
         _TyAllocator const & _rA = _TyAllocator() )
		: m_pgco( t_TyGCO::template PGCOCreate1< t_TyEl const & >( _rEl, _rA ) )
  {
  }

  // Create from a reference gco holder of same type:
  // Allow implicit conversion from a gcr.
	_gcp( _TyGcr const & _rgcr )
		: m_pgco( _rgcr.PGCO() )
	{
		if ( m_pgco )
		{
			m_pgco->AddRef();
		}
	}

	~_gcp() _BIEN_NOTHROW
	{
		_Release();
	}

	void	Release() _BIEN_NOTHROW
	{
		_Release();
		m_pgco = 0;
	}

	// Creation of garage collected object:
	void	Create( _TyAllocator const & _rA = _TyAllocator() )
	{
    _TyGCO * pgco = t_TyGCO::PGCOCreate( _rA );
		_Release();
		m_pgco = pgco;
	}

	template < class t_TyP1 >
	void	Create1(	t_TyP1 _r1,
									_TyAllocator const & _rA = _TyAllocator() )
	{
    _TyGCO * pgco = t_TyGCO::template PGCOCreate1< t_TyP1 >( _r1, _rA );
		_Release();
		m_pgco = pgco;
	}

	template < class t_TyP1, class t_TyP2 >
	void	Create2(	t_TyP1 _r1, t_TyP2 _r2,
									_TyAllocator const & _rA = _TyAllocator() )
	{
    _TyGCO * pgco = t_TyGCO::template PGCOCreate2< t_TyP1, t_TyP2 >( _r1, _r2, _rA );
		_Release();
		m_pgco = pgco;
	}

  // Member template that allows creation of polymorphic objects:
  template < class t_TyPolyGCO >
  struct create
  {
    // The allocator we use is that defined in the polymorphic object:
    typedef typename t_TyPolyGCO::_TyAllocator _TyAllocator;

	  static void	Create( _TyThis & _r, 
                        _TyAllocator const & _rA = _TyAllocator() )
	  {
      _TyGCO * pgco = t_TyPolyGCO::PGCOCreate( _rA );
		  _r._Release();
		  _r.m_pgco = pgco;
	  }

	  template < class t_TyP1 >
	  static void	Create1(  _TyThis & _r, 
                          t_TyP1 _r1,
									        _TyAllocator const & _rA = _TyAllocator() )
	  {
      _TyGCO * pgco = t_TyPolyGCO::template PGCOCreate1< t_TyP1 >( _r1, _rA );
		  _r._Release();
		  _r.m_pgco = pgco;
	  }

	  template < class t_TyP1, class t_TyP2 >
	  static void	Create2(  _TyThis & _r, 
                          t_TyP1 _r1, t_TyP2 _r2,
									        _TyAllocator const & _rA = _TyAllocator() )
	  {
      _TyGCO * pgco = t_TyPolyGCO::template PGCOCreate2< t_TyP1, t_TyP2 >( _r1, _r2, _rA );
		  _r._Release();
		  _r.m_pgco = pgco;
	  }
  };

// access:

// named access:
	t_TyEl *	PElNonConst()
	{
    return _gcp_dispatch_el_access< _TyThis, t_fCopyOnWrite >::PElNonConst( *this );
	}
	const t_TyEl *	PElConst() const _BIEN_NOTHROW
	{
		// May have a pointer or be a NULL element:
		return m_pgco ? &const_cast< const t_TyEl & >( static_cast< t_TyEl & >( *m_pgco ) ) : 0;
	}

	t_TyEl & RElNonConst()
	{
		return *PElNonConst();
	}
	const t_TyEl & RElConst() const _BIEN_NOTHROW
	{
		return *PElConst();
	}

// typed access:
	operator t_TyEl * ()
	{
		return PElNonConst();
	}
	operator const t_TyEl * () const _BIEN_NOTHROW
	{
		return PElConst();
	}

  // REVIEW: should we allow conversion to reference operator ?
	operator t_TyEl & ()
	{
		return RElNonConst();
	}
	operator const t_TyEl & () const _BIEN_NOTHROW
	{
		return RElconst();
	}

	t_TyEl *	operator -> ()
	{
		return PElNonConst();
	}
	const t_TyEl *	operator -> () const _BIEN_NOTHROW
	{
		return PElConst();
	}

  t_TyEl &  operator * ()
  {
    return RElNonConst();
  }
  const t_TyEl &  operator * () const _BIEN_NOTHROW
  {
    return RElConst();
  }

// operations:
	_TyThis &	operator = ( _TyThis const & _rgcp ) _BIEN_NOTHROW
	{
		_SetEqual( rgcp.m_pgco );
		return *this;
	}

	_TyThis &	operator = ( _TyGcr const & _rgcr ) _BIEN_NOTHROW
	{
		_SetEqual( _rgcr.m_pgco );
		return *this;
	}
  // No equal operator for _TyEl - use operator *().

  // We allow comparison when we have null pointer, this
  //  allows STL containers to store NULL elements.
	bool operator == ( _TyThis const & rgcp ) const	
  { 
    return rgcp.m_pgco && m_pgco ?
            m_pgco->operator == ( *rgcp.m_pgco ) :
            rgcp.m_pgco == m_pgco;
  }
	bool operator != ( _TyThis const & rgcp ) const
  { 
    return rgcp.m_pgco && m_pgco ?
            m_pgco->operator != ( *rgcp.m_pgco ) :
            rgcp.m_pgco != m_pgco;
  }
	bool operator > ( _TyThis const & rgcp ) const
  { 
    return rgcp.m_pgco && m_pgco ?
            m_pgco->operator > ( *rgcp.m_pgco ) :
            rgcp.m_pgco > m_pgco;
  }
	bool operator < ( _TyThis const & rgcp ) const
  { 
    return rgcp.m_pgco && m_pgco ?
            m_pgco->operator < ( *rgcp.m_pgco ) :
            rgcp.m_pgco < m_pgco;
  }

  void  swap( _TyThis & _r )
  {
    __STD::swap( m_pgco, _r.m_pgco );
  }

protected:

// named access to GCO - these don't participate in copy-on-write:
	t_TyGCO *		PGCO() const _BIEN_NOTHROW
	{
		return m_pgco;
	}
	t_TyGCO &		RGCO() const _BIEN_NOTHROW
	{
		assert( m_pgco );
		return *m_pgco;
	}

	t_TyGCO ** PPGCO() _BIEN_NOTHROW
	{
		assert( !m_pgco );	// We may have a populated reference - giving the address of it to someone else is dangerous.
		return &m_pgco;
	}

	t_TyGCO *& PRGCO() _BIEN_NOTHROW
	{
		assert( !m_pgco );	// We may have a populated reference - giving the address of it to someone else is dangerous.
		return m_pgco;
	}

	void	_Release() _BIEN_NOTHROW
	{
		if ( m_pgco && !m_pgco->Release() )
		{
			m_pgco->destroy( );
		}
	}

	void	_SetEqual( t_TyGCO * _pgco ) _BIEN_NOTHROW
	{
    if ( _pgco != m_pgco )
    {
		  _Release();
		  if ( m_pgco = _pgco )
		  {
			  m_pgco->AddRef();
		  }
    }
	}

	// REVIEW: Move out-of-line ?, compiler likely will anyway:
	t_TyEl *	_CopyOnWrite()
	{
		if ( m_pgco )
		{
			if ( 1 < m_pgco->m_ref )
			{
				// Need to copy:
				_DoCopy();
			}
			return &static_cast< t_TyEl & >( *m_pgco );
		}
		else
		{
			return 0;
		}
	}

	void	_DoCopy()
	{
		t_TyGCO *	pgco = m_pgco->copy();

		__STD_OR_GLOBAL_QUALIFIER swap( pgco, m_pgco );	// We own the copy.

		pgco->Release();
    assert( pgco->m_ref );
	}
};

template < class t_TyGcr, bool t__fCopyOnWrite >
struct _gcr_dispatch_el_access
{
  static inline typename t_TyGcr::_TyEl & RElNonConst( t_TyGcr & _r )
  {
    return _r._CopyOnWrite();
  }
};

template < class t_TyGcr >
struct _gcr_dispatch_el_access< t_TyGcr, false >
{
  static inline typename t_TyGcr::_TyEl & RElNonConst( t_TyGcr & _r )
  {
		return static_cast< typename t_TyGcr::_TyEl & >( _r.RGCO() );
  }
};

// Pointer to reference container.
template <	class t_TyEl, 
						class t_TyGCO,
						bool t_fCopyOnWrite >
class _gcr
{
private:

	typedef	_gcr< t_TyEl, t_TyGCO, t_fCopyOnWrite >		_TyThis;
	typedef	typename t_TyGCO::_TyAllocator						_TyAllocator;
	typedef _gcp< t_TyEl, t_TyGCO, t_fCopyOnWrite >		_TyGcp;

  typedef t_TyGCO _TyGCO;

	template <	class t__TyEl, 
							class t__TyGCO,
							bool t__fCopyOnWrite >
	friend class _gcp;

  template < class t_TyGcr, bool t__fCopyOnWrite >
  friend struct _gcr_dispatch_el_access;

protected:

	t_TyGCO *	m_pgco;		// Pointer to garbage collected object.

  // Protected constructor from pointer to gco, should have ref
  //  count of 1.
	explicit _gcr( t_TyGCO * _pgco )
		: m_pgco( _pgco )
  {
    assert( 1 == ( _pgco->AddRef(), _pgco->Release() ) );
  }

public:

  typedef t_TyEl  _TyEl;

// ctor, dtor:	
	explicit _gcr( _TyAllocator const & _rA = _TyAllocator() )
		: m_pgco( t_TyGCO::PGCOCreate( _rA ) )
	{
		RGCO().AddRef();
	}
	explicit _gcr( _TyThis const & _rgcr )
		: m_pgco( _rgcr.m_pgco )
	{
		RGCO().AddRef();
	}

	// Allow implicit construction from gcp - this allows insertion of gcp's into containers:
	// ASSUMPTION: As with all other _gcr ctors - is that the _gcp is populated.
	_gcr( _TyGcp const & _rgcp )
		: m_pgco( _rgcp.m_pgco )
	{
		RGCO().AddRef();
	}

  // Create from a reference to a contained element:
  // REVIEW: explicit or not.
  _gcr(  t_TyEl const & _rEl,
         _TyAllocator const & _rA = _TyAllocator() )
		: m_pgco( t_TyGCO::template PGCOCreate1< t_TyEl const & >( _rEl, _rA ) )
  {
  }

	~_gcr()
	{
    _Release();
	}

protected:
  void  _Release()
  {
#ifndef _BIEN_USE_EXCEPTIONS	// If we aren't using exceptions then we need to test the address.
		if ( m_pgco )
#endif //!_BIEN_USE_EXCEPTIONS
		if ( !RGCO().Release() )
		{
			RGCO().destroy( );
		}
  }
public:

	// Creation of garage collected object:
	void	Create( _TyAllocator const & _rA = _TyAllocator() )
	{
    _TyGCO * pgco = t_TyGCO::PGCOCreate( _rA );
		_Release();
		m_pgco = pgco;
	}

	template < class t_TyP1 >
	void	Create1(	t_TyP1 _r1,
									_TyAllocator const & _rA = _TyAllocator() )
	{
    _TyGCO * pgco = t_TyGCO::template PGCOCreate1< t_TyP1 >( _r1, _rA );
		_Release();
		m_pgco = pgco;
	}

	template < class t_TyP1, class t_TyP2 >
	void	Create2(	t_TyP1 _r1, t_TyP2 _r2,
									_TyAllocator const & _rA = _TyAllocator() )
	{
    _TyGCO * pgco = t_TyGCO::template PGCOCreate2< t_TyP1, t_TyP2 >( _r1, _r2, _rA );
		_Release();
		m_pgco = pgco;
	}

  // Member template that allows creation of polymorphic objects:
  template < class t_TyPolyGCO >
  struct create
  {
    // The allocator we use is that defined in the polymorphic object:
    typedef typename t_TyPolyGCO::_TyAllocator _TyAllocator;

	  static void	Create( _TyThis & _r, 
                        _TyAllocator const & _rA = _TyAllocator() )
	  {
      _TyGCO * pgco = t_TyPolyGCO::PGCOCreate( _rA );
		  _r._Release();
		  _r.m_pgco = pgco;
	  }

	  template < class t_TyP1 >
	  static void	Create1(  _TyThis & _r, 
                          t_TyP1 _r1,
									        _TyAllocator const & _rA = _TyAllocator() )
	  {
      _TyGCO * pgco = t_TyPolyGCO::template PGCOCreate1< t_TyP1 >( _r1, _rA );
		  _r._Release();
		  _r.m_pgco = pgco;
	  }

	  template < class t_TyP1, class t_TyP2 >
	  static void	Create2(  _TyThis & _r, 
                          t_TyP1 _r1, t_TyP2 _r2,
									        _TyAllocator const & _rA = _TyAllocator() )
	  {
      _TyGCO * pgco = t_TyPolyGCO::template PGCOCreate2< t_TyP1, t_TyP2 >( _r1, _r2, _rA );
		  _r._Release();
		  _r.m_pgco = pgco;
	  }
  };

// access:
	t_TyEl &	RElNonConst()
	{
    return _gcr_dispatch_el_access< _TyThis, t_fCopyOnWrite >::RElNonConst( *this );
	}

	const t_TyEl & RElConst() const _BIEN_NOTHROW
	{
		return const_cast< const t_TyEl & >( static_cast< t_TyEl & >( RGCO() ) );
	}

	t_TyEl * PElNonConst()
	{
		return &RElNonConst();
	}
	const t_TyEl * PElConst() const _BIEN_NOTHROW
	{
		return &RElConst();
	}
	
  // REVIEW: Should we have conversion to pointer ?	
	operator t_TyEl * ()
	{
		return PElNonConst();
	}
	operator const t_TyEl * () const _BIEN_NOTHROW
	{
		return PElConst();
	}

	operator t_TyEl & ()
	{
		return RElNonConst();
	}
	operator const t_TyEl & () const _BIEN_NOTHROW
	{
		return RElConst();
	}

	t_TyEl * operator -> ()
	{
		return PElNonConst();
	}
	const t_TyEl * operator -> () const _BIEN_NOTHROW
	{
		return PElConst();
	}

// operations:
  // This sets the values equal to one qnother:
	_TyThis &	operator = ( _TyThis const & _rgcr )
	{
		RElNonConst() = _rgcr.RElConst();
		return *this;
	}

	_TyThis &	operator = ( _TyGcp const & _rgcp )
	{
		RElNonConst() = _rgcp.RElConst();
		return *this;
	}

  _TyThis & operator = ( t_TyEl const & _rel )
  {
    RElNonConst() = _rel;
		return *this;
  }

	bool operator == ( _TyThis const & _r ) const { return RGCO().operator == ( _r.RGCO() ); }
	bool operator != ( _TyThis const & _r ) const { return RGCO().operator != ( _r.RGCO() ); }
	bool operator > ( _TyThis const & _r ) const { return RGCO().operator > ( _r.RGCO() ); }
	bool operator < ( _TyThis const & _r ) const { return RGCO().operator < ( _r.RGCO() ); }

// transfer referenced object:

  template < class t_TyGc >
  void  SetObject( t_TyGc const & _r )
  {
    // REVIEW: could have if ( _r.PGCO() ) instead:
    assert( _r.PGCO() );
    _Release();
    m_pgco = _r.PGCO();
  }

  void  swap( _TyThis & _r ) _BIEN_NOTHROW
  {
    __STD_OR_GLOBAL_QUALIFIER swap( m_pgco, _r.m_pgco );
  }

protected:

	t_TyGCO *		PGCO() const _BIEN_NOTHROW
	{
		return m_pgco;
	}
	t_TyGCO &		RGCO() const _BIEN_NOTHROW
	{
		return *PGCO();
	}

	t_TyEl &	_CopyOnWrite()
	{
		if ( 1 < RGCO().m_ref )
		{
			// Need to copy:
			_DoCopy();
		}
		return static_cast< t_TyEl & >( RGCO() );
	}

	// TODO: We will have to change the reference to a pointer to implement this:
	void	_DoCopy()
	{
		t_TyGCO *	pgco = m_pgco->copy();
		__STD_OR_GLOBAL_QUALIFIER swap( pgco, m_pgco );	// We own the copy.
    pgco->Release();
    assert( pgco->m_ref );
	}
};

// This class allows creation of a _gcr for a derived object:
template <	class t_TyGCODerived,
            class t_TyGcrBase >
class _gcr_create : public t_TyGcrBase
{
  typedef _gcr_create< t_TyGCODerived, t_TyGcrBase >  _TyThis;
  typedef t_TyGcrBase                                 _TyBase;

  typedef typename t_TyGCODerived::_TyDerived _TyDerived;
	typedef typename _TyBase::_TyAllocator _TyAllocator;

public:

  _gcr_create(  _TyDerived const & _rd,
                _TyAllocator const & _rA = _TyAllocator() )
    : _TyBase( t_TyGCODerived::template PGCOCreate1< _TyDerived const & >( _rd, _rA ) )
  {
  }
};

__BIENUTIL_END_NAMESPACE

#endif //__GARCOLL_H
