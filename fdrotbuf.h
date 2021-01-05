
// fdrotbuf.h
// A file descriptor read using a rotating buffer.
// dbien
// 20DEC2020

// This object backs an fd with a rotating buffer as it is read. 
// The reader then indicates that it has consumed portions of the files and 
//  as this happens buffers are rotated from the front to the back.
// Note that no seeking is done on the fd - only reading.
// There is no assumption that seeking will be succesful (as in seeking of STDIN).
// Thus the postiion where the fd is at when passed to the object is considered 0 position.
// The user of the object owns the lifetime of _hFile.
// At most _stchLenRead contains the number of bytes to read from the file before returning EOF -
//  even if EOF is not encountered. Otherwise reading will read until EOF.
// The rotating buffer has to have at least 2 chunks to operate correctly so that is what we will create it with.
// From then on out it is dynamically sizing up - but doesn't size down except by "handing out" memory chunks.
// In fact since the maximum token length will determine the number of chunks allocated this will cut down
//  on growth of the FdReadRotating object beyond 2 chunks for any length of time.
// The current end of the buffer is register with the current position of the fd.
// When reading from a file the user should set _stchLenRead to the length of the file wanting to be read and
//  also set _fReadAhead to true which will limit the number of calls to the system call read() and thus
//  improve performance probably.
// Currently we do not support non-blocking fds appropriately.

#include <stdlib.h>
#include <unistd.h>
#include <cerrno>
#include <utility>
#include <limits>
#include "segarray.h"

__BIENUTIL_BEGIN_NAMESPACE

// Templatize by character type - which is fine if it is unsigned char for bytes, 
//  but adds convenience for other character types, and even integer types if desired.
template < class t_tyChar >
class FdReadRotating
{
  typedef FdReadRotating _tyThis;
public:
  typedef t_tyChar _tyChar;
  typedef SegArrayRotatingBuffer< _tyChar > _tySegArray;
  typedef typename _tySegArray::_tySizeType _tySizeType;

  FdReadRotating( FdReadRotating const & ) = delete;
  FdReadRotating & operator = ( FdReadRotating const & ) = delete;
  FdReadRotating() = default; // produces an FdReadRotating that FIsNull(). 
  FdReadRotating( _tySizeType _nbySizeSegment = 4096 / sizeof( t_tyChar ) )
    : m_saBuffer( _nbySizeSegment )
  {
  }
  FdReadRotating( FdReadRotating && _rr )
  {
    swap( _rr );
  }
  void swap( FdReadRotating & _r )
  {
    m_saBuffer.swap( _r.m_saBuffer );
    std::swap( m_stchLenRead, _r.m_stchLenRead );
    std::swap( m_stchLenRemaining, _r.m_stchLenRemaining );
    std::swap( m_posCur, _r.m_posCur );
    std::swap( m_hFile, _r.m_hFile );
    std::swap( m_fReadAhead, _r.m_fReadAhead );
  }

  // Initialize - we should be empty.
  void Init( vtyFileHandle _hFile, size_t _posCur = 0, bool _fReadAhead = false, 
    size_t _stchLenRead = (std::numeric_limits<size_t>::max)(), _tySizeType _nbySizeSegment = 4096 / sizeof( t_tyChar ) )
  {
    Assert( !m_saBuffer.FHasAnyCapacity() );
    m_hFile = _hFile;
    m_posCur = _posCur;
    m_stchLenRead = _stchLenRead;
    m_stchLenRemaining = _stchLenRead;
    m_fReadAhead = _fReadAhead;
    m_saBuffer.InitSegmentSize( _nbySizeSegment );
  }

  void AssertValid() const
  {
#if ASSERTSENABLED
    m_saBuffer.AssertValid();
    Assert( ( vkhInvalidFileHandle != m_hFile ) || ( (std::numeric_limits<size_t>::max)() == m_stchLenRead ) );
    Assert( ( (std::numeric_limits<size_t>::max)() == m_stchLenRead ) || ( ( m_stchLenRead - m_stchLenRemaining ) == m_saBuffer.NElements() ) );
    Assert( m_fReadAhead || ( m_posCur == m_saBuffer.NElements() ) );
    Assert( !m_fReadAhead || !( !( m_saBuffer.NElements() % m_saBuffer.NElsPerSegment() ) ) );
#endif //ASSERTSENABLED  
  }
  void AssertValidRange( size_t _posBegin, size_t _posEnd ) const
  {
#if ASSERTSENABLED
    Assert( _posEnd >= _posBegin );
    Assert( _posEnd <= m_posCur );
    Assert( _posBegin >= m_saBuffer.IBaseElement() );
#endif //ASSERTSENABLED  
  }
  bool FIsNull() const
  {
    AssertValid();
    return vkhInvalidFileHandle == m_hFile;
  }
  size_t PosCurrent() const
  {
    return m_posCur;
  }
  size_t PosBase() const
  {
    return m_saBuffer.IBaseElement();
  }
  // Return a character and true, or false for EOF. Throws on read error.
  bool FGetChar( _tyChar & _rc )
  {
    AssertValid();
    if ( !m_stchLenRemaining )
      return false;
    if ( m_posCur == m_saBuffer.NElements() )
    {
      if ( !m_fReadAhead )
      {
        size_t stRead;
        int iReadResult = FileRead( m_hFile, &_rc, sizeof _rc, &stRead );
        if ( -1 == iReadResult )
          THROWNAMEDEXCEPTIONERRNO( GetLastErrNo(), "FileRead(): 1 char, m_hFile[0x%lx]", (uint64_t)m_hFile );
        if ( stRead < sizeof _rc )
        {
          Assert( !stRead );
          return false; // EOF.
        }
        m_saBuffer.Overwrite( m_posCur++, &_rc, sizeof _rc );
        if ( (numeric_limits< size_t >::max)() != m_stchLenRemaining )
          --m_stchLenRemaining;
        AssertValid();
        return true;
      }
      else
      {
        static constexpr size_t s_knSegmentsReadAhead = 4;
        // Then we should read ahead by the amount we like to read ahead by - which is s_knSegmentsReadAhead segments.
        Assert( !( m_posCur % m_saBuffer.NElsPerSegment() ) ); // Should always be at a tile boundary when we are reading ahead.
        _tySizeType sizeAdd = min( m_saBuffer.NElsPerSegment() * s_knSegmentsReadAhead, m_stchLenRemaining );
        if ( !sizeAdd )
          return false;
        m_saBuffer.SetSize( m_posCur + sizeAdd );
        _tySizeType nRead = m_saBuffer.NApplyContiguous( m_posCur, m_posCur + ( m_saBuffer.NElsPerSegment() * s_knSegmentsReadAhead ),
          [this]( _tyChar * _pcBegin, _tyChar * _pcEnd ) -> _tySizeType
          {
            size_t stRead;
            int iReadResult = FileRead( m_hFile, _pcBegin, ( _pcEnd - _pcBegin ) * sizeof( _tyChar ), &stRead );
            if ( -1 == iReadResult )
              THROWNAMEDEXCEPTIONERRNO( GetLastErrNo(), "FileRead(): m_hFile[0x%lx] m[%lu]", (uint64_t)m_hFile, ( _pcEnd - _pcBegin ) * sizeof( _tyChar ) );
            Assert( ( (_tySizeType)stRead / sizeof( _tyChar ) ) == ( _pcEnd - _pcBegin ) );
            return (_tySizeType)stRead / sizeof( _tyChar );
          }
        );
        Assert( nRead == sizeAdd );
        nRead = m_saBuffer.Read( m_posCur++, &_rc, sizeof _rc );
        Assert( nRead ); // Should always get something here since we would have failed above.
        if ( (numeric_limits< size_t >::max)() != m_stchLenRemaining )
          --m_stchLenRemaining;
        AssertValid();
        return true;
      }
    }
    else
    {
      Assert( m_fReadAhead ); // Should always get into the above loop when not reading ahead.
      _tySizeType nRead = m_saBuffer.Read( m_posCur++, &_rc, sizeof _rc );
      Assert( nRead ); // Should always get something here since we would have failed above.
      if ( (numeric_limits< size_t >::max)() != m_stchLenRemaining )
        --m_stchLenRemaining;
      AssertValid();
      return true;
    }
  }

  // This method causes "consumption" of the data - i.e. it will be transfered to the caller in the passed SegArrayRotatingBuffer.
  // _lenBuf characters are consumed starting at PosBase().
  // Using the SegArrayRotatingBuffer as the return buffer allows direct passing of already populated chunks.
  // The resultant SegArrayRotatingBuffer is based at m_saBuffer.IBaseElement() and is (_posEnd - m_saBuffer.IBaseElement()) characters long.
  void ConsumeData( _tyChar * _pcBuf, _tySizeType _lenBuf )
  {
    m_saBuffer.CopyDataAndAdvanceBuffer( m_saBuffer.IBaseElement(), _pcBuf, _lenBuf ); // this consumes data residing in the rotating buffer.
    m_posCur = m_saBuffer.IBaseElement(); // Advance the current position to the end of what was consumed.
  }
  // Discard the data until _posEnd.
  void DiscardData( _tySizeType _posEnd )
  {
    m_saBuffer.SetIBaseEl( _posEnd );
  }
  // Return the data in the buffer between the base of the buffer and m_posCur.
  template < class t_tyString >
  void GetCurrentString( t_tyString & _rstr )
  {
    m_saBuffer.GetString( _rstr,m_saBuffer.IBaseElement(), m_posCur );
  }
protected:
  // The current "base" of the SegArrayRotatingBuffer is the beginning of the "view".
  // For non-infinite values of m_stchLenRead:
  //  ( m_stchLenRead - m_stchLenRemaining ) == m_saBuffer.NElements()
  _tySegArray m_saBuffer;
  size_t m_stchLenRead{(std::numeric_limits<size_t>::max)()};
  size_t m_stchLenRemaining{(std::numeric_limits<size_t>::max)()};
  size_t m_posCur{0};
  vtyFileHandle m_hFile{vkhInvalidFileHandle};
  bool m_fReadAhead{false};
};

__BIENUTIL_END_NAMESPACE
