#pragma once

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// segarray.h
// Segmented array.
// dbien: 20MAR2020
// Would like to templatize by allocator but I have to propagate it and it's annoying right now. Later.

template < class t_tyT, class t_tyFOwnLifetime, class t_tySizeType = size_t >
class SegArray
{
    typedef SegArray _tyThis;
public:
    typedef t_tyFOwnLifetime _tyFOwnLifetime;
    static constexpr bool s_kfOwnLifetime = _tyFOwnLifetime::value;
    static constexpr bool s_kfNotOwnLifetime = !s_kfOwnLifetime;
    typedef t_tyT _tyT;
    typedef t_tySizeType _tySizeType;
    static_assert( !std::numeric_limits< _tySizeType >::is_signed );
    typedef typename std::make_signed<_tySizeType>::type _tySignedSize;

    SegArray() = delete;
    SegArray( _tySizeType _nbySizeSegment = 65536/sizeof( _tyT ) )
        : m_nbySizeSegment( _nbySizeSegment - ( _nbySizeSegment % sizeof( _tyT ) ) ) // even number of t_tyT's.
    {
    }
    SegArray( SegArray const & _r )
        : m_nbySizeSegment( _r.m_nbySizeSegment )
    {
        _r.AssertValid();
        if ( !!_r.m_ppbySegments )
        {
            uint8_t ** ppbySegments = (uint8_t **)malloc( ( _r.m_ppbyEndSegments - _r.m_ppbySegments ) * sizeof(uint8_t *) );
            if ( !ppbySegments )
                THROWNAMEDEXCEPTION( "SegArray::SegArray(const&): OOM for malloc(%lu).", ( _r.m_ppbyEndSegments - _r.m_ppbySegments ) * sizeof(uint8_t *) );
            ns_bienutil::FreeVoid fvFreeSegments( ppbySegments ); // In case we throw below.
            memset( ppbySegments, 0, ( _r.m_ppbyEndSegments - _r.m_ppbySegments ) * sizeof(uint8_t *) ); // not strictly necessary but nice and cheap.
            
            uint8_t ** ppbyCurThis = ppbySegments;
            bool fAllocError;
            _tySizeType nElementsCopied = 0; // This is only used when s_kfOwnLifetime cuz otherwise we don't care.
            {//B
                // Copy all the full and partial segments.
                uint8_t ** ppbyEndDataOther = _r._PpbyGetCurSegment();
                if ( ( ppbyEndDataOther != _r.m_ppbyEndSegments ) && !!*ppbyEndDataOther )
                    ++ppbyEndDataOther;
                uint8_t ** ppbyCurOther = _r.m_ppbySegments;
                for ( ; ppbyCurOther != ppbyEndDataOther; ++ppbyCurOther, ++ppbyCurThis )
                {
                    *ppbyCurThis = (uint8_t*)malloc( m_nbySizeSegment );
                    if ( !*ppbyCurThis )
                        break; // We can't throw here because we have to clean up still.
                    // If we own the object lifetime then we have to copy construct each object:
                    if ( s_kfOwnLifetime )
                    {
                        try
                        {
                            const _tyT * ptCurOther = (const _tyT *)*ppbyCurOther;
                            const _tyT * const ptEndOther = ptCurOther + NElsPerSegment();
                            _tyT * ptCurThis = (_tyT *)*ppbyCurThis;
                            for ( ; ptEndOther != ptCurOther; ++ptCurOther, ++ptCurThis )
                            {
                                new ( ptCurThis ) _tyT( *ptCurOther );
                                if ( ++nElementsCopied == _r.m_nElements )
                                    break; // We have copied all the elements.
                            }
                        }
                        catch ( std::exception const & rexc )
                        {
                            n_SysLog::Log( eslmtError, "SegArray::SegArray(const&): Caught exception copy constructing an element [%s].", rexc.what() );
                            break; // Can't do anything about - we will clean up and throw a separate exception below.
                        }
                    }
                    else // otherwise we can just memcpy.
                    {
                        _tySizeType nbySizeCopy = m_nbySizeSegment;
                        if ( ppbyCurOther+1 == ppbyEndDataOther ) // we are on the last segment.
                        {
                            _tySizeType nLeftOver = _r.m_nElements % NElsPerSegment();
                            nbySizeCopy = nLeftOver * sizeof(_tyT);
                        }
                        memcpy( *ppbyCurThis, *ppbyCurOther, nbySizeCopy );
                    }
                }
                fAllocError = ( ppbyCurOther < ppbyEndDataOther );
            }//EB
            
            if ( fAllocError )
            {
                // Then we need to free any non-null segment pointers allocated above:
                _tySizeType nElementsDestroyed = 0; // This is only used when s_kfOwnLifetime cuz otherwise we don't care.
                uint8_t ** const ppbyEndThis = ppbyCurThis;
                ppbyCurThis = ppbySegments;
                for ( ; ppbyEndThis != ppbyCurThis; ++ppbyCurThis )
                {
                    if ( s_kfOwnLifetime && *ppbyCurThis )
                    {
                        _tyT * ptCurThis = (_tyT *)*ppbyCurThis;
                        const _tyT * const ptEndThis = ptCurThis + NElsPerSegment();
                        for ( ; ptEndThis != ptCurThis; ++ptCurThis )
                        {
                            ptCurThis->~_tyT();
                            if ( ++nElementsDestroyed == nElementsCopied )
                                break; // We have destroyed all the elements.
                        }
                    }
                    free( *ppbyCurThis ); // might be 0 but free doesn't care.
                }
                THROWNAMEDEXCEPTION( s_kfOwnLifetime ? "SegArray::SegArray(const&): OOM or exception copy constructing an element." : "SegArray::SegArray(const&): OOM." ); // The ppbySegments block is freed upon throw.                
            }
            // Success.
            m_ppbySegments = ppbySegments;
            (void)fvFreeSegments.PvTransfer();
            m_ppbyEndSegments = m_ppbySegments + ( _r.m_ppbyEndSegments - _r.m_ppbySegments );
            m_nElements = _r.m_nElements;
        }        
    }
    SegArray( SegArray && _rr )
        : m_nbySizeSegment( _rr.m_nbySizeSegment ) // Hopefully get a non-zero value from this object.
    {
        _rr.AssertValid();
        swap( _rr );
    }
    ~SegArray()
    {
        AssertValid();
         if ( !!m_ppbySegments )
            _Clear();
    }
    void AssertValid() const
#if ASSERTSENABLED
    {
        Assert( !!m_nbySizeSegment && !( m_nbySizeSegment % sizeof( _tyT ) ) );
        Assert( !m_nElements == !m_ppbySegments );
        Assert( !m_nElements == !m_ppbyEndSegments );
        Assert( !m_nElements || !( m_nElements % NElsPerSegment() ) == !*_PpbyGetCurSegment() );
    }
#else //!ASSERTSENABLED
    { }
#endif //!ASSERTSENABLED
    void Clear()
    {
        AssertValid();
        if ( !!m_ppbySegments )
            _Clear();
    }
    void swap( SegArray & _r )
    {
        AssertValid();
        _r.AssertValid();
        std::swap( m_ppbySegments, _r.m_ppbySegments );
        std::swap( m_ppbyEndSegments, _r.m_ppbyEndSegments );
        std::swap( m_nElements, _r.m_nElements );
        std::swap( m_nbySizeSegment, _r.m_nbySizeSegment );
    }
    SegArray & operator = ( const SegArray & _r )
    {
        SegArray saCopy( _r );
        swap( saCopy );
    }
    SegArray & operator = ( SegArray && _rr )
    {
        // Caller doesn't want _rr to have anything in it or they would have just called swap directly.
        // So Close() first.
        Clear();
        swap( _rr );
    }

    // Set the number of elements in this object.
    // If we manage the lifetime of these object then they must have a default constructor.
    void SetSize( _tySizeType _nElements, bool _fCompact = false, _tySizeType _nNewBlockMin = 16 )
    {
        if ( _nElements < m_nElements )
           return SetSizeSmaller( _nElements, _fCompact );
       else
        if ( m_nElements < _nElements )
        {
            AssertValid();
            if ( s_kfOwnLifetime )
            {
                _nElements -= m_nElements;
                while ( _nElements-- )
                {
                    new ( _PbyAllocEnd() ) _tyT();
                    ++m_nElements;
                }
            }
            else
            {
                _tySizeType nBlocksNeeded = ( (_nElements-1) / NElsPerSegment() ) + 1;
                if ( nBlocksNeeded > ( m_ppbyEndSegments - m_ppbySegments ) )
                {
                    _tySizeType nNewBlocks = nBlocksNeeded - ( m_ppbyEndSegments - m_ppbySegments );
                    AllocNewSegmentPointerBlock( ( !_fCompact && ( nNewBlocks < _nNewBlockMin ) ) ? _nNewBlockMin : nNewBlocks );
                    _fCompact = false; // There is no reason to compact since we are adding elements.
                }
                uint8_t ** ppbyInitNewEnd = m_ppbySegments + ((_nElements-1) / NElsPerSegment())+1; // one beyond what we need to init below.
                uint8_t ** ppbyCurAlloc = _PpbyGetCurSegment();
                for ( ; ppbyCurAlloc != ppbyInitNewEnd; ++ppbyCurAlloc )
                {
                    if ( !*ppbyCurAlloc )
                    {
                        *ppbyCurAlloc = (uint8_t*)malloc( NElsPerSegment() * sizeof(_tyT) );
                        if ( !*ppbyCurAlloc )
                            THROWNAMEDEXCEPTION( "SegArray::SetSize(): OOM for malloc(%lu).", NElsPerSegment() * sizeof(_tyT) );
                    }
                }
                Assert( !( _nElements % NElsPerSegment() ) == !*_PpbyGetCurSegment() );
                m_ppbyEndSegments = m_ppbySegments + nBlocksNeeded;
                m_nElements = _nElements;
            }
        }
        if ( _fCompact )
            Compact();
        AssertValid();
    }

    // Set the number of elements in this object less than the current number.
    // If the object contained within doesn't have a default constructor you can still call this method.
    void SetSizeSmaller( _tySizeType _nElements, bool _fCompact = false )
    {
        AssertValid();
        Assert( _nElements <= m_nElements );
        if ( _nElements < m_nElements )
        {
            if ( s_kfOwnLifetime )
            {
                // Then just destruct all the elements:
                while( m_nElements != _nElements )
                    ElGet( --m_nElements ).~_tyT(); // destructors should never throw.
            }
            else
                m_nElements = _nElements;
        }
        if ( _fCompact )
            Compact();
        AssertValid();
    }

    // This will remove segments that aren't in use.
    // We'll just leave the block pointer array the same size - at least for now.
    void Compact()
    {
        AssertValid();
        _tySizeType nBlocksNeeded = ( (m_nElements-1) / NElsPerSegment() ) + 1;
        if ( nBlocksNeeded < ( m_ppbyEndSegments - m_ppbySegments ) )
        {
            uint8_t ** ppbyDealloc = m_ppbySegments + nBlocksNeeded;
            for ( ; ppbyDealloc != m_ppbyEndSegments; ++ppbyDealloc )
            {
                if ( *ppbyDealloc )
                {
                    uint8_t * pbyDealloc = *ppbyDealloc;
                    *ppbyDealloc = 0;
                    free( pbyDealloc );
                }
            }
        }
        AssertValid();
    }

    // We enable insertion for non-contructed types only.
    void Insert( _tySizeType _nPos, const _tyT * _pt, _tySizeType _nEls ) requires ( s_kfNotOwnLifetime )
    {
        AssertValid();
        if ( !_nEls )
            return;
        _tySizeType nElsOld = m_nElements;
        bool fBeyondEnd = ( _nPos >= m_nElements );
        SetSize( ( fBeyondEnd ? _nPos : m_nElements ) + _nEls ); // we support insertion beyond the end.

        // Now we need to move any data that needs moving before we copy in the new data.
        if ( !fBeyondEnd )
        {
            // We will move starting at the end, needless to say, and going backwards.
            _tySizeType stEndDest = m_nElements;
            _tySizeType stEndOrig = nElsOld;
            _tySizeType nElsLeft = nElsOld - _nPos;
            while( !!nElsLeft )
            {
                _tySizeType stBackOffDest = ((stEndDest-1) % NElsPerSegment())+1;
                _tySizeType stBackOffOrig = ((stEndOrig-1) % NElsPerSegment())+1;
                _tySizeType stMin = std::min( nElsLeft, std::min( stBackOffDest, stBackOffOrig ) );
                Assert( stMin ); // We should always have something here.
                memmove( &ElGet( stEndDest - stMin ), &ElGet( stEndOrig - stMin ), stMin * sizeof( _tyT ) );
                nElsLeft -= stMin;
                stEndDest -= stMin;
                stEndOrig -= stMin;
            }
        }

        // And now copy in the new elements - we'll go backwards as well cuz I like it...
        _tySizeType nElsLeft = _nEls;
        _tySizeType stEndDest = _nPos + _nEls;
        const _tyT * ptEndOrig = _pt + _nEls;
        while( !!nElsLeft )
        {
            _tySizeType stBackOffDest = ((stEndDest-1) % NElsPerSegment())+1;
            _tySizeType stMin = std::min( nElsLeft, stBackOffDest );
            Assert( stMin );
            memcpy( &ElGet( stEndDest - stMin ), ptEndOrig - stMin, stMin * sizeof( _tyT ) );
            nElsLeft -= stMin;
            stEndDest -= stMin;
            ptEndOrig -= stMin;
        }
        AssertValid();
    }

    // We enable overwriting for non-contructed types only.
    void Overwrite( _tySizeType _nPos, const _tyT * _pt, _tySizeType _nEls ) requires ( s_kfNotOwnLifetime )
    {
        AssertValid();
        if ( !_nEls )
            return;
        _tySizeType nElsOld = m_nElements;
        if ( ( _nPos + _nEls ) > m_nElements )
            SetSize( _nPos + _nEls );

        // And now copy in the new elements - we'll go backwards as above...
        _tySizeType nElsLeft = _nEls;
        _tySizeType stEndDest = _nPos + _nEls;
        const _tyT * ptEndOrig = _pt + _nEls;
        while( !!nElsLeft )
        {
            _tySizeType stBackOffDest = ((stEndDest-1) % NElsPerSegment())+1;
            _tySizeType stMin = std::min( nElsLeft, stBackOffDest );
            Assert( stMin );
            memcpy( &ElGet( stEndDest - stMin, true ), ptEndOrig - stMin, stMin * sizeof( _tyT ) );
            nElsLeft -= stMin;
            stEndDest -= stMin;
            ptEndOrig -= stMin;
        }
        AssertValid();
    }

    // Read data from the given stream overwriting data in this segmented array.
    // May throw due to allocation error or _rs throwing an EOF error, or perhaps some other reason.
    template < class t_tyStream >
    void OverwriteFromStream( _tySizeType _nPosWrite, t_tyStream const & _rs, _tySizeType _nPosRead, _tySizeType _nElsRead ) noexcept(false) requires ( s_kfNotOwnLifetime )
    {
        AssertValid();
        if ( !_nElsRead )
            return;
        _tySizeType nElsOld = m_nElements;
        if ( ( _nPosWrite + _nElsRead ) > m_nElements )
            SetSize( _nPosWrite + _nElsRead );

        _tySizeType nElsLeft = _nElsRead;
        _tySizeType stCurWrite = _nPosWrite;
        _tySizeType nPosReadCur = _nPosRead;
        while ( !!nElsLeft )
        {
            _tySizeType stFwdOffWrite = NElsPerSegment() - ( stCurWrite % NElsPerSegment() );
            _tySizeType stMin = std::min( nElsLeft, stFwdOffOrig );
            Assert( stMin );
            // We read a stream in bytes.
            _rs.ReadAtPos( &ElGet( stCurWrite ), nPosReadCur * sizeof( _tyT ), stMin * sizeof( _tyT ) ); // Throws on EOF.
            nElsLeft -= stMin;
            stCurWrite += stMin;
            nPosReadCur += stMin;
        }
        AssertValid();
    }

    // We enable reading for non-contructed types only.
    _tySignedSize Read( _tySizeType _nPos, _tyT * _pt, _tySizeType _nEls ) const requires ( s_kfNotOwnLifetime )
    {
        AssertValid();
        if ( !_nEls )
            return 0;
        
        if ( ( _nPos + _nEls ) > m_nElements )
        {
            if ( _nPos >= m_nElements )
                return 0; // nothing to read.
            _nEls -= ( _nPos + _nEls ) - m_nElements;
        }

        // Read em out backwards:
        _tySizeType nElsLeft = _nEls;
        _tySizeType stEndOrig = _nPos + _nEls;
        _tyT * ptEndDest = _pt + _nEls;
        while( !!nElsLeft )
        {
            _tySizeType stBackOffOrig = ((stEndOrig-1) % NElsPerSegment())+1;
            _tySizeType stMin = std::min( nElsLeft, stBackOffOrig );
            Assert( stMin );
            memcpy( ptEndDest - stMin, &ElGet( stEndOrig - stMin ), stMin * sizeof( _tyT ) );
            nElsLeft -= stMin;
            stEndOrig -= stMin;
            ptEndDest -= stMin;
        }
        return _nEls;
    }

    // We allow writing to a file for all types because why not? It might not make sense but you can do it.
    void WriteToFd( int _fd, _tySizeType _nPos = 0, _tySizeType _nElsWrite = std::numeric_limits< _tySizeType >::max() ) const
    {
        AssertValid();
        if ( std::numeric_limits< _tySizeType >::max() == _nElsWrite )
        {
            if ( _nPos > m_nElements )
                THROWNAMEDEXCEPTION( "SegArray::WriteToFd(): Attempt to write data beyond end of segmented array." );
            _nElsWrite = m_nElements - _nPos;
        }
        else
        if ( _nPos + _nElsWrite > m_nElements )
            THROWNAMEDEXCEPTION( "SegArray::WriteToFd(): Attempt to write data beyond end of segmented array." );

        _tySizeType nElsLeft = _nElsWrite;
        _tySizeType stCurOrig = _nPos;
        while ( !!nElsLeft )
        {
            _tySizeType stFwdOffOrig = NElsPerSegment() - ( stCurOrig % NElsPerSegment() );
            _tySizeType stMin = std::min( nElsLeft, stFwdOffOrig );
            Assert( stMin );
            ssize_t sstWrote = ::write( _fd, &ElGet( stCurOrig ), stMin * sizeof(_tyT) );
            if ( -1 == sstWrote )
                THROWNAMEDEXCEPTIONERRNO( errno, "SegArray::WriteToFd(): error writing to fd[%d].", _fd );
            if ( stMin * sizeof(_tyT) != sstWrote )
                THROWNAMEDEXCEPTIONERRNO( errno, "SegArray::WriteToFd(): didn't write all data to fd[%d].", _fd );
            nElsLeft -= stMin;
            stCurOrig += stMin;
        }
    }

    _tySizeType NElements() const
    {
        return m_nElements;
    }
    _tySizeType NElsPerSegment() const
    {
        return m_nbySizeSegment / sizeof( _tyT );
    }

    _tyT & ElGet( _tySizeType _nEl, bool _fMaybeEnd = false )
    {
        AssertValid();
        if ( ( _nEl > m_nElements ) || ( !_fMaybeEnd && ( _nEl == m_nElements ) ) )
            THROWNAMEDEXCEPTION( "SegArray::ElGet(): Out of bounds _nEl[%lu] m_nElements[%lu].", _nEl, m_nElements );
        return ((_tyT*)m_ppbySegments[ _nEl / NElsPerSegment() ])[ _nEl % NElsPerSegment() ];
    }
    _tyT const & ElGet( _tySizeType _nEl ) const
    {
        return const_cast< SegArray* >( this )->ElGet( _nEl );
    }

    template < class... t_vtyArgs >
    _tyT & emplaceAtEnd( t_vtyArgs&&... _args )
    {
        AssertValid();
        _tyT * pt = new( _PbyAllocEnd() ) _tyT( std::forward< t_vtyArgs >( _args )... );
        ++m_nElements;
        return *pt;
    }

protected:
    uint8_t ** _PpbyGetCurSegment() const
    {
        return m_ppbySegments + m_nElements / NElsPerSegment();
    }
    void AllocNewSegmentPointerBlock( _tySizeType _nNewBlocks )
    {
        uint8_t ** ppbySegments = (uint8_t**)realloc( m_ppbySegments, ( ( m_ppbyEndSegments - m_ppbySegments ) + _nNewBlocks ) * sizeof(uint8_t*) );
        if ( !ppbySegments )
            THROWNAMEDEXCEPTION( "SegArray::AllocNewSegmentPointerBlock(): OOM for realloc(%lu).", ( ( m_ppbyEndSegments - m_ppbySegments ) + _nNewBlocks ) * sizeof(uint8_t*) );
        memset( ppbySegments + ( m_ppbyEndSegments - m_ppbySegments ), 0, _nNewBlocks * sizeof(uint8_t*) );
        m_ppbyEndSegments = ppbySegments + ( m_ppbyEndSegments - m_ppbySegments ) + _nNewBlocks;
        m_ppbySegments = ppbySegments;
    }

    uint8_t * _PbyAllocEnd()
    {
        uint8_t ** ppbyCurSegment = _PpbyGetCurSegment();
        if ( ppbyCurSegment == m_ppbyEndSegments )
        {
            static const _tySizeType s_knNumBlocksAlloc = 16;
            AllocNewSegmentPointerBlock( s_knNumBlocksAlloc );
            ppbyCurSegment = _PpbyGetCurSegment();
        }
        if ( !*ppbyCurSegment )
        {
            Assert( !( m_nElements % NElsPerSegment() ) );
            *ppbyCurSegment = (uint8_t*)malloc( m_nbySizeSegment );
            if ( !*ppbyCurSegment )
                THROWNAMEDEXCEPTION( "SegArray::_PbyAllocEnd(): OOM for malloc(%lu).", m_nbySizeSegment );
        }
        return *ppbyCurSegment + ( ( m_nElements % NElsPerSegment() ) * sizeof( _tyT ) );
    }

    void _Clear()
    {
        auto ppbySegments = m_ppbySegments;
        m_ppbySegments = 0;
        _tySizeType nElements = m_nElements;
        m_nElements = 0;
        uint8_t ** ppbyEndData = _PpbyGetCurSegment();
        if ( ( ppbyEndData != m_ppbyEndSegments ) && !!*ppbyEndData )
            ++ppbyEndData;
        m_ppbyEndSegments = 0;
        _tySizeType nElementsDestroyed = 0;
        for ( uint8_t ** ppbyCurThis = ppbySegments; ppbyEndData != ppbyCurThis; ++ppbyCurThis )
        {
            if ( s_kfOwnLifetime )
            {
                Assert( !!*ppbyCurThis );
                _tyT * ptCurThis = (_tyT *)*ppbyCurThis;
                const _tyT * const ptEndThis = ptCurThis + m_nbySizeSegment/sizeof( _tyT );
                for ( ; ptEndThis != ptCurThis; ++ptCurThis )
                {
                    ptCurThis->~_tyT();
                    if ( ++nElementsDestroyed == nElements )
                        break; // We have destroyed all the elements.
                }
            }
            free( *ppbyCurThis );
        }
    }
protected:
    uint8_t ** m_ppbySegments{};
    uint8_t ** m_ppbyEndSegments{};
    _tySizeType m_nElements{};
    _tySizeType m_nbySizeSegment{};
};
