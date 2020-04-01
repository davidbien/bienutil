#ifndef __DBGTHRW_H
#define __DBGTHRW_H

#ifndef __NDEBUG_THROW

#include "bienutil.h"
#include <cassert>
#include <algorithm>
//#include <slist>
#include <map>
#include <string>
#include <ostream>
#include "_namdexc.h"

// _dbgthrw.h

// Objects for testing throw-safety in a repeatable manner.

#ifndef __DBGTHROW_DEFAULT_ALLOCATOR
#if defined(_USE_STLPORT) && !defined(NDEBUG)
#define __DBGTHROW_DEFAULT_ALLOCATOR __STD::_stlallocator< char, __STD::__debug_alloc< __STD::__malloc_alloc > >
#define __DBGTHROW_GET_ALLOCATOR(t) __STD::_stlallocator< t, __STD::__debug_alloc< __STD::__malloc_alloc > >
#else // defined(_USE_STLPORT) && !defined(NDEBUG)
#define __DBGTHROW_DEFAULT_ALLOCATOR __STD::allocator< char >
#define __DBGTHROW_GET_ALLOCATOR(t) __STD::allocator< t >
#endif //defined(_USE_STLPORT) && !defined(NDEBUG)
#endif //!__DBGTHROW_DEFAULT_ALLOCATOR

__BIENUTIL_BEGIN_NAMESPACE

enum EThrowType : unsigned int
{
  e_ttMemory      = 0x00000001,
  e_ttFileOutput  = 0x00000002,
  e_ttFileInput   = 0x00000004,

  // This identifies areas in which "chronic" throwing would
  //  occur - i.e. indicating that the next throw point with
  //  this same type would also throw.
  e_ttChronicMask = e_ttFileOutput | e_ttMemory
};


#ifdef __NAMDDEXC_STDBASE
#pragma push_macro("std")
#undef std
#endif //__NAMDDEXC_STDBASE

class _debug_memory_except : public std::_t__Named_exception< __DBGTHROW_DEFAULT_ALLOCATOR >
{
  typedef std::_t__Named_exception< __DBGTHROW_DEFAULT_ALLOCATOR > _TyBase;
public:
  _debug_memory_except()
    : _TyBase( string_type("_debug_memory_except") )
  {
  }
};
class _debug_output_except : public std::_t__Named_exception< __DBGTHROW_DEFAULT_ALLOCATOR >
{
  typedef std::_t__Named_exception< __DBGTHROW_DEFAULT_ALLOCATOR > _TyBase;
public:
  _debug_output_except()
    : _TyBase( "_debug_output_except" )
  {
  }
};
class _debug_input_except : public std::_t__Named_exception< __DBGTHROW_DEFAULT_ALLOCATOR >
{
  typedef std::_t__Named_exception< __DBGTHROW_DEFAULT_ALLOCATOR > _TyBase;
public:
  _debug_input_except()
    : _TyBase( "_debug_input_except" )
  {
  }
};
#ifdef __NAMDDEXC_STDBASE
#pragma pop_macro("std")
#endif //__NAMDDEXC_STDBASE

struct _throw_static_base;

struct _throw_object_base
{
private:
  typedef _throw_object_base _TyThis;
public:
  unsigned long m_rgttType{};
  const char *  m_cpFileName{};
  unsigned long m_ulLineNumber{};
  static _throw_static_base ms_tsb;

  _throw_object_base() = default;
  _throw_object_base( unsigned long _rgttType, 
                      const char * _cpFileName,
                      unsigned long _ulLineNumber,
                      bool _fMaybeThrow = false,
                      bool _fAlwaysThrow = false ) :
    m_rgttType( _rgttType ),
    m_cpFileName( _cpFileName ),
    m_ulLineNumber( _ulLineNumber )
  {
    assert( m_rgttType );
    if ( _fMaybeThrow || _fAlwaysThrow )
    {
      _maybe_throw( _fAlwaysThrow );
    }
  }
  bool  operator < ( const _TyThis & _r ) const
  {
    int iCmp = strcmp( m_cpFileName, _r.m_cpFileName );
    if ( !iCmp )
    {
      return m_ulLineNumber < _r.m_ulLineNumber;
    }
    return iCmp < 0;
  }

protected:
  void  _maybe_throw( bool _fAlwaysThrow );
};              

struct _throw_object_with_throw_rate
  : public _throw_object_base
{
  _throw_object_with_throw_rate() = default;
  explicit _throw_object_with_throw_rate( _throw_object_base const & _r )
    : _throw_object_base( _r )
  {
  }

  int m_iThrowRate{};
  bool m_fHitOnce{};
};

struct _throw_hit_stats
{
  unsigned  m_uPossible;
  unsigned  m_uHit;
};

__INLINE size_t
_count_set_bits( size_t _i )
{
  size_t stSet = 0;
  while( _i )
  {
    _i &= _i-1;
    ++stSet;
  }
  return stSet;
}

struct _throw_static_base
{
  bool          m_fOn{false};
  unsigned int  m_uRandSeed{0};
  int           m_iThrowRate{1000}; 
    // A number less than RAND_MAX that determines whether a given throw object will throw.

  unsigned long m_rgttTypeAccum{0};
    // This accumulates the type of exceptions thrown.

  // Current throw parameters:
  unsigned long m_rgttTypeCur{};
  const char *  m_cpFileNameCur{};
  unsigned long m_ulLineNumberCur{};

  // Save a set of previous throw parameters for utility:
  static const int  ms_kiNumSaved = 200;
  _throw_object_base  m_rgSaved[ ms_kiNumSaved ];

  unsigned      m_uNumThrows{0};
  int           m_iThrowOneOnly{-1};

  // Determine whether we have exhausted the throw points:
  typedef map< _throw_object_base, _throw_hit_stats >::value_type _tyMapValueType;
  typedef map<  _throw_object_base, _throw_hit_stats,
                less<_throw_object_base>,
				__DBGTHROW_GET_ALLOCATOR(_tyMapValueType) >   _TyMapHitThrows;
  _TyMapHitThrows m_mapHitThrows;
  unsigned      m_uHitThrows{0};

  // Allow the programmer to set in a sorted array of file/line throw points
  //  with special throwing requirements:
  _throw_object_with_throw_rate * m_ptobtrStart{0};
  _throw_object_with_throw_rate * m_ptobtrEnd{0};
  
  _throw_static_base()
  {
	  // Scale the throw rate according to the impl's RAND_MAX.
	  const unsigned long long llMinRandMax = 0x7fff;
	  unsigned long long llRandMax = RAND_MAX;
	  llRandMax *= m_iThrowRate;
	  llRandMax /= llMinRandMax;
	  m_iThrowRate = (int)llRandMax;
  }

  void set_on( bool _fOn )
  {
    m_fOn = _fOn;
  }

  void set_seed( unsigned int _uRandSeed )
  {
    m_uRandSeed = _uRandSeed;
    srand( m_uRandSeed );
  }

  void set_throw_rate( int _iThrowRate )
  {
    m_iThrowRate = _iThrowRate;
  }

  void  throw_one_only( int _iThrowOneOnly )
  {
    m_iThrowOneOnly = _iThrowOneOnly;
  }

  void  handle_throw()
  {
    assert( m_rgttTypeAccum );
    m_rgttTypeAccum = 0;
  }

  void  clear_hit_map()
  {
    m_mapHitThrows.clear();
    m_uHitThrows = 0;
  }

  void  get_hit_stats( unsigned & ruHit, size_t & ruPossible )
  {
    ruPossible = m_mapHitThrows.size();
    ruHit = m_uHitThrows;
  }

  void  report_unhit( ostream & _ros )
  {
    for ( _TyMapHitThrows::iterator mit = m_mapHitThrows.begin();
          mit != m_mapHitThrows.end();
          ++mit )
    {
      if ( !mit->second.m_uHit )
      {
        _ros << "Unhit [" << mit->first.m_cpFileName << "] [" << mit->first.m_ulLineNumber << 
          "] Possible [" << mit->second.m_uPossible << "].\n";
      }
    }
  }

  void  reset_hit_once()
  {
    if ( m_ptobtrStart )
    {
      for ( _throw_object_with_throw_rate * _ptobtr = m_ptobtrStart;
            _ptobtr != m_ptobtrEnd;
            ++_ptobtr )
      {
        if ( _ptobtr->m_fHitOnce )
        {
          _ptobtr->m_iThrowRate = RAND_MAX;
        }
      }
    }
  }

  void  _set_params( _throw_object_base * _ptob )
  {
    m_cpFileNameCur = _ptob->m_cpFileName;
    m_ulLineNumberCur = _ptob->m_ulLineNumber;

		// If we have multiple types then choose one:
		m_rgttTypeCur = _ptob->m_rgttType;

		size_t stSetBits;
		if ( 1 < ( stSetBits = _count_set_bits( m_rgttTypeCur ) ) )
		{
			// Choose a random bit among those present:
			int _iR = rand() % (int)stSetBits;

			// Clear each bit successively:
			while( _iR )
			{
				m_rgttTypeCur &= m_rgttTypeCur-1;
				_iR--;
			}

			m_rgttTypeCur &= ~( m_rgttTypeCur & m_rgttTypeCur-1 );
			assert( 1 == _count_set_bits( m_rgttTypeCur ) );
		}
		m_rgttTypeAccum |= m_rgttTypeCur;
  }

  void  _throw()
  {
    if (  ( m_iThrowOneOnly == m_uNumThrows++ ) ||
          ( m_iThrowOneOnly < 0 ) )
    {
      // Save the current parameters on the last thrown list:
      memmove( m_rgSaved+1, m_rgSaved, ms_kiNumSaved-1 );
      *m_rgSaved = _throw_object_base(  m_rgttTypeCur, 
                                        m_cpFileNameCur,
                                        m_ulLineNumberCur );

      // Check to see if we have hit this throw point yet:
      _TyMapHitThrows::iterator mit = m_mapHitThrows.find( *m_rgSaved );
      assert( mit != m_mapHitThrows.end() );
      if ( !mit->second.m_uHit )
      {
        m_uHitThrows++;
      }
      mit->second.m_uHit++;

      switch( m_rgttTypeCur )
      {
        case e_ttMemory:
        {
#ifdef __DEBUG_THROW_VERBOSE
					fprintf( stderr, __FILE__ ":" ppmacroxstr(__LINE__) ": Throwing _debug_memory_except().\n" );
#endif 
          throw _debug_memory_except();
        }
        break;

        case e_ttFileOutput:
        {
          throw _debug_output_except();
        }
        break;

        case e_ttFileInput:
        {
          throw _debug_input_except();
        }
        break;

        default:
        {
          assert( 0 );
        }
        break;
      }
    }
  }

  void  _maybe_throw( _throw_object_base * _ptob, bool _fAlwaysThrow )
  {
    if ( !m_fOn )
      return;

    // Ensure that this throw object is in the map:
    _throw_hit_stats  ths = { 0, 0 };
    _TyMapHitThrows::value_type vtHit( *_ptob, ths );
    pair< _TyMapHitThrows::iterator, bool > pib = m_mapHitThrows.insert( vtHit );
    pib.first->second.m_uPossible++;

    if ( _fAlwaysThrow || ( e_ttChronicMask & m_rgttTypeAccum & _ptob->m_rgttType ) )
    {
      // Then a chronic throwing condition:
      // Throw the same as the last:
      _set_params( _ptob );
      _throw();
    }
    else
    {
      int iThrowRate = m_iThrowRate;

      if ( m_ptobtrStart )
      {
        pair< _throw_object_with_throw_rate *, _throw_object_with_throw_rate * > pitEqual = 
          equal_range( m_ptobtrStart, m_ptobtrEnd, _throw_object_with_throw_rate( *_ptob ) );
        if ( pitEqual.first != pitEqual.second )
        {
          assert( pitEqual.first+1 == pitEqual.second );
          if ( pitEqual.first->m_fHitOnce )
          {
            if ( pitEqual.first->m_iThrowRate )
            {
              pitEqual.first->m_iThrowRate = 0;
              iThrowRate = RAND_MAX;
            }
            else
            {
              return; // Already been hit.
            }
          }
          else
          {
            iThrowRate = pitEqual.first->m_iThrowRate;
          }
        }
      }

      // Don't throw until the seed is set:
      if ( m_uRandSeed && ( rand() <= iThrowRate ) )
      {
        _set_params( _ptob );
        _throw();
      }
    }
  }
};

__INLINE void 
_throw_object_base::_maybe_throw( bool _fAlwaysThrow )
{
  ms_tsb._maybe_throw( this, _fAlwaysThrow );
}

#define __THROWPT( _type )  _throw_object_base( _type, __FILE__, __LINE__, true );
#define __THROWPTALWAYS( _type )  _throw_object_base( _type, __FILE__, __LINE__, true, true );

__BIENUTIL_END_NAMESPACE

#else //!__NDEBUG_THROW

#define __THROWPT( _type )

#endif //!__NDEBUG_THROW


#endif //__DBGTHRW_H
