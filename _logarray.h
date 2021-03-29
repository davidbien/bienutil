#pragma once

// _logarray.h
// Logarithmic array.
// dbien
// 28MAR2021

// The idea here is to create a segmented array (similar to segarray) that instead has a variable block size
//  which starts at some power of two and ends at another power of two. The final power of two block size is then repeated
//  as the array grows. This allows constant time calculation of block size and reasonably easy loops to write.
// My ideas for uses of this are for arrays of object that are generally very small, but you want to allow to be very
//  large in more rare situations. Ideally then you don't want to (as well) change this thing after it is created.
// When initially created the object does not have an array of pointers, just a single pointer to a block of size 2^t_knPow2Min.
// My current plan for usage for this is as the underlying data structure for the lexang _l_data.h, _l_data<> template.

__BIENUTIL_BEGIN_NAMESPACE

inline constexpr bool KMultiplyTestOverflow( uint64_t _u64L, uint64_t _u64R, uint64_t & _ru64Result )
{
  _ru64Result = _u64L * _u64R;
  return ( _ru64Result < _u64L ) || ( _ru64Result < _u64R );
}

// Can't use intrinsics since we want this to be constexpr. 
// Also we will throw on overflow so that the compile will fail during constexpr evaluation.
inline constexpr uint64_t KUPow( uint64_t _u64Base, uint8_t _u8Exp )
{
  uint64_t u64Result = 1;
  switch( KMSBitSet( _u8Exp ) )
  {
    default:
      THROWNAMEDEXCEPTION("Guaranteed overflow _u64Base > 1.");
    break;
    case 6:
      if ( ( 1u & _u8Exp ) && 
           ( KMultiplyTestOverflow( u64Result, _u64Base, u64Result ) ||
             KMultiplyTestOverflow( _u64Base, _u64Base, _u64Base ) ) )
        THROWNAMEDEXCEPTION("Overflow u64Result.");
      _u8Exp >>= 1;
      [[fallthrough]];
    case 5:
      if ( ( 1u & _u8Exp ) && 
           ( KMultiplyTestOverflow( u64Result, _u64Base, u64Result ) ||
             KMultiplyTestOverflow( _u64Base, _u64Base, _u64Base ) ) )
        THROWNAMEDEXCEPTION("Overflow u64Result.");
      _u8Exp >>= 1;
      [[fallthrough]];
    case 4:
      if ( ( 1u & _u8Exp ) && 
           ( KMultiplyTestOverflow( u64Result, _u64Base, u64Result ) ||
             KMultiplyTestOverflow( _u64Base, _u64Base, _u64Base ) ) )
        THROWNAMEDEXCEPTION("Overflow u64Result.");
      _u8Exp >>= 1;
      [[fallthrough]];
    case 3:
      if ( ( 1u & _u8Exp ) && 
           ( KMultiplyTestOverflow( u64Result, _u64Base, u64Result ) ||
             KMultiplyTestOverflow( _u64Base, _u64Base, _u64Base ) ) )
        THROWNAMEDEXCEPTION("Overflow u64Result.");
      _u8Exp >>= 1;
      [[fallthrough]];
    case 2:
      if ( ( 1u & _u8Exp ) && 
           ( KMultiplyTestOverflow( u64Result, _u64Base, u64Result ) ||
             KMultiplyTestOverflow( _u64Base, _u64Base, _u64Base ) ) )
        THROWNAMEDEXCEPTION("Overflow u64Result.");
      _u8Exp >>= 1;
      [[fallthrough]];
    case 1:
      if ( ( 1u & _u8Exp ) && 
           KMultiplyTestOverflow( u64Result, _u64Base, u64Result ) )
        THROWNAMEDEXCEPTION("Overflow u64Result.");
    break;    
  }
  return u64Result;
}

// Range of block size are [2^t_knPow2Min, 2^t_knPow2Max].
// If t_TyFOwnLifetime is false_type then the contained object's lifetimes are not managed - i.e. they are assumed to have
//  no side effects upon destruction and are not destructed.
template < class t_TyT, class t_TyFOwnLifetime, size_t t_knPow2Min, size_t t_knPow2Max >
class LogArray
{
  typedef LogArray _TyThis;
public:
  static_assert( t_knPow2Max >= t_knPow2Min );
  typedef t_TyT _TyT;
  typedef t_TyFOwnLifetime _TyFOwnLifetime;
  static constexpr bool s_kfOwnObjectLifetime = _TyFOwnLifetime::value;
  static constexpr size_t s_knPow2Min = t_knPow2Min;
  static constexpr size_t s_knPow2Max = t_knPow2Max;
  static constexpr size_t s_knElementsFixedBoundary = ( t_knPow2Min == t_knPow2Max ) ? 0 : ( ( 1 << t_knPow2Max ) - ( 1 << t_knPow2Min ) );
  static constexpr size_t s_knSingleBlockSizeLimit = ( 1 << t_knPow2Min );
  static constexpr size_t s_knAllocateBlockPtrsInBlocksOf = 4; // We allocate block pointers in blocks so that we don't have to reallocate every time - unless we decide to set this to one...


  LogArray() = default;
  LogArray( size_t _nElements )
  {
    SetSize( _nElements );
  }
  LogArray( LogArray const & _r )
  {
    // todo.
  }
  LogArray & operator =( LogArray const & _r )
  {
    _TyThis copy( _r )
    swap( copy );
    return *this;
  }
  void swap( _TyThis & _r )
  {
    std::swap( m_nElements, _r.m_nElements );
    std::swap( m_ptSingleBlock, _r.m_ptSingleBlock );
  }
  void AssertValid() const
  {
#if ASSERTSENABLED
    if ( ! m_ptSingleBlock )
    {
      Assert( !m_nElements ); // We can have a block without elements because insertion of the first object might fail after call to _PvAllocEnd().
    }
    else
    if ( m_nElements > s_knSingleBlockSizeLimit ) // nothing we can really assert about a single block element.
    {
      // We should have a block in every position up until the end:
      size_t nBlockEnd = _NBlockFromEl( m_nElements-1 ) + 1;
      
    }
#endif //!ASSERTSENABLED
  }
  void AssertValidRange( _tySizeType _posBegin, _tySizeType _posEnd ) const
  {
#if ASSERTSENABLED
    Assert( _posEnd >= _posBegin );
    Assert( _posEnd <= m_nElements );
#endif //!ASSERTSENABLED
  }

  template <class... t_tysArgs>
  _tyT &emplaceAtEnd(t_tysArgs &&... _args)
  {
    AssertValid();
    _tyT *pt = new (_PvAllocEnd()) _tyT(std::forward<t_tysArgs>(_args)...);
    Assert( m_nElements < 0 );
    m_nElements = -m_nElements; // element successfully constructed.
    return *pt;
  }
  // To do this correctly, this method must increment m_nElements - but there are some catch-22s.
  // So we increment and negate m_nElements to indicate that we have a "non-constructed tail" on the array.
  void *_PvAllocEnd()
  {
    if ( !m_ptSingleBlock )
    {
      Assert( !m_nElements );
      // Then we are always allocating just the first block:
      m_ptSingleBlock = (_TyT*)malloc( s_knSingleBlockSizeLimit * sizeof( _TyT ) );
      if ( !m_ptSingleBlock )
        ThrowOOMException();
      m_nElements = -1; // We have an allocated but non-constructed element at position 0.
      return m_ptSingleBlock;
    }
    size_t nBlockSize;
    if ( m_nElements < 0 )
    {
      // Then there is an allocated element at the end which threw during construction. Just use it:
      size_t nElInBlock = -m_nElements - 1;
      size_t nBlockNextEl = _NBlockFromEl( nElInBlock, nBlockSize );
      return _GetBlockEl( nBlockNextEl, nElInBlock, nBlockSize );
    }
    size_t nElInBlock = m_nElements;
    size_t nBlockNextEl = _NBlockFromEl( nElInBlock, nBlockSize );
    if ( ( s_knPow2Min > 0 ) && !nBlockNextEl ) // Can't get here unless first block is bigger than one.
    {
      m_nElements = -( m_nElements + 1 );
      return m_ptSingleBlock + nElInBlock; // boundary.
    }
    if ( ( 1 == nBlockNextEl ) && !nElInBlock )
    {
      // Transition from single block to multiple blocks:
      size_t nBlockPtrs = (max)( 2, s_knAllocateBlockPtrsInBlocksOf );
      _TyT ** pptBlockPtrs = (_TyT **)malloc( nBlockPtrs * sizeof( _TyT * ) );
      _TyT * ptNewBlock = (_TyT*)malloc( nBlockSize * sizeof( _TyT ) );
      if ( !pptBlockPtrs || !ptNewBlock )
      {
        free( pptBlockPtrs ); // calling free() on null has null effect.
        free( ptNewBlock );  // avoid leaks.
        ThrowOOMException();
      }
      *pptBlockPtrs = m_ptSingleBlock;
      pptBlockPtrs[1] = ptNewBlock;
      if ( s_knAllocateBlockPtrsInBlocksOf > 2 )
        memset( pptBlockPtrs + 2, 0, nBlockPtrs - 2 * sizeof( _TyT * ) );
      m_nElements = -( m_nElements + 1 );
      m_pptBlocksBegin = pptBlockPtrs; // We now own the malloc()'d memory.
      return ptNewBlock;
    }
    // General operation - this method is getting long... lol.
    if ( !nElInBlock )
    {
      // Then we are adding a new block - check to see if we need to add block pointers:
      _TyT ** pptNewBlockPtrs = nullptr;
      if ( !( nBlockNextEl % s_knAllocateBlockPtrsInBlocksOf ) )
      {
        // need to reallocate the block pointers - this is slightly tricky.
        size_t nNewBlocks = nBlockNextEl + s_knAllocateBlockPtrsInBlocksOf;
        pptNewBlockPtrs = (_TyT**)malloc( nNewBlocks * sizeof( _TyT * ) );
        if ( !pptNewBlockPtrs )
          ThrowOOMException();
        memcpy( pptNewBlockPtrs, m_pptBlocksBegin, nBlockNextEl * sizeof( _TyT * ) ); // copy in old data.
      }
      _TyT * ptNewBlock = (_TyT*)malloc( nBlockSize * sizeof( _TyT ) );
      if ( !ptNewBlock )
        ThrowOOMException();
      // Everything is allocated now - we won't throw for the rest of this method.
      if ( pptNewBlockPtrs )
      {
        if ( s_knAllocateBlockPtrsInBlocksOf > 1 )
          memset( pptNewBlockPtrs + nBlockNextEl + 1, 0, ( s_knAllocateBlockPtrsInBlocksOf - 1 ) * sizeof( _TyT * ) );
        m_pptBlocksBegin = pptNewBlockPtrs;
      }
      m_pptBlocksBegin[nBlockNextEl] = ptNewBlock;
    }
    m_nElements = -( m_nElements + 1 );
    return _GetBlockEl( nBlockNextEl, nElInBlock, nBlockSize );
  }

  // _rnEl returns as the nth element in size block <n>.
  size_t _NBlockFromEl( size_t & _rnEl, size_t & _rnBlockSize ) const
  {
    if ( _rnEl >= s_knElementsFixedBoundary )
    {
      _rnBlockSize = 1 << t_knPow2Max;
      _rnEl -= s_knElementsFixedBoundary;
      size_t nBlock = _rnEl / _rnBlockSize;
      _rnEl %= _rnBlockSize;
      return nBlock;
    }
    size_t nElShifted = _rnEl + ( 1 << t_knPow2Min ); // Must shift for both log2 use and potential non-zero t_knPow2Min. ( _rnEl+1 ) + ( ( 1 << t_knPow2Min ) -1 ).
    size_t nBlock = Log2( nElShifted );
    _rnBlockSize = 1 << nBlock;
    _rnEl -= ( _rnBlockSize - ( 1 << t_knPow2Min ) );
    nBlock -= t_knPow2Min;
    return nBlock;
  }


  void SetSize( size_t _nElements )
  {
    if ( s_kfOwnObjectLifetime )
    {
      // We do all allocation using _PvAllocEnd() in this case.
    }
    else
    {
      // We can manage the blocks more roughly.
    }
  }

protected:
  ssize_t m_nElements[0]; // If m_nElements < 0 then there is an uninitialized element at the end of the array.
  union
  {
    _TyT * m_ptSingleBlock{nullptr}; // When m_nElements <= 2^t_knPow2Min then we point at only this single block. Note that this requires that we compress when the elements fall below this threshold during removal.
    _TyT ** m_pptBlocksBegin; 
  };
};

__BIENUTIL_END_NAMESPACE
