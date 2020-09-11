#ifndef __C_PPLST_H
#define __C_PPLST_H

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _c_pplst.h

// property sets

// Design goals:
// 1) Minimize storage costs - hopefully store as variable length struct.
// 2) Allow fast access to properties - keep a sorted lookup table separate from properties.
// 3) Keep the fixed length properties and the variable length properties in different sets.

// increasing order of execution:
// 0) find a property
// 1) change fixed length property
// 3) add fixed length property.
// 4) add variable length property.
// 5) remove fixed length property.
// 6) remove variable length property.
// 7) change variable length property. This can be as fast as (1) if allocation is less.

// Since this is a utility class we will make part of std.
_STLP_BEGIN_NAMESPACE

interface IStream;	// For OLE persistence.

enum EPropertyTypeId
{
	e_ptChar	= 1,
	e_ptUnsignedChar,
	e_ptShort,
	e_ptUnsignedShort,
	e_ptInt,
	e_ptUnsigned,
	e_ptLong,
	e_ptUnsignedLong,
	e_ptFloat,
	e_ptDouble,
	e_ptString,
	e_ptWideString,
	e_ptGUID,
	
	e_ptFirstExternal	// First property type to be used by other stuff.
};

// A template that translates a type into an enumerator indicating the type.
template < class t_Ty >
struct _pset_typeid_of;	// No default implementation.

#define _IMP_PSET_TYPEID_OF( _type, _id ) template <> _pset_typeid_of< _type > \
	{ \
		static const EPropertyTypeId	ms_keptId = _id; \
	};

_IMP_PSET_TYPEID_OF( char, e_ptChar );
_IMP_PSET_TYPEID_OF( char, e_ptUnsignedChar );
_IMP_PSET_TYPEID_OF( short, e_ptShort );
_IMP_PSET_TYPEID_OF( unsigned short, e_ptUnsignedShort );
_IMP_PSET_TYPEID_OF( int, e_ptInt );
_IMP_PSET_TYPEID_OF( unsigned int, e_ptUnsigned );
_IMP_PSET_TYPEID_OF( long, e_ptLong );
_IMP_PSET_TYPEID_OF( unsigned long, e_ptUnsignedLong );
_IMP_PSET_TYPEID_OF( float, e_ptFloat );
_IMP_PSET_TYPEID_OF( double, e_ptDouble );
_IMP_PSET_TYPEID_OF( char *, e_ptString );
_IMP_PSET_TYPEID_OF( wchar_t *, e_ptWideString );
_IMP_PSET_TYPEID_OF( GUID, e_ptGUID );

// A type traits object that gives us the info we need to use a type in the property set.
template < class t_Ty >
struct _pset_type_traits
{
	typedef std::true_type	_TyFIsFixed;	// Sometimes nice to have both a type and a bool.
	static const bool		ms_kfIsFixed = true;					// default is fixed size.
	static const size_t	ms_kstSize = sizeof( t_Ty );	// default size is the in memory size.

	template < class t_TyPersistTo >
	static void	persist( t_TyPersistTo _p, const t_Ty & _rv );

	static inline void	persist( char * _pc, const t_Ty & _rv )
	{
		(void)memcpy( _pc, &_rv, ms_kstSize );
	}

	static inline void	persist( ostream & _ros, const t_Ty & _rv )
	{
		_ros.write( (const unsigned char*)&_rv, ms_kstSize );
	}

	static inline void	persist( IStream * _pis, const t_Ty & _rv )
	{
		__ThrowOLEFAIL( _pis->Write( &_rv, ms_kstSize, 0 ) );
	}

	template < class t_TyPersistTo >
	static void	unpersist( t_TyUnpersistFrom _p, t_Ty & _rv );

	static inline void	unpersist( const char * _pc, t_Ty & _rv )
	{
		(void)memcpy( &_rv, _pc, ms_kstSize );
	}

	static inline void	unpersist( istream & _ris, t_Ty & _rv )
	{
		_ris.read( (unsigned char*)&_rv, ms_kstSize );
	}

	static inline void	unpersist( IStream * _pis, t_Ty & _rv )
	{
		__ThrowOLEFAIL( _pis->Read( &_rv, ms_kstSize, 0 ) );
	}

	static inline void	dump( ostream & _ros, t_Ty const & _rv )
	{
		_ros << _rv;
	}
};

// For pointer types we default to variable size:
template < class t_Ty >
struct _pset_type_traits< t_Ty * >
{
	typedef std::false_type	_TyFIsFixed;	// Sometimes nice to have both a type and a bool.
	static const bool		ms_kfIsFixed = false;
	static const size_t	ms_kstSize = sizeof( t_Ty );	// size in this case is size of element.

	static size_t	length( const t_Ty * _pt )	// Default is to assume zero termination.
	{
		t_Ty *	_ptCur = _pt;
		while( *_ptCur++ )
			;
		return _ptCur - _pt;	// include zero terminator in length.
	}

	template < class t_TyPersistTo >
	static void	persist( t_TyPersistTo _p, const t_Ty * _pv, size_t _stLength );

	static inline void	persist( char * _pc, const t_Ty * _pv, size_t _stLength )
	{
		(void)memcpy( _pc, _pv, _stLength * ms_kstSize );
	}

	static inline void	persist( ostream & _ros, const t_Ty * _pv, size_t _stLength )
	{
		_ros.write( (const unsigned char*)_pv, _stLength * ms_kstSize );
	}

	static inline void	persist( IStream * _pis, const t_Ty * _pv, size_t _stLength )
	{
		__ThrowOLEFAIL( _pis->Write( _pv, _stLength * ms_kstSize, 0 ) );
	}

	template < class t_TyPersistTo >
	static void	unpersist( t_TyUnpersistFrom _p, t_Ty * _pv, size_t _stLength );

	static inline void	unpersist( const char * _pc, t_Ty * _pv, size_t _stLength )
	{
		(void)memcpy( _pv, _pc, _stLength * ms_kstSize );
	}

	static inline void	unpersist( istream & _ris, t_Ty * _pv, size_t _stLength )
	{
		_ris.read( (unsigned char*)_pv, _stLength * ms_kstSize );
	}

	static inline void	unpersist( IStream * _pis, t_Ty * _pv, size_t _stLength )
	{
		__ThrowOLEFAIL( _pis->Read( _pv, _stLength * ms_kstSize, 0 ) );
	}

	static inline void	dump( ostream & _ros, const t_Ty * _pv, size_t _stLength )
	{
		_ros << _pv;
	}
};

struct _ftw_impl_base
{
private:
	typedef _ftw_impl_base	_TyThis;
public:

	virtual ~_ftw_impl_base()
	{
	}

	virtual const char *	type_name() = 0;
	virtual size_t				size_of() = 0;
	virtual void	persist( char * _pc, const void * _pv ) = 0;
	virtual void	persist( ostream & _ros, const void * _pv ) = 0;
	virtual void	persist( IStream * _pis, const void * _pv ) = 0;
	virtual void	unpersist( const char * _pc, void * _pv ) = 0;
	virtual void	unpersist( istream & _ris, void * _pv ) = 0;
	virtual void	unpersist( IStream * _pis, void * _pv ) = 0;
	virtual void	dump( ostream & _ros, const void * _pv ) = 0;
};

// If you see one of these assertions then you didn't 
//	appropriately specialize _fixed_type_wrapper.
struct _ftw_impl_def : public _ftw_impl_base
{
	const char *	type_name()
	{
		assert( 0 );
		return 0;
	}
	size_t	size_of()
	{
		assert( 0 );
		return 0;
	}
	void	persist( char * _pc, const void * _pv )
	{
		assert( 0 );
	}
	void	persist( ostream & _ros, const void * _pv )
	{
		assert( 0 );
	}
	void	persist( IStream * _pis, const void * _pv )
	{
		assert( 0 );
	}

	void	unpersist( const char * _pc, void * _pv )
	{
		assert( 0 );
	}
	void	unpersist( istream & _ris, void * _pv )
	{
		assert( 0 );
	}
	void	unpersist( IStream * _pis, void * _pv )
	{
		assert( 0 );
	}

	void	dump( ostream & _ros, const void * _pv )
	{
		assert( 0 );
	}
};

template < class t_Ty >
struct _ftw_impl : public _ftw_impl_base
{
private:
	typedef _ftw_impl< t_Ty >	_TyThis;
public:

	virtual const char *	type_name() = 0;

	size_t	size_of()
	{
		return sizeof( t_Ty );
	}

	void	persist( char * _pc, const void * _pv )
	{
		_pset_type_traits< t_Ty >::persist( _pc, *(const t_Ty*)_pv );
	}
	void	persist( ostream & _ros, const void * _pv )
	{
		_pset_type_traits< t_Ty >::persist( _ros, *(const t_Ty*)_pv );
	}
	void	persist( IStream * _pis, const void * _pv )
	{
		_pset_type_traits< t_Ty >::persist( _ros, *(const t_Ty*)_pv );
	}

	void	unpersist( const char * _pc, void * _pv )
	{
		_pset_type_traits< t_Ty >::unpersist( _pc, *(t_Ty*)_pv );
	}
	void	unpersist( istream & _ris, void * _pv )
	{
		_pset_type_traits< t_Ty >::unpersist( _ris, *(t_Ty*)_pv );
	}
	void	unpersist( IStream * _pis, void * _pv )
	{
		_pset_type_traits< t_Ty >::unpersist( _pis, *(t_Ty*)_pv );
	}

	void	dump( ostream & _ros, const void * _pv )
	{
		_pset_type_traits< t_Ty >::dump( _ros, *(const t_Ty*)_pv );
	}
};

// Default implementation causes assertions in all methods - needs to be overridden.
template < int t_iTypeId >
struct _fixed_type_wrapper
{
	static _ftw_impl_def	ms_ftwWrapper;
};

#define _IMP_FIXED_TYPE_WRAPPER( _id, _type ) \
template <> \
_fixed_type_wrapper< _id > \
{ \
	struct _ftw : public _ftw_impl< _type > \
	{ \
		const char * type_name() \
		{ \
			return #_type; \
		} \
	}; \
	static _ftw	ms_ftwWrapper; \
};

_IMP_FIXED_TYPE_WRAPPER( e_ptChar, char )
_IMP_FIXED_TYPE_WRAPPER( e_ptUnsignedChar, unsigned char )
_IMP_FIXED_TYPE_WRAPPER( e_ptShort, short )
_IMP_FIXED_TYPE_WRAPPER( e_ptUnsignedShort, unsigned short )
_IMP_FIXED_TYPE_WRAPPER( e_ptInt, int )
_IMP_FIXED_TYPE_WRAPPER( e_ptUnsigned, unsigned int )
_IMP_FIXED_TYPE_WRAPPER( e_ptLong, long )
_IMP_FIXED_TYPE_WRAPPER( e_ptUnsignedLong, unsigned long )
_IMP_FIXED_TYPE_WRAPPER( e_ptFloat, float )
_IMP_FIXED_TYPE_WRAPPER( e_ptDouble, double )
_IMP_FIXED_TYPE_WRAPPER( e_ptGUID, GUID )

struct _vtw_impl_base
{
private:
	typedef _vtw_impl_base	_TyThis;
public:

	virtual const char * type_name() = 0;
	virtual void	persist( char * _pc, const void * _pv, size_t _stLength ) = 0;
	virtual void	persist( ostream & _ros, const void * _pv, size_t _stLength ) = 0;
	virtual void	persist( IStream * _pis, const void * _pv, size_t _stLength ) = 0;
	virtual void	unpersist( const char * _pc, void * _pv, size_t _stLength ) = 0;
	virtual void	unpersist( istream & _ris, void * _pv, size_t _stLength ) = 0;
	virtual void	unpersist( IStream * _pis, void * _pv, size_t _stLength ) = 0;
	virtual void	dump( ostream & _ros, const void * _pv, size_t _stLength ) = 0;

	virtual ~_vtw_impl_base() { }
};

// If you see one of these assertions then you didn't 
//	appropriately specialize _variable_type_wrapper.
struct _vtw_impl_def : public _vtw_impl_base
{
	const char * type_name()
	{
		assert( 0 );
		return 0;
	}

	void	persist( char * _pc, const void * _pv, size_t _stLength )
	{
		assert( 0 );
	}
	void	persist( ostream & _ros, const void * _pv, size_t _stLength )
	{
		assert( 0 );
	}
	void	persist( IStream * _pis, const void * _pv, size_t _stLength )
	{
		assert( 0 );
	}

	void	unpersist( const char * _pc, void * _pv, size_t _stLength )
	{
		assert( 0 );
	}
	void	unpersist( istream & _ris, void * _pv, size_t _stLength )
	{
		assert( 0 );
	}
	void	unpersist( IStream * _pis, void * _pv, size_t _stLength )
	{
		assert( 0 );
	}

	void	dump( ostream & _ros, const void * _pv, size_t _stLength )
	{
		assert( 0 );
	}
};

template < class t_Ty >
struct _vtw_impl : public _vtw_impl_base
{
private:
	typedef _vtw_impl< t_Ty >	_TyThis;
public:

	virtual const char * type_name() = 0;

	void	persist( char * _pc, const void * _pv, size_t _stLength )
	{
		_pset_type_traits< t_Ty >::persist( _pc, *(const t_Ty*)_pv, _stLength );
	}
	void	persist( ostream & _ros, const void * _pv, size_t _stLength )
	{
		_pset_type_traits< t_Ty >::persist( _ros, *(const t_Ty*)_pv, _stLength );
	}
	virtual void	persist( IStream * _pis, const void * _pv, size_t _stLength ) = 0;
	{
		_pset_type_traits< t_Ty >::persist( _ros, *(const t_Ty*)_pv, _stLength );
	}

	void	unpersist( const char * _pc, void * _pv, size_t _stLength )
	{
		_pset_type_traits< t_Ty >::unpersist( _pc, *(t_Ty*)_pv, _stLength );
	}
	void	unpersist( istream & _ris, void * _pv, size_t _stLength )
	{
		_pset_type_traits< t_Ty >::unpersist( _ris, *(t_Ty*)_pv, _stLength );
	}
	void	unpersist( IStream * _pis, void * _pv, size_t _stLength )
	{
		_pset_type_traits< t_Ty >::unpersist( _pis, *(t_Ty*)_pv, _stLength );
	}

	void	dump( ostream & _ros, const void * _pv, size_t _stLength )
	{
		_pset_type_traits< t_Ty >::dump( _ros, *(const t_Ty*)_pv, _stLength );
	}
};

// Default implementation causes assertions in all methods - needs to be overridden.
template < int t_iTypeId >
struct _variable_type_wrapper
{
	static _vtw_impl_def	ms_vtwWrapper;
};

#define _IMP_VARIABLE_TYPE_WRAPPER( _id, _type ) \
template <> \
_variable_type_wrapper< _id > \
{ \
	struct _vtw : public _vtw_impl< _type > \
	{ \
		const char * type_name() \
		{ \
			return #_type; \
		} \
	}; \
	static _vtw	ms_vtwWrapper; \
};

_IMP_VARIABLE_TYPE_WRAPPER( e_ptString, char * )
_IMP_VARIABLE_TYPE_WRAPPER( e_ptWideString, wchar_t * )

struct _pset_lookup_fixed
{
	typedef	unsigned long	_TyPropId;
	typedef EPropertyTypeId	_TyPropTypeId;
	
	_TyPropId				m_id;
	_TyPropTypeId		m_type;
	size_t					m_stOffset;
};

struct _pset_lookup_variable : public _pset_lookup_fixed
{
	size_t					m_stLength;
	size_t					m_stAllocLength;
};

struct _pset_compare_prop_id
	: public binary_function<	_pset_lookup_fixed, 
														_pset_lookup_fixed::_TyPropId, bool >
{
	bool	operator () ( _pset_lookup_fixed const & _rplf, 
											_pset_lookup_fixed::_TyPropId const & _rid ) const
	{
		return _rplf.m_id < _rid;
	}
};

template <	class t_TyAllocator, 
						int t_kiPropBlockSize = 10 >
struct _pset
	: public _alloc_base< char, t_TyAllocator >
{
private:
	typedef	_pset< t_TyAllocator >	_TyThis;
	typedef _alloc_base< char, t_TyAllocator >	_TyCharAllocBase;
public:

	typedef _pset_lookup_fixed		_TyPropLookupFixed;
	typedef _pset_lookup_variable	_TyPropLookupVariable;

	_TyPropLookupFixed *	m_rgPropLookupFixed;	// sorted property id lookup.
	size_type							m_stPropLookupFixed;
	size_type							m_stPropLookupFixedAllocated;

	_TyPropLookupVariable *	m_rgPropLookupVariable;	// sorted property id lookup.
	size_type								m_stPropLookupVariable;
	size_type								m_stPropLookupVariableAllocated;

	char *					m_pcFixed;
	size_type				m_stFixed;
	size_type				m_stFixedAllocated;

	char *					m_pcVariable;
	size_type				m_stVariable;
	size_type				m_stVariableAllocated;

	_pset_compare_prop_id	m_compId;

	_pset( t_TyAllocator const & _rAlloc )
		: _TyCharAllocBase( _rAlloc )
	{
		memset( &m_rgPropLookupFixed, 0, 
			offsetof( _TyThis, m_compId ) - offsetof( _TyThis, m_rgPropLookupFixed ) );
	}

	~_pset()
	{
		_clear();
	}

	void	clear()
	{
		_clear();
		memset( &m_rgPropLookupFixed, 0, 
			offsetof( _TyThis, m_compId ) - offsetof( _TyThis, m_rgPropLookupFixed ) );
	}

	void	_clear()
	{
		if ( m_rgPropLookupFixed )
		{
			_TyCharAllocBase::deallocate_n( m_rgPropLookupFixed, m_stPropLookupAllocated * sizeof _TyPropLookup );
		}
		if ( m_rgPropLookupVariable )
		{
			_TyCharAllocBase::deallocate_n( m_rgPropLookupVariable, m_stPropLookupAllocated * sizeof _TyPropLookup );
		}
		if ( m_pcFixed )
		{
			_TyCharAllocBase::deallocate_n( m_pcFixed, m_stFixedAllocated );
		}
		if ( m_pcVariable )
		{
			_TyCharAllocBase::deallocate_n( m_pcVariable, m_stVariableAllocated );
		}
	}

	_TyPropLookupFixed *	begin_fixed()
	{
		return m_rgPropLookupFixed;
	}	
	_TyPropLookupFixed *	end_fixed()
	{
		return m_rgPropLookupFixed + m_stPropLookupFixed;
	}
	
	_TyPropLookupVariable *	begin_variable()
	{
		return m_rgPropLookupVariable;
	}	
	_TyPropLookupVariable *	end_variable()
	{
		return m_rgPropLookupVariable + m_stPropLookupVariable;
	}

	_TyPropLookupVariable *	find_variable( _TyPropId const & _id )
	{
		return lower_bound( begin_variable(), end_variable(), _id, m_compId );
	}
	_TyPropLookupFixed *	find_fixed( _TyPropId const & _id )
	{
		return lower_bound( begin_fixed(), end_fixed(), _id, m_compId );
	}

	template < class t_TyValue >
	void	insert( _TyPropId const & _id, 
								t_TyValue const & _rv )
	{
		// Dispatch according to variable or fixed size.
		_insert_value( _rv, _id, 
			typename _pset_type_traits< t_TyValue >::_TyFIsFixed() );
	}

	// Return a copy ( for fixed ) or a reference to the internal value for a variable type:
	template < class t_TyValue >
	void	get(	_TyPropId const & _id, 
							t_TyValue & _rv )
	{
		_get_value( _rv, _id, 
			typename _pset_type_traits< t_TyValue >::_TyFIsFixed() );
	}

	// Return the length of the given property in bytes:
	size_t	get_length( _TyPropId const & _id,
											_TyPropLookupFixed ** _ppplf )
	{
		// Generally this method is used for variable types, so look there first:
		_TyPropLookupVariable * pplv = find_variable( _id );
		if (	pplv == end_variable() ||
					( pplv->m_id != _id ) )
		{
			_TyPropLookupFixed * pplf = find_fixed( _id );
			if (	pplf != end_fixed() &&
						( pplf->m_id == _id ) )
			{
				return get_fixed_type_wrapper( pplf->m_type )->size_of();
			}
			else
			{
				return 0;
			}
		}
		else
		{
			*_ppplf = pplv;
			return pplv->m_stLength;
		}
	}

	// Gets a copy of a given property - this is generally used for variable
	//	length properties - first call get_length().
	template < class t_TyValue >
	void	get_copy(	const _TyPropLookupFixed * _pplf, 
									t_TyValue & _rv )
	{
		return _get_copy( _pplf, _rv, 
			typename _pset_type_traits< t_TyValue >::_TyFIsFixed() );
	}

	template < class t_TyValue >
	void	_get_copy(	const _TyPropLookupFixed * _pplf, 
										t_TyValue & _rv.
										std::true_type )	// fixed.
	{
		_pset_type_traits< t_TyValue >::unpersist( m_cpFixed + _pplf->m_stOffset, _rv );
	}

	template < class t_TyValue >
	void	_get_copy(	const _TyPropLookupFixed * _pplf, 
										t_TyValue & _rv.
										std::false_type )	// variable.
	{
		// We copy the value in:
		const _TyPropLookupVariable * pplv = static_cast< const _TyPropLookupVariable * >( _pplf );
		_pset_type_traits< t_TyValue >::unpersist( m_cpVariable + pplv->m_stOffset, _rv, pplv->m_stLength );
	}

	void	remove( _TyPropId const & _id )
	{
		_TyPropLookupFixed * pplf = find_fixed( _id );
		if (	pplf == end_fixed() ||
					( pplf->m_id != _id ) )
		{
			_TyPropLookupVariable * pplv = find_variable( _id );
			if (	pplv != end_variable() &&
						( pplv->m_id == _id ) )
			{
				remove_variable( pplv );
			}
		}
		else
		{
			remove_fixed( pplf );
		}
	}

	// Remove the variable length index record located at <_pplv>.
	// Remove the associated value at the same time.
	void	remove_variable( _TyPropLookupVariable * _pplv ) __STLP_NOTHROW
	{
		assert( _valid_variable( _pplv ) );

		_remove_variable_value( _pplv );
		m_stVariable -= _pplv->m_stAllocLength;

		if ( _pplv+1 != end_variable() )
		{
			memmove( _pplv, _pplv + 1, end_variable() - _ppvl - 1 );
		}
		--m_stPropLookupVariable;
	}
	
	// Remove the fixed length index record located at <_pplf>.
	// Remove the associated value at the same time.
	void	remove_fixed( _TyPropLookupFixed * _pplf ) __STLP_NOTHROW
	{
		assert( _valid_fixed( _pplv ) );

		_TyFixedTypeWrapper * pftw = get_fixed_type_wrapper( pplf->m_type );
		size_t	stSize = pftw->size_of();
		_remove_fixed_value( _pplf, stSize );
		m_stFixed -= stSize;

		if ( _pplf+1 != end_fixed() )
		{
			memmove( _pplf, _pplf + 1, end_fixed() - _ppvl - 1 );
		}
		--m_stPropLookupFixed;
	}
	
	// binary save.
	template < class t_TySaveTo >
	void	save( t_TySaveTo _s )
	{
		save_fixed( _s );
		save_variable( _s );
	}	

	void	save_fixed( ostream & _ros )
	{
		// Write the number of fixed records, the fixed records, then dump the entire record block:
		_ros.write( &m_stPropLookupFixed, sizeof m_stPropLookupFixed );
		_ros.write( (char*)begin_fixed(), 
								m_stPropLookupFixed * sizeof _TyPropLookupFixed );		
		_ros.write( &m_stFixed, sizeof m_stFixed );
		_ros.write( m_cpFixed, m_stFixed );
	}

	void	save_variable( ostream & _ros )
	{
		// Write the number of variable records, the variable records, then dump the entire record block:
		_ros.write( &m_stPropLookupVariable, sizeof m_stPropLookupVariable );
		_TyPropLookupVariable * pplvEnd = end_variable();
		size_t	stUnused = 0;
		const size_t	kstUnusedLimit = 64;	// Allow bytes of unused space before we compressed.
		do
		{
			streampos	sp = _ros.tellp();
			for ( _TyPropLookupVariable * pplv = begin_variable();
						pplvEnd != pplv;
						++pplv )
			{
				_ros.write( pplv, sizeof *pplv );
				stUnused += pplv->m_stAllocLength - pplv->m_stLength;
			}

			if ( stUnused > kstUnusedLimit )
			{
				try
				{
					_compress_variable( stUnused );
				}
				__CATCH_MEMORY_EXCEPTION
				{
					stUnused = 0;	// Failed allocating memory - continue the write.
				}
				if ( stUnused > kstUnusedLimit )
				{
					_ros.seekp( sp );
				}
			}
		}
		while( stUnused > kstUnusedLimit );
		
		_ros.write( &m_stVariable, sizeof m_stVariable );
		_ros.write( m_cpVariable, m_stVariable );
	}

	// binary load.
	template < class t_TyLoadFrom >
	void	load( t_TyLoadFrom _s )
	{
		clear();	// we don't maintain state on throw so...
		_BIEN_TRY
		{
			load_fixed( _s );
			load_variable( _s );
		}
		_BIEN_UNWIND( clear() );	// ensure that we leave this method in valid state.
	}

	void
	load_fixed( istream & _ris )
	{
		// Write the number of fixed records, the fixed records, then dump the entire record block:
		_ris.read( &m_stPropLookupFixed, sizeof m_stPropLookupFixed );
		// We always load tight - we never modify these properties - first alloc will cause over-alloc:
		_TyCharAllocBase::allocate_n( m_rgPropLookupFixed, m_stPropLookupFixed );
		m_stPropLookupAllocated = m_stPropLookupFixed;
		_ris.read( m_rgPropLookupFixed, 
			m_stPropLookupFixed * sizeof _TyPropLookupFixed );

		_ris.read( &m_stFixed, sizeof m_stFixed );
		_TyCharAllocBase::allocate_n( m_cpFixed, m_stFixed );	
		m_stFixedAllocated = m_stFixed;
		_ris.read( m_cpFixed, m_stFixed );
	}

	void
	load_variable( istream & _ris )
	{
		// Write the number of variable records, the variable records, then dump the entire record block:
		_ris.read( &m_stPropLookupVariable, sizeof m_stPropLookupVariable );
		// We always load tight - we never modify these properties - first alloc will cause over-alloc:
		_TyCharAllocBase::allocate_n( m_rgPropLookupVariable, m_stPropLookupVariable );
		m_stPropLookupAllocated = m_stPropLookupVariable;
		_ris.read( m_rgPropLookupVariable, 
			m_stPropLookupVariable * sizeof _TyPropLookupVariable );

		_ris.read( &m_stVariable, sizeof m_stVariable );
		_TyCharAllocBase::allocate_n( m_cpVariable, m_stVariable );	
		m_stVariableAllocated = m_stVariable;
		_ris.read( m_cpVariable, m_stVariable );
	}

	void	compress()
	{
		_TyPropLookupVariable * pplvEnd = end_variable();
		for ( _TyPropLookupVariable * pplv = begin_variable();
					pplvEnd != pplv;
					++pplv )
		{
			stUnused += pplv->m_stAllocLength - pplv->m_stLength;
		}
		if ( stUnused )
		{
			_compress_variable( stUnused );
		}
	}

	template < class t_TyStream >
	void	dump( t_TyStream _ros )
	{
		dump_fixed( _ros );
		dump_variable( _ros );
	}

	void	dump_fixed( ostream & _ros )
	{
		_ros << "fixed:\n";
		_TyPropLookupFixed * pplfEnd = end_fixed();
		for ( _TyPropLookupFixed * pplf = begin_fixed();
					pplfEnd != pplf;
					++pplf )
		{
			_TyFixedTypeWrapper * pftw = get_fixed_type_wrapper( pplf->m_type );
			_ros << "\t[" << pplf->m_id << "] " << pftw->type_name();
			_ros << " [";
			pftw->dump( _ros, m_pcVariable + pplf->m_stOffset );
			_ros << "]\n";
		}
	}

	void	dump_variable( ostream & _ros )
	{
		_ros << "variable:\n";
		_TyPropLookupVariable * pplvEnd = end_variable();
		for ( _TyPropLookupVariable * pplv = begin_variable();
					pplvEnd != pplv;
					++pplv )
		{
			_TyVariableTypeWrapper * pvtw = get_variable_type_wrapper( pplv->m_type );
			_ros << "\t[" << pplf->m_id << "] " << pftw->type_name();
			_ros << " [";
			pvtw->dump( _ros, m_pcVariable + pplv->m_stOffset, pplv->m_stLength );
			_ros << "]\n";
		}
	}

	_TyFixedTypeWrapper *
	get_fixed_type_wrapper( _TyPropTypeId _type )
	{
		// We allow for 16 distinct fixed types - this might be enough - but if not the
		//	switch can be augmented:
		switch( (int)_type )
		{
			case 1:
			{
				return &_fixed_type_wrapper< 1 >::ms_ftwWrapper;
			}
			break;
			case 2:
			{
				return &_fixed_type_wrapper< 2 >::ms_ftwWrapper;
			}
			break;
			case 3:
			{
				return &_fixed_type_wrapper< 3 >::ms_ftwWrapper;
			}
			break;
			case 4:
			{
				return &_fixed_type_wrapper< 4 >::ms_ftwWrapper;
			}
			break;
			case 5:
			{
				return &_fixed_type_wrapper< 5 >::ms_ftwWrapper;
			}
			break;
			case 6:
			{
				return &_fixed_type_wrapper< 6 >::ms_ftwWrapper;
			}
			break;
			case 7:
			{
				return &_fixed_type_wrapper< 7 >::ms_ftwWrapper;
			}
			break;
			case 8:
			{
				return &_fixed_type_wrapper< 8 >::ms_ftwWrapper;
			}
			break;
			case 9:
			{
				return &_fixed_type_wrapper< 9 >::ms_ftwWrapper;
			}
			break;
			case 10:
			{
				return &_fixed_type_wrapper< 10 >::ms_ftwWrapper;
			}
			break;
			case 11:
			{
				return &_fixed_type_wrapper< 11 >::ms_ftwWrapper;
			}
			break;
			case 12:
			{
				return &_fixed_type_wrapper< 12 >::ms_ftwWrapper;
			}
			break;
			case 13:
			{
				return &_fixed_type_wrapper< 13 >::ms_ftwWrapper;
			}
			break;
			case 14:
			{
				return &_fixed_type_wrapper< 14 >::ms_ftwWrapper;
			}
			break;
			case 15:
			{
				return &_fixed_type_wrapper< 15 >::ms_ftwWrapper;
			}
			break;
			case 16:
			{
				return &_fixed_type_wrapper< 16 >::ms_ftwWrapper;
			}
			break;
			default:
			{
				assert( 0 );
			}
			break;
		}
	}

	_TyVariableTypeWrapper *
	get_variable_type_wrapper( _TyPropTypeId _type )
	{
		// We allow for 8 distinct variable types - this might be enough - but if not the
		//	switch can be augmented:
		switch( (int)_type )
		{
			case 1:
			{
				return &_variable_type_wrapper< 1 >::ms_vtwWrapper;
			}
			break;
			case 2:
			{
				return &_variable_type_wrapper< 2 >::ms_vtwWrapper;
			}
			break;
			case 3:
			{
				return &_variable_type_wrapper< 3 >::ms_vtwWrapper;
			}
			break;
			case 4:
			{
				return &_variable_type_wrapper< 4 >::ms_vtwWrapper;
			}
			break;
			case 5:
			{
				return &_variable_type_wrapper< 5 >::ms_vtwWrapper;
			}
			break;
			case 6:
			{
				return &_variable_type_wrapper< 6 >::ms_vtwWrapper;
			}
			break;
			case 7:
			{
				return &_variable_type_wrapper< 7 >::ms_vtwWrapper;
			}
			break;
			case 8:
			{
				return &_variable_type_wrapper< 8 >::ms_vtwWrapper;
			}
			break;
			case 9:
			{
				return &_variable_type_wrapper< 9 >::ms_vtwWrapper;
			}
			break;
			case 10:
			{
				return &_variable_type_wrapper< 10 >::ms_vtwWrapper;
			}
			break;
			case 11:
			{
				return &_variable_type_wrapper< 11 >::ms_vtwWrapper;
			}
			break;
			case 12:
			{
				return &_variable_type_wrapper< 12 >::ms_vtwWrapper;
			}
			break;
			case 13:
			{
				return &_variable_type_wrapper< 13 >::ms_vtwWrapper;
			}
			break;
			case 14:
			{
				return &_variable_type_wrapper< 14 >::ms_vtwWrapper;
			}
			break;
			case 15:
			{
				return &_variable_type_wrapper< 15 >::ms_vtwWrapper;
			}
			break;
			case 16:
			{
				return &_variable_type_wrapper< 16 >::ms_vtwWrapper;
			}
			break;
			default:
			{
				assert( 0 );
			}
			break;
		}
	}

protected:

	template < class t_TyValue >
	void _insert_value(	t_TyValue const & _rv, 
											_TyPropId const & _id,
											std::true_type )	// fixed size
	{
		_TyPropLookupFixed *	pplf = find_fixed( _id );
		if ( ( end_fixed() == pplf ) || ( _id != pplf->m_id ) )
		{
			// Then new value:
			pplf = _new_fixed( pplf, _pset_type_traits< t_TyValue >::ms_kstSize );
			pplf->m_id = _id;
			pplf->m_type = _pset_typeid_of< t_TyValue >::ms_keptId;
		}
		_pset_type_traits< t_TyValue >::persist( m_cpFixed + pplf->m_stOffset, _rv );
	}

	template < class t_TyValue >
	void _insert_value(	t_TyValue const & _rv, 
											_TyPropId const & _id,
											std::false_type )	// variable size
	{
		_TyPropLookupVariable *	pplv = find_variable( _id );
		if ( pplv == end_variable() || ( pplv->m_id != _id ) )
		{
			// Then new value:
			pplv = _vacate_variable( pplv );
			pplv->m_id = _id;
			pplv->m_type = _type;
			pplv->m_stAllocLength = 0;	// We don't need to be throw-safe as this is a valid entry.
		}
		pplv->m_stLength = _pset_type_traits< t_TyValue >::length( _rv );
		if ( !pplv->m_stLength || ( pplv->m_stLength > pplv->m_stAllocLength ) )
		{
			_new_variable_value( pplv );
		}

		_pset_type_traits< t_TyValue >::
			persist( m_pcVariable + _pplv->m_stOffset, _rv, pplv->m_stLength );
	}

	template < class t_TyValue >
	void	_get_value( _TyPropId const & _id, 
										t_TyValue & _rv,
										std::true_type )	// fixed.
	{
		_TyPropLookupFixed *	pplf = find_fixed( _id );
		if ( ( end_fixed() != pplf ) && ( _id == pplf->m_id ) )
		{
			_pset_type_traits< t_TyValue >::unpersist( m_cpFixed + pplf->m_stOffset, _rv );
		}
	}

	// The variable _get_value returns the value contained in the property set.
	// Use _get_variable_size/_get_variable_value to get a copy of the value.
	// This method works correctly for the standard variable values ( single and wide strings ).
	template < class t_TyValue >
	void	_get_value( _TyPropId const & _id, 
										t_TyValue & _rv,
										std::false_type )	// variable.
	{
		_TyPropLookupVariable *	pplf = find_variable( _id );
		if ( ( end_variable() != pplf ) && ( _id == pplf->m_id ) )
		{
			_rv = (t_TyValue)( m_cpVariable + pplf->m_stOffset );
		}
	}

	void
	_new_fixed( _TyPropLookupFixed * _pplf, size_t _stSize )
	{
		char *	pcFixedNew;
		size_t	stAlloced;
		if ( m_stFixed + _stSize > m_stFixedAllocated )
		{
			// Then we need new memory for the value:
			_TyCharAllocBase::allocate_n( pcFixedNew, 
				stAlloced = m_stFixedAllocated - m_stFixed + _stSize + ms_kiOverAllocateFixed );

			memcpy( pcFixedNew, m_pcFixed, m_stFixed );
		}
		else
		{
			pcFixedNew = 0;
		}
			
		_BIEN_TRY
		{
			assert( _valid_fixed( _pplf ) );
			if ( m_stPropLookupFixedAllocated == m_stPropLookupFixed )
			{
				_pplf = _new_fixed_block( _pplf );
			}
			else
			if ( _pplf != end_fixed() )
			{
				memmove( _pplf + 1, _pplf, end_fixed() - _pplf );
			}
			++m_stPropLookupFixed;
		}
		_BIEN_UNWIND( if ( pcFixedNew ) _TyCharAllocBase::deallocate_n( pcFixedNew, stAlloced ) );

		if ( pcFixedNew )
		{
			m_pcFixed = pcFixedNew;
			m_stFixedAllocated = stAlloced;
		}

		_pplf->m_stOffset = m_stFixed;
		m_stFixed += _stSize;
		assert( m_stFixed <= m_stFixedAllocated );

		return _pplf;
	}

	void _remove_fixed_value( _TyPropLookupFixed * _pplf,
														size_t _stSize )
	{
		assert( _valid_fixed( _pplf ) );
		// Remove the value:
		size_t	stExtent;
		if (	( _stSize != 0 ) &&
					( stExtent = _pplf->m_stOffset + _stSize ) != m_stFixed )
		{
			memmove(	m_pcFixed + _pplf->m_stOffset, 
								m_pcFixed + stExtent,
								m_stFixed - stExtent );
			// Update offsets:
			for ( _TyPropLookupFixed * pplfUpdate = begin_fixed();
						 pplfUpdate != end_fixed(); 
						 ++pplfUpdate )
			{
				if ( pplfUpdate->m_stOffset >= stExtent )
				{
					pplfUpdate->m_stOffset -= _stSize;
				}
			}
		}
	}

	// Make room for a new variable length index record after passed position.
	_TyPropLookupVariable *	_vacate_variable( _TyPropLookupVariable * _pplv )
	{
		assert( _valid_variable( _pplv ) );
		if ( m_stPropLookupVariableAllocated == m_stPropLookupVariable )
		{
			_pplv = _new_variable_block( _pplv );
		}
		else
		if ( _pplv != end_variable() )
		{
			memmove( _pplv + 1, _pplv, end_variable() - _pplv );
		}
		++m_stPropLookupVariable;
		return _pplv;
	}

	// Insert a new variable length value - <_pplv> indicates the particulars
	//	about the value.
	// NOTE: when <_pplv> indicates a new value it should contain an alloc length of zero
	//	indicating that it is not currently within the variable length buffer.
	void	_new_variable_value( _TyPropLookupVariable * _pplv )
	{
		// Then we need to insert a new, longer value:
		if ( _pplv->m_stLength > ( m_stVariableAllocated - m_stVariable + _pplv->m_stAllocLength ) )
		{
			// Then we need to allocate more memory:
			char * pcVarNew;
			size_t	stAlloced;
			_TyCharAllocBase::allocate_n( pcVarNew, 
				stAlloced = m_stVariableAllocated - m_stVariable + _pplv->m_stLength + ms_kiOverAllocateVariable );
			// No throwing after this.
			if ( !_pplv->m_stAllocLength )
			{
				_pplv->m_stOffset	= m_stVariable;
			}
			if ( _pplv->m_stOffset )
			{
				memcpy( pcVarNew, m_pcVariable, _pplv->m_stOffset );
			}
			size_t	stExtent;
			if ( ( stExtent = _pplv->m_stOffset + _pplv->m_stAllocLength ) != m_stVariable )
			{
				memcpy( pcVarNew + _pplv->m_stOffset, 
								m_pcVariable + stExtent, m_stVariable - stExtent );
				// Update offsets:
				for ( _TyPropLookupVariable * pplvUpdate = begin_variable();
							 pplvUpdate != end_variable(); 
							 ++pplvUpdate )
				{
					if ( pplvUpdate->m_stOffset >= stExtent )
					{
						pplvUpdate->m_stOffset -= _pplv->m_stAllocLength;
					}
				}
			}
			_TyCharAllocBase::deallocate_n( m_pcVariable, m_stVariableAllocated );
			m_stVariableAllocated = stAlloced;
			m_pcVariable = pcVarNew;
		}
		else
		{
			_remove_variable_value( _pplv );
		}

		_pplv->m_stOffset = m_stVariable - _pplv->m_stAllocLength;
		_pplv->m_stAllocLength = _pplv->m_stLength;
		m_stVariable = _pplv->m_stOffset + _pplv->m_stAllocLength;
	}

	void _remove_variable_value( _TyPropLookupVariable * _pplv )
	{
		assert( _valid_variable( _pplv ) );
		// Remove the value:
		size_t	stExtent;
		if (	( _pplv->m_stAllocLength != 0 ) &&
					( stExtent = _pplv->m_stOffset + _pplv->m_stAllocLength ) != m_stVariable )
		{
			memmove(	m_pcVariable + _pplv->m_stOffset, 
								m_pcVariable + stExtent,
								m_stVariable - stExtent );
			// Update offsets:
			for ( _TyPropLookupVariable * pplvUpdate = begin_variable();
						 pplvUpdate != end_variable(); 
						 ++pplvUpdate )
			{
				if ( pplvUpdate->m_stOffset >= stExtent )
				{
					pplvUpdate->m_stOffset -= _pplv->m_stAllocLength;
				}
			}
		}
	}

	// Augment the variable length lookup table by a new block, leave room
	//	when copying data if <_pplv> not at end.
	_TyPropLookupVariable * _new_variable_block( _TyPropLookupVariable * _pplv )
	{
		_TyPropLookupVariable * pplvNew;
		_TyCharAllocBase::allocate_n( pplvNew, m_stPropLookupVariableAllocated + t_kiPropBlockSize );
		if ( _pplv != end_variable() )
		{
			assert( _pplv < end_variable() );
			size_t	stCopy = _pplv - begin_variable();
			memcpy( pplvNew, begin_variable(), stCopy );
			memcpy( pplvNew + stCopy + 1, _pplv, end_variable() - _pplv );
		}
		else
		{
			memcpy( pplvNew, begin_variable(), m_stPropLookupVariable );
		}
		_TyCharAllocBase::deallocate_n( m_rgPropLookupVariable, m_stPropLookupVariableAllocated );
		_TyPropLookupVariable * pplvRtn = pplvNew + ( _pplv - m_rgPropLookupVariable );
		m_rgPropLookupVariable = pplvNew;
		m_stPropLookupVariableAllocated += t_kiPropBlockSize;
		return pplvRtn;
	}

	// Augment the fixed length lookup table by a new block, leave room
	//	when copying data if <_pplf> not at end.
	_TyPropLookupFixed * _new_fixed_block( _TyPropLookupFixed * _pplf )
	{
		_TyPropLookupFixed * pplfNew;
		_TyCharAllocBase::allocate_n( pplfNew, 
			m_stPropLookupFixedAllocated + t_kiPropBlockSize );
		if ( _pplf != end_fixed() )
		{
			assert( _pplf < end_fixed() );
			size_t	stCopy = _pplf - begin_fixed();
			memcpy( pplfNew, begin_fixed(), stCopy );
			memcpy( pplfNew + stCopy + 1, _pplf, end_fixed() - _pplf );
		}
		else
		{
			memcpy( pplfNew, begin_fixed(), m_stPropLookupFixed );
		}
		if ( m_stPropLookupFixedAllocated )
		{
			_TyCharAllocBase::deallocate_n( m_rgPropLookupFixed, m_stPropLookupFixedAllocated );
		}
		_TyPropLookupFixed * pplfRtn = pplfNew + ( _pplf - m_rgPropLookupFixed );
		m_rgPropLookupFixed = pplfNew;
		m_stPropLookupFixedAllocated += t_kiPropBlockSize;
		return pplfRtn;
	}
	
	// Compress the variable properties - removing any unused space.
	void	_compress_variable( size_t _stUnused )
	{
		char * pcVariableNew = _TyCharAllocBase::allocate_n( m_stVariableAllocated - _stUnused );
		char * pcCopy = pcVariableNew;
		_TyPropLookupVariable * pplvEnd = end_variable();
		for ( _TyPropLookupVariable * pplv = begin_variable();
					pplvEnd != pplv;
					++pplv )
		{
			memcpy( pcCopy, m_pcVariable + pplv->m_stOffset, pplv->m_stLength );
			pplv->m_stOffset = pcCopy - pcVariableNew;
			pplv->m_stAllocLength = pplv->m_stLength;
			pcCopy += pplv->m_stLength;
		}
	}

};

_STLP_END_NAMESPACE

#endif //__C_PPLST_H
