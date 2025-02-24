#pragma once 

//          Copyright David Lawrence Bien 1997 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

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
#ifndef WIN32
#include <unistd.h>
#endif //!WIN32
#include <cerrno>
#include <utility>
#include <limits>
#include "segarray.h"

__BIENUTIL_BEGIN_NAMESPACE

// Templatize by character type - which is fine if it is unsigned char for bytes, 
//  but adds convenience for other character types, and even integer types if desired.
template < class t_tyChar, bool t_fSwitchEndian = false >
class FdReadRotating
{
  typedef FdReadRotating _tyThis;
public:
  typedef t_tyChar _tyChar;
  static constexpr bool s_kfSwitchEndian = t_fSwitchEndian;
  typedef SegArrayRotatingBuffer< _tyChar, uint64_t > _tySegArray; // We need to be able to read files larger than 4GB even in 32bit.
  typedef typename _tySegArray::_tySizeType _tySizeType;

  ~FdReadRotating()
  {
  }
  FdReadRotating( FdReadRotating const & ) = delete;
  FdReadRotating & operator = ( FdReadRotating const & ) = delete;
  FdReadRotating() = default; // produces an FdReadRotating that FIsNull(). 
  FdReadRotating( _tySizeType _nbySizeSegment = 4096 / sizeof( t_tyChar ) )
    : m_saBuffer( _nbySizeSegment )
  {
  }
  FdReadRotating( FdReadRotating && _rr ) noexcept
  {
    swap( _rr );
  }
  void swap( FdReadRotating & _r )
  {
    m_saBuffer.swap( _r.m_saBuffer );
    std::swap( m_stchLenRead, _r.m_stchLenRead );
    std::swap( m_posCur, _r.m_posCur );
    std::swap( m_hFile, _r.m_hFile );
    std::swap( m_fReadAhead, _r.m_fReadAhead );
  }

  // Initialize - we should be empty.
  void Init( vtyFileHandle _hFile, _tySizeType _posCur = 0, bool _fReadAhead = false, 
    _tySizeType _stchLenRead = (std::numeric_limits<_tySizeType>::max)(), _tySizeType _nbySizeSegment = 4096 / sizeof( t_tyChar ) )
  {
    Assert( !m_saBuffer.FHasAnyCapacity() );
    m_hFile = _hFile;
    m_posCur = _posCur;
    m_stchLenRead = _stchLenRead;
    m_fReadAhead = _fReadAhead;
    m_saBuffer.InitSegmentSize( _nbySizeSegment );
    AssertValid();
  }
  _tySizeType _NLenRemaining() const
  {
    return m_stchLenRead - ( ( (std::numeric_limits<_tySizeType>::max)() == m_stchLenRead ) ? 0 : m_posCur );
  }
  void AssertValid() const
  {
#if ASSERTSENABLED
    m_saBuffer.AssertValid();
    Assert( ( vkhInvalidFileHandle != m_hFile ) || ( (std::numeric_limits<_tySizeType>::max)() == m_stchLenRead ) );
    Assert( m_stchLenRead >= m_posCur );
    Assert( m_fReadAhead || ( m_posCur == m_saBuffer.NElements() ) );
#endif //ASSERTSENABLED  
  }
  void AssertValidRange( _tySizeType _posBegin, _tySizeType _posEnd ) const
  {
#if ASSERTSENABLED
    Assert( _posEnd >= _posBegin );
    Assert( _posEnd <= m_posCur );
    Assert( m_saBuffer.IBaseElement() >= 0 );
    Assert( _posBegin >= (_tySizeType)m_saBuffer.IBaseElement() );
#endif //ASSERTSENABLED  
  }
  bool FIsNull() const
  {
    AssertValid();
    return vkhInvalidFileHandle == m_hFile;
  }
  _tySizeType PosCurrent() const
  {
    return m_posCur;
  }
  _tySizeType PosBase() const
  {
    return m_saBuffer.IBaseElement();
  }
  void ResetPositionToBase()
  {
    m_posCur = PosBase();
  }
  // Return a character and true, or false for EOF. Throws on read error.
  bool FGetChar( _tyChar & _rc )
  {
    AssertValid();
    if ( !_NLenRemaining() )
      return false;
    if ( m_posCur == m_saBuffer.NElements() )
    {
      if ( !m_fReadAhead )
      {
        _tySizeType stRead;
        int iReadResult = FileRead( m_hFile, &_rc, sizeof _rc, &stRead );
        if ( -1 == iReadResult )
          THROWNAMEDEXCEPTIONERRNO( GetLastErrNo(), "FileRead(): 1 char, m_hFile[0x%zx]", (size_t)m_hFile );
        if ( stRead < sizeof _rc )
        {
          Assert( !stRead );
          return false; // EOF.
        }
        if ( s_kfSwitchEndian )
          SwitchEndian( _rc );
        m_saBuffer.Overwrite( m_posCur++, &_rc, sizeof _rc );
        AssertValid();
        return true;
      }
      else
      {
        static constexpr _tySizeType s_knSegmentsReadAhead = 4;
        // Then we should read ahead by the amount we like to read ahead by - which is s_knSegmentsReadAhead segments.
        Assert( !( m_posCur % m_saBuffer.NElsPerSegment() ) ); // Should always be at a tile boundary when we are reading ahead.
        _tySizeType sizeAdd = min( m_saBuffer.NElsPerSegment() * s_knSegmentsReadAhead, _NLenRemaining() );
        if ( !sizeAdd )
          return false;
        m_saBuffer.SetSize( m_posCur + sizeAdd );
        _tySizeType nRead = m_saBuffer.NApplyContiguous( m_posCur, m_posCur + sizeAdd,
          [this]( _tyChar * _pcBegin, _tyChar * _pcEnd ) -> _tySizeType
          {
            _tySizeType stRead;
            int iReadResult = FileRead( m_hFile, _pcBegin, ( _pcEnd - _pcBegin ) * sizeof( _tyChar ), &stRead );
            if ( -1 == iReadResult )
              THROWNAMEDEXCEPTIONERRNO( GetLastErrNo(), "FileRead(): m_hFile[0x%zx] m[%zu]", (size_t)m_hFile, ( _pcEnd - _pcBegin ) * sizeof( _tyChar ) );
            if ( s_kfSwitchEndian )
              SwitchEndian( _pcBegin, _pcEnd );
            Assert( ( (_tySizeType)stRead / sizeof( _tyChar ) ) == ( _pcEnd - _pcBegin ) );
            return (_tySizeType)stRead / sizeof( _tyChar );
          }
        );
        Assert( nRead == sizeAdd );
        nRead = m_saBuffer.Read( m_posCur++, &_rc, 1 );
        Assert( nRead ); // Should always get something here since we would have failed above.
        AssertValid();
        return true;
      }
    }
    else
    {
      Assert( m_fReadAhead ); // Should always get into the above loop when not reading ahead.
      _tySizeType nRead = m_saBuffer.Read( m_posCur++, &_rc, 1 );
      Assert( nRead ); // Should always get something here since we would have failed above.
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
    AssertValid();
    m_saBuffer.CopyDataAndAdvanceBuffer( m_saBuffer.IBaseElement(), _pcBuf, _lenBuf ); // this consumes data residing in the rotating buffer.
    m_posCur = m_saBuffer.IBaseElement(); // Advance the current position to the end of what was consumed.
    AssertValid();
  }
  // Discard the data until _posEnd.
  void DiscardData( _tySizeType _posEnd )
  {
    AssertValid();
    m_saBuffer.SetIBaseEl( _posEnd );
    m_posCur = _posEnd;
    AssertValid();
  }
  // Return the data in the buffer between the base of the buffer and m_posCur.
  template < class t_tyString >
  void GetCurrentString( t_tyString & _rstr ) const
  {
    AssertValid();
    m_saBuffer.GetString( _rstr, (_tySizeType)m_saBuffer.IBaseElement(), m_posCur );
  }
  bool FSpanChars( _tySizeType _posBegin, _tySizeType _posEnd, const _tyChar * _pszCharSet ) const
  {
    AssertValid();
    return m_saBuffer.FSpanChars( _posBegin, _posEnd, _pszCharSet );
  }
  bool FMatchString( _tySizeType _posBegin, _tySizeType _posEnd, const _tyChar * _pszMatch ) const
  {
    AssertValid();
    return m_saBuffer.FMatchString( _posBegin, _posEnd, _pszMatch );
  }
protected:
  // The current "base" of the SegArrayRotatingBuffer is the beginning of the "view".
  _tySegArray m_saBuffer;
  _tySizeType m_stchLenRead{(std::numeric_limits<_tySizeType>::max)()};
  _tySizeType m_posCur{0};
  vtyFileHandle m_hFile{vkhInvalidFileHandle};
  bool m_fReadAhead{false};
};

__BIENUTIL_END_NAMESPACE
