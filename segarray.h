#pragma once

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// segarray.h
// Segmented array.
// dbien: 20MAR2020
// Would like to templatize by allocator but I have to propagate it and it's annoying right now. Later.

// Predeclare.
#include <unicode/urename.h>
#include "_strutil.h"

__BIENUTIL_BEGIN_NAMESPACE

template <class t_tyT, class t_tyFOwnLifetime = std::false_type, class t_tySizeType = size_t>
class SegArrayView;

// REVIEW: <dbien>: We could break out a base from SegArray which would be agnostic of t_tyFOwnLifetime and allow
//   the SegArrayView to then be not templatized by t_tyFOwnLifetime. However this is a minor improvement and probably not worth the effort.

template <class t_tyT, class t_tyFOwnLifetime, class t_tySizeType = size_t>
class SegArray
{
  typedef SegArray _tyThis;

public:
  typedef t_tyFOwnLifetime _tyFOwnLifetime;
  static constexpr bool s_kfOwnLifetime = _tyFOwnLifetime::value;
  static constexpr bool s_kfNotOwnLifetime = !s_kfOwnLifetime;
  typedef t_tyT _tyT;
  typedef t_tySizeType _tySizeType;
  static_assert(!std::numeric_limits<_tySizeType>::is_signed);
  typedef typename std::make_signed<_tySizeType>::type _tySignedSizeType;
  static const _tySizeType s_knbySizeSegment;// = (std::max)(sizeof(t_tyT) * 16, size_t(4096));
 
  SegArray()
    : m_nbySizeSegment(s_knbySizeSegment - (s_knbySizeSegment % sizeof(_tyT))) // even number of t_tyT's.
  {
  }
  SegArray(_tySizeType _nbySizeSegment )
      : m_nbySizeSegment(_nbySizeSegment - (_nbySizeSegment % sizeof(_tyT))) // even number of t_tyT's.
  {
  }
  SegArray(SegArray const &_r)
      : m_nbySizeSegment(_r.m_nbySizeSegment)
  {
    _r.AssertValid();
    if (!!_r.m_ppbySegments)
    {
      uint8_t **ppbySegments = (uint8_t **)malloc((_r.m_ppbyEndSegments - _r.m_ppbySegments) * sizeof(uint8_t *));
      if (!ppbySegments)
        THROWNAMEDEXCEPTION("OOM for malloc(%lu).", (_r.m_ppbyEndSegments - _r.m_ppbySegments) * sizeof(uint8_t *));
      ns_bienutil::FreeVoid fvFreeSegments(ppbySegments);                                      // In case we throw below.
      memset(ppbySegments, 0, (_r.m_ppbyEndSegments - _r.m_ppbySegments) * sizeof(uint8_t *)); // not strictly necessary but nice and cheap.

      uint8_t **ppbyCurThis = ppbySegments;
      bool fAllocError;
      _tySizeType nElementsCopied = 0; // This is only used when s_kfOwnLifetime cuz otherwise we don't care.
      {                                //B
        // Copy all the full and partial segments.
        uint8_t **ppbyEndDataOther = _r._PpbyGetCurSegment();
        if ((ppbyEndDataOther != _r.m_ppbyEndSegments) && !!*ppbyEndDataOther)
          ++ppbyEndDataOther;
        uint8_t **ppbyCurOther = _r.m_ppbySegments;
        for (; ppbyCurOther != ppbyEndDataOther; ++ppbyCurOther, ++ppbyCurThis)
        {
          *ppbyCurThis = (uint8_t *)malloc(m_nbySizeSegment);
          if (!*ppbyCurThis)
            break; // We can't throw here because we have to clean up still.
          // If we own the object lifetime then we have to copy construct each object:
          if (s_kfOwnLifetime)
          {
            try
            {
              const _tyT *ptCurOther = (const _tyT *)*ppbyCurOther;
              const _tyT *const ptEndOther = ptCurOther + NElsPerSegment();
              _tyT *ptCurThis = (_tyT *)*ppbyCurThis;
              for (; ptEndOther != ptCurOther; ++ptCurOther, ++ptCurThis)
              {
                new (ptCurThis) _tyT(*ptCurOther);
                if (++nElementsCopied == _r.m_nElements)
                  break; // We have copied all the elements.
              }
            }
            catch (std::exception const &rexc)
            {
              n_SysLog::Log(eslmtError, "SegArray::SegArray(const&): Caught exception copy constructing an element [%s].", rexc.what());
              break; // Can't do anything about - we will clean up and throw a separate exception below.
            }
          }
          else // otherwise we can just memcpy.
          {
            _tySizeType nbySizeCopy = m_nbySizeSegment;
            if (ppbyCurOther + 1 == ppbyEndDataOther) // we are on the last segment.
            {
              _tySizeType nLeftOver = _r.m_nElements % NElsPerSegment();
              nbySizeCopy = nLeftOver * sizeof(_tyT);
            }
            memcpy(*ppbyCurThis, *ppbyCurOther, nbySizeCopy);
          }
        }
        fAllocError = (ppbyCurOther < ppbyEndDataOther);
      } //EB

      if (fAllocError)
      {
        // Then we need to free any non-null segment pointers allocated above:
        _tySizeType nElementsDestroyed = 0; // This is only used when s_kfOwnLifetime cuz otherwise we don't care.
        uint8_t **const ppbyEndThis = ppbyCurThis;
        ppbyCurThis = ppbySegments;
        for (; ppbyEndThis != ppbyCurThis; ++ppbyCurThis)
        {
          if (s_kfOwnLifetime && *ppbyCurThis)
          {
            _tyT *ptCurThis = (_tyT *)*ppbyCurThis;
            const _tyT *const ptEndThis = ptCurThis + NElsPerSegment();
            for (; ptEndThis != ptCurThis; ++ptCurThis)
            {
              ptCurThis->~_tyT();
              if (++nElementsDestroyed == nElementsCopied)
                break; // We have destroyed all the elements.
            }
          }
          free(*ppbyCurThis); // might be 0 but free doesn't care.
        }
        THROWNAMEDEXCEPTION(s_kfOwnLifetime ? "OOM or exception copy constructing an element." : "SegArray::SegArray(const&): OOM."); // The ppbySegments block is freed upon throw.
      }
      // Success.
      m_ppbySegments = ppbySegments;
      (void)fvFreeSegments.PvTransfer();
      m_ppbyEndSegments = m_ppbySegments + (_r.m_ppbyEndSegments - _r.m_ppbySegments);
      m_nElements = _r.m_nElements;
    }
  }
  SegArray(SegArray &&_rr)
      : m_nbySizeSegment(_rr.m_nbySizeSegment) // Hopefully get a non-zero value from this object.
  {
    _rr.AssertValid();
    swap(_rr);
  }
  ~SegArray()
  {
    AssertValid();
    if (!!m_ppbySegments)
      _Clear();
  }
  void AssertValid() const
  {
#if ASSERTSENABLED
    Assert(!!m_nbySizeSegment && !(m_nbySizeSegment % sizeof(_tyT)));
    // We could do a little better here.
#endif //!ASSERTSENABLED
  }
  void AssertValidRange( _tySizeType _posBegin, _tySizeType _posEnd ) const
  {
#if ASSERTSENABLED
    Assert( _posEnd >= _posBegin );
    Assert( _posEnd <= m_nElements );
#endif //!ASSERTSENABLED
  }
  void Clear()
  {
    AssertValid();
    if (!!m_ppbySegments)
      _Clear();
  }
  void swap(SegArray &_r)
  {
    AssertValid();
    _r.AssertValid();
    std::swap(m_ppbySegments, _r.m_ppbySegments);
    std::swap(m_ppbyEndSegments, _r.m_ppbyEndSegments);
    std::swap(m_nElements, _r.m_nElements);
    std::swap(m_nbySizeSegment, _r.m_nbySizeSegment);
  }
  SegArray &operator=(const SegArray &_r)
  {
    SegArray saCopy(_r);
    swap(saCopy);
  }
  SegArray &operator=(SegArray &&_rr)
  {
    // Caller doesn't want _rr to have anything in it or they would have just called swap directly.
    // So Close() first.
    Clear();
    swap(_rr);
  }

  _tySizeType NElements() const
  {
    return m_nElements;
  }
  _tySizeType GetSize() const
  {
    return NElements();
  }
  _tySizeType NElsPerSegment() const
  {
    return m_nbySizeSegment / sizeof(_tyT);
  }
  bool FHasAnyCapacity() const
  {
    return ( m_ppbyEndSegments != m_ppbySegments ) && !!*m_ppbySegments;
  }

  _tyT &ElGet(_tySizeType _nEl, bool _fMaybeEnd = false)
  {
    AssertValid();
    Assert( !((_nEl > m_nElements) || (!_fMaybeEnd && (_nEl == m_nElements))) );
    if ((_nEl > m_nElements) || (!_fMaybeEnd && (_nEl == m_nElements)))
      THROWNAMEDEXCEPTION("Out of bounds _nEl[%lu] m_nElements[%lu].", _nEl, m_nElements);
    return ((_tyT *)m_ppbySegments[_nEl / NElsPerSegment()])[_nEl % NElsPerSegment()];
  }
  _tyT const &ElGet(_tySizeType _nEl) const
  {
    return const_cast<SegArray *>(this)->ElGet(_nEl);
  }
  _tyT &operator[](_tySizeType _n)
  {
    return ElGet(_n);
  }
  const _tyT &operator[](_tySizeType _n) const
  {
    return ElGet(_n);
  }

  template < class t_tyStringView, class t_tyDataRange >
  bool FGetStringView( t_tyStringView & _rsv, _tySizeType _posBegin, _tySizeType _posEnd ) const
    requires ( sizeof( typename t_tyStringView::value_type ) == sizeof( _tyT ) )
  {
    Assert( _rsv.empty() );
    if ( _posBegin == _posEnd )
      return true; // empty result.
    if ( _posBegin / NElsPerSegment() == ( _posEnd - 1 ) / NElsPerSegment() )
    {
      _rsv = t_tyStringView( (const typename t_tyStringView::value_type*)&ElGet( _posBegin ), _posEnd - _posBegin );
      return true;
    }
    return false;
  }
  template < class t_tyStringView, class t_tyDataRange >
  bool FGetStringView( t_tyStringView & _rsv, t_tyDataRange const & _rdr ) const
    requires ( sizeof( typename t_tyStringView::value_type ) == sizeof( _tyT ) )
  {
    return FGetStringView( _rdr.begin(), _rdr.end() );
  }
  void _CopyStringToBuf( _tySizeType _posBegin, _tySizeType _posEnd, _tyT * _ptBuf )
  {
    _tyT * ptCur = _ptBuf;
    ApplyContiguous( _posBegin, _posEnd, 
      [&ptCur]( const _tyT * _ptBegin, const _tyT * _ptEnd )
      {
        memcpy( ptCur, _ptBegin, ( _ptEnd - _ptBegin ) * sizeof(_tyT) );
        ptCur += ( _ptEnd - _ptBegin );
      }
    );
    Assert( ptCur == _ptBuf + ( _posEnd - _posBegin ) );
  }
  template < class t_tyString, class t_tyDataRange >
  void GetString( t_tyString & _rstr, _tySizeType _posBegin, _tySizeType _posEnd ) const
    requires ( sizeof( typename t_tyString::value_type ) == sizeof( _tyT ) ) // non-converting version.
  {
    Assert( _rstr.empty() );
    if ( _posBegin == _posEnd )
      return; // empty result.
    VerifyThrowSz( ( _posEnd >= _posBegin ) && ( _posEnd <= NElements() ), "_posBegin[%lu],_posEnd[%lu],NElements()[%lu]", uint64_t(_posBegin), uint64_t(_posEnd), uint64_t(NElements()) );
    _rstr.resize( _posEnd - _posBegin );
    _CopyStringToBuf( _posBegin, _posEnd, (_tyT*)&_rstr[0] );
    Assert( StrNLen( _rstr.c_str() ) == (_posEnd - _posBegin ) );
  }
  template < class t_tyString, class t_tyDataRange >
  void GetString( t_tyString & _rstr, _tySizeType _posBegin, _tySizeType _posEnd ) const
    requires ( sizeof( typename t_tyString::value_type ) != sizeof( _tyT ) ) // converting version.
  {
    Assert( _rstr.empty() );
    if ( _posBegin == _posEnd )
      return; // empty result.
    VerifyThrowSz( ( _posEnd >= _posBegin ) && ( _posEnd <= NElements() ), "_posBegin[%lu],_posEnd[%lu],NElements()[%lu]", uint64_t(_posBegin), uint64_t(_posEnd), uint64_t(NElements()) );
    static size_t knchMaxAllocaSize = vknbyMaxAllocaSize / sizeof(_tyT);
    std::basic_string< _tyT > strTempBuf; // For when we have more than knchMaxAllocaSize - avoid using t_tyString as it may be some derivation of string or anything else for that matter.
    _tySizeType nLen = _posEnd - _posBegin;
    _tyT * ptBuf;
    if ( nLen > knchMaxAllocaSize )
    {
      strTempBuf.resize( nLen );
      ptBuf = &strTempBuf[0];
    }
    else
      ptBuf = (_tyT*)alloca( nLen * sizeof( _tyT ) );
    _CopyStringToBuf( _posBegin, _posEnd, ptBuf );
    ConvertString( _rstr, ptBuf, _posEnd - _posBegin );
  }
  template < class t_tyString, class t_tyDataRange >
  void GetString( t_tyString & _rstr, t_tyDataRange const & _rdr ) const
  {
    return GetString( _rstr, _rdr.begin(), _rdr.end() );
  }
  bool FSpanChars( _tySizeType _posBegin, _tySizeType _posEnd, const _tyT * _pszCharSet ) const
  {
    _tySizeType nApplied = NApplyContiguous( _posBegin, _posEnd,
      [_pszCharSet]( const _tyT * _ptBegin, const _tyT * _ptEnd )
      {
        return StrSpn( _ptBegin, _ptEnd - _ptBegin, _pszCharSet );
      }
    );
    return nApplied == (_posEnd - _posBegin);
  }
  bool FMatchString( _tySizeType _posBegin, _tySizeType _posEnd, const _tyT * _pszMatch ) const
  {
    Assert( StrNLen( _pszMatch, _posEnd - _posBegin ) == ( _posEnd - _posBegin ) );
    const _tyT * pcCur = _pszMatch;
    _tySizeType nApplied = NApplyContiguous( _posBegin, _posEnd,
      [pcCur]( const _tyT * _ptBegin, const _tyT * _ptEnd )
      {
        const _tyT * ptCur = _ptBegin;
        for ( ; ( ptCur != _ptEnd ) && ( *ptCur == *pcCur ); ++ptCur, ++pcCur )
          ;
        return ptCur - _ptBegin;
      }
    );
    return nApplied == (_posEnd - _posBegin);
  }
  template <class... t_tysArgs>
  _tyT &emplaceAtEnd(t_tysArgs &&... _args)
  {
    AssertValid();
    _tyT *pt = new (_PbyAllocEnd()) _tyT(std::forward<t_tysArgs>(_args)...);
    ++m_nElements;
    return *pt;
  }
  // Set the number of elements in this object.
  // If we manage the lifetime of these object then they must have a default constructor.
  void SetSize(_tySizeType _nElements, bool _fCompact = false, _tySizeType _nNewBlockMin = 16)
  {
    if (_nElements < m_nElements)
      return SetSizeSmaller(_nElements, _fCompact);
    else if (m_nElements < _nElements)
    {
      AssertValid();
      if (s_kfOwnLifetime)
      {
        _nElements -= m_nElements;
        while (_nElements--)
        {
          new (_PbyAllocEnd()) _tyT();
          ++m_nElements;
        }
      }
      else
      {
        _tySizeType nBlocksNeeded = ((_nElements - 1) / NElsPerSegment()) + 1;
        if (nBlocksNeeded > _tySizeType(m_ppbyEndSegments - m_ppbySegments))
        {
          _tySizeType nNewBlocks = nBlocksNeeded - (m_ppbyEndSegments - m_ppbySegments);
          AllocNewSegmentPointerBlock((!_fCompact && (nNewBlocks < _nNewBlockMin)) ? _nNewBlockMin : nNewBlocks);
          _fCompact = false; // There is no reason to compact since we are adding elements.
        }
        uint8_t **ppbyInitNewEnd = m_ppbySegments + ((_nElements - 1) / NElsPerSegment()) + 1; // one beyond what we need to init below.
        uint8_t **ppbyCurAlloc = _PpbyGetCurSegment();
        for (; ppbyCurAlloc != ppbyInitNewEnd; ++ppbyCurAlloc)
        {
          if (!*ppbyCurAlloc)
          {
            *ppbyCurAlloc = (uint8_t *)malloc(NElsPerSegment() * sizeof(_tyT));
            if (!*ppbyCurAlloc)
              THROWNAMEDEXCEPTION("OOM for malloc(%lu).", NElsPerSegment() * sizeof(_tyT));
          }
        }
        Assert(!(_nElements % NElsPerSegment()) == !*_PpbyGetCurSegment());
        m_ppbyEndSegments = m_ppbySegments + nBlocksNeeded;
        m_nElements = _nElements;
      }
    }
    if (_fCompact)
      Compact();
    AssertValid();
  }

  // Set the number of elements in this object less than the current number.
  // If the object contained within doesn't have a default constructor you can still call this method.
  void SetSizeSmaller(_tySizeType _nElements, bool _fCompact = false)
  {
    AssertValid();
    Assert(_nElements <= m_nElements);
    if (_nElements < m_nElements)
    {
      if (s_kfOwnLifetime)
      {
        // Then just destruct all the elements:
        for (; m_nElements != _nElements; --m_nElements )
          ElGet( m_nElements - 1 ).~_tyT();
      }
      else
        m_nElements = _nElements;
    }
    if (_fCompact)
      Compact();
    AssertValid();
  }

  // This will remove segments that aren't in use.
  // We'll just leave the block pointer array the same size - at least for now.
  void Compact()
  {
    AssertValid();
    _tySizeType nBlocksNeeded = ((m_nElements - 1) / NElsPerSegment()) + 1;
    if (nBlocksNeeded < _tySizeType(m_ppbyEndSegments - m_ppbySegments))
    {
      uint8_t **ppbyDealloc = m_ppbySegments + nBlocksNeeded;
      for (; ppbyDealloc != m_ppbyEndSegments; ++ppbyDealloc)
      {
        if (*ppbyDealloc)
        {
          uint8_t *pbyDealloc = *ppbyDealloc;
          *ppbyDealloc = 0;
          free(pbyDealloc);
        }
      }
    }
    AssertValid();
  }

  // We enable insertion for non-contructed types only.
  void Insert(_tySizeType _nPos, const _tyT *_pt, _tySizeType _nEls) requires(s_kfNotOwnLifetime)
  {
    AssertValid();
    if (!_nEls)
      return;
    _tySizeType nElsOld = m_nElements;
    bool fBeyondEnd = (_nPos >= m_nElements);
    SetSize((fBeyondEnd ? _nPos : m_nElements) + _nEls); // we support insertion beyond the end.

    // Now we need to move any data that needs moving before we copy in the new data.
    if (!fBeyondEnd)
    {
      // We will move starting at the end, needless to say, and going backwards.
      _tySizeType stEndDest = m_nElements;
      _tySizeType stEndOrig = nElsOld;
      _tySizeType nElsLeft = nElsOld - _nPos;
      while (!!nElsLeft)
      {
        _tySizeType stBackOffDest = ((stEndDest - 1) % NElsPerSegment()) + 1;
        _tySizeType stBackOffOrig = ((stEndOrig - 1) % NElsPerSegment()) + 1;
        _tySizeType stMin = (std::min)(nElsLeft, (std::min)(stBackOffDest, stBackOffOrig));
        Assert(stMin); // We should always have something here.
        memmove(&ElGet(stEndDest - stMin), &ElGet(stEndOrig - stMin), stMin * sizeof(_tyT));
        nElsLeft -= stMin;
        stEndDest -= stMin;
        stEndOrig -= stMin;
      }
    }

    // And now copy in the new elements - we'll go backwards as well cuz I like it...
    _tySizeType nElsLeft = _nEls;
    _tySizeType stEndDest = _nPos + _nEls;
    const _tyT *ptEndOrig = _pt + _nEls;
    while (!!nElsLeft)
    {
      _tySizeType stBackOffDest = ((stEndDest - 1) % NElsPerSegment()) + 1;
      _tySizeType stMin = (std::min)(nElsLeft, stBackOffDest);
      Assert(stMin);
      memcpy(&ElGet(stEndDest - stMin), ptEndOrig - stMin, stMin * sizeof(_tyT));
      nElsLeft -= stMin;
      stEndDest -= stMin;
      ptEndOrig -= stMin;
    }
    AssertValid();
  }

  // We enable overwriting for non-contructed types only.
  void Overwrite(_tySizeType _nPos, const _tyT *_pt, _tySizeType _nEls) requires(s_kfNotOwnLifetime)
  {
    AssertValid();
    if (!_nEls)
      return;
    _tySizeType nElsOld = m_nElements;
    if ((_nPos + _nEls) > m_nElements)
      SetSize(_nPos + _nEls);

    // And now copy in the new elements - we'll go backwards as above...
    _tySizeType nElsLeft = _nEls;
    _tySizeType stEndDest = _nPos + _nEls;
    const _tyT *ptEndOrig = _pt + _nEls;
    while (!!nElsLeft)
    {
      _tySizeType stBackOffDest = ((stEndDest - 1) % NElsPerSegment()) + 1;
      _tySizeType stMin = (std::min)(nElsLeft, stBackOffDest);
      Assert(stMin);
      memcpy(&ElGet(stEndDest - stMin, true), ptEndOrig - stMin, stMin * sizeof(_tyT));
      nElsLeft -= stMin;
      stEndDest -= stMin;
      ptEndOrig -= stMin;
    }
    AssertValid();
  }

  // Read data from the given stream overwriting data in this segmented array.
  // May throw due to allocation error or _rs throwing an EOF error, or perhaps some other reason.
  template <class t_tyStream>
  void OverwriteFromStream(_tySizeType _nPosWrite, t_tyStream const &_rs, _tySizeType _nPosRead, _tySizeType _nElsRead) noexcept(false) requires(s_kfNotOwnLifetime)
  {
    AssertValid();
    if (!_nElsRead)
      return;
    _tySizeType nElsOld = m_nElements;
    if ((_nPosWrite + _nElsRead) > m_nElements)
      SetSize(_nPosWrite + _nElsRead);

    _tySizeType nElsLeft = _nElsRead;
    _tySizeType stCurWrite = _nPosWrite;
    _tySizeType nPosReadCur = _nPosRead;
    _tySizeType stSegRemainWrite = NElsPerSegment() - (stCurWrite % NElsPerSegment());
    while (!!nElsLeft)
    {
      _tySizeType stMin = (std::min)(nElsLeft, stSegRemainWrite);
      stSegRemainWrite = NElsPerSegment(); // From here on out.
      Assert(stMin);
      // We read a stream in bytes.
      _rs.Read(nPosReadCur * sizeof(_tyT), &ElGet(stCurWrite), stMin * sizeof(_tyT)); // Throws on EOF.
      nElsLeft -= stMin;
      stCurWrite += stMin;
      nPosReadCur += stMin;
    }
    AssertValid();
  }

  // We enable reading for non-contructed types only.
  _tySizeType Read(_tySizeType _nPos, _tyT *_pt, _tySizeType _nEls) const requires(s_kfNotOwnLifetime)
  {
    AssertValid();
    if (!_nEls)
      return 0;

    if ((_nPos + _nEls) > m_nElements)
    {
      if (_nPos >= m_nElements)
        return 0; // nothing to read.
      _nEls -= (_nPos + _nEls) - m_nElements;
    }

    // Read em out backwards:
    _tySizeType nElsLeft = _nEls;
    _tySizeType stEndOrig = _nPos + _nEls;
    _tyT *ptEndDest = _pt + _nEls;
    while (!!nElsLeft)
    {
      _tySizeType stBackOffOrig = ((stEndOrig - 1) % NElsPerSegment()) + 1;
      _tySizeType stMin = (std::min)(nElsLeft, stBackOffOrig);
      Assert(stMin);
      memcpy(ptEndDest - stMin, &ElGet(stEndOrig - stMin), stMin * sizeof(_tyT));
      nElsLeft -= stMin;
      stEndOrig -= stMin;
      ptEndDest -= stMin;
    }
    return _nEls;
  }

  template < class t_tyIter >
  _tySignedSizeType ReadSegmented( t_tyIter _itBegin, t_tyIter _itEnd, _tyT *_pt, _tySizeType _nEls ) const 
    requires(s_kfNotOwnLifetime)
  {
    AssertValid();
    _tySizeType nRangeObjs = distance( _itBegin, _itEnd );
    if ( !nRangeObjs )
      return 0; // nothing to do.
    _tyT * ptCur = _pt;
    _tySizeType nElsRemaining = _nEls; // don't buffer overrun.
    for ( ; !!nElsRemaining && ( _itBegin != _itEnd ); ++_itBegin )
    {
      Assert( _itBegin->end() >= _itBegin->begin() );
      _tySignedSizeType nReadCur = _tySignedSizeType(_itBegin->end()) - _tySignedSizeType(_itBegin->begin());
      if ( nReadCur > 0 )
      {
        _tySizeType nReadMin = min( nReadCur, nElsRemaining );
        _tySizeType nRead = Read( _itBegin->begin(), ptCur, nReadMin );
        Assert( nRead == nReadMin );
        ptCur += nRead;
        nElsRemaining -= nRead;
      }
    }
    return _nEls - nElsRemaining;
  }

  // We allow writing to a file for all types because why not? It might not make sense but you can do it.
  void WriteToFile(vtyFileHandle _hFile, _tySizeType _nPos = 0, _tySizeType _nElsWrite = (std::numeric_limits<_tySizeType>::max)()) const
  {
    AssertValid();
    if ((std::numeric_limits<_tySizeType>::max)() == _nElsWrite)
    {
      if (_nPos > m_nElements)
        THROWNAMEDEXCEPTION("Attempt to write data beyond end of segmented array.");
      _nElsWrite = m_nElements - _nPos;
    }
    else if (_nPos + _nElsWrite > m_nElements)
      THROWNAMEDEXCEPTION("Attempt to write data beyond end of segmented array.");

    _tySizeType nElsLeft = _nElsWrite;
    _tySizeType stCurOrig = _nPos;
    _tySizeType stSegRemainWrite = NElsPerSegment() - (stCurOrig % NElsPerSegment());
    while (!!nElsLeft)
    {
      _tySizeType stMin = (std::min)(nElsLeft, stSegRemainWrite);
      stSegRemainWrite = NElsPerSegment(); // From here on out.
      Assert(stMin);
      size_t stWritten;
      int iWrite = FileWrite( _hFile, &ElGet(stCurOrig), stMin * sizeof(_tyT), &stWritten );
      if ( !!iWrite || ( stWritten != stMin * sizeof(_tyT) ) )
        THROWNAMEDEXCEPTIONERRNO(GetLastErrNo(), "Error writing to _hFile[0x%lx], towrite[%lu] stWritten[%lu].", (uint64_t)_hFile, stMin * sizeof(_tyT), stWritten );
      nElsLeft -= stMin;
      stCurOrig += stMin;
    }
  }

  // This will call t_tyApply with contiguous ranges of [begin,end) elements to be applied to.
  template < class t_tyApply >
  void ApplyContiguous( _tySizeType _posBegin, _tySizeType _posEnd, t_tyApply && _rrapply )
  {
    AssertValid();
    Assert( _posEnd >= _posBegin );
    if ( _posEnd <= _posBegin )
      return; // no-op.
    _tySizeType nElsLeft = _posEnd - _posBegin;
    _tySizeType stCurApply = _posBegin;
    _tySizeType stSegRemainWrite = NElsPerSegment() - (stCurApply % NElsPerSegment());
    while (!!nElsLeft)
    {
      _tySizeType stMin = (std::min)(nElsLeft, stSegRemainWrite);
      stSegRemainWrite = NElsPerSegment(); // From here on out.
      Assert(stMin);
      _tyT * pBegin = &ElGet(stCurApply);
      std::forward<t_tyApply>(_rrapply)( pBegin, pBegin + stMin );
      nElsLeft -= stMin;
      stCurApply += stMin;
    }
  }
  // const version.
  template < class t_tyApply >
  void ApplyContiguous( _tySizeType _posBegin, _tySizeType _posEnd, t_tyApply && _rrapply ) const
  {
    AssertValid();
    Assert( _posEnd >= _posBegin );
    if ( _posEnd <= _posBegin )
      return; // no-op.
    _tySizeType nElsLeft = _posEnd - _posBegin;
    _tySizeType stCurApply = _posBegin;
    _tySizeType stSegRemainWrite = NElsPerSegment() - (stCurApply % NElsPerSegment());
    while (!!nElsLeft)
    {
      _tySizeType stMin = (std::min)(nElsLeft, stSegRemainWrite);
      stSegRemainWrite = NElsPerSegment(); // From here on out.
      Assert(stMin);
      const _tyT * pBegin = &ElGet(stCurApply);
      std::forward<t_tyApply>(_rrapply)( pBegin, pBegin + stMin );
      nElsLeft -= stMin;
      stCurApply += stMin;
    }
  }
  
  // As above but the _apply object's operator() returns the count of applications.
  // If the count is less than the full contiguous region supplied then the iteration is aborted.
  // The return value is the total of all calls to _apply() performed.
  template < class t_tyApply >
  _tySizeType NApplyContiguous( _tySizeType _posBegin, _tySizeType _posEnd, t_tyApply && _rrapply )
  {
    AssertValid();
    Assert( _posEnd >= _posBegin );
    if ( _posEnd <= _posBegin )
      return 0; // no-op.
    _tySizeType nElsLeft = _posEnd - _posBegin;
    _tySizeType stCurApply = _posBegin;
    _tySizeType stSegRemainWrite = NElsPerSegment() - (stCurApply % NElsPerSegment());
    _tySizeType stNAppls = 0;
    while (!!nElsLeft)
    {
      const _tySizeType stMin = (std::min)(nElsLeft, stSegRemainWrite);
      stSegRemainWrite = NElsPerSegment(); // From here on out.
      Assert(stMin);
      _tyT * pBegin = &ElGet(stCurApply);
      _tySizeType stNAppl = std::forward<t_tyApply>(_rrapply)( pBegin, pBegin + stMin );
      stNAppls += stNAppl;
      if ( stMin != stNAppl )
        break;
      nElsLeft -= stMin;
      stCurApply += stMin;
    }
    return stNAppls;
  }
  // const version.
  template < class t_tyApply >
  _tySizeType NApplyContiguous( _tySizeType _posBegin, _tySizeType _posEnd, t_tyApply && _rrapply ) const
  {
    AssertValid();
    Assert( _posEnd >= _posBegin );
    if ( _posEnd <= _posBegin )
      return 0; // no-op.
    _tySizeType nElsLeft = _posEnd - _posBegin;
    _tySizeType stCurApply = _posBegin;
    _tySizeType stSegRemainWrite = NElsPerSegment() - (stCurApply % NElsPerSegment());
    _tySizeType stNAppls = 0;
    while (!!nElsLeft)
    {
      const _tySizeType stMin = (std::min)(nElsLeft, stSegRemainWrite);
      stSegRemainWrite = NElsPerSegment(); // From here on out.
      Assert(stMin);
      const _tyT * pBegin = &ElGet(stCurApply);
      _tySizeType stNAppl = std::forward<t_tyApply>(_rrapply)( pBegin, pBegin + stMin );
      stNAppls += stNAppl;
      if ( stMin != stNAppl )
        break;
      nElsLeft -= stMin;
      stCurApply += stMin;
    }
    return stNAppls;
  }
protected:
  uint8_t **_PpbyGetCurSegment() const
  {
    return m_ppbySegments + m_nElements / NElsPerSegment();
  }
  void AllocNewSegmentPointerBlock(_tySizeType _nNewBlocks)
  {
    uint8_t **ppbySegments = (uint8_t **)realloc(m_ppbySegments, ((m_ppbyEndSegments - m_ppbySegments) + _nNewBlocks) * sizeof(uint8_t *));
    if (!ppbySegments)
      THROWNAMEDEXCEPTION("OOM for realloc(%lu).", ((m_ppbyEndSegments - m_ppbySegments) + _nNewBlocks) * sizeof(uint8_t *));
    memset(ppbySegments + (m_ppbyEndSegments - m_ppbySegments), 0, _nNewBlocks * sizeof(uint8_t *));
    m_ppbyEndSegments = ppbySegments + (m_ppbyEndSegments - m_ppbySegments) + _nNewBlocks;
    m_ppbySegments = ppbySegments;
  }

  uint8_t *_PbyAllocEnd()
  {
    uint8_t **ppbyCurSegment = _PpbyGetCurSegment();
    if (ppbyCurSegment == m_ppbyEndSegments)
    {
      static const _tySizeType s_knNumBlocksAlloc = 16;
      AllocNewSegmentPointerBlock(s_knNumBlocksAlloc);
      ppbyCurSegment = _PpbyGetCurSegment();
    }
    if (!*ppbyCurSegment)
    {
      Assert(!(m_nElements % NElsPerSegment()));
      *ppbyCurSegment = (uint8_t *)malloc(m_nbySizeSegment);
      if (!*ppbyCurSegment)
        THROWNAMEDEXCEPTION("OOM for malloc(%lu).", m_nbySizeSegment);
    }
    return *ppbyCurSegment + ((m_nElements % NElsPerSegment()) * sizeof(_tyT));
  }

  void _Clear()
  {
    // First, if we own the lifetimes, then end them all:
    if ( s_kfOwnLifetime && !!m_nElements )
    {
      for ( ; m_nElements; --m_nElements )
        ElGet(m_nElements-1).~_tyT();
    }    
    uint8_t **ppbyEndData = m_ppbyEndSegments;
    m_ppbyEndSegments = 0;
    uint8_t **ppbySegments = m_ppbySegments;
    m_ppbySegments = 0;
    for (uint8_t **ppbyCurThis = ppbySegments; ppbyEndData != ppbyCurThis; ++ppbyCurThis)
    {
      if ( *ppbyCurThis )
        free(*ppbyCurThis);
    }
  }

protected:
  uint8_t **m_ppbySegments{};
  uint8_t **m_ppbyEndSegments{};
  _tySizeType m_nElements{};
  _tySizeType m_nbySizeSegment{};
};

template <class t_tyT, class t_tyFOwnLifetime, class t_tySizeType>
inline const t_tySizeType
SegArray< t_tyT, t_tyFOwnLifetime, t_tySizeType >::s_knbySizeSegment = (std::max)(sizeof(t_tyT) * 16, size_t(4096));

// SegArrayRotatingBuffer:
// This object stores a "current base position" with the SegArray. Nothing before this base position is accessible anymore - but some of it may still
//  be allocated. As the base position is moved forward, vacated memory chunks are recycled and placed on the end, creating a rotating buffer with no
//  reallocation required.
// Alternate usage: I want to return SegArrayRotatingBuffers in two different manners:
//  1) I want to return them where I waste the beginning of the data by having m_iBaseEl be offset into the segment chunk because I don't care about
//      this little bit of waste because the memory is already in the right place and I don't have to touch it (or allocate anything else to hold it).
//  2) When it's the case that (1) is grossly inefficient (i.e. I would be wasting a lot of memory in comparison to what I am returning) I want to
//      have m_iBaseEl merely indicate the which element the 0th element of the SegArray base class corresponds to. In this case I am not wanting to move
//      this m_iBaseEl around at all whereas in (1)'s case I am planning on moving it up continuously until the end of the file.
// So, for (1) above we will use a m_iBaseEl that is >= zero.
// For (2) we will use a m_iBaseEl that is less than zero.
// This does 
template <class t_tyT, class t_tySizeType = size_t>
class SegArrayRotatingBuffer : protected SegArray< t_tyT, std::false_type, t_tySizeType >
{
  typedef SegArrayRotatingBuffer _tyThis;
  typedef SegArray< t_tyT, std::false_type, t_tySizeType > _tyBase;
public:
  using typename _tyBase::_tyT;
  using typename _tyBase::_tySizeType;
  using typename _tyBase::_tySignedSizeType;
  using _tyBase::s_knbySizeSegment;

  SegArrayRotatingBuffer() = delete;
  SegArrayRotatingBuffer( _tySizeType _nbySizeSegment = s_knbySizeSegment )
    : _tyBase( _nbySizeSegment )
  {
  }
  SegArrayRotatingBuffer( SegArrayRotatingBuffer const & ) = default;
  SegArrayRotatingBuffer( SegArrayRotatingBuffer &&_rr )
      : _tyBase(_rr.m_nbySizeSegment) // Hopefully get a non-zero value from this object.
  {
    _rr.AssertValid();
    swap(_rr);
  }
  ~SegArrayRotatingBuffer() = default;

  void AssertValid() const
#if ASSERTSENABLED
  {
    using _tyBase::m_nElements;
    _tyBase::AssertValid();
    // Assert we aren't overflowing when computing the number of elements.
    // Since we are using a signed m_iBaseEl we have limited the size of the file we can process to 2^63-1 in size
    //  from 2^64-1 in size had we used an unsigned m_iBaseEl. Thats fine with me.
    Assert( ( NBaseElMagnitude() + m_nElements ) < (numeric_limits<_tySignedSizeType>::max)() );
  }
#else  //!ASSERTSENABLED
  {
  }
#endif //!ASSERTSENABLED
  void Clear()
  {
    _tyBase::Clear();
    m_iBaseEl = 0;
  }
  void swap( _tyThis &_r )
  {    
    _tyBase::swap( _r );
    std::swap(m_iBaseEl, _r.m_iBaseEl);
  }
  _tyThis &operator=(const _tyThis &_r)
  {
    _tyThis saCopy(_r);
    swap(saCopy);
  }
  _tyThis &operator=(_tyThis &&_rr)
  {
    // Caller doesn't want _rr to have anything in it or they would have just called swap directly.
    // So Clear() first.
    Clear();
    swap(_rr);
  }

  _tySignedSizeType IBaseElement() const
  {
    return m_iBaseEl;
  }
  _tySizeType NBaseElMagnitude() const
  {
    return m_iBaseEl < 0 ? -m_iBaseEl : m_iBaseEl;
  }
  // The offset of the base within the first chunk when positive, when negative it merely sets the base of the 
  _tySizeType _NBaseOffset() const
  {
    return m_iBaseEl < 0 ? -m_iBaseEl : ( m_iBaseEl - ( m_iBaseEl % NElsPerSegment() ) );
  }
  // We constantly track the thing we are buffering with a window that starts _NBaseOffset() and ends m_nElements beyond that.
  // The user of this object can only access between m_iBaseEl and ( _NBaseOffset() + m_nElements ).
  _tySizeType NElements() const
  {
    AssertValid();
    return _tyBase::m_nElements + _NBaseOffset();
  }
  _tySizeType GetSize() const
  {
    return NElements();
  }
  using _tyBase::NElsPerSegment;
  using _tyBase::FHasAnyCapacity;

  // Base element updating:
  // This resets the base element to any value the caller wants without any reallocation occuring.
  void HardResetIBaseEl( _tySignedSizeType _iBaseEl )
  {
    m_iBaseEl = _iBaseEl;
  }
  // This method has two possibilities:
  //  1) m_iBaseEl is <= 0 and _iBaseEl < 0: Just set m_iBaseEl to _iBaseEl and return. This just "rebases" 
  //      existing data without any "rotation" aspect. There is no rotation aspect when m_iBaseEl is negative.
  //  2) m_iBaseEl is >= 0 and _iBasaEl >= 0: This activates rotation when m_iBaseEl goes over a segment boundary.
  void SetIBaseEl( _tySignedSizeType _iBaseEl )
  {
    using _tyBase::m_ppbyEndSegments;
    using _tyBase::m_ppbySegments;
    if ( _iBaseEl < 0 )
    {
      VerifyThrowSz( m_iBaseEl <= 0, "Trying to switch signs on the base element index which is not permitted.");
      m_iBaseEl = _iBaseEl;
      return;
    }
    VerifyThrowSz( _iBaseEl >= m_iBaseEl, 
      "Trying to set the base element to something less than the current base or switch signs which is not possible. _iBaseEl[%ld], m_iBaseEl[%ld].", int64_t(_iBaseEl), int64_t(m_iBaseEl) );
    // If the new base el is less than the current number of elements then:
    //  1) Set the new base el.
    //  2) Move all blocks before the block containing the base el to the end of the block pointer array - in any order. Just need to memmove,etc.
    // If the new base el is beyond the current number of elements then:
    //  1) Just set the number of elements to ( m_iBaseEl % NElsPerSegment() ).
    if ( m_iBaseEl != _iBaseEl )
    {
      if ( _iBaseEl >= NElements() )
      {
        m_iBaseEl = _iBaseEl;
        _tyBase::SetSize( m_iBaseEl % NElsPerSegment() );
      }
      else
      {
        AssertStatement( _tySizeType ast_nElsBefore = NElements() );
        ssize_t sstShifted = ( _iBaseEl / NElsPerSegment() ) - ( m_iBaseEl / NElsPerSegment() );
        Assert( sstShifted < ( m_ppbyEndSegments - m_ppbySegments ) );
        m_iBaseEl = _iBaseEl;
        if ( !!( sstShifted % ( m_ppbyEndSegments - m_ppbySegments ) ) ) // in case the above assert can fail - in my mind currently it can't.
        {
          // This is the rotation code, btw, lol:
          Assert( sstShifted > 0 );
          uint8_t ** ppbyBuffer = (uint8_t **)alloca( sstShifted * sizeof( uint8_t * ) );
          memcpy( ppbyBuffer, m_ppbySegments, sstShifted * sizeof( uint8_t * ) );
          memmove( m_ppbySegments, m_ppbySegments + sstShifted, ( ( m_ppbyEndSegments - m_ppbySegments ) - sstShifted ) * sizeof(uint8_t *) );
          memcpy( m_ppbySegments + ( ( m_ppbyEndSegments - m_ppbySegments ) - sstShifted ) * sizeof(uint8_t *), ppbyBuffer, sstShifted * sizeof( uint8_t * ) );
          _tyBase::m_nElements -= sstShifted * NElsPerSegment();
        }
        Assert( NElements() == ast_nElsBefore );
      }
    }
  }

  _tyT &ElGet(_tySizeType _nEl, bool _fMaybeEnd = false)
  {
    AssertValid();
    _tySizeType nEls = NElements();
    if ( ( _nEl < NBaseElMagnitude() ) || ( _nEl > nEls ) ||  (!_fMaybeEnd && ( _nEl == nEls ) ) )
      THROWNAMEDEXCEPTION("Out of bounds m_iBaseEl[%ld] _nEl[%lu] m_nElements[%lu].", (int64_t)m_iBaseEl, (uint64_t)_nEl, (uint64_t)nEls);
    return _tyBase::ElGet( _nEl - _NBaseOffset() );
  }
  _tyT const &ElGet(_tySizeType _nEl) const
  {
    return const_cast<_tyThis *>(this)->ElGet(_nEl);
  }
  _tyT &operator[](_tySizeType _n)
  {
    return ElGet(_n);
  }
  const _tyT &operator[](_tySizeType _n) const
  {
    return ElGet(_n);
  }

  template < class t_tyStringView, class t_tyDataRange >
  bool FGetStringView( t_tyStringView & _rsv, _tySizeType _posBegin, _tySizeType _posEnd ) const
    requires ( sizeof( typename t_tyStringView::value_type ) == sizeof( _tyT ) )
  {
    Assert( _rsv.empty() );
    if ( _posBegin == _posEnd )
      return true;
    VerifyThrowSz( ( _posBegin >= NBaseElMagnitude() ), 
      "Trying to read data before the base of the rotating buffer, _posBegin[%lu], m_iBaseEl[%ld].", uint64_t(_posBegin), int64_t(m_iBaseEl) );
    _tySizeType nOff = _NBaseOffset();
    return _tyBase::FGetStringView( _rsv, _posBegin - nOff, _posEnd - nOff );
  }
  template < class t_tyStringView, class t_tyDataRange >
  bool FGetStringView( t_tyStringView & _rsv, t_tyDataRange const & _rdr ) const
    requires ( sizeof( typename t_tyStringView::value_type ) == sizeof( _tyT ) )
  {
    return FGetStringView( _rdr.begin(), _rdr.end() );
  }
  template < class t_tyString, class t_tyDataRange >
  void GetString( t_tyString & _rstr, _tySizeType _posBegin, _tySizeType _posEnd ) const
  {
    Assert( _rstr.empty() );
    if ( _posBegin == _posEnd )
      return; // empty result.
    VerifyThrowSz( ( _posBegin >= NBaseElMagnitude() ), 
      "Trying to read data before the base of the rotating buffer, _posBegin[%lu], m_iBaseEl[%ld].", uint64_t(_posBegin), int64_t(m_iBaseEl) );
    _tySizeType nOff = _NBaseOffset();
    return _tyBase::GetString( _rstr, _posBegin - nOff, _posEnd - nOff );
  }
  template < class t_tyString, class t_tyDataRange >
  void GetString( t_tyString & _rstr, t_tyDataRange const & _rdr ) const
  {
    return GetString( _rstr, _rdr.begin(), _rdr.end() );
  }
  bool FSpanChars( _tySizeType _posBegin, _tySizeType _posEnd, const _tyT * _pszCharSet ) const
  {
    VerifyThrowSz( ( _posBegin >= NBaseElMagnitude() ), 
      "Trying to read data before the base of the rotating buffer, _posBegin[%lu], m_iBaseEl[%ld].", uint64_t(_posBegin), int64_t(m_iBaseEl) );
    _tySizeType nOff = _NBaseOffset();
    return _tyBase::FSpanChars( _posBegin - nOff, _posEnd - nOff, _pszCharSet );
  }
  bool FMatchString( _tySizeType _posBegin, _tySizeType _posEnd, const _tyT * _pszMatch ) const
  {
    VerifyThrowSz( ( _posBegin >= NBaseElMagnitude() ), 
      "Trying to read data before the base of the rotating buffer, _posBegin[%lu], m_iBaseEl[%ld].", uint64_t(_posBegin), int64_t(m_iBaseEl) );
    _tySizeType nOff = _NBaseOffset();
    return _tyBase::FMatchString( _posBegin - nOff, _posEnd - nOff, _pszMatch );
  }

  using _tyBase::emplaceAtEnd;

  // This will destructively transfer data from [_posBegin,_posEnd) from *this to _rsaTo.
  // It will move the m_iBaseEl to _posEnd.
  void CopyDataAndAdvanceBuffer( _tySizeType _posBegin, _tyT * _ptBuf, _tySizeType _nLenEls )
  {
    AssertValid();
    VerifyThrowSz( m_iBaseEl >= 0, 
      "Can't call CopyDataAndAdvanceBuffer() on a negative m_iBaseEl SegArrayRotatingBuffer, m_iBaseEl[%ld].", int64_t(m_iBaseEl) );
    _tyT * ptBufCur = _ptBuf;
    ApplyContiguous( _posBegin, _posBegin + _nLenEls, 
      [&ptBufCur]( const _tyT * _ptBegin, const _tyT * _ptEnd )
      {
        memcpy( ptBufCur, _ptBegin, ( _ptEnd - _ptBegin ) * sizeof( _tyT ) );
        ptBufCur += ( _ptEnd - _ptBegin );
      }
    );
    Assert( ptBufCur == ( _ptBuf + _nLenEls ) );
    SetIBaseEl( _posBegin + _nLenEls ); // Consume that data that we copied to the user.
  }
  // Set the number of elements in this object. Note that it is invalid to set the number of elements to be less than m_iBaseEl.
  // If we manage the lifetime of these object then they must have a default constructor.
  void SetSize(_tySizeType _nElements, bool _fCompact = false, _tySizeType _nNewBlockMin = 16)
  {
    AssertValid();
    VerifyThrowSz( _nElements >= NBaseElMagnitude(), 
      "Trying to set the number of elements less than the base of the rotating buffer, _nElements[%lu], m_iBaseEl[%ld].", uint64_t(_nElements), int64_t(m_iBaseEl) );
    _tyBase::SetSize( _nElements - _NBaseOffset(), _fCompact, _nNewBlockMin );
  }
  // Set the number of elements in this object less than the current number.
  // If the object contained within doesn't have a default constructor you can still call this method.
  void SetSizeSmaller(_tySizeType _nElements, bool _fCompact = false)
  {
    AssertValid();
    VerifyThrowSz( _nElements >= NBaseElMagnitude(), 
      "Trying to set the number of elements less than the base of the rotating buffer, _nElements[%lu], m_iBaseEl[%ld].", uint64_t(_nElements), int64_t(m_iBaseEl) );
    _tyBase::SetSizeSmaller( _nElements - _NBaseOffset(), _fCompact );
  }
  using _tyBase::Compact;

  // We enable insertion for non-contructed types only.
  void Insert(_tySizeType _nPos, const _tyT *_pt, _tySizeType _nEls)
  {
    AssertValid();
    VerifyThrowSz( _nPos >= NBaseElMagnitude(), 
      "Trying to insert before the base of the rotating buffer, _nPos[%lu], m_iBaseEl[%ld].", uint64_t(_nPos), int64_t(m_iBaseEl) );
    _tyBase::Insert( _nPos - _NBaseOffset(), _pt, _nEls );
    AssertValid();
  }
  // We enable overwriting for non-contructed types only.
  void Overwrite(_tySizeType _nPos, const _tyT *_pt, _tySizeType _nEls)
  {
    AssertValid();
    VerifyThrowSz( _nPos >= NBaseElMagnitude(), 
      "Trying to overwrite before the base of the rotating buffer, _nPos[%lu], m_iBaseEl[%ld].", uint64_t(_nPos), int64_t(m_iBaseEl) );
    _tyBase::Overwrite( _nPos - _NBaseOffset(), _pt, _nEls );
    AssertValid();
  }
  // Read data from the given stream overwriting data in this segmented array.
  // May throw due to allocation error or _rs throwing an EOF error, or perhaps some other reason.
  template <class t_tyStream>
  void OverwriteFromStream(_tySizeType _nPosWrite, t_tyStream const &_rs, _tySizeType _nPosRead, _tySizeType _nElsRead) noexcept(false)
  {
    AssertValid();
    VerifyThrowSz( _nPosWrite >= NBaseElMagnitude(), 
      "Trying to write before the base of the rotating buffer, _nPosWrite[%lu], m_iBaseEl[%ld].", uint64_t(_nPosWrite), int64_t(m_iBaseEl) );
    _tyBase::OverwriteFromStream( _nPosWrite - _NBaseOffset(), _rs, _nPosRead, _nElsRead );
    AssertValid();
  }
  // We enable reading for non-contructed types only.
  _tySizeType Read(_tySizeType _nPos, _tyT *_pt, _tySizeType _nEls) const
  {
    AssertValid();
    VerifyThrowSz( _nPos >= NBaseElMagnitude(), 
      "Trying to read before the base of the rotating buffer, _nPos[%lu], m_iBaseEl[%ld].", uint64_t(_nPos), int64_t(m_iBaseEl) );
    return _tyBase::Read( _nPos - _NBaseOffset(), _pt, _nEls );
  }
  // This one just needs to be done here since each range in the iteration must be offset.
  template < class t_tyIter >
  _tySignedSizeType ReadSegmented( t_tyIter _itBegin, t_tyIter _itEnd, _tyT *_pt, _tySizeType _nEls ) const 
  {
    AssertValid();
    _tySizeType nRangeObjs = distance( _itBegin, _itEnd );
    if ( !nRangeObjs )
      return 0; // nothing to do.
    _tyT * ptCur = _pt;
    _tySizeType nElsRemaining = _nEls; // don't buffer overrun.
    for ( ; !!nElsRemaining && ( _itBegin != _itEnd ); ++_itBegin )
    {
      Assert( _itBegin->end() >= _itBegin->begin() );
      _tySignedSizeType nReadCur = _tySignedSizeType(_itBegin->end()) - _tySignedSizeType(_itBegin->begin());
      if ( nReadCur > 0 )
      {
        _tySizeType nReadMin = min( nReadCur, nElsRemaining );
        _tySizeType nRead = Read( _itBegin->begin(), ptCur, nReadMin );
        Assert( nRead == nReadMin );
        ptCur += nRead;
        nElsRemaining -= nRead;
      }
    }
    return _nEls - nElsRemaining;
  }
  // We allow writing to a file for all types because why not? It might not make sense but you can do it.
  void WriteToFile(vtyFileHandle _hFile, _tySizeType _nPos, _tySizeType _nElsWrite = (std::numeric_limits<_tySizeType>::max)()) const
  {
    AssertValid();
    VerifyThrowSz( _nPos >= NBaseElMagnitude(), 
      "Trying to read before the base of the rotating buffer, _nPos[%lu], m_iBaseEl[%ld].", uint64_t(_nPos), int64_t(m_iBaseEl) );
    _tyBase::WriteToFile( _hFile, _nPos - _NBaseOffset(), _nElsWrite );
  }
  // This will call t_tyApply with contiguous ranges of [begin,end) elements to be applied to.
  template < class t_tyApply >
  void ApplyContiguous( _tySizeType _posBegin, _tySizeType _posEnd, t_tyApply && _rrapply )
  {
    AssertValid();
    VerifyThrowSz( _posBegin >= NBaseElMagnitude(), 
      "Trying to apply before the base of the rotating buffer, _posBegin[%lu], m_iBaseEl[%ld].", uint64_t(_posBegin), int64_t(m_iBaseEl) );
    _tyBase::ApplyContiguous( _posBegin - _NBaseOffset(), _posEnd - _NBaseOffset(), std::forward<t_tyApply>(_rrapply) );
  }
  // const version.
  template < class t_tyApply >
  void ApplyContiguous( _tySizeType _posBegin, _tySizeType _posEnd, t_tyApply && _rrapply ) const
  {
    AssertValid();
    VerifyThrowSz( _posBegin >= NBaseElMagnitude(), 
      "Trying to apply before the base of the rotating buffer, _posBegin[%lu], m_iBaseEl[%ld].", uint64_t(_posBegin), int64_t(m_iBaseEl) );
    _tyBase::ApplyContiguous( _posBegin - _NBaseOffset(), _posEnd - _NBaseOffset(), std::forward<t_tyApply>(_rrapply) );
  }
  template < class t_tyApply >
  _tySizeType NApplyContiguous( _tySizeType _posBegin, _tySizeType _posEnd, t_tyApply && _rrapply )
  {
    AssertValid();
    VerifyThrowSz( _posBegin >= NBaseElMagnitude(), 
      "Trying to apply before the base of the rotating buffer, _posBegin[%lu], m_iBaseEl[%ld].", uint64_t(_posBegin), int64_t(m_iBaseEl) );
    return _tyBase::NApplyContiguous( _posBegin - _NBaseOffset(), _posEnd - _NBaseOffset(), std::forward<t_tyApply>(_rrapply) );
  }
  template < class t_tyApply >
  _tySizeType NApplyContiguous( _tySizeType _posBegin, _tySizeType _posEnd, t_tyApply && _rrapply ) const
  {
    AssertValid();
    VerifyThrowSz( _posBegin >= NBaseElMagnitude(), 
      "Trying to apply before the base of the rotating buffer, _posBegin[%lu], m_iBaseEl[%ld].", uint64_t(_posBegin), int64_t(m_iBaseEl) );
    return _tyBase::NApplyContiguous( _posBegin - _NBaseOffset(), _posEnd - _NBaseOffset(), std::forward<t_tyApply>(_rrapply) );
  }
protected:
  // The current base element. The invariant that is maintained is that m_iBaseEl is always located in the first chunk.
  //  When m_iBaseEl is moved into the next chunk (or beyond) those chunk(s) are rotated around to the end.
  _tySignedSizeType m_iBaseEl{0}; 
};

// SegArrayView:
// This is an object that contains a "view" into a SegArray that may not be contiguous.
// Note that we specifically do not define a value_type as _tyT on this object as we are using
//  this to discern std strings and string_views when setting strings into objects (and we want to override operator[]() here for ease of use)
// We optimize for the case where the object fits inside of a segment by storing the pointer in that case.
template <class t_tyT, class t_tyFOwnLifetime, class t_tySizeType >
class SegArrayView
{
  typedef SegArrayView _tyThis;
public:
  typedef t_tyT _tyT;
  typedef std::remove_cv_t< _tyT > _tyTRemoveCV;
  typedef t_tySizeType _tySizeType;
  typedef typename std::make_signed<_tySizeType>::type _tySignedSizeType;
  typedef SegArray< std::remove_cv_t< _tyT >, t_tyFOwnLifetime, t_tySizeType > _tySegArray;

  ~SegArrayView() = default;
  SegArrayView( SegArrayView const & ) = default;
  SegArrayView& operator=( SegArrayView const & ) = default;  

  SegArrayView()
      : m_stBegin((numeric_limits<_tySizeType>::max)())
  {
  }
  SegArrayView(_tySegArray *_psaContainer)
      : m_psaContainer(_psaContainer),
        m_stBegin((numeric_limits<_tySizeType>::max)())
  {
    AssertValid();
  }
  SegArrayView(_tySegArray *_psaContainer, _tyT * _ptBegin, _tySizeType _sstLen )
      : m_psaContainer(_psaContainer),
        m_ptBegin(_ptBegin),
        m_sstLen( -((_tySignedSizeType)_sstLen)) 
  {
    Assert( _sstLen < (_tySizeType)(numeric_limits< _tySignedSizeType >::max)() );
    AssertValid();
  }
  SegArrayView(_tySegArray *_psaContainer, _tySizeType _stBegin, _tySizeType _sstLen )
      : m_psaContainer(_psaContainer),
        m_stBegin(_stBegin),
        m_sstLen(_sstLen)
  {
    AssertValid();
  }
  void swap( _tyThis & _r )
  {
    AssertValid();
    _r.AssertValid();
    static_assert( offsetof(_tyThis, m_stBegin ) == offsetof(_tyThis, m_ptBegin ) );
    std::swap( m_stBegin, _r.m_stBegin );
    std::swap( m_sstLen, _r.m_sstLen );
    std::swap( m_psaContainer, _r.m_psaContainer );
  }
  void AssertValid() const
  {
#if ASSERTSENABLED
    Assert( !!m_psaContainer || !m_sstLen );
    Assert( !!m_sstLen || ( (std::numeric_limits<_tySizeType>::max)() == m_stBegin ) );
#endif //ASSERTSENABLED
  }
  bool FIsNull()
  {
    AssertValid();
    return ( (numeric_limits<_tySizeType>::max)() == m_stBegin );
  }
  // Return contiguous memory - either a string_view if possible, or a string if not.
  // This is only relevant for t_tyT's which are character types.
  // Return true if we were able to populate the string_view, false for string.
  template < class t_tyStrView, class t_tyString > bool
  FGetStringViewOrString( t_tyStrView & _rStrView, t_tyString & _rStr ) const
    requires( TIsCharType_v< _tyTRemoveCV > && 
              ( sizeof( typename t_tyStrView::value_type ) == sizeof(_tyTRemoveCV ) ) && 
              ( sizeof( typename t_tyString::value_type ) == sizeof( _tyTRemoveCV ) ) )
  {
    AssertValid();
    Assert( _rStrView.empty() ); // we expect empties.
    Assert( _rStr.empty() );
    if ( !m_psaContainer || !m_sstLen )
      return true; // The empty string view is perfect.
    if ( m_sstLen < 0 )
    {
      _rStrView = t_tyStrView( (const typename t_tyStrView::value_type*)m_ptBegin, (_tySizeType)-m_sstLen );
      return true;
    }
    else
    {
      _rStr.resize( (_tySizeType)m_sstLen );
      _tySizeType nRead = m_psaContainer->Read( m_stBegin, (_tyTRemoveCV*)&_rStr[0], (_tySizeType)m_sstLen );
      Assert( (_tySizeType)m_sstLen == nRead );
      _rStr.resize( nRead );
      return false;
    }
  }
  // This is the converting method. When there is any data it always returns true and returns a string since it converted the character type.
  // If there is no data then it returns false and a null string_view.
  template < class t_tyStrView, class t_tyString > bool
  FGetStringViewOrString( t_tyStrView & _rStrView, t_tyString & _rStr ) const
    requires( TIsCharType_v< _tyTRemoveCV > && 
              ( sizeof( typename t_tyStrView::value_type ) != sizeof(_tyTRemoveCV ) ) )
  {
    static_assert( sizeof( typename t_tyString::value_type ) == sizeof( typename t_tyStrView::value_type ) );
    AssertValid();
    Assert( _rStrView.empty() ); // we expect empties.
    Assert( _rStr.empty() );
    if ( !m_psaContainer || !m_sstLen )
      return true; // The empty string view is perfect.
    const _tySizeType stLen = m_sstLen < 0 ? (_tySizeType)-m_sstLen : (_tySizeType)m_sstLen;
    static const _tySizeType knchMaxAllocaSize = vknbyMaxAllocaSize / sizeof( _tyTRemoveCV ); // Allow 512KB on the stack. After that we go to a string.
    basic_string< _tyTRemoveCV > strTempBuf;
    _tyTRemoveCV * ptContigBuf;
    if ( m_sstLen < 0 )
    {
      ptContigBuf = m_ptBegin;
    }
    else
    {
      if ( stLen > knchMaxAllocaSize )
      {
        strTempBuf.resize( stLen );
        ptContigBuf = &strTempBuf[0];
      }
      else
        ptContigBuf = (_tyTRemoveCV*)alloca( stLen * sizeof( _tyTRemoveCV ) );
      const _tySizeType kstRead = m_psaContainer->Read( m_stBegin, ptContigBuf, stLen );
      Assert( kstRead == stLen );
      stLen = kstRead;
    }
    // Do the conversion:
    ConvertString( _rStr, ptContigBuf, stLen );
    return false;
  }

  // Return contiguous memory - either a string_view if possible, or a string if not.
  // This is only relevant for t_tyT's which are character types.
  // Return true if we were able to populate the string_view, false for string.
  template < class t_tyString > void
  GetString( t_tyString & _rStr  ) const
    requires( TIsCharType_v< _tyTRemoveCV > && ( sizeof( typename t_tyString::value_type ) == sizeof( _tyTRemoveCV ) ) )
  {
    AssertValid();
    Assert( _rStr.empty() );
    if ( !m_psaContainer || !m_sstLen )
      return; // The empty string is perfect.
    if ( m_sstLen < 0 )
      _rStr.assign( (typename t_tyString::value_type const *)m_ptBegin, (_tySizeType)-m_sstLen );
    else
    {
      _rStr.resize( (_tySizeType)m_sstLen );
      _tySizeType stRead = m_psaContainer->Read( m_stBegin, (_tyTRemoveCV*)&_rStr[0], (_tySizeType)m_sstLen );
      Assert( stRead == (_tySizeType)m_sstLen );
      _rStr.resize( stRead );
    }
  }
  // This is the converting method.
  template < class t_tyString > void
  GetString( t_tyString & _rStr  ) const
    requires( TIsCharType_v< _tyTRemoveCV > && ( sizeof( typename t_tyString::value_type ) != sizeof( _tyTRemoveCV ) ) )
  {
    AssertValid();
    Assert( _rStr.empty() );
    if ( !m_psaContainer || !m_sstLen )
      return; // The empty string is perfect.
    const _tySizeType stLen = m_sstLen < 0 ? (_tySizeType)-m_sstLen : (_tySizeType)m_sstLen;
    static const _tySizeType knchMaxAllocaSize = vknbyMaxAllocaSize/ sizeof( _tyTRemoveCV ); // Allow 512KB on the stack. After that we go to a string.
    basic_string< _tyTRemoveCV > strTempBuf;
    _tyTRemoveCV * ptContigBuf;
    if ( m_sstLen < 0 )
    {
      ptContigBuf = m_ptBegin;
    }
    else
    {
      if ( stLen > knchMaxAllocaSize )
      {
        strTempBuf.resize( stLen );
        ptContigBuf = &strTempBuf[0];
      }
      else
        ptContigBuf = (_tyTRemoveCV*)alloca( stLen * sizeof( _tyTRemoveCV ) );
      
      _tySizeType stRead = m_psaContainer->Read( m_stBegin, ptContigBuf, stLen );
      Assert( stRead == stLen );
      stLen = stRead;
    }
    // Do the conversion:
    ConvertString( _rStr, ptContigBuf, stLen );
  }

protected:
  union
  {
    _tySizeType m_stBegin; // { (numeric_limits< _tySizeType >::max)() }; // Can't initialize inside of a union but this is the null value.
    _tyT *m_ptBegin;       // When the length in m_sstEnd is < 0 then this is populated and the length is -m_sstLen.
  };
  // When m_sstLen is zero then m_stBegin should contain max() and this object is NULL.
  _tySignedSizeType m_sstLen{0};
  _tySegArray *m_psaContainer{nullptr}; // The SegArray container to which we are connected.
};

__BIENUTIL_END_NAMESPACE

namespace std
{
__BIENUTIL_USING_NAMESPACE
  template <class t_tyT, class t_tyFOwnLifetime, class t_tySizeType>
  void swap(SegArray< t_tyT, t_tyFOwnLifetime, t_tySizeType >& _rl, SegArray< t_tyT, t_tyFOwnLifetime, t_tySizeType >& _rr)
  {
    _rl.swap(_rr);
  }
  template <class t_tyT, class t_tySizeType>
  void swap(SegArrayRotatingBuffer< t_tyT, t_tySizeType >& _rl, SegArrayRotatingBuffer< t_tyT, t_tySizeType >& _rr)
  {
    _rl.swap(_rr);
  }
  template <class t_tyT, class t_tyFOwnLifetime, class t_tySizeType>
  void swap(SegArrayView< t_tyT, t_tyFOwnLifetime, t_tySizeType >& _rl, SegArrayView< t_tyT, t_tyFOwnLifetime, t_tySizeType >& _rr)
  {
    _rl.swap(_rr);
  }
}

