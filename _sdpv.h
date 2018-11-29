#ifndef __SDPV_H
#define __SDPV_H

// _sdpv.h

// "virtual smart pointer" - supports polymorphism, cloning

__BIENUTIL_BEGIN_NAMESPACE

template < class t_TyPBase >
struct _sdp_vbase;

template <	class t_TyP, class t_TyAllocator, 
						class t_TyPBase = typename __map_to_base_class< t_TyP >::_TyBase >
class _sdp_valloc;

template < class t_TyP, class t_TyAllocator >
class _sdpv
	: public _sdp< t_TyP, t_TyAllocator, _sdp_valloc< t_TyP, t_TyAllocator > >
{
	typedef _sdp< t_TyP, t_TyAllocator,
								_sdp_valloc< t_TyP, t_TyAllocator > >			_TyBase;
	typedef _sdpv< t_TyP, t_TyAllocator >										_TyThis;

	typedef typename __map_to_base_class< t_TyP >::_TyBase	_TyPBase;
	typedef _sdp_vbase< _TyPBase >													_TyVBase;

	typedef typename _TyBase::_TySdpMD _TySdpMD;

	friend _sdp_valloc< t_TyP, t_TyAllocator >;

	bool	m_fConstructed;	// Set to true if this object has been successfully constructed.

	// Can't construct directly - use static construction stuff below:
	_sdpv( t_TyAllocator const & _rAlloc ) _STLP_NOTHROW
		: _TyBase( _rAlloc ),
			m_fConstructed( false )
	{
	}

public:

	~_sdpv()
	{
		if ( m_fConstructed )
		{
			_TyBase::destruct();
		}
	}

	// Static construction templates:
	static _TyThis *	construct( t_TyAllocator const & _rAlloc = t_TyAllocator() )
	{
		_TyThis *	p = PSdpCreate( _rAlloc );
		_STLP_TRY
		{
			p->allocate();
			new ( p->Ptr() ) t_TyP();
			p->m_fConstructed = true;
		}
		_STLP_UNWIND( p->~_TyThis() );
		return p;	
	}

	template < class t_TyP1 >
	static _TyThis *	construct1( t_TyP1 _p1,
																t_TyAllocator const & _rAlloc = t_TyAllocator() )
	{
		_TyThis *	p = PSdpCreate( _rAlloc );
		_STLP_TRY
		{
			p->allocate();
			new ( p->Ptr() ) t_TyP( _p1 );
			p->m_fConstructed = true;
		}
		_STLP_UNWIND( p->~_TyThis() );
		return p;	
	}

	template < class t_TyP1, class t_TyP2 >
	static _TyThis *	construct2( t_TyP1 _p1, t_TyP2 _p2,
																t_TyAllocator const & _rAlloc = t_TyAllocator() )
	{
		_TyThis *	p = PSdpCreate( _rAlloc );
		_STLP_TRY
		{
			p->allocate();
			new ( p->Ptr() ) t_TyP( _p1, _p2 );
			p->m_fConstructed = true;
		}
		_STLP_UNWIND( p->~_TyThis() );
		return p;	
	}
	// etc...

	void	destruct()
	{
		if ( m_fConstructed )
		{
			m_fConstructed = false;
			_TyBase::destruct();
		}
	}

	void	clone( _TyVBase ** _ppb ) const
	{
		assert( m_fConstructed );	// Can't clone an unconstructed object.
		// We could support this by uncommenting code below:

		_TySdpMD *	pSdp = PSdpCreate( get_allocator() );
		_STLP_TRY
		{
			pSdp->allocate();
//		if ( m_fConstructed )
			{
				new ( pSdp->Ptr() ) t_TyP( *m_pt );
				pSdp->m_fConstructed = true;
			}
		}
		_STLP_UNWIND( pSdp->~_TySdpMD() );
		*_ppb = pSdp;
	}

	_TyPBase *	GetBaseP() const _STLP_NOTHROW
	{
		return m_pt;
	}
};


// A base class that provides virtual deallocation/destruction capabilities:
// ( The contained class can be virtually destroyed without a virtual destructor -
//		as long as the _sdp::t_TyP - is most derived. )
template < class t_TyPBase >
struct _sdp_vbase
{
private:
	typedef _sdp_vbase< t_TyPBase >	_TyThis;

protected:

	typedef void ( _TyThis :: * _TyPMFnDeallocThis )();
	// This member not valid until ~_sdp_valloc().
	_TyPMFnDeallocThis m_pmfnDeallocThis;
	
public:
	
	_sdp_vbase()
	{
	}
	_sdp_vbase( _TyThis const & )
	{
	}
	
	virtual ~_sdp_vbase()
	{
		// Deallocate ourselves:
		(this->*m_pmfnDeallocThis)();
	}
	virtual void clone( _TyThis ** ) const = 0;
	virtual void destruct() = 0;
	virtual void allocate() = 0;
	virtual t_TyPBase *	GetBaseP() const _STLP_NOTHROW = 0;

	operator t_TyPBase * () const _STLP_NOTHROW
	{
		return GetBaseP();
	}
	t_TyPBase * operator -> () const _STLP_NOTHROW
	{
		return GetBaseP();
	}
	t_TyPBase & operator * () const _STLP_NOTHROW
	{
		return *GetBaseP();
	}
};

template <	class t_TyP, class t_TyAllocator, 
						class t_TyPBase >
class _sdp_valloc
	: public _sdp_vbase< t_TyPBase >,
		public _alloc_base< t_TyP, t_TyAllocator >
{
private:
	typedef _sdp_valloc< t_TyP, t_TyAllocator, t_TyPBase >	_TyThis;
	typedef _sdp_vbase< t_TyPBase >													_TyVBase;
	typedef _alloc_base< t_TyP, t_TyAllocator >							_TyAllocBase;

protected:
	typedef _sdpv< t_TyP, t_TyAllocator >										_TySdpMD;
	typedef _alloc_base< _TySdpMD, t_TyAllocator >					_TyAllocSdpMD;

	char					m_rgcAllocSelf[ sizeof( _TyAllocSdpMD ) ]; // This allocator allocated this.

	// Most derived sdp creation:
	static _TySdpMD *	PSdpCreate( t_TyAllocator const & _rAlloc )
	{
		_TyAllocSdpMD	allocSelf( _rAlloc );
		_TySdpMD *	p = allocSelf.allocate_type();
		_STLP_TRY
		{
			new ( p ) _TySdpMD( _rAlloc );
		}
		_STLP_UNWIND( allocSelf.deallocate_type( p ) );
		return p;
	}

	_sdp_valloc( t_TyAllocator const & _rAlloc )
		: _TyAllocBase( _rAlloc )
	{
		new ( m_rgcAllocSelf ) _TyAllocSdpMD( _rAlloc );
	}

	~_sdp_valloc()
	{
		// We cannot deallocate ourselves - since our most base class still exists -
		//	and since its destructor must be called - we register a deallocation method with
		//	the most base object:
		m_pmfnDeallocThis = static_cast< _TyPMFnDeallocThis >( &_TyThis::DeallocThis );
	}

	void	DeallocThis()
	{
    _TyAllocSdpMD alloc( *((_TyAllocSdpMD*)m_rgcAllocSelf) );
		((_TyAllocSdpMD*)m_rgcAllocSelf)->~_TyAllocSdpMD();
		alloc.deallocate_type( static_cast< _TySdpMD * >( this ) );
	}
};

__BIENUTIL_END_NAMESPACE

#endif __SDPV_H
