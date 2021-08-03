#pragma once

// _logarray.h
// Logarithmic array. Kinda a misnomer but there ya are.
// dbien
// 28MAR2021

// The idea here is to create a segmented array (similar to segarray) that instead has a variable block size
//  which starts at some power of two and ends at another power of two. The final power of two block size is then repeated
//  as the array grows. This allows constant time calculation of block size and reasonably easy loops to write.
// My ideas for uses of this are for arrays of object that are generally very small, but you want to allow to be very
//  large in more rare situations. Ideally then you don't want to (as well) change this thing after it is created other than
//  to add and remove elements from the end.
// When initially created the object does not have an array of pointers, just a single pointer to a block of size 2^t_knPow2Min.
// My current plan for usage for this is as the underlying data structure for the lexang _l_data.h, _l_data<> template.

#include "_bitutil.h"

__BIENUTIL_BEGIN_NAMESPACE

// Range of block size are [2^t_knPow2Min, 2^t_knPow2Max].
// Always manages the lifetimes of the elements.
template < class t_TyT, size_t t_knPow2Min, size_t t_knPow2Max >
class LogArray
{
  typedef LogArray _TyThis;
public:
  static_assert( t_knPow2Max >= t_knPow2Min );
  typedef t_TyT _TyT;
  static constexpr size_t s_knPow2Min = t_knPow2Min;
  static constexpr size_t s_knPow2Max = t_knPow2Max;
  static constexpr size_t s_knElementsFixedBoundary = ( ( 1ull << t_knPow2Max ) - ( 1ull << t_knPow2Min ) );
  static constexpr size_t s_knBlockFixedBoundary = t_knPow2Max - t_knPow2Min;
  static constexpr size_t s_knSingleBlockSizeLimit = ( 1ull << t_knPow2Min );
  static constexpr size_t s_knAllocateBlockPtrsInBlocksOf = 4; // We allocate block pointers in blocks so that we don't have to reallocate every time - unless we decide to set this to one...

  ~LogArray()
  {
    AssertValid();
    if ( m_nElements )
      _Clear();
  }
  // If the destructor of _TyT cannot throw (or that's what it at least claims) then we can be cavalier about
  //  our approach here:
  LogArray() = default;
  LogArray( size_t _nElements )
  {
    SetSize( _nElements );
  }
  // The way we construct we can copy any LogArray.
  LogArray( LogArray const & _r )
  {
    // Piecewise copy construct.
    _r.ApplyContiguous( 0, _r.NElements(), 
      [this]( const _TyT * _ptBegin, const _TyT * _ptEnd )
      {
        for ( const _TyT * ptCur = _ptBegin; _ptEnd != ptCur; ++ptCur )
          emplaceAtEnd( *ptCur );
      }
    );
  }
  LogArray & operator =( LogArray const & _r )
  {
    _TyThis copy( _r );
    swap( copy );
    return *this;
  }
  // The way we construct we can copy any LogArray.
  template < size_t t_knPow2MinOther, size_t t_knPow2MaxOther >
  LogArray( LogArray< _TyT, t_knPow2MinOther, t_knPow2MaxOther > const & _r )
  {
    // Piecewise copy construct.
    _r.ApplyContiguous( 0, _r.NElements(), 
      [this]( const _TyT * _ptBegin, const _TyT * _ptEnd )
      {
        for ( const _TyT * ptCur = _ptBegin; _ptEnd != ptCur; ++ptCur )
          emplaceAtEnd( *ptCur );
      }
    );
  }
  template < size_t t_knPow2MinOther, size_t t_knPow2MaxOther >
  LogArray & operator =( LogArray< _TyT, t_knPow2MinOther, t_knPow2MaxOther > const & _r )
  {
    _TyThis copy( _r );
    swap( copy );
    return *this;
  }
  LogArray( LogArray && _rr )
  {
    swap( _rr );
  }
  LogArray & operator = ( LogArray && _rr )
  {
    _TyThis acquire( std::move( _rr ) );
    swap( acquire );
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
    static bool s_fInAssertValid = false;
    if ( !s_fInAssertValid )
    {
      s_fInAssertValid = true;
      try
      {
        Assert( !m_nElements == !m_ptSingleBlock );
        if ( !_FHasSingleBlock() ) // nothing we can really assert about a single block element.
        {
          // Test that:
          // 1) We have a non-null block ptr everywhere one should be.
          // 2) We have a null block ptr everywhere one should be.
          size_t nElInBlock = NElementsAllocated() - 1;
          size_t nSizeBlock;
          const size_t knBlock = _NBlockFromEl( nElInBlock, nSizeBlock );
          size_t nBlockPtrs = (knBlock+1) + ( s_knAllocateBlockPtrsInBlocksOf - (knBlock+1) ) % s_knAllocateBlockPtrsInBlocksOf;
          size_t nBlockCur = 0;
          for ( ; nBlockCur <= knBlock; ++nBlockCur )
            Assert( !!m_pptBlocksBegin[nBlockCur] );
          for ( ; nBlockCur < nBlockPtrs; ++nBlockCur )
            Assert( !m_pptBlocksBegin[nBlockCur] );
          // That's about all we can do.
        }
      }
      catch( exception const & )
      {
        s_fInAssertValid = false;
        throw;
      }
      s_fInAssertValid = false;
    }
#endif //!ASSERTSENABLED
  }
  bool FIsValidRange( size_t _posBegin, size_t _posEnd ) const
  {
    return ( ( _posEnd >= _posBegin ) && ( _posEnd <= NElements() ) );
  }
  void AssertValidRange( size_t _posBegin, size_t _posEnd ) const
  {
#if ASSERTSENABLED
    Assert( FIsValidRange( _posBegin, _posEnd ) );
#endif //!ASSERTSENABLED
  }
  void VerifyValidRange( size_t _posBegin, size_t _posEnd ) const
  {
    VerifyThrow( FIsValidRange( _posBegin, _posEnd ) );
  }
  size_t NElements() const
  {
    AssertValid();
    return m_nElements < 0 ? ( -m_nElements - 1 ) : m_nElements;
  }
  size_t GetSize() const
  {
    return NElements();
  }
  size_t NElementsAllocated() const
  {
    AssertValid();
    return m_nElements < 0 ? -m_nElements : m_nElements;
  }
  template <class... t_TysArgs>
  _TyT &emplaceAtEnd(t_TysArgs &&... _args)
  {
    AssertValid();
    _TyT *pt = new (_PvAllocEnd()) _TyT(std::forward<t_TysArgs>(_args)...);
    Assert( m_nElements < 0 );
    m_nElements = -m_nElements; // element successfully constructed.
    return *pt;
  }
  bool _FHasSingleBlock() const
  {
    return NElementsAllocated() <= s_knSingleBlockSizeLimit;
  }
  _TyT & RTail()
  {
    VerifyThrow( NElements() );
    return ElGet( NElements() - 1 );
  }
  const _TyT & RTail() const
  {
    VerifyThrow( NElements() );
    return ElGet( NElements() - 1 );
  }
  _TyT & ElGet( size_t _nEl )
  {
    VerifyThrow( _nEl < NElements() );
    if ( _FHasSingleBlock() )
      return m_ptSingleBlock[_nEl];
    size_t nElInBlock = _nEl;
    size_t nSizeBlock;
    const size_t knBlock = _NBlockFromEl( nElInBlock, nSizeBlock );
    return m_pptBlocksBegin[knBlock][nElInBlock];
  }
  const _TyT & ElGet( size_t _nEl ) const
  {
    return const_cast< _TyThis *>( this )->ElGet(_nEl);
  }
  _TyT & operator []( size_t _nEl )
  {
    return ElGet( _nEl );
  }
  const _TyT & operator []( size_t _nEl ) const
  {
    return ElGet( _nEl );
  }
  void Clear() noexcept
  {
    if ( m_ptSingleBlock )
    {
      _Clear();
      m_ptSingleBlock = nullptr;
      m_nElements = 0;
    }
    AssertValid();
  }
  template <class... t_TysArgs>
  void SetSize( size_t _nElements, t_TysArgs &&... _args )
  {
    AssertValid();
    size_t nElements = NElements();
    if ( _nElements < nElements )
      _SetSizeSmaller( _nElements );
    else
    if ( _nElements > nElements )
      _SetSizeLarger( _nElements, std::forward<t_TysArgs>(_args)... );
  }
  void SetSizeSmaller( size_t _nElements )
  {
    VerifyThrowSz( _nElements < NElements(), "Size is not smaller." );
    _SetSizeSmaller( _nElements );
  }
// Element removal:
  // Remove _nEls starting at _nPos.
  // This is the version where the move assignment cannot throw.
  void Remove( size_t _nPos, size_t _nEls ) noexcept( std::is_nothrow_destructible_v< _TyT > )
    requires ( std::is_nothrow_move_assignable_v< _TyT > )
  {
    // std::move() each element into its new place...
    const size_t nElements = NElements();
    Assert( _nPos <= nElements );
    if ( !_nEls )
      return; // allow.
    Assert( _nPos + _nEls <= nElements );
    VerifyThrowSz( _nPos + _nEls <= nElements, "Range of elements to be removed extends beyond current array size." );
    // Just move each element into place - note that we still must delete the moved elements as they may have just been copied
    //  if there is no move assigment operator.
    for ( size_t nCur = _nPos + _nEls; nElements != nCur; ++nCur )
      ElGet( nCur - _nEls ) = std::move( ElGet( nCur ) ); // can't throw.
    _SetSizeSmaller( nElements - _nEls ); // might throw.
  }
  // This version is the same except that it always might throw.
  // We don't try to protect the movement against throwing since there isn't much to be done about it.
  // In particular if a movement throws then that's the end of this method and the size won't get smaller.
  void Remove( size_t _nPos, size_t _nEls ) noexcept( false )
    requires ( !std::is_nothrow_move_assignable_v< _TyT > )
  {
    // std::move() each element into its new place...
    const size_t nElements = NElements();
    Assert( _nPos <= nElements );
    if ( !_nEls )
      return; // allow.
    Assert( _nPos + _nEls <= nElements );
    VerifyThrowSz( _nPos + _nEls <= nElements, "Range of elements to be removed extends beyond current array size." );
    // Just move each element into place - note that we still must delete the moved elements as they may have just been copied
    //  if there is no move assigment operator.
    for ( size_t nCur = _nPos + _nEls; nElements != nCur; ++nCur )
      ElGet( nCur - _nEls ) = std::move( ElGet( nCur ) ); // might throw.
    _SetSizeSmaller( nElements - _nEls ); // might throw.
  }
  // This method efficiently removes elements as indicated by bits set in the passed _rbv.
  // This one might not throw.
  template < class t_tyBitVector >
  void RemoveBvElements( t_tyBitVector const & _rbv ) noexcept( std::is_nothrow_destructible_v< _TyT > )
    requires ( std::is_nothrow_move_assignable_v< _TyT > )
  {
    VerifyThrowSz( _rbv.size() == NElements(), "Algorithm requires that size of bit vector equals number of elements." );
    size_t nCur = _rbv.getfirstset();
    const size_t nElements = NElements();
    if ( nCur == nElements )
      return; // noop.
    size_t nCurWrite = nCur;
    size_t nElsRemoved = 0;
    size_t nNotSet = _rbv.getnextnotset( nCur );
    for ( ; nNotSet != nElements; )
    {
      nCur = _rbv.getnextset( nNotSet );
      nElsRemoved += nNotSet - nCur;
      for ( ; nNotSet != nCur; ++nNotSet, ++nCurWrite )
        ElGet( nCurWrite ) = std::move( ElGet( nNotSet ) );
      nNotSet = _rbv.getnextnotset( nCur );
    }
    _SetSizeSmaller( nElements - nElsRemoved ); // might throw
  }
  // This one always can throw.
  template < class t_tyBitVector >
  void RemoveBvElements( t_tyBitVector const & _rbv ) noexcept( false )
    requires ( !std::is_nothrow_move_assignable_v< _TyT > )
  {
    if ( !_rbv.size() )
      return; // noop
    VerifyThrowSz( _rbv.size() == NElements(), "Algorithm requires that size of bit vector equals number of elements." );
    size_t nCur = _rbv.getfirstset();
    const size_t nElements = NElements();
    if ( nCur == nElements )
      return; // noop.
    size_t nCurWrite = nCur;
    size_t nElsRemoved = 0;
    size_t nNotSet = _rbv.getnextnotset( nCur );
    for ( ; nNotSet != nElements; )
    {
      nCur = _rbv.getnextset( nNotSet );
      nElsRemoved += nNotSet - nCur;
      for ( ; nNotSet != nCur; ++nNotSet, ++nCurWrite )
        ElGet( nCurWrite ) = std::move( ElGet( nNotSet ) );
      nNotSet = _rbv.getnextnotset( nCur );
    }
    _SetSizeSmaller( nElements - nElsRemoved ); // might throw
  }

  // call _rrftor with sets of contiguous blocks spanning [_posBegin,_posEnd) in forward order.
  template < class t_TyFunctor >
  void ApplyContiguous( size_t _posBegin, size_t _posEnd, t_TyFunctor && _rrftor )
  {
    AssertValid();
    if ( _posEnd == _posBegin )
      return;
    VerifyValidRange( _posBegin, _posEnd );
    // specially code the single block scenario:
    if ( _FHasSingleBlock() )
      return std::forward< t_TyFunctor >( _rrftor )( m_ptSingleBlock + _posBegin, m_ptSingleBlock + _posEnd );
    size_t nElInBlockTail = _posEnd-1;
    size_t nSizeBlockTail;
    const size_t knBlockTail = _NBlockFromEl( nElInBlockTail, nSizeBlockTail );
    size_t nElInBlockBegin = _posBegin;
    size_t nSizeBlockBegin;
    size_t nBlockBegin = _NBlockFromEl( nElInBlockBegin, nSizeBlockBegin );
    // We do the tail element specially:
    for ( size_t nBlockCur = nBlockBegin; knBlockTail != nBlockCur; ++nBlockCur, ( nElInBlockBegin = 0 ) )
    {
      _TyT * ptBlockCur = m_pptBlocksBegin[nBlockCur];
      std::forward< t_TyFunctor >( _rrftor )( ptBlockCur + nElInBlockBegin, ptBlockCur + nSizeBlockBegin );
      if ( nBlockCur < s_knBlockFixedBoundary )
        nSizeBlockBegin <<= 1ull; // double the block size before the boundary.
    }
    // Now the tail element:
    _TyT * ptBlockTail = m_pptBlocksBegin[knBlockTail];
    std::forward< t_TyFunctor >( _rrftor )( ptBlockTail + nElInBlockBegin, ptBlockTail + nElInBlockTail + 1 );
  }
  // Const version.
  template < class t_TyFunctor >
  void ApplyContiguous( size_t _posBegin, size_t _posEnd, t_TyFunctor && _rrftor ) const
  {
    AssertValid();
    if ( _posEnd == _posBegin )
      return;
    VerifyValidRange( _posBegin, _posEnd );
    // specially code the single block scenario:
    if ( _FHasSingleBlock() )
      return std::forward< t_TyFunctor >( _rrftor )( m_ptSingleBlock + _posBegin, m_ptSingleBlock + _posEnd );
    size_t nElInBlockTail = _posEnd-1;
    size_t nSizeBlockTail;
    const size_t knBlockTail = _NBlockFromEl( nElInBlockTail, nSizeBlockTail );
    size_t nElInBlockBegin = _posBegin;
    size_t nSizeBlockBegin;
    size_t nBlockBegin = _NBlockFromEl( nElInBlockBegin, nSizeBlockBegin );
    // We do the tail element specially:
    for ( size_t nBlockCur = nBlockBegin; knBlockTail != nBlockCur; ++nBlockCur, ( nElInBlockBegin = 0 ) )
    {
      const _TyT * ptBlockCur = m_pptBlocksBegin[nBlockCur];
      std::forward< t_TyFunctor >( _rrftor )( ptBlockCur + nElInBlockBegin, ptBlockCur + nSizeBlockBegin );
      if ( nBlockCur < s_knBlockFixedBoundary )
        nSizeBlockBegin <<= 1ull; // double the block size before the boundary.
    }
    // Now the tail element:
    const _TyT * ptBlockTail = m_pptBlocksBegin[knBlockTail];
    std::forward< t_TyFunctor >( _rrftor )( ptBlockTail + nElInBlockBegin, ptBlockTail + nElInBlockTail + 1 );
  }
  // As above but the _apply object's operator() returns the count of applications.
  // If the count is less than the full contiguous region supplied then the iteration is aborted.
  // The return value is the total of the return values from all calls to _rrftor() performed.
  template < class t_TyFunctor >
  size_t NApplyContiguous( size_t _posBegin, size_t _posEnd, t_TyFunctor && _rrftor )
  {
    AssertValid();
    if ( _posEnd == _posBegin )
      return 0;
    VerifyValidRange( _posBegin, _posEnd );
    // specially code the single block scenario:
    if ( _FHasSingleBlock() )
      return std::forward< t_TyFunctor >( _rrftor )( m_ptSingleBlock + _posBegin, m_ptSingleBlock + _posEnd );
    size_t nElInBlockTail = _posEnd-1;
    size_t nSizeBlockTail;
    const size_t knBlockTail = _NBlockFromEl( nElInBlockTail, nSizeBlockTail );
    size_t nElInBlockBegin = _posBegin;
    size_t nSizeBlockBegin;
    size_t nBlockBegin = _NBlockFromEl( nElInBlockBegin, nSizeBlockBegin );
    // We do the tail element specially:
    size_t nApplied = 0;
    for ( size_t nBlockCur = nBlockBegin; knBlockTail != nBlockCur; ++nBlockCur, ( nElInBlockBegin = 0 ) )
    {
      _TyT * ptBlockCur = m_pptBlocksBegin[nBlockCur];
      size_t nAppliedCur = std::forward< t_TyFunctor >( _rrftor )( ptBlockCur + nElInBlockBegin, ptBlockCur + nSizeBlockBegin );
      nApplied += nAppliedCur;
      if ( nAppliedCur != ( nSizeBlockBegin - nElInBlockBegin ) )
        return nApplied;
      if ( nBlockCur < s_knBlockFixedBoundary )
        nSizeBlockBegin <<= 1ull; // double the block size before the boundary.
    }
    // Now the tail element:
    _TyT * ptBlockTail = m_pptBlocksBegin[knBlockTail];
    return std::forward< t_TyFunctor >( _rrftor )( ptBlockTail + nElInBlockBegin, ptBlockTail + nElInBlockTail + 1 ) + nApplied;
  }
  template < class t_TyFunctor >
  size_t NApplyContiguous( size_t _posBegin, size_t _posEnd, t_TyFunctor && _rrftor ) const
  {
    AssertValid();
    if ( _posEnd == _posBegin )
      return 0;
    VerifyValidRange( _posBegin, _posEnd );
    // specially code the single block scenario:
    if ( _FHasSingleBlock() )
      return std::forward< t_TyFunctor >( _rrftor )( m_ptSingleBlock + _posBegin, m_ptSingleBlock + _posEnd );
    size_t nElInBlockTail = _posEnd-1;
    size_t nSizeBlockTail;
    const size_t knBlockTail = _NBlockFromEl( nElInBlockTail, nSizeBlockTail );
    size_t nElInBlockBegin = _posBegin;
    size_t nSizeBlockBegin;
    size_t nBlockBegin = _NBlockFromEl( nElInBlockBegin, nSizeBlockBegin );
    // We do the tail element specially:
    size_t nApplied = 0;
    for ( size_t nBlockCur = nBlockBegin; knBlockTail != nBlockCur; ++nBlockCur, ( nElInBlockBegin = 0 ) )
    {
      const _TyT * ptBlockCur = m_pptBlocksBegin[nBlockCur];
      size_t nAppliedCur = std::forward< t_TyFunctor >( _rrftor )( ptBlockCur + nElInBlockBegin, ptBlockCur + nSizeBlockBegin );
      nApplied += nAppliedCur;
      if ( nAppliedCur != ( nSizeBlockBegin - nElInBlockBegin ) )
        return nApplied;
      if ( nBlockCur < s_knBlockFixedBoundary )
        nSizeBlockBegin <<= 1ull; // double the block size before the boundary.
    }
    // Now the tail element:
    const _TyT * ptBlockTail = m_pptBlocksBegin[knBlockTail];
    return std::forward< t_TyFunctor >( _rrftor )( ptBlockTail + nElInBlockBegin, ptBlockTail + nElInBlockTail + 1 ) + nApplied;
  }
protected:
  static size_t _Log2( size_t _n )
  {
    return MSBitSet( _n );
  }
  // _rnEl returns as the nth element in size block <n>.
  static size_t _NBlockFromEl( size_t & _rnEl, size_t & _rnBlockSize )
  {
    if ( _rnEl >= s_knElementsFixedBoundary )
    {
      _rnBlockSize = 1ull << t_knPow2Max;
      _rnEl -= s_knElementsFixedBoundary;
      size_t nBlock = s_knBlockFixedBoundary + _rnEl / _rnBlockSize;
      _rnEl %= _rnBlockSize;
      return nBlock;
    }
    size_t nElShifted = _rnEl + ( 1ull << t_knPow2Min ); // Must shift for both log2 use and potential non-zero t_knPow2Min. ( _rnEl+1 ) + ( ( 1ull << t_knPow2Min ) -1 ).
    size_t nBlock = _Log2( nElShifted );
    _rnBlockSize = 1ull << nBlock;
    _rnEl -= ( _rnBlockSize - ( 1ull << t_knPow2Min ) );
    nBlock -= t_knPow2Min;
    return nBlock;
  }
  template <class... t_TysArgs>
  void _SetSizeLarger( size_t _nElements, t_TysArgs &&... _args )
  {
    size_t nElements = NElements();
    Assert( nElements < _nElements );
    size_t nNewElements = _nElements - nElements;
    while ( nNewElements-- )
    {
      new ( _PvAllocEnd() ) _TyT(std::forward<t_TysArgs>(_args)...);
      Assert( m_nElements < 0 );
      m_nElements = -m_nElements;
    }
    AssertValid();
  }
  _TyT * _GetBlockEl( size_t _nBlock, size_t _nElInBlock )
  {
    if ( _FHasSingleBlock() )
      return m_ptSingleBlock + _nElInBlock;
    else
      return m_pptBlocksBegin[_nBlock] + _nElInBlock;
  }
public: // Allow access for testing.
  // To do this correctly, this method must increment m_nElements - but there are some catch-22s.
  // So we increment and negate m_nElements to indicate that we have a "non-constructed tail" on the array.
  void * _PvAllocEnd()
  {
    if ( !m_ptSingleBlock )
    {
      Assert( !m_nElements );
      // Then we are always allocating just the first block:
      m_ptSingleBlock = (_TyT*)malloc( s_knSingleBlockSizeLimit * sizeof( _TyT ) );
      if ( !m_ptSingleBlock )
        THROWNAMEDBADALLOC( "" );
      m_nElements = -1;
      return m_ptSingleBlock;
    }
    size_t nBlockSize;
    if ( m_nElements < 0 )
    {
      // Then there is an allocated element at the end which threw during construction. Just use it:
      size_t nElInBlock = -m_nElements - 1;
      size_t nBlockNextEl = _NBlockFromEl( nElInBlock, nBlockSize );
      return _GetBlockEl( nBlockNextEl, nElInBlock );
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
      size_t nBlockPtrs = (max)( size_t(2), s_knAllocateBlockPtrsInBlocksOf );
      _TyT ** pptBlockPtrs = (_TyT **)malloc( nBlockPtrs * sizeof( _TyT * ) );
      _TyT * ptNewBlock = (_TyT*)malloc( nBlockSize * sizeof( _TyT ) );
      if ( !pptBlockPtrs || !ptNewBlock )
      {
        ::free( pptBlockPtrs ); // calling free() on null has null effect.
        ::free( ptNewBlock );  // avoid leaks.
        THROWNAMEDBADALLOC( "" );
      }
      *pptBlockPtrs = m_ptSingleBlock;
      pptBlockPtrs[1] = ptNewBlock;
      if ( s_knAllocateBlockPtrsInBlocksOf > 2 )
        memset( pptBlockPtrs + 2, 0, ( s_knAllocateBlockPtrsInBlocksOf - 2 ) * sizeof( _TyT * ) );
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
          THROWNAMEDBADALLOC( "" );
        memcpy( pptNewBlockPtrs, m_pptBlocksBegin, nBlockNextEl * sizeof( _TyT * ) ); // copy in old data.
      }
      _TyT * ptNewBlock = (_TyT*)malloc( nBlockSize * sizeof( _TyT ) );
      if ( !ptNewBlock )
        THROWNAMEDBADALLOC( "" );
      // Everything is allocated now - we won't throw for the rest of this method.
      if ( pptNewBlockPtrs )
      {
        if ( s_knAllocateBlockPtrsInBlocksOf > 1 )
          memset( pptNewBlockPtrs + nBlockNextEl + 1, 0, ( s_knAllocateBlockPtrsInBlocksOf - 1 ) * sizeof( _TyT * ) );
        std::swap( m_pptBlocksBegin, pptNewBlockPtrs );
        ::free( pptNewBlockPtrs ); // free the old pointers - duh.
      }
      m_pptBlocksBegin[nBlockNextEl] = ptNewBlock;
    }
    m_nElements = -( m_nElements + 1 );
    return _GetBlockEl( nBlockNextEl, nElInBlock );
  }
  protected:
  void _SetSizeSmaller( size_t _nElements )
  {
    Assert( _nElements < NElements() );
    // We know that the destructors won't throw so we can batch delete for speed.
    // We want to delete in reverse again.
    if ( !_nElements ) // boundary.
    {
      Clear();
      return;
    }
    // Find the endpoints and then delete all elements in between them+1, backwards:
    size_t nElInBlockTail = NElementsAllocated()-1;
    size_t nSizeBlockTail;
    size_t nBlockTail = _NBlockFromEl( nElInBlockTail, nSizeBlockTail );
    size_t nElInBlockNew = _nElements; // We want the point beyond the new last element for the beginning endpoint.
    size_t nSizeBlockNew;
    size_t nBlockNew = _NBlockFromEl( nElInBlockNew, nSizeBlockNew );
    size_t nBlocksNew = !nElInBlockNew ? nBlockNew : nBlockNew+1; //boundary.
    
    _TyT ** pptBlockPtrsNew;
    size_t nBlockPtrsOld, nBlockPtrsNew; // only valid when pptBlockPtrsNew != nullptr.
    if ( 1 == nBlocksNew )
      pptBlockPtrsNew = nullptr; // single block scenario.
    else
    {
      // We check now to find the size of the resultant set of required blocks and allocate it now to avoid any possibility of failure from here on out:
      nBlockPtrsOld = ( nBlockTail+1 ) + ( s_knAllocateBlockPtrsInBlocksOf - ( nBlockTail+1 ) ) % s_knAllocateBlockPtrsInBlocksOf;
      nBlockPtrsNew = nBlocksNew + ( s_knAllocateBlockPtrsInBlocksOf - nBlocksNew ) % s_knAllocateBlockPtrsInBlocksOf;
      if ( nBlockPtrsOld == nBlockPtrsNew )
        pptBlockPtrsNew = m_pptBlocksBegin; // note that this may be the single block - but we don't care.
      else
      {
        pptBlockPtrsNew = (_TyT**)malloc( nBlockPtrsNew * sizeof(_TyT*) );
        // memset( pptBlockPtrsNew, 0, nBlockPtrsNew * sizeof(_TyT*) ); - unnecessary - we are copying in the old one 
        if ( !pptBlockPtrsNew )
          THROWNAMEDBADALLOC( "" ); // The only reallocation that we have to do in this method and we haven't changed anything yet... good.
      }
    }
    // Now we start moving backwards and destructing and removing blocks as we go:
    if ( nBlockTail != nBlockNew )
    {
      for ( ; nBlockTail != nBlockNew;  )
      {
        if ( m_nElements < 0 )
          m_nElements = -m_nElements - 1; // nElInBlockTail is a size due there being an unconstructed tail element.
        else
          ++nElInBlockTail; // Turn it into a size.
        _TyT * ptBlock = m_pptBlocksBegin[nBlockTail];
        m_pptBlocksBegin[nBlockTail] = nullptr;
        _DestructContigRange( ptBlock, ptBlock + nElInBlockTail ); // nElInBlockTail may be 0 here if there was an unconstructed tail at the start of a block.
        ::free( ptBlock );
        if ( --nBlockTail < s_knBlockFixedBoundary )
          nSizeBlockTail >>= 1ull;
        nElInBlockTail = nSizeBlockTail-1; // We leave this at an index because we turn it into a size above and below.
      }
    }
    else
    {
      // We didn't go through the loop above which means the elements were in the same block to begin with:
      if ( m_nElements < 0 )
      {
        Assert( nElInBlockTail );
        --nElInBlockTail;
      }
      // Check for "single block at start" scenario here and short circuit:
      if ( !nBlockTail )
      {
        // Then we started with a single block (that must be able to contain more than one element):
        Assert( t_knPow2Min > 0 );
        Assert( nElInBlockNew < nElInBlockTail+1 ); // we must be destructing something or why would we be in this method?
        _DestructContigRange( m_ptSingleBlock + nElInBlockNew, m_ptSingleBlock + nElInBlockTail + 1 );
        Assert( nElInBlockNew == _nElements );
        m_nElements = _nElements;
        return;
      }
    }
    // Now we are on the last block:
    _TyT * ptBlockElNew = m_pptBlocksBegin[nBlockTail];
    _DestructContigRange( ptBlockElNew + nElInBlockNew, ptBlockElNew + nElInBlockTail + 1 );
    if ( !nElInBlockNew )
    {
      ::free( ptBlockElNew );
      m_pptBlocksBegin[nBlockTail] = nullptr;
    }
    // Set the count of elements and update the block pointers appropriately:
    m_nElements = _nElements;
    if ( !pptBlockPtrsNew )
    {
      pptBlockPtrsNew = m_pptBlocksBegin;
      m_ptSingleBlock = pptBlockPtrsNew[0];
      ::free( pptBlockPtrsNew );
    }
    else
    if ( pptBlockPtrsNew != m_pptBlocksBegin )
    {
      memcpy( pptBlockPtrsNew, m_pptBlocksBegin, nBlockPtrsNew * sizeof(_TyT*) );
      std::swap( pptBlockPtrsNew, m_pptBlocksBegin );
      ::free( pptBlockPtrsNew );
    }
    AssertValid(); // whew!
  }
  void _DestructEl( _TyT * _pt ) noexcept
    requires( is_nothrow_destructible_v< _TyT > )
  {
    _pt->~_TyT();
  }
  void _DestructEl( _TyT * _pt ) noexcept
    requires( !is_nothrow_destructible_v< _TyT > )
  {
    try
    {
      _pt->~_TyT();
    }
    catch ( std::exception const & _rexc )
    {
      // We could log this? It could fill up the log pretty quick as well.
      // For now we log it:
      LOGEXCEPTION( _rexc, "Element threw an exception in its destructor." );
    }
    catch( ... )
    {
    }
  }
  void _DestructContigRange( _TyT * const , _TyT * const ) noexcept
    requires( is_trivially_destructible_v< _TyT > )
  {
    // no-op.
  }
  void _DestructContigRange( _TyT * const _ptBegin, _TyT * const _ptEnd ) noexcept
    requires( !is_trivially_destructible_v< _TyT > )
  {
    // Destruct the range backwards.
    for ( _TyT * ptCur = _ptEnd; _ptBegin != ptCur--; )
      _DestructEl( ptCur );
  }
  void _Clear() noexcept
  {
    AssertValid();
    Assert( !!m_ptSingleBlock ); // shouldn't be here.
    // Move through and destroy eacn element. Destruct backwards just for fun.
    size_t nElementsDestruct = NElements();
    size_t nElementsAllocated = NElementsAllocated();
    if ( nElementsAllocated <= s_knSingleBlockSizeLimit )
    {
      _DestructContigRange( m_ptSingleBlock, m_ptSingleBlock + nElementsDestruct );
      // ::free( m_ptSingleBlock ); This is freed below in common code.
    }
    else
    {
      size_t nElementTailInBlock = nElementsAllocated-1;
      size_t nTailBlockSize;
      size_t nBlockTail = _NBlockFromEl( nElementTailInBlock, nTailBlockSize );
      
      if ( ( m_nElements < 0 ) && !nElementTailInBlock-- ) // sneaky post decrement for else case...
      {
        // The last block has no constructed elements - just free the block:
        ::free( m_pptBlocksBegin[ nBlockTail-- ] ); // no reason to zero this.
        if ( nBlockTail < s_knBlockFixedBoundary )
          nTailBlockSize >>= 1ull; // we may already be done here but we don't care.
        nElementTailInBlock = nTailBlockSize-1;
      }
      _TyT ** pptBlockPtrCur = m_pptBlocksBegin + nBlockTail + 1; // This starts pointing at the end of the blocks.
      for ( ; m_pptBlocksBegin != pptBlockPtrCur--; )
      {
        _TyT * ptBlockCur = *pptBlockPtrCur;
        _DestructContigRange( ptBlockCur, ptBlockCur + nElementTailInBlock + 1 );
        ::free( *pptBlockPtrCur );
        if ( --nBlockTail < s_knBlockFixedBoundary )
          nTailBlockSize >>= 1ull; // we may already be done here but we don't care.
        nElementTailInBlock = nTailBlockSize-1;
      }
    }
    ::free( m_pptBlocksBegin ); // This is either freeing the single block or the block of blocks.
  }

  ssize_t m_nElements{0}; // If m_nElements < 0 then there is an uninitialized element at the end of the array.
  union
  {
    _TyT * m_ptSingleBlock{nullptr}; // When m_nElements <= 2^t_knPow2Min then we point at only this single block. Note that this requires that we compress when the elements fall below this threshold during removal.
    _TyT ** m_pptBlocksBegin; 
  };
};

__BIENUTIL_END_NAMESPACE
