#pragma once

// segarray.h
// Segmented array.
// dbien: 20MAR2020

template < class t_tyT, bool t_fOwnLifetime >
class SegArray
{
    typedef SegArray _tyThis;
public:
    static const bool s_fOwnLifetime = t_fOwnLifetime;
    typedef t_tyT _tyT;

    SegArray( uint32_t _nbySizeSegment = 65536/sizeof( _tyT ) )
        : m_nbySizeSegment( _nbySizeSegment - ( _nbySizeSegment % sizeof( _tyT ) ) ) // even number of t_tyT's.
    {
    }
    SegArray( SegArray const & _r )
        : m_nbySizeSegment( _r.m_nbySizeSegment )
    {
        if ( !!_r.m_ppbySegments )
        {
            ppbySegments = (uint8_t **)malloc( ( _r.m_ppbyEndSegments - _r.m_ppbySegments ) * sizeof(uint8_t *) );
            if ( !ppbySegments )
                THROWNAMEDEXCEPTION( "SegArray::SegArray(const&): OOM for malloc(%lu).", ( _r.m_ppbyEndSegments - _r.m_ppbySegments ) * sizeof(uint8_t *) );
            FreeVoid fvFreeSegments( ppbySegments ); // In case we throw below.
            memset( ppbySegments, 0, ( _r.m_ppbyEndSegments - _r.m_ppbySegments ) * sizeof(uint8_t *) ); // not strictly necessary but nice and cheap.
            
            uint8_t ** ppbyCurThis = ppbySegments;
            bool fAllocError;
            uint64_t nElementsCopied = 0; // This is only used when t_fOwnLifetime cuz otherwise we don't care.
            {//B
                // Copy all the full and partial segments.
                uint8_t ** ppbyEndDataOther = _r.m_ppbyCurSegment;
                if ( ( _r.m_ppbyCurSegment != _r.m_ppbyEndSegments ) && !!*_r.m_ppbyCurSegment )
                    ++ppbyEndDataOther;
                for ( uint8_t ** ppbyCurOther = _r.m_ppbySegments; ppbyCurOther != ppbyEndDataOther; ++ppbyCurOther, ++ppbyCurThis )
                {
                    *ppbyCurThis = (uint8_t*)malloc( m_nbySizeSegment );
                    if ( !*ppbyCurThis )
                        break; // We can't throw here because we have to clean up still.
                    // If we own the object lifetime then we have to copy construct each object:
                    if ( t_fOwnLifetime )
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
                        uint32_t nbySizeCopy = m_nbySizeSegment;
                        if ( ppbyCurOther+1 == ppbyEndDataOther ) // we are on the last segment.
                        {
                            uint64_t nLeftOver = _r.m_nElements % NElsPerSegment();
                            nbySizeCopy = nLeftOver * sizeof _tyT;
                        }
                        memcpy( *ppbyCurThis, *ppbyCurOther, nbySizeCopy );
                    }
                }
                fAllocError = ( ppbyCurOther < ppbyEndDataOther );
            }//EB
            
            if ( fAllocError )
            {
                // Then we need to free any non-null segment pointers allocated above:
                uint64_t nElementsDestroyed = 0; // This is only used when t_fOwnLifetime cuz otherwise we don't care.
                uint8_t ** const ppbyEndThis = ppbyCurThis;
                ppbyCurThis = ppbySegments;
                for ( ; ppbyEndThis != ppbyCurThis; ++ppbyCurThis )
                {
                    if ( t_fOwnLifetime && *ppbyCurThis )
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
                THROWNAMEDEXCEPTION( t_fOwnLifetime ? "SegArray::SegArray(const&): OOM or exception copy constructing an element." : "SegArray::SegArray(const&): OOM." ); // The ppbySegments block is freed upon throw.                
            }
            // Success.
            m_ppbySegments = ppbySegments;
            (void)fvFreeSegments.PvTransfer();
            m_ppbyCurSegment = m_ppbySegments + ( _r.m_ppbyCurSegment - _r.m_ppbySegments );
            m_ppbyEndSegments = m_ppbySegments + ( _r.m_ppbyEndSegments - _r.m_ppbySegments );
            m_nElements = _r.m_nElements;
        }        
    }
    ~SegArray()
    {
         if ( !!m_ppbySegments )
            _Clear();
    }
    void Clear()
    {
        if ( !!m_ppbySegments )
            _Clear();
    }

    // Set the number of elements in this object.
    // If we manage the lifetime of these object then they must have a default constructor.
    void SetSize( uint64_t _nElements, bool _fCompact = false, uint64_t _nNewBlockMin = 16 )
    {
        if ( _nElements < m_nElements )
           return SetSizeSmaller( _nElements, _fCompact );
       else
        if ( m_nElements < _nElements )
        {
            if ( t_fOwnLifetime )
            {
                _nElements -= m_nElements;
                while ( _nElements-- )
                {
                    new ( _PbyAllocEnd ) _tyT();
                    ++m_nElements;
                }
            }
            else
            {
                uint64_t nBlocksNeeded = ( (_nElements-1) / NElsPerSegment() ) + 1;
                if ( nBlocksNeeded > ( m_ppbyEndSegments - m_ppbySegments ) )
                {
                    uint64_t nNewBlocks = nBlocksNeeded - ( m_ppbyEndSegments - m_ppbySegments );
                    AllocNewSegmentPointerBlock( ( !_fCompact && ( nNewBlocks < _nNewBlockMin ) ) ? _nNewBlockMin : nNewBlocks );
                    _fCompact = false; // There is no reason to compact since we are adding elements.
                }
                uint8_t ** ppbyInitNewEnd = m_ppbySegments + ((_nElements-1) / NElsPerSegment())+1; // one beyond what we need to init below.
                uint8_t ** ppbyCurAlloc = m_ppbyCurSegment;
                for ( ; ppbyCurAlloc != ppbyInitNewEnd; ++ppbyCurAlloc )
                {
                    if ( !*ppbyCurAlloc )
                    {
                        *ppbyCurAlloc = malloc( NElsPerSegment() * sizeof(_tyT) );
                        if ( !*ppbyCurAlloc )
                            THROWNAMEDEXCEPTION( "SegArray::SetSize(): OOM for malloc(%lu).", NElsPerSegment() * sizeof(_tyT) );
                    }
                }
                m_ppbyCurSegment = m_ppbySegments + _nElements / NElsPerSegment();
                assert( !( _nElements % NElsPerSegment() ) || !!*m_ppbyCurSegment );
                m_ppbyEndSegments = m_ppbySegments + nBlocksNeeded;
            }
        }
        if ( _fCompact )
            Compact();
    }

    // Set the number of elements in this object less than the current number.
    // If the object contained within doesn't have a default constructor you can still call this method.
    void SetSizeSmaller( uint64_t _nElements, bool _fCompact = false )
    {
        assert( _nElements <= m_nElements );
        if ( _nElements < m_nElements )
        {
            if ( t_fOwnLifetime )
            {
                // Then just destruct all the elements:
                while( m_nElements != _nElements )
                    ElGet( --m_nElements ).~_tyT(); // destructors should never throw.
            }
            else
                m_nElements = _nElements;
            m_ppbyCurSegment = m_ppbySegments + ( m_nElements / NElsPerSegment() );
        }
        if ( _fCompact )
            Compact();
    }

    // This will remove segments that aren't in use.
    // We'll just leave the block pointer array the same size - at least for now.
    void Compact()
    {
        uint64_t nBlocksNeeded = ( (m_nElements-1) / NElsPerSegment() ) + 1;
        if ( nBlocksNeeded < ( m_ppbyEndSegments - m_ppbySegments ) )
        {
            uint8_t ** ppbyDealloc = m_ppbySegments + nBlocksNeed;
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
    }

    uint64_t NElements() const
    {
        return m_nElements;
    }
    uint32_t NElsPerSegment() const
    {
        return m_nbySizeSegment / sizeof( _tyT );
    }

    t_tyElement & ElGet( uint64_t _nEl )
    {
        if ( _nEl >= m_nElements )
            THROWNAMEDEXCEPTION( "SegArray::ElGet(): Out of bounds _nEl[%lu] m_nElements[%lu].", _nEl, m_nElements );
        return ((_tyT*)m_ppbySegments[ _nEl / NElsPerSegment() ])[ _nEl % NElsPerSegment() ];
    }
    t_tyElement const & ElGet( uint64_t _nEl ) const
    {
        return const_cast< SegArray* >( this )->ElGet( _nEl );
    }

    template < class... t_vtyArgs >
    _tyT & emplaceAtEnd( t_vtyArgs&&... _args )
    {
        new( _PbyAllocEnd() ) _tyT( std::forward< t_vtyArgs >( _args )... );
        ++m_nElements;
    }

protected:

    void AllocNewSegmentPointerBlock( uint64_t _nNewBlocks )
    {
        uint8_t * ppbySegments = realloc( m_ppbySegments, ( ( m_ppbyEndSegments - m_ppbySegments ) + _nNewBlocks ) * sizeof(uint8_t*) );
        if ( !ppbySegments )
            THROWNAMEDEXCEPTION( "SegArray::AllocNewSegmentPointerBlock(): OOM for realloc(%lu).", ( ( m_ppbyEndSegments - m_ppbySegments ) + _nNewBlocks ) * sizeof(uint8_t*) );
        memset( ppbySegments + ( m_ppbyEndSegments - m_ppbySegments ), 0, _nNewBlocks * sizeof(uint8_t*) )
        m_ppbyCurSegment = ppbySegments + ( m_ppbyCurSegment - m_ppbySegments );
        m_ppbyEndSegments = ppbySegments + ( m_ppbyEndSegments - m_ppbySegments );
        m_ppbySegments = ppbySegments;
    }

    uint8_t * _PbyAllocEnd()
    {
        if ( m_ppbyCurSegment == m_ppbyEndSegments )
        {
            static const uint64_t s_knNumBlocksAlloc = 16;
            AllocNewSegmentPointerBlock( s_knNumBlocksAlloc );
        }
        if ( !*m_ppbyCurSegment )
        {
            assert( !( m_nElements % NElsPerSeg() ) );
            *m_ppbyCurSegment = malloc( m_nbySizeSegment );
            if ( !*m_ppbyCurSegment )
                THROWNAMEDEXCEPTION( "SegArray::_PbyAllocEnd(): OOM for malloc(%lu).", m_nbySizeSegment );
        }
        return *m_ppbyCurSegment + ( ( m_nElements % NElsPerSegment() ) * sizeof( _tyT ) );
    }

    void _Clear()
    {
        auto ppbySegments = m_ppbySegments;
        m_ppbySegments = 0;
        uint64_t nElements = m_nElements;
        m_nElements = 0;
        uint8_t ** ppbyEndData = m_ppbyCurSegment;
        if ( ( m_ppbyCurSegment != m_ppbyEndSegments ) && !!*m_ppbyCurSegment )
            ++ppbyEndData;
        m_ppbyCurSegment = m_ppbyEndSegments = 0;
        uint64_t nElementsDestroyed = 0;
        for ( uint8_t ** ppbyCurThis = ppbySegments; ppbyEndData != ppbyCurThis; ++ppbyCurThis )
        {
            if ( t_fOwnLifetime )
            {
                assert( !!*ppbyCurThis );
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
    uint8_t ** m_ppbySegments{};
    uint8_t ** m_ppbyCurSegment{};
    uint8_t ** m_ppbyEndSegments{};
    uint64_t m_nElements{};
    uint32_t m_nbySizeSegment{};
};