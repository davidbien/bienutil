#pragma once

// utfconvert.h
// An attempt at providing an efficient piecewise UTFN->UTFM set of methods.
// dbien
// 20FEB2021

__BIENUTIL_BEGIN_NAMESPACE

static constexpr char32_t vkutf32Max = (char32_t)0x10ffff;
static constexpr char32_t vkutf32MaxUTF16 = (char32_t)0xffff;
static constexpr char32_t vkutf32SurrogateStart = (char32_t)0xD800;
static constexpr char32_t vkutf32SurrogateEnd = (char32_t)0xDFFF;
static constexpr char32_t vkutf32SurrogateHighStart = (char32_t)0xD800;
static constexpr char32_t vkutf32SurrogateHighEnd = (char32_t)0xDBFF;
static constexpr char32_t vkutf32SurrogateLowStart = (char32_t)0xDC00;
static constexpr char32_t vkutf32SurrogateLowEnd = (char32_t)0xDFFF;
// We don't want to support anything above vkutf32MaxUTF16 for this so we explicitly limit it to UTF16.
static constexpr char16_t vkutf16ReplacementCharDefault = (char16_t)0xFFFD;
// Use vkutf16ReplacementCharError to indicate to the API to fail when it would otherwise use the replacement character.
static constexpr char16_t vkutf16ReplacementCharError = 0;

static constexpr char32_t vkutf32HalfBase = (char32_t)0x0010000;
static constexpr char32_t vkutf32HalfMask = (char32_t)0x3FF;

inline constexpr bool FIsSurrogate( char32_t _utf32 ) noexcept
{
  return _utf32 >= vkutf32SurrogateStart && _utf32 <= vkutf32SurrogateEnd;
}
inline constexpr bool FIsHighSurrogate( char32_t _utf32 ) noexcept
{
  return _utf32 >= vkutf32SurrogateHighStart && _utf32 <= vkutf32SurrogateHighEnd;
}
inline constexpr bool FIsLowSurrogate( char32_t _utf32 ) noexcept
{
  return _utf32 >= vkutf32SurrogateHighStart && _utf32 <= vkutf32SurrogateHighEnd;
}
inline constexpr bool FInvalidUTF32( char32_t _utf32 ) noexcept
{
  return _utf32 > vkutf32Max || FIsSurrogate( _utf32 );
}
inline constexpr bool FIsValidUTF32( char32_t _utf32 ) noexcept
{
  return !FInvalidUTF32( _utf32 );
}

// traits: These determine input and output buffer sizes so that we don't have to call a method back
template < class t_TyChar >
struct utf_traits;

template <>
struct utf_traits< char >
{
  static const size_t max_length = 4ull;
  static const size_t max = 0xffull; 
};
template <>
struct utf_traits< char8_t >
{
  static const size_t max_length = 4ull;
  static const size_t max = 0xffull; 
};
template <>
struct utf_traits< char16_t >
{
  static const size_t max_length = 2ull;
  static const size_t max = 0xffffull; 
};
template <>
struct utf_traits< char32_t >
{
  static const size_t max_length = 1ull;
  static const size_t max = 0x10ffffull; 
};
template <>
struct utf_traits< wchar_t >
{
#ifdef BIEN_WCHAR_16BIT
  static const size_t max_length = 2ull;
  static const size_t max = 0xffffull; 
#else //!BIEN_WCHAR_16BIT
  static const size_t max_length = 4ull;
  static const size_t max = 0x10ffffull; 
#endif //!BIEN_WCHAR_16BIT
};

template < class t_TyCharType >
struct TGetNormalUTFCharType
{
  typedef t_TyCharType type; // default to the same type.
};
template <>
struct TGetNormalUTFCharType< char >
{
  typedef char8_t type;
};
template <>
struct TGetNormalUTFCharType< wchar_t >
{
#ifdef BIEN_WCHAR_16BIT
  typedef char16_t type;
#else //!BIEN_WCHAR_16BIT
  typedef char32_t type;
#endif //!BIEN_WCHAR_16BIT
};

template < class t_TyCharType >
using TGetNormalUTFCharType_t = typename TGetNormalUTFCharType< t_TyCharType >::type;

static inline constexpr char8_t vrgFirstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

// This one does no error checking - for internal use.
inline char8_t * _PCConvertUTF32ToUTF8( char32_t _utf32Source, char8_t _rgu8Buffer[ utf_traits< char8_t >::max_length ] ) noexcept
{
  Assert( FIsValidUTF32( _utf32Source ) ); // Should never call this with an invalid char.
  size_t nbyWrite;
  if ( _utf32Source < U'\U00000080' )
    nbyWrite = 1;
  else 
  if ( _utf32Source < U'\U00000800' )
    nbyWrite = 2;
  else
  if ( _utf32Source < U'\U00010000' )
    nbyWrite = 3;
  else 
    nbyWrite = 4;
  const char32_t utf32Mask = 0xBF;
  const char32_t utf32Mark = 0x80; 
  char8_t * pu8End = _rgu8Buffer + nbyWrite;
  switch (nbyWrite) 
  {
    case 4:
      *--pu8End = static_cast< char8_t >( ( _utf32Source | utf32Mark ) & utf32Mask );
      _utf32Source >>= 6;
      [[fallthrough]];
    case 3:
      *--pu8End = static_cast< char8_t >( ( _utf32Source | utf32Mark ) & utf32Mask );
      _utf32Source >>= 6;
      [[fallthrough]];
    case 2:
      *--pu8End = static_cast< char8_t >( ( _utf32Source | utf32Mark ) & utf32Mask );
      _utf32Source >>= 6;
      [[fallthrough]];
    case 1: 
      *--pu8End = static_cast< char8_t >( _utf32Source | vrgFirstByteMark[nbyWrite] );
  }
  return _rgu8Buffer + nbyWrite;
}

// FConvertUTF. Do these all noexcept and just use old school per-thread error number that are available on most systems.
// We set the global error number to vkerrInvalidArgument when we encounter a bogus source codepoint.
// Return the end of the conversion wrt _rgu8Buffer, or nullptr on error.
// If the replacement 
// UTF32->UTF8
inline char8_t * PCConvertUTF(  const char32_t *& _rputf32Source, char8_t _rgu8Buffer[ utf_traits< char8_t >::max_length ], const char32_t * const _putf32SourceEnd, 
                                const char16_t _utf16ReplacementChar = vkutf16ReplacementCharDefault ) noexcept
{
  Assert( _rputf32Source < _putf32SourceEnd );
  if ( _rputf32Source >= _putf32SourceEnd )
  {
    SetLastErrNo( vkerrInvalidArgument );
    return nullptr; // We must return nullptr if we don't advance the source buffer.
  }
  char32_t utf32Source = *_rputf32Source;
  if ( FInvalidUTF32( utf32Source ) )
  {
    if ( vkutf16ReplacementCharError == _utf16ReplacementChar )
    {
      // We use invalid argument for this:
      SetLastErrNo( vkerrInvalidArgument );
      return nullptr;
    }
    utf32Source = _utf16ReplacementChar;
  }
  ++_rputf32Source; // We will succeed from here on out.
  return _PCConvertUTF32ToUTF8( utf32Source, _rgu8Buffer );
}
// UTF32->UTF16.
inline char16_t * PCConvertUTF( const char32_t *& _rputf32Source, char16_t _rgu16Buffer[ utf_traits< char16_t >::max_length ], const char32_t * const _putf32SourceEnd, 
                                const char16_t _utf16ReplacementChar = vkutf16ReplacementCharDefault ) noexcept
{
  Assert( _rputf32Source < _putf32SourceEnd );
  if ( _rputf32Source >= _putf32SourceEnd )
  {
    SetLastErrNo( vkerrInvalidArgument );
    return nullptr; // We must return nullptr if we don't advance the source buffer.
  }
  char32_t utf32Source = *_rputf32Source;
  if ( FInvalidUTF32( utf32Source ) )
  {
    if ( vkutf16ReplacementCharError == _utf16ReplacementChar )
    {
      // We use invalid argument for this:
      SetLastErrNo( vkerrInvalidArgument );
      return nullptr;
    }
    utf32Source = _utf16ReplacementChar;
  }
  ++_rputf32Source; // We will succeed from here on out.
  if ( utf32Source <= vkutf32MaxUTF16 )
  {
    *_rgu16Buffer = static_cast<char16_t>( utf32Source );
    return _rgu16Buffer + 1;
  }
  static const char32_t knHalfShift = 10u;
  utf32Source -= vkutf32HalfBase;
  *_rgu16Buffer = static_cast< char16_t >( ( utf32Source >> knHalfShift ) + vkutf32SurrogateHighStart );
  _rgu16Buffer[1] = static_cast< char16_t >( ( utf32Source & vkutf32HalfMask ) + vkutf32SurrogateLowStart );
  return _rgu16Buffer + 2;
}
// UTF16->UTF8
inline char8_t * PCConvertUTF(  const char16_t *& _rputf16Source, char8_t _rgu8Buffer[ utf_traits< char8_t >::max_length ], const char16_t * const _putf16SourceEnd, 
                                const char16_t _utf16ReplacementChar = vkutf16ReplacementCharDefault ) noexcept
{
  Assert( _rputf16Source < _putf16SourceEnd );
  if ( _rputf16Source >= _putf16SourceEnd )
  {
    SetLastErrNo( vkerrInvalidArgument );
    return nullptr; // We must return nullptr if we don't advance the source buffer.
  }
  char32_t utf32Source = *_rputf16Source;
  if ( FIsHighSurrogate( utf32Source ) )
  {
    if ( ( _putf16SourceEnd == _rputf16Source + 1 ) || !FIsLowSurrogate( _rputf16Source[1] ) )
    {
      // No room for low surrogate or no low surrogate present - error or replace:
      if ( vkutf16ReplacementCharError == _utf16ReplacementChar )
      {
        // We use invalid argument for this:
        SetLastErrNo( vkerrInvalidArgument );
        return nullptr;
      }
      utf32Source = _utf16ReplacementChar;
      // don't increment in this case.
    }
    else
    {
      // We have a high and low surrogate - convert to UTF32:
      char32_t utf32Source2 = _rputf16Source[1];
      static const char32_t knHalfShift = 10u;
      utf32Source = ( ( utf32Source - vkutf32SurrogateHighStart ) << knHalfShift ) +
                    ( utf32Source2 - vkutf32SurrogateLowStart ) + vkutf32HalfBase;
      Assert( FIsValidUTF32( utf32Source ) ); // not sure if we can get invalids out of this...
      ++_rputf16Source; // We got both a high and low surrogate - and we will increment below.
    }
  }
  ++_rputf16Source;
  return _PCConvertUTF32ToUTF8( utf32Source, _rgu8Buffer );
}
// UTF16->UTF32: Easy peasy.
inline char32_t * PCConvertUTF( const char16_t *& _rputf16Source, char32_t _rgu32Buffer[ utf_traits< char32_t >::max_length ], const char16_t * const _putf16SourceEnd, 
                                const char16_t _utf16ReplacementChar = vkutf16ReplacementCharDefault ) noexcept
{
  Assert( _rputf16Source < _putf16SourceEnd );
  if ( _rputf16Source >= _putf16SourceEnd )
  {
    SetLastErrNo( vkerrInvalidArgument );
    return nullptr; // We must return nullptr if we don't advance the source buffer.
  }
  *_rgu32Buffer = *_rputf16Source;
  if ( FIsHighSurrogate( *_rgu32Buffer ) )
  {
    if ( ( _putf16SourceEnd == _rputf16Source + 1 ) || !FIsLowSurrogate( _rputf16Source[1] ) )
    {
      // No room for low surrogate or no low surrogate present - error or replace:
      if ( vkutf16ReplacementCharError == _utf16ReplacementChar )
      {
        // We use invalid argument for this:
        SetLastErrNo( vkerrInvalidArgument );
        return nullptr;
      }
      *_rgu32Buffer = _utf16ReplacementChar;
    }
    else
    {
      // We have a high and low surrogate - convert to UTF32:
      char32_t utf32Source2 = _rputf16Source[1];
      static const char32_t knHalfShift = 10u;
      *_rgu32Buffer = ( ( *_rgu32Buffer - vkutf32SurrogateHighStart ) << knHalfShift ) +
                      ( utf32Source2 - vkutf32SurrogateLowStart ) + vkutf32HalfBase;
      Assert( FIsValidUTF32( *_rgu32Buffer ) ); // not sure if we can get invalids out of this...
      ++_rputf16Source; // We got both a high and low surrogate - and we will increment below.
    }
  }
  ++_rputf16Source;
  return _rgu32Buffer + 1;
}

// Now for the reamaing UTF8-> conversions - a bit more involved.
// Trailing bytes for UTF8->UTFN conversion.
// Note that the entries for 4 and 5 bytes indicate invalid UTF8 sequences.
static inline const char8_t vkrgu8TrailBytesUTF8[256] = 
{
    0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,
    0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,
    0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,
    0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,
    0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,
    0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,
    1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,1u,
    2u,2u,2u,2u,2u,2u,2u,2u,2u,2u,2u,2u,2u,2u,2u,2u,3u,3u,3u,3u,3u,3u,3u,3u,4u,4u,4u,4u,5u,5u,5u,5u
};

inline bool FIsTrailByteUTF8( char8_t _u8 ) noexcept
{
  static constexpr char8_t ku8Mask = 0x80;
  return ku8Mask == ( ku8Mask & _u8 );
}

// This just searches for the last trail byte which might be as far as _nchLen.
static const char8_t * PcLastTrailByte( const char8_t * _pu8, size_t _nchLen ) noexcept
{
  const char8_t * pu8Cur = _pu8 + 1;
  for ( const char8_t * pu8End =  _pu8 + _nchLen; ( pu8End != pu8Cur ) && FIsTrailByteUTF8( *pu8Cur ); ++pu8Cur )
    ;
  return pu8Cur;
}
// Check if this is a valid sequence and as well return the end of valid trail bytes afawct.
// nullptr is returned on success
static const char8_t * PcValidUTF8Sequence( const char8_t * _pu8, size_t _nchLen ) noexcept
{
  const char8_t * pu8End = _pu8 + _nchLen;
  switch( _nchLen )
  {
    default:
      return PcLastTrailByte( _pu8, _nchLen ); // Not valid - find the end of the trail bytes.
    case 4:
      if ( !FIsTrailByteUTF8(*--pu8End) )
        return PcLastTrailByte( _pu8, _nchLen );
    [[fallthrough]];
    case 3:
      if ( !FIsTrailByteUTF8(*--pu8End) )
        return PcLastTrailByte( _pu8, _nchLen );
    [[fallthrough]];
    case 2:
      if ( !FIsTrailByteUTF8(*--pu8End) )
        return PcLastTrailByte( _pu8, _nchLen );
      switch (*_pu8) 
      {
          case 0xE0u: 
            if ( *pu8End < 0xA0u ) 
              return _pu8 + _nchLen; // We have found trail bytees the entire way to here.
          break;
          case 0xEDu: 
            if ( *pu8End > 0x9Fu )
              return _pu8 + _nchLen;
          break;
          case 0xF0u:
            if ( *pu8End < 0x90u ) 
              return _pu8 + _nchLen;
          break;
          case 0xF4u:
            if ( *pu8End > 0x8Fu )
              return _pu8 + _nchLen;
          break;
          default:   
            if ( *pu8End < 0x80u ) 
              return _pu8 + _nchLen;
          break;
      }
    [[fallthrough]];
    case 1:
      if ( ( *_pu8 >= 0x80u ) && ( *_pu8 < 0xC2u ) ) 
        return _pu8 + _nchLen;
  }
  return *_pu8 <= 0xF4u ? nullptr : _pu8 + _nchLen;
}

/*
 * Magic values subtracted from a buffer value during UTF8 conversion.
 * This table contains as many values as there might be trailing bytes
 * in a UTF-8 sequence.
 */
static inline const char32_t vkrgutf32OffsetsFromUTF8[6] = { 0x00000000u, 0x00003080u, 0x000E2080u, 
                                                             0x03C82080u, 0xFA082080u, 0x82082080u };


// For all UTF8->UTFN conversions we first produce single UTF32 value.
// Return 0 on failure, return a valid UTF32 codepoint upon success.
// Note that we might return a surrogate value here. The caller needs to check for that.
inline char32_t UTF32ConvertUTF8(  const char8_t *& _rputf8Source, const char8_t * const _putf8SourceEnd, 
                                   const char16_t _utf16ReplacementChar = vkutf16ReplacementCharDefault ) noexcept
{
  Assert( _rputf8Source < _putf8SourceEnd );
  if ( _rputf8Source >= _putf8SourceEnd )
  {
    SetLastErrNo( vkerrInvalidArgument );
    return 0u; // We must return error if we don't advance the source buffer.
  }
  // See how many trail bytes we should have, check for room for such a sequence and if so then
  //  check the validity of the sequence and find the end of an invalid sequence at the same time.
  size_t nbyRemaining = vkrgu8TrailBytesUTF8[*_rputf8Source];
  const char8_t * pucEndTrailBytesOnError = nullptr;
  if ( ( nbyRemaining >= size_t( _putf8SourceEnd - _rputf8Source ) ) ||
       !!( pucEndTrailBytesOnError = PcValidUTF8Sequence( _rputf8Source, nbyRemaining+1 ) ) )
  {
    if ( vkutf16ReplacementCharError == _utf16ReplacementChar )
    {
      // We use overflow for this:
      SetLastErrNo( vkerrOverflow );
      return 0u;
    }
    _rputf8Source = !pucEndTrailBytesOnError ? _putf8SourceEnd : pucEndTrailBytesOnError; // Set end appropriately.
    return _utf16ReplacementChar;
  }
  // Valid sequence.
  Assert( nbyRemaining < 4 ); // valid UTF only after this point - if 5 or 6 bytes become valid here is the place to fix it.
  char32_t utf32Result = 0;
  switch ( nbyRemaining ) 
  {
    case 3: 
      utf32Result += *_rputf8Source++; 
      utf32Result <<= 6u;
    [[fallthrough]];
    case 2: 
      utf32Result += *_rputf8Source++; 
      utf32Result <<= 6u;
    [[fallthrough]];
    case 1: 
      utf32Result += *_rputf8Source++; 
      utf32Result <<= 6u;
    [[fallthrough]];
    case 0: 
      utf32Result += *_rputf8Source++;
  }
  utf32Result -= vkrgutf32OffsetsFromUTF8[ nbyRemaining ];
  return utf32Result;
}

// UTF8->UTF16:
inline char16_t * PCConvertUTF(  const char8_t *& _rputf8Source, char16_t _rgu16Buffer[ utf_traits< char16_t >::max_length ], const char8_t * const _putf8SourceEnd, 
                                 const char16_t _utf16ReplacementChar = vkutf16ReplacementCharDefault ) noexcept
{
  char32_t utf32Result = UTF32ConvertUTF8( _rputf8Source, _putf8SourceEnd, _utf16ReplacementChar );
  if ( !utf32Result )
    return nullptr; // SetLastErrNo() already called.
  if ( FIsSurrogate( utf32Result ) )
  {
    if ( vkutf16ReplacementCharError == _utf16ReplacementChar )
    {
      // We use invalid argument for this:
      SetLastErrNo( vkerrInvalidArgument );
      return nullptr;
    }
    *_rgu16Buffer = _utf16ReplacementChar;
    return _rgu16Buffer + 1;
  }
  Assert( FIsValidUTF32( utf32Result ) );
  // Valid UTF32, might have to split into two UTF16s.
  if ( utf32Result <= vkutf32MaxUTF16 )
  {
    *_rgu16Buffer = static_cast<char16_t>( utf32Result );
    return _rgu16Buffer + 1;
  }
  static const char32_t knHalfShift = 10u;
  utf32Result -= vkutf32HalfBase;
  *_rgu16Buffer = static_cast< char16_t >( ( utf32Result >> knHalfShift ) + vkutf32SurrogateHighStart );
  _rgu16Buffer[1] = static_cast< char16_t >( ( utf32Result & vkutf32HalfMask ) + vkutf32SurrogateLowStart );
  return _rgu16Buffer + 2;
}
// UTF8->UTF32:
inline char32_t * PCConvertUTF(  const char8_t *& _rputf8Source, char32_t _rgu32Buffer[ utf_traits< char32_t >::max_length ], const char8_t * const _putf8SourceEnd, 
                                 const char16_t _utf16ReplacementChar = vkutf16ReplacementCharDefault ) noexcept
{
  char32_t utf32Result = UTF32ConvertUTF8( _rputf8Source, _putf8SourceEnd, _utf16ReplacementChar );
  if ( !utf32Result )
    return nullptr; // SetLastErrNo() already called.
  if ( FIsSurrogate( utf32Result ) )
  {
    if ( vkutf16ReplacementCharError == _utf16ReplacementChar )
    {
      // We use invalid argument for this:
      SetLastErrNo( vkerrInvalidArgument );
      return nullptr;
    }
    *_rgu32Buffer = _utf16ReplacementChar;
    return _rgu32Buffer + 1;
  }
  Assert( FIsValidUTF32( utf32Result ) );
  *_rgu32Buffer = utf32Result;
  return _rgu32Buffer + 1;
}


// We choose not to throw here and just use a bool return to allow the caller to perhaps throw the last error, etc,
//  which would depend on stream impl.
template < class t_TyCharDest, class t_TyCharSource, class t_TyStream >
bool FWriteUTFStream( const t_TyCharSource * _pcSource, size_t _stWrite, t_TyStream & _rstrm,
                      const char16_t _utf16ReplacementChar = vkutf16ReplacementCharDefault ) noexcept
  requires ( TAreSameSizeTypes_v< t_TyCharDest, t_TyCharSource > ) // non-converting.
{
  if ( !_stWrite )
    return true; // no-op.
  return _rstrm.FWrite( _pcSource, _stWrite );
}
template < class t_TyCharDest, class t_TyCharSource, class t_TyStream >
bool FWriteUTFStream( const t_TyCharSource * _pcSource, size_t _stWrite, t_TyStream & _rstrm,
                      const char16_t _utf16ReplacementChar = vkutf16ReplacementCharDefault ) noexcept
  requires ( !TAreSameSizeTypes_v< t_TyCharDest, t_TyCharSource > ) // converting.
{
  if ( !_stWrite )
    return true; // no-op.
  typedef TGetNormalUTFCharType_t< t_TyCharSource > _TyCharSource;
  typedef TGetNormalUTFCharType_t< t_TyCharDest > _TyCharDest;
  typedef utf_traits< _TyCharDest > _TyUTFTraitsDest;
  _TyCharDest rgBufDest[ _TyUTFTraitsDest::max_length ];
  const _TyCharSource * pcSourceCur = (const _TyCharSource *)_pcSource;
  // REVIEW<dbien>: We could implement some short buffering here to avoid calling the stream so much
  //  since the semantics would be exactly the same.
  for ( const _TyCharSource * pcSourceEnd = pcSourceCur + _stWrite; pcSourceEnd != pcSourceCur;  )
  {
    _TyCharDest * pcDestEnd = PCConvertUTF( pcSourceCur, rgBufDest, pcSourceEnd, _utf16ReplacementChar ); // This advances pcSourceCur or it returns an error.
    if ( !pcDestEnd ) // error encountered - SetLastErrNo() should have already been called.
      return false;
    if ( !_rstrm.FWrite( rgBufDest, pcDestEnd - rgBufDest ) )
      return false;
  }
  return true;
}
// Write through a functor which takes the destination character type.
// The prototype of the functor is:
// bool operator()( const t_tyCharDest * _pcBuf, size_t _stLen )
template < class t_TyCharDest, class t_TyCharSource, class t_TyFunctor >
bool FWriteUTFFunctor(  const t_TyCharSource * _pcSource, size_t _stWrite, t_TyFunctor && _rrftor,
                        const char16_t _utf16ReplacementChar = vkutf16ReplacementCharDefault ) noexcept
  requires ( TAreSameSizeTypes_v< t_TyCharDest, t_TyCharSource > ) // non-converting.
{
  if ( !_stWrite )
    return true; // no-op.
  return std::forward< t_TyFunctor >( _rrftor )( _pcSource, _stWrite );
}
template < class t_TyCharDest, class t_TyCharSource, class t_TyFunctor >
bool FWriteUTFFunctor(  const t_TyCharSource * _pcSource, size_t _stWrite, t_TyFunctor && _rrftor,
                        const char16_t _utf16ReplacementChar = vkutf16ReplacementCharDefault ) noexcept
  requires ( !TAreSameSizeTypes_v< t_TyCharDest, t_TyCharSource > ) // converting.
{
  if ( !_stWrite )
    return true; // no-op.
  typedef TGetNormalUTFCharType_t< t_TyCharSource > _TyCharSource;
  typedef TGetNormalUTFCharType_t< t_TyCharDest > _TyCharDest;
  typedef utf_traits< _TyCharDest > _TyUTFTraitsDest;
  _TyCharDest rgBufDest[ _TyUTFTraitsDest::max_length ];
  const _TyCharSource * pcSourceCur = (const _TyCharSource *)_pcSource;
  // REVIEW<dbien>: We could implement some short buffering here to avoid calling the functor so much
  //  since the semantics would be exactly the same.
  for ( const _TyCharSource * pcSourceEnd = pcSourceCur + _stWrite; pcSourceEnd != pcSourceCur;  )
  {
    _TyCharDest * pcDestEnd = PCConvertUTF( pcSourceCur, rgBufDest, pcSourceEnd, _utf16ReplacementChar ); // This advances pcSourceCur or it returns an error.
    if ( !pcDestEnd ) // error encountered - SetLastErrNo() should have already been called.
      return false;
    if ( !std::forward< t_TyFunctor >( _rrftor )( rgBufDest, pcDestEnd - rgBufDest ) )
      return false;
  }
  return true;
}
// Convert as much of the passed string as possible given the destination buffer that has been passed.
// Return the end of the set of characters that has been converted. _rpcDest is left at the end of the converted buffer.
template < class t_TyCharSource, class t_TyCharDest >
const t_TyCharSource * PCConvertString( const t_TyCharSource * _pcSource, size_t _nchSource, t_TyCharDest *& _rpcDest, size_t _nchDest,
                                        const char16_t _utf16ReplacementChar = vkutf16ReplacementCharDefault ) noexcept
  requires ( TAreSameSizeTypes_v< t_TyCharDest, t_TyCharSource > ) // non-converting.
{
  if ( !_nchSource )
    return true; // no-op.
  size_t nchCopy = (min)( _nchSource, _nchDest );
  memcpy( _rpcDest, _pcSource, nchCopy * sizeof( t_TyCharDest ) );
  _rpcDest += nchCopy;
  return _pcSource + nchCopy;
}
template < class t_TyCharSource, class t_TyCharDest >
const t_TyCharSource * PCConvertString( const t_TyCharSource * _pcSource, size_t _nchSource, t_TyCharDest *& _rpcDest, size_t _nchDest,
                                        const char16_t _utf16ReplacementChar = vkutf16ReplacementCharDefault ) noexcept
  requires ( !TAreSameSizeTypes_v< t_TyCharDest, t_TyCharSource > ) // converting.
{
  if ( !_nchSource )
    return _pcSource; // no-op.    
  typedef TGetNormalUTFCharType_t< t_TyCharSource > _TyCharSource;
  typedef TGetNormalUTFCharType_t< t_TyCharDest > _TyCharDest;
  typedef utf_traits< _TyCharDest > _TyUTFTraitsDest;
  if ( _nchDest < _TyUTFTraitsDest::max_length )
  {
    SetLastErrNo( vkerrInvalidArgument );
    return nullptr;
  }
  const _TyCharSource * pcSourceCur = (const _TyCharSource *)_pcSource;
  for ( const _TyCharSource * const pcSourceEnd = pcSourceCur + _nchSource; ( pcSourceEnd != pcSourceCur ) && ( _nchDest >= _TyUTFTraitsDest::max_length );  )
  {
    _TyCharDest * pcDestEnd = PCConvertUTF( pcSourceCur, _rpcDest, pcSourceEnd, _utf16ReplacementChar ); // This advances pcSourceCur or it returns an error.
    if ( !pcDestEnd ) // error encountered - SetLastErrNo() should have already been called.
      return nullptr;
    _nchDest -= ( pcDestEnd - _rpcDest );
    _rpcDest = pcDestEnd;
  }
  return reinterpret_cast< const t_TyCharSource * >( pcSourceCur ); // We may not have finished converting the string.
}

// namespace for conversion using my utfconvert.h conversion methods that correspond to ICU conversion methods in _strutil.h.
namespace ns_CONVBIEN
{

// Note that this methods may throw unlike the above methods.
template < class t_TyStringDest, class t_TyCharSource >
void ConvertString( t_TyStringDest & _rstrDest, const t_TyCharSource * _pcSource, size_t _nchSource = (std::numeric_limits<size_t>::max)(), 
                    const char16_t _utf16ReplacementChar = vkutf16ReplacementCharDefault )
  requires( TAreSameSizeTypes_v< typename t_TyStringDest::value_type, t_TyCharSource > ) // non-converting.
{
  // We assume worst case scenario because the important thing is to just get the string converted.
  if ( _nchSource == (std::numeric_limits<size_t>::max)() )
    _nchSource = StrNLen( _pcSource );
  _rstrDest.assign( _pcSource, _nchSource );
}
template < class t_TyStringDest, class t_TyCharSource >
void ConvertString( t_TyStringDest & _rstrDest, const t_TyCharSource * _pcSource, size_t _nchSource = (std::numeric_limits<size_t>::max)(), 
                    const char16_t _utf16ReplacementChar = vkutf16ReplacementCharDefault )
  requires( !TAreSameSizeTypes_v< typename t_TyStringDest::value_type, t_TyCharSource > ) // non-converting.
{
  typedef TGetNormalUTFCharType_t< t_TyCharSource > _TyCharSource;
  typedef TGetNormalUTFCharType_t< typename t_TyStringDest::value_type > _TyCharDest;
  typedef utf_traits< _TyCharDest > _TyUTFTraitsDest;
  if ( _nchSource == (std::numeric_limits<size_t>::max)() )
    _nchSource = StrNLen( _pcSource );
  // Allocate assuming that every source character is a single code that generates the maximum count of destination characters:
  size_t nchDest = _TyUTFTraitsDest::max_length * _nchSource;
  size_t nbyDest = nchDest * sizeof( _TyCharDest );
  _TyCharDest * pcBufDest;
  if ( nbyDest > vknbyMaxAllocaSize )
  {
    _rstrDest.resize( nchDest );
    pcBufDest = reinterpret_cast< _TyCharDest *>( &_rstrDest[0] );
  }
  else
    pcBufDest = (_TyCharDest*)alloca( nbyDest );
  _TyCharDest * pcEndDest = pcBufDest;
  const t_TyCharSource * pcSrcEnd = PCConvertString( _pcSource, _nchSource, pcEndDest, nchDest, _utf16ReplacementChar );
  Assert( !pcSrcEnd || ( pcSrcEnd == ( _pcSource + _nchSource ) ) ); // We always should have allocated enough space up front.
  if ( !pcSrcEnd )
    THROWNAMEDEXCEPTIONERRNO(GetLastErrNo(), "Error converting string.");
  if ( nbyDest > vknbyMaxAllocaSize )
    _rstrDest.resize( pcEndDest - pcBufDest );
  else
    _rstrDest.assign( reinterpret_cast< typename t_TyStringDest::value_type * >( pcBufDest ), pcEndDest - pcBufDest );
}

} // namespace ns_CONVBIEN

__BIENUTIL_END_NAMESPACE

