#ifndef __MTLIST_H
#define __MTLIST_H

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _mtlist.h

// This module defines a multi-tier list. This is very useful for lists of
//	large number of elements - allowing insert and remove in nlogn operations.

#include <function>
#include </dv/stdlib/_booltyp.h>
#include </dv/stdlib/_rltrait.h>
#include </dv/memlib/_bitutil.h>

// We require additional __type_traits info to optimize this container:

// Keep the nodes POD's ( if _TyEl is a POD ).
template < const int _kiTiers >
struct _mtlist_node_base_1
{
private:
	typedef _mtlist_node_base_1< _kiTiers >	_TyThis;
public:
	typedef _TyThis					_TyNonConstThis;
	typedef const _TyThis		_TyConstThis;

	typedef _TyThis		*			_TyNodeArray[ _kiTiers ];
	typedef const _TyThis *	_TyConstNodeArray[ _kiTiers ];

	typedef _TyThis **							_TyNodePPArray[ _kiTiers ];
	typedef const _TyThis * const *	_TyConstNodePPArray[ _kiTiers ];

	int						m_iUsed;	// the number of used tiers.
	_TyNodeArray	m_rgpn;		// array of pointers to next nodes in given tier.
};

template < class _TyEl, const int _kiTiers >
struct _mtlist_node_1 : public _mtlist_node_base_1< _kiTiers >
{
private:
	typedef _mtlist_node_1< _TyEl, _kiTiers >	_TyThis;
public:
	typedef _TyThis				_TyNonConstThis;
	typedef const _TyThis	_TyConstThis;

	_mtlist_node_1( _TyEl const & _rtEl )
		: m_tEl( _rtEl )
	{
	}

	_TyEl		m_tEl;			// the element at this node.
};

// This node class places the element at the first position - this allows
//	list nodes to be created from extant memory - such as in the implementation
//	of a memory heap.
template < class _TyEl, const int _kiTiers >
struct _mtlist_node_2
{
private:
	typedef _mtlist_node_2< _TyEl, _kiTiers >	_TyThis;
public:
	typedef _TyThis					_TyNonConstThis;
	typedef const _TyThis		_TyConstThis;

	typedef _TyThis *				_TyNodeArray[ _kiTiers ];
	typedef const _TyThis *	_TyConstNodeArray[ _kiTiers ];

	_mtlist_node_2( _TyEl const & _rtEl )
		: m_tEl( _rtEl )
	{
	}

	_TyEl					m_tEl;		// the element at this node.
	int						m_iUsed;	// the number of used tiers.
	_TyNodeArray	m_rgpn;		// array of pointers to next nodes in given tier.
};

template < const int _kiTiers, class _TyNodeBase >
struct _mtlist_iterator_base
{
private:
	typedef _mtlist_iterator_base< _kiTiers, _TyNodeBase >	_TyThis;
public:

	typedef size_t               size_type;
	typedef ptrdiff_t            difference_type;
	typedef forward_iterator_tag iterator_category;

	_TyNodeBase	*	m_pnbNode;

	explicit _mtlist_iterator_base( _TyNodeBase * _pnb ) : m_pnbNode( _pnb ) { }

	void inc() { m_pnbNode = m_pnbNode->m_rgpn[ 0 ]; }

	bool operator == ( const _TyThis & _r ) const 
	{
		return m_pnbNode == _r.m_pnbNode;
	}
	bool operator != ( const _TyThis & _r ) const 
	{
		return m_pnbNode != _r.m_pnbNode;
	}
};

template <	class _TyEl, 
						class _TyRef, 
						class _TyPtr, 
						const int _kiTiers,
						class _TyNode,
						class _TyNodeBase >
struct _mtlist_iterator : public _mtlist_iterator_base< _kiTiers, _TyNodeBase >
{
private:
	typedef _mtlist_iterator< _TyEl, _TyRef, _TyPtr, _kiTiers, _TyNode, _TyNodeBase >	_TyThis;
	typedef _mtlist_iterator_base< _kiTiers, _TyNodeBase >														_TyBase;
public:

	typedef _mtlist_iterator< _TyEl, _TyEl&, _TyEl*, _kiTiers, typename _TyNode::_TyNonConstThis, typename _TyNodeBase::_TyNonConstThis >				iterator;
	typedef _mtlist_iterator< _TyEl, const _TyEl&, const _TyEl*, _kiTiers, typename _TyNode::_TyConstThis, typename _TyNodeBase::_TyConstThis >	const_iterator;
	typedef _TyEl																					value_type;
	typedef _TyPtr																					pointer;
	typedef _TyRef																					reference;

	_mtlist_iterator( _TyNode * _pn ) : _TyBase( _pn )										{ }
	_mtlist_iterator( ) : _TyBase( 0 )																		{ }
	_mtlist_iterator( const iterator & _rit ) : _TyBase( _rit.m_pnbNode )	{ }

	reference operator* ( ) const	{ return ( (_TyNode*)m_pnbNode )->m_tEl; }
	pointer operator-> ( ) const	{ return &(operator*()); }

	_TyThis	& operator++( )
	{
		inc();
		return *this;
	}

	_TyThis		operator++( int )
	{
		_TyThis _t = *this;
		inc();
		return _t;
	}
};

template <	class _TyEl,
						const int _kiTiers, 
						class _TyAllocator, 
						class _TyNode,
						bool _fIsStatic >
class _mtlist_base
{

public:

	typedef typename _Alloc_traits< _TyNode, _TyAllocator >::_Alloc_type			_TyAllocNode;
	typedef typename _Alloc_traits< _TyNode, _TyAllocator >::allocator_type		_TyAllocatorNode;

protected:
	
	_TyAllocatorNode	m_alloc;

public:

	_mtlist_base( _TyAllocator const & _r )
		: m_alloc( _r )
	{
	}

	_TyAllocatorNode const & get_allocator() const					{ return m_alloc; }

	_TyNode *	PNodeAllocate( )
	{
		return m_alloc.allocate( 1 );
	}

	void		NodeDeallocate( _TyNode * _ptEl )
	{
		m_alloc.deallocate( _ptEl, 1 );
	}
	
};

template < class _TyEl, const int _kiTiers, class _TyAllocator, class _TyNode >
class _mtlist_base< _TyEl, _kiTiers, _TyAllocator, _TyNode, true >
{
	
public:
	typedef typename _Alloc_traits< _TyNode, _TyAllocator >::_Alloc_type			_TyAllocNode;
	typedef typename _Alloc_traits< _TyNode, _TyAllocator >::allocator_type		_TyAllocatorNode;


	_mtlist_base( _TyAllocator const & )
	{
	}

	_TyAllocatorNode const & get_allocator() const					{ return _TyAllocatorNode(); }

	_TyNode *	PNodeAllocate( )
	{
		return _TyAllocNode::allocate( 1 );
	}

	void		NodeDeallocate( _TyNode * _ptEl )
	{
		_TyAllocNode::deallocate( _ptEl, 1 );
	}
};

template <	class _TyEl, 
						class _TyCompare = less<_TyEl>, 
						const int _kiTiers = 4, 
						class _TyAllocator = allocator< _TyEl >,
						class _TyNode = _mtlist_node_1< _TyEl, _kiTiers >,
						class _TyNodeBase = _mtlist_node_base_1< _kiTiers > >
class mtlist : 
	public _mtlist_base<	_TyEl, _kiTiers, _TyAllocator, _TyNode,
											_Alloc_traits< _TyEl, _TyAllocator >::_S_instanceless >
{
private:
	typedef _mtlist_base<	_TyEl, _kiTiers, _TyAllocator,  _TyNode,
												_Alloc_traits< _TyEl, _TyAllocator >::_S_instanceless >				_TyBase;
	typedef mtlist<	_TyEl, _TyCompare, _kiTiers, _TyAllocator,  _TyNode, _TyNodeBase >	_TyThis;
public:

	_TyNode			m_tElHead;	// Head of list - magic element.
	_TyCompare	m_comp;			// comparison object.

public:

	typedef _TyEl								value_type;
	typedef value_type *				pointer;
	typedef const value_type *	const_pointer;
	typedef value_type &				reference;
	typedef const value_type &	const_reference;
	typedef size_t							size_type;
	typedef ptrdiff_t						difference_type;

	typedef _mtlist_iterator< _TyEl, _TyEl&, _TyEl*, _kiTiers, typename _TyNode::_TyNonConstThis, typename _TyNodeBase::_TyNonConstThis >				iterator;
	typedef _mtlist_iterator< _TyEl, const _TyEl&, const _TyEl*, _kiTiers, typename _TyNode::_TyConstThis, typename _TyNodeBase::_TyConstThis >	const_iterator;

	typedef _TyNode							__TyNode;

	typedef typename _TyBase::_TyAllocatorNode		allocator_type;
	allocator_type const &	get_allocator() const					{ return _TyBase::get_allocator(); }

	mtlist( _TyCompare const & _comp = _TyCompare(),
					_TyAllocator const & _alloc = _TyAllocator(),
					_TyEl const & _rElInit = _TyEl() )
		: m_comp( _comp ),
			_TyBase( _alloc ),
			m_tElHead( _rElInit )
	{
		// Set magic number.
		m_tElHead.m_iUsed = INT_MAX;

		// clear all next pointers.
		memset( m_tElHead.m_rgpn, 0, _kiTiers * sizeof( *m_tElHead.m_rgpn ) );

		// Need to have a maximum value for this algorithm.
		static_assert( typename __relation_traits<_TyEl,_TyCompare>::has_maximum_value::value );
		static_assert( typename __relation_traits<_TyEl,_TyCompare>::unused_maximum_value::value );
		__relation_traits<_TyEl,_TyCompare>::set_maximum_value( m_tElHead.m_tEl );
	}

	~mtlist()
	{
		// Destroy the list:
		_TyNode *	_pnDealloc;
		_TyNode *	_pnNext = (_TyNode *)( m_tElHead.m_rgpn[0] );
		while( _pnDealloc = _pnNext )
		{
			_pnNext = (_TyNode *)( _pnDealloc->m_rgpn[0] );
			NodeDeallocate( _pnDealloc );
		}
	}

  iterator begin()							{ return iterator( (_TyNode*)(m_tElHead.m_rgpn[0]) ); }
  const_iterator begin() const	{ return const_iterator( (_TyNode*)(m_tElHead.m_rgpn[0]) ); }

  iterator end()								{ return iterator(); }
  const_iterator end() const		{ return const_iterator(); }

  bool empty() const						{ return m_tElHead.m_rgpn[0] == 0; }

  reference front()							{ return m_tElHead.m_rgpn[0]->m_tEl; }
  const_reference front() const	{ return m_tElHead.m_rgpn[0]->m_tEl; }

// node creation/destruction:
	_TyNode *	create_node()
	{
		_TyNode	*	_pn = PNodeAllocate();
		_BIEN_TRY
		{
			construct( &_pn->m_tEl );
		}
		_BIEN_UNWIND( NodeDeallocate( _pn ) );
		return _pn;
	}

	_TyNode *	create_node( const _TyEl & _rtEl )
	{
		_TyNode	*	_pn = PNodeAllocate();
		_BIEN_TRY
		{
			construct( &_pn->m_tEl, _rtEl );
		}
		_BIEN_UNWIND( NodeDeallocate( _pn ) );
		return _pn;
	}

	iterator &	convert_iterator( const_iterator const & _r )
	{
		// a non-const container should allow conversion of a const_iterator to an iterator:
		return reinterpret_cast< iterator & > ( const_cast< const_iterator & >( _r ) );	// Since we know that iterators are data compatible.
	}

	void		destroy_node( _TyNode * _pn )	// be careful...
	{
		destroy( _pn );
		NodeDeallocate( _pn );
	}

	void	insert( const _TyEl & _rtEl )
	{
		_TyNode *	_pn = create_node( _rtEl );
		insert( _pn );
	}

	template < const int _kiGenTiers >
	int	_GenUsedTiers()
	{
		static int	s_iTiers = 0;
		int		iTiers = ( s_iTiers++ % ( 2 << _kiGenTiers ) );
		iTiers &= ~( iTiers & ( iTiers - 1 ) );
		iTiers = UMSBitSet( iTiers ) + 1;
		return iTiers;
	}

#if 0
	template <>
	int _GenUsedTiers<4>()
	{
	#warning _GenUsedTiers(): Use better method of generating tier levels.
		static int	iTiers = 0;
		static int	rgTiers[] = { 4, 3, 2, 1, 3, 2, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 2, 1, 2, 1, 2, 1, 1, 1, 1, 1, 1, 1 };
		return rgTiers[ ( ++iTiers ) % 34 ];
	}

	template <>
	int _GenUsedTiers<7>()
	{
	#warning _GenUsedTiers(): Use better method of generating tier levels.
		static int	iTiers = 0;
		static int	rgTiers[] = { 7, 6, 5, 4, 3, 2, 1, 4, 3, 2, 1, 3, 2, 1, 2, 1, 1, 5, 4, 3, 2, 1, 3, 2, 1, 1, 6, 5, 4, 3, 2, 1, 3, 2, 1, 2, 1, 1, 4, 3, 2, 1, 3, 2, 1, 2, 1, 1, 3, 2, 1, 2, 1, 1  };
		return rgTiers[ ( ++iTiers ) % 52 ];
	}
#endif //0

	void	insert( _TyNode * _pn )
	{
		_pn->m_iUsed = _GenUsedTiers<_kiTiers>();

		_TyNode * _pnBeforeFound = &m_tElHead;

		int	iTier = _kiTiers-1;
		do
		{
			for (	;	_pnBeforeFound->m_rgpn[ iTier ] &&
							m_comp( _pn->m_tEl, ((_TyNode*)(_pnBeforeFound->m_rgpn)[ iTier ])->m_tEl );
						_pnBeforeFound = (_TyNode*)(_pnBeforeFound->m_rgpn[ iTier ]) )
				;

			if ( iTier < _pn->m_iUsed )
			{
				_pn->m_rgpn[ iTier ] = _pnBeforeFound->m_rgpn[ iTier ];
				_pnBeforeFound->m_rgpn[ iTier ] = _pn;
			}
		}
		while( iTier-- );
	}

	// Find the element before a given element.
	const_iterator	find_before( const _TyEl & _rtElFind ) const
	{
		const _TyNode * _pnBeforeFound = &m_tElHead;

		int	iTier = _kiTiers-1;
		do
		{
			for ( ;	_pnBeforeFound->m_rgpn[ iTier ] &&
					m_comp( _rtElFind, ((_TyNode*)(_pnBeforeFound->m_rgpn)[ iTier ])->m_tEl );
					_pnBeforeFound = (const _TyNode*)(_pnBeforeFound->m_rgpn[ iTier ]) )
				;
		}
		while( iTier-- );

		return const_iterator( _pnBeforeFound );
	}

	// Find a given element - returns end() if not present.
	const_iterator	find( const _TyEl & _rtElFind ) const
	{
		const_iterator	litBeforeFound = find_before( _rtElFind );
		if ( end() != ++litBeforeFound )
		{
			if ( m_comp( *litBeforeFound, _rtElFind ) )
			{
				litBeforeFound = end();
			}
		}
		return litBeforeFound;
	}

	iterator			find( const _TyEl & _rtElFind )
	{
		return convert_iterator( ((const _TyThis &)(*this)).find( _rtElFind ) );
	}

	// Find the set of pointers that preced this element.
	const_iterator		_find_pointers_before(	const _TyEl & _rtElFind, 
																						typename _TyNodeBase::_TyConstNodePPArray & rgppn ) const
	{
		const _TyNode * _pnBeforeFound = &m_tElHead;

		int	iTier = _kiTiers-1;
		do
		{
			for ( ;	_pnBeforeFound->m_rgpn[ iTier ] &&
					m_comp( _rtElFind, ((const _TyNode *)(_pnBeforeFound->m_rgpn[ iTier ]))->m_tEl );
					_pnBeforeFound = (_TyNode*)(_pnBeforeFound->m_rgpn[ iTier ]) )
				;

			rgppn[ iTier ] = _pnBeforeFound->m_rgpn + iTier;
		}
		while( iTier-- );

		return const_iterator( _pnBeforeFound );
	}

	iterator			_find_pointers_before(	const _TyEl & _rtElFind, 
																				typename _TyNodeBase::_TyConstNodePPArray & rgppn )
	{
		return convert_iterator( ((const _TyThis &)(*this))._find_pointers_before( _rtElFind, rgppn ) );
	}

	// Remove the element less than or equal to <_rtElFind>.
	bool	remove( const _TyEl & _rtElFind )
	{
		typename _TyNode::_TyNodeArray rgpn;
		iterator lit = _find_pointers_before( rgpn );
		if ( end() != ++lit )
		{
			_remove_with_pointers( lit, rgpn );
			return true;
		}
		else
		{
			return false;
		}
	}

	void	_remove_with_pointers(	const_iterator litRemove, 
																typename _TyNodeBase::_TyConstNodePPArray const & _rgppn, 
																_TyNode ** _ppn = 0 )
	{
		int	iTier = _kiTiers-1;

		_TyNode * _pnNode = const_cast< _TyNode * >( static_cast< const _TyNode * >( litRemove.m_pnbNode ) );
		
		do
		{
			Assert( _rgppn[ iTier ] );
			if ( ( iTier < _pnNode->m_iUsed ) )
			{
				// Then update needed:
				*const_cast< _TyNodeBase ** >( _rgppn[ iTier ] ) = _pnNode->m_rgpn[ iTier ];
			}
		}
		while( iTier-- );

		if ( _ppn )
		{
			*_ppn = _pnNode;
		}
		else
		{
			destroy_node( _pnNode );
		}
	}

};

#endif //__MTLIST_H
