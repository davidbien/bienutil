#ifndef __DBGTHRW_H
#define __DBGTHRW_H

#ifndef __NDEBUG_THROW

#include <slist.h>
#include <map>
#include <ostream.h>
#include "bienutil/_namdexc.h"

// _dbgthrw.h

// Objects for testing throw-safety in a repeatable manner.

#ifndef __DBGTHROW_DEFAULT_ALLOCATOR
#ifndef NDEBUG
#define __DBGTHROW_DEFAULT_ALLOCATOR std::__allocator< char, std::malloc_alloc >
#else !NDEBUG
#define __DBGTHROW_DEFAULT_ALLOCATOR std::allocator< char >
#endif !NDEBUG
#endif !__DBGTHROW_DEFAULT_ALLOCATOR

__BIENUTIL_BEGIN_NAMESPACE

enum EThrowType
{
  e_ttMemory      = 0x00000001,
  e_ttFileOutput  = 0x00000002,
  e_ttFileInput   = 0x00000004,

  // This identifies areas in which "chronic" throwing would
  //  occur - i.e. indicating that the next throw point with
  //  this same type would also throw.
  e_ttChronicMask = e_ttFileOutput | e_ttMemory
};

class _debug_memory_except : public _t__Named_exception< __DBGTHROW_DEFAULT_ALLOCATOR >
{
  typedef _t__Named_exception< __DBGTHROW_DEFAULT_ALLOCATOR > _TyBase;
public:
  _debug_memory_except()
    : _TyBase( "_debug_memory_except" )
  {
  }
};
class _debug_output_except : public _t__Named_exception< __DBGTHROW_DEFAULT_ALLOCATOR >
{
  typedef _t__Named_exception< __DBGTHROW_DEFAULT_ALLOCATOR > _TyBase;
public:
  _debug_output_except()
    : _TyBase( "_debug_output_except" )
  {
  }
};
class _debug_input_except : public _t__Named_exception< __DBGTHROW_DEFAULT_ALLOCATOR >
{
  typedef _t__Named_exception< __DBGTHROW_DEFAULT_ALLOCATOR > _TyBase;
public:
  _debug_input_except()
    : _TyBase( "_debug_input_except" )
  {
  }
};

struct _throw_static_base;

struct _throw_object_base
{
private:
  typedef _throw_object_base _TyThis;
public:
  unsigned long m_rgttType;
  const char *  m_cpFileName;
  unsigned long m_ulLineNumber;

  _throw_object_base( unsigned long _rgttType, 
                      const char * _cpFileName,
                      unsigned long _ulLineNumber,
                      bool _fMaybeThrow = false ) :
    m_rgttType( _rgttType ),
    m_cpFileName( _cpFileName ),
    m_ulLineNumber( _ulLineNumber )
  {
    assert( m_rgttType );
    if ( _fMaybeThrow )
    {
      _maybe_throw();
    }
  }

  _throw_object_base()
  {
  }

  static _throw_static_base ms_tsb;

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
  void  _maybe_throw();
};              

struct _throw_object_with_throw_rate
  : public _throw_object_base
{
  _throw_object_with_throw_rate( _throw_object_base & _r )
    : _throw_object_base( _r )
  {
  }
  _throw_object_with_throw_rate()
  {
  }

  int   m_iThrowRate;
  bool  m_fHitOnce;
};

struct _throw_hit_stats
{
  unsigned  m_uPossible;
  unsigned  m_uHit;
};

__INLINE size_t
_count_set_bits( int _i )
{
  int iSet = 0;
  while( _i )
  {
    _i &= _i-1;
    ++iSet;
  }
  return iSet;
}

struct _throw_static_base
{
  bool          m_fOn;
  unsigned int  m_uRandSeed;
  int           m_iThrowRate; 
    // A number less than RAND_MAX that determines whether a given throw object will throw.

  unsigned long m_rgttTypeAccum;
    // This accumulates the type of exceptions thrown.

  // Current throw parameters:
  unsigned long m_rgttTypeCur;
  const char *  m_cpFileNameCur;
  unsigned long m_ulLineNumberCur;

  // Save a set of previous throw parameters for utility:
  static const int  ms_kiNumSaved = 200;
  _throw_object_base  m_rgSaved[ ms_kiNumSaved ];

  unsigned      m_uNumThrows;
  int           m_iThrowOneOnly;

  // Determine whether we have exhausted the throw points:
  typedef map<  _throw_object_base, _throw_hit_stats, 
                less<_throw_object_base>,
                __allocator< char, malloc_alloc > >   _TyMapHitThrows;
  _TyMapHitThrows m_mapHitThrows;
  unsigned      m_uHitThrows;

  // Allow the programmer to set in a sorted array of file/line throw points
  //  with special throwing requirements:
  _throw_object_with_throw_rate * m_ptobtrStart;
  _throw_object_with_throw_rate * m_ptobtrEnd;
  
  _throw_static_base()
    : m_fOn( false ),
      m_uRandSeed( 0 ),
      m_iThrowRate( 50 ),
      m_rgttTypeAccum( 0 ),
      m_uNumThrows( 0 ),
      m_iThrowOneOnly( -1 ),
      m_uHitThrows( 0 ),
      m_ptobtrStart( 0 )
  {
  }

  void  set_on( bool _fOn )
  {
    m_fOn = _fOn;
  }

  void  set_seed( unsigned int _uRandSeed )
  {
    m_uRandSeed = _uRandSeed;
    srand( m_uRandSeed );
  }

  void  set_throw_rate( int _iThrowRate )
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

  void  get_hit_stats( unsigned & ruHit, unsigned & ruPossible )
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
          _ptobtr->m_iThrowRate = 1;
        }
      }
    }
  }

  void  _set_params( _throw_object_base * _ptob )
  {
    m_cpFileNameCur = _ptob->m_cpFileName;
    m_ulLineNumberCur = _ptob->m_ulLineNumber;
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

  void  _maybe_throw( _throw_object_base * _ptob )
  {
    if ( !m_fOn )
      return;

    // Ensure that this throw object is in the map:
    _throw_hit_stats  ths = { 0, 0 };
    _TyMapHitThrows::value_type vtHit( *_ptob, ths );
    pair< _TyMapHitThrows::iterator, bool > pib = m_mapHitThrows.insert( vtHit );
    pib.first->second.m_uPossible++;

    if ( e_ttChronicMask & m_rgttTypeAccum & _ptob->m_rgttType )
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
              iThrowRate = 1;
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
      if ( m_uRandSeed && ( 0 == ( rand() % iThrowRate ) ) )
      {
        _set_params( _ptob );
        // If we have multiple types then choose one:
        m_rgttTypeCur = _ptob->m_rgttType;

        int  iSetBits;
        if ( 1 < ( iSetBits = _count_set_bits( m_rgttTypeCur ) ) )
        {
          // Choose a random bit among those present:
          int _iR = rand() % iSetBits;

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

        _throw();
      }
    }
  }
};

__INLINE void 
_throw_object_base::_maybe_throw()
{
  ms_tsb._maybe_throw( this );
}

#define __THROWPT( _type )  _throw_object_base( _type, __FILE__, __LINE__, true );

__BIENUTIL_END_NAMESPACE

#else !__NDEBUG_THROW

#define __THROWPT( _type )

#endif !__NDEBUG_THROW


#endif __DBGTHRW_H
