
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
// The user of the object owns the lifetime of _fd.
// At most _stbyLenRead contains the number of bytes to read from the file before returning EOF -
//  even if EOF is not encountered. Otherwise reading will read until EOF.
// The rotating buffer has to have at least 2 chunks to operate correctly so that is what we will create it with.
// From then on out it is dynamically sizing up - but doesn't size down except by "handing out" memory chunks.
// In fact since the maximum token length will determine the number of chunks allocated this will cut down
//  on growth of the FdReadRotating object beyond 2 chunks for any length of time.
// The current end of the buffer is register with the current position of the fd.
// When reading from a file the user should set _stbyLenRead to the length of the file wanting to be read and
//  also set _fReadAhead to true which will limit the number of calls to the system call read() and thus
//  improve performance probably.
// Currently we do not support non-blocking fds appropriately.

#include <stdlib.h>
#include <unistd.h>
#include <cerrno>
#include <utility>
#include <limits>

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
  FdReadRotating( int _fd, bool _fReadAhead = false, 
    size_t _stbyLenRead = std::numeric_limits< size_t >::max(), _tySizeType _nbySizeSegment = s_knbySizeSegment )
    : m_saBuffer( _nbySizeSegment ),
      m_fd( _fd ),
      m_stbyLenRead( _stbyLenRead ),
      m_stbyLenRemaining( _stbyLenRead ),
      m_fReadAhead( _fReadAhead )
  {
  }
  // Note that the returned 
  FdReadRotating( FdReadRotating && _rr )
  {
    swap( _rr );
  }
  void swap( FdReadRotating & _r )
  {
    std::swap( m_fd, _r.m_fd );
    std::swap( m_stbyLenRead, _r.m_stbyLenRead );
  }

  void AssertValid()
  {
#ifndef NDEBUG
    m_saBuffer.AssertValid();
    Assert( ( -1 != m_fd ) || ( std::numeric_limits< size_t >::max() == m_stbyLenRead ) );
    Assert( ( std::numeric_limits< size_t >::max() == m_stbyLenRead ) || ( ( m_stbyLenRead - m_stbyLenRemaining ) == m_saBuffer.NElements() ) );
    Assert( m_fReadAhead || ( m_posCur == m_saBuffer.NElements() ) );
    Assert( !m_fReadAhead || !( !( NElements() % m_saBuffer.NElsPerSegment() ) ) );
#endif //!NDEBUG    
  }
  bool FIsNull() const
  {
    AssertValid();
    return -1 == m_fd;
  }

  // Return a character and true, or false for EOF. Throws on read error.
  // Note that this always translates into a potentially blocking read from m_fd - by design.
  // Because we want to be able to operate as a "filter" we block as the user of this object asks
  //  us to.
  bool FGetChar( _tyChar & _rc )
  {
    AssertValid();
    if ( !m_stbyLenRemaining )
      return false;
    if ( m_posCur == m_saBuffer.NElements() )
    {
      if ( !m_fReadAhead )
      {
        errno = 0;
        ssize_t sstRead ::read( m_fd, &_rc, sizeof _rc );
        if ( !sstRead )
          return false; // EOF.
        if ( -1 == sstRead )
          THROWNAMEDEXCEPTIONERRNO( errno, "::read(): 1 char, m_fd[0x%x]", m_fd );
        m_saBuffer.Overwrite( m_posCur++, &_rc, sizeof _rc );
        AssertValid();
        return true;
      }
      else
      {
        // Then we should read ahead by the amount we like to read ahead by - which is s_knSegmentsReadAhead segments.
        Assert( !( m_posCur % m_saBuffer.NElsPerSegment() ) ); // Should always be at a tile boundary when we are reading ahead.
        m_saBuffer.SetSize( m_nPosCur + ( m_saBuffer.NElsPerSegment() * s_knSegmentsReadAhead ) );
        _tySizeType nRead = m_saBuffer.NApplyContiguous( m_posCur, m_posCur + ( m_saBuffer.NElsPerSegment() * s_knSegmentsReadAhead ),
          [this]( _TyChar * _pcBegin, _TyChar * _pcEnd ) -> _tySizeType
          {
            errno = 0;
            ssize_t sstRead ::read( m_fd, _pcBegin, ( _pcEnd - _pcBegin ) * sizeof( _tyChar ) );
            if ( -1 == sstRead )
              THROWNAMEDEXCEPTIONERRNO( errno, "::read(): m_fd[0x%x] m[%lu]", m_fd,( _pcEnd - _pcBegin ) * sizeof( _tyChar ) );
            return (_tySizeType)sstRead;
          }
        );
        if ( !nRead )
          return false;
        nRead = m_saBuffer.Read( m_posCur++, &_rc, sizeof _rc );
        Assert( nRead ); // Should always get something here since we would have failed above.
        AssertValid();
        return true;
      }
    }
    else
    {
      Assert( m_fReadAhead ); // Should always get into the above loop when not reading ahead.
      nRead = m_saBuffer.Read( m_posCur++, &_rc, sizeof _rc );
      Assert( nRead ); // Should always get something here since we would have failed above.
      AssertValid();
      return true;
    }
  }

  // This method causes "consumption" of the data - i.e. it will be transfered to the caller in the passed SegArrayRotatingBuffer.
  // Using the SegArrayRotatingBuffer as the return buffer allows direct passing of already populated chunks.
  // The resultant SegArrayRotatingBuffer is based at _posBegin and is (_posEnd - _posBegin) characters long.
  void ConsumeData( _tySizeType _posBegin, _tySizeType _posEnd, _tySegArray & _rsaConsumed )
  {
    m_saBuffer.CopyOrTransferData( _posBegin, _posEnd, _rsaConsumed );
  }

protected:
  // The current "base" of the SegArrayRotatingBuffer is the beginning of the "view".
  // For non-infinite values of m_stbyLenRead:
  //  ( m_stbyLenRead - m_stbyLenRemaining ) == m_saBuffer.NElements()
  _tySegArray m_saBuffer;
  size_t m_stbyLenRead{std::numeric_limits< size_t >::max()>};
  size_t m_stbyLenRemaining{std::numeric_limits< size_t >::max()>};
  size_t m_posCur{0};
  int m_fd{-1};
  bool m_fReadAhead{false};
};
