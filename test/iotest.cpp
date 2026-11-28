//          Copyright David Lawrence Bien 1997 - 2026.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// iotest.cpp
// Unit tests of the files bienutil reads and writes with: the memory file and its streams ( memfile.h ), the file
//  functions of the compatibility layer ( _compat.h, _compat.inl ) and the objects which own a file or its mapping
//  ( _fdobjs.h ) - what xmlp reads a document with and the JSON writer writes to.
// dbien
// 27NOV2026

#include "bienutiltest.h"
#include "memfile.h"
#include "fdrotbuf.h"
#include <filesystem>
#include <fstream>
#include <random>
#include <thread>
#ifndef WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

std::string g_strProgramName;

namespace ns_bienutiltest
{
__BIENUTIL_USING_NAMESPACE

static std::string StrTempPath( const char * _pszName )
{
  return ( std::filesystem::path( testing::TempDir() ) / ( std::string( "bienutil_iotest_" ) + _pszName ) ).string();
}
static std::string StrReadFile( const std::string & _rstrPath )
{
  std::ifstream ifs( _rstrPath, std::ios::binary );
  return std::string( ( std::istreambuf_iterator< char >( ifs ) ), std::istreambuf_iterator< char >() );
}
static void WriteFile( const std::string & _rstrPath, const std::string & _rstr )
{
  std::ofstream ofs( _rstrPath, std::ios::binary | std::ios::trunc );
  ofs.write( _rstr.data(), _rstr.size() );
}

// ------------------------------------------------------------------------------------------------------------------
// MemFile, MemStream

typedef MemFileContainer< size_t, false > _TyMfc;
typedef MemStream< size_t, false > _TyMs;

static std::string StrOfStream( _TyMs & _rms )
{
  std::string str( _rms.GetEndPos(), '\0' );
  _rms.Seek( 0, vkSeekBegin );
  EXPECT_EQ( _rms.Read( str.data(), str.length() ), ssize_t( str.length() ) );
  return str;
}

TEST( MemStream, RandomAgainstAString )
{
  std::mt19937 rng( 3 );
  for ( size_t nbyBlock : { 1, 7, 64, 65536 } )
  {
    _TyMfc mfc( nbyBlock );
    _TyMs ms;
    mfc.OpenStream( ms );
    std::string str;
    size_t nPos = 0;
    for ( int nOp = 0; nOp < 2000; ++nOp )
    {
      switch ( rng() % 6 )
      {
      case 0:
      case 1:
      {
        // A write at the position - over what is there, past the end, or beyond the end leaving a gap.
        std::string strWrite( rng() % 50, ' ' );
        for ( char & rch : strWrite )
          rch = char( rng() );
        ASSERT_EQ( ms.Write( strWrite.data(), strWrite.length() ), ssize_t( strWrite.length() ) );
        if ( nPos + strWrite.length() > str.length() )
          str.resize( nPos + strWrite.length(), '\0' );
        str.replace( nPos, strWrite.length(), strWrite );
        // The gap's bytes are undefined: write zeros into both so they compare.
        nPos += strWrite.length();
      }
      break;
      case 2:
      {
        const size_t nTo = rng() % ( str.length() + 1 );
        switch ( rng() % 3 )
        {
        case 0: ASSERT_EQ( ms.Seek( ssize_t( nTo ), vkSeekBegin ), ssize_t( nTo ) ); break;
        case 1: ASSERT_EQ( ms.Seek( ssize_t( nTo ) - ssize_t( nPos ), vkSeekCur ), ssize_t( nTo ) ); break;
        case 2: ASSERT_EQ( ms.Seek( ssize_t( nTo ) - ssize_t( str.length() ), vkSeekEnd ), ssize_t( nTo ) ); break;
        }
        nPos = nTo;
      }
      break;
      case 3:
      {
        std::string strRead( rng() % 60, '#' );
        const ssize_t nRead = ms.Read( strRead.data(), strRead.length() );
        const size_t nExpect = ( std::min )( strRead.length(), str.length() - ( std::min )( nPos, str.length() ) );
        ASSERT_EQ( nRead, ssize_t( nExpect ) );
        ASSERT_EQ( strRead.substr( 0, nExpect ), str.substr( nPos, nExpect ) );
        nPos += nExpect;
      }
      break;
      case 4:
      {
        std::string strIns( rng() % 20, 'i' );
        ASSERT_EQ( ms.Insert( strIns.data(), strIns.length() ), ssize_t( strIns.length() ) );
        str.insert( ( std::min )( nPos, str.length() ), strIns );
        if ( nPos > str.length() - strIns.length() )
          str.resize( nPos + strIns.length(), '\0' );
        nPos += strIns.length();
      }
      break;
      case 5:
      {
        const size_t nBegin = rng() % ( str.length() + 1 ), nEnd = nBegin + rng() % ( str.length() - nBegin + 1 );
        std::string strApplied;
        ms.Apply( nBegin, nEnd, [ & ]( const uint8_t * _pBegin, const uint8_t * _pEnd ) { strApplied.append( (const char *)_pBegin, (const char *)_pEnd ); } );
        ASSERT_EQ( strApplied, str.substr( nBegin, nEnd - nBegin ) );
      }
      break;
      }
      ASSERT_EQ( ms.GetEndPos(), str.length() );
      ASSERT_EQ( ms.GetCurPos(), nPos );
    }
  }
}

TEST( MemStream, SeekAndShare )
{
  _TyMfc mfc( 16 );
  _TyMs msA, msB;
  mfc.OpenStream( msA );
  mfc.OpenStream( msB );
  msA.Write( "shared file", 11 );
  EXPECT_EQ( msB.GetEndPos(), 11u ); // two streams, one file.
  char rgc[ 7 ] = {}; // 6 and the terminator.
  EXPECT_EQ( msB.Read( rgc, 6 ), 6 );
  EXPECT_STREQ( rgc, "shared" );
  EXPECT_THROW( msA.Seek( -1, vkSeekBegin ), std::exception );
  EXPECT_THROW( msA.Seek( -100, vkSeekCur ), std::exception );
  EXPECT_THROW( msA.Seek( -12, vkSeekEnd ), std::exception );
  EXPECT_EQ( msA.Seek( 100, vkSeekBegin ), 100 ); // beyond the end is allowed.
  EXPECT_EQ( msA.Read( rgc, 1 ), 0 );
  _TyMs msNone;
  EXPECT_EQ( msNone.Write( "x", 1 ), -1 ); // not open.
  EXPECT_EQ( msNone.Read( rgc, 1 ), -1 );
  _TyMs msCopy( msA );
  EXPECT_EQ( msCopy.GetCurPos(), 100u );
  msCopy.swap( msNone );
  EXPECT_EQ( msNone.GetEndPos(), 11u );
}

TEST( MemStream, WriteToFile )
{
  _TyMfc mfc( 5 );
  _TyMs ms;
  mfc.OpenStream( ms );
  ms.Write( "0123456789abcdef", 16 );
  const std::string strPath = StrTempPath( "memfile.bin" );
  {
    FileObj fo( CreateWriteOnlyFile( strPath.c_str() ) );
    ASSERT_TRUE( fo.FIsOpen() );
    ms.Seek( 3, vkSeekBegin );
    ms.WriteToFile( fo.HFileGet() ); // from the position to the end.
  }
  EXPECT_EQ( StrReadFile( strPath ), "3456789abcdef" );
  std::filesystem::remove( strPath );
}

// A file which is shared between threads: each writes its own part.
TEST( MemFile, Multithreaded )
{
  MemFile< size_t, true > mf( 32 );
  const size_t knPer = 4096;
  std::vector< std::thread > rgThreads;
  for ( int nThread = 0; nThread < 8; ++nThread )
    rgThreads.emplace_back( [ &mf, nThread ]
                            {
                              const std::string str( knPer, char( 'A' + nThread ) );
                              for ( size_t n = 0; n < knPer; n += 64 )
                                mf.Write( nThread * knPer + n, str.data() + n, 64 );
                            } );
  for ( auto & rthr : rgThreads )
    rthr.join();
  ASSERT_EQ( mf.GetEndPos(), 8 * knPer );
  MemFile< size_t, true > mfCopy( mf );
  for ( int nThread = 0; nThread < 8; ++nThread )
  {
    std::string str( knPer, '\0' );
    ASSERT_EQ( mfCopy.Read( nThread * knPer, str.data(), knPer ), ssize_t( knPer ) );
    EXPECT_EQ( str, std::string( knPer, char( 'A' + nThread ) ) );
  }
  mf.Insert( 0, "ins", 3 );
  EXPECT_EQ( mf.GetEndPos(), 8 * knPer + 3 );
}

// ------------------------------------------------------------------------------------------------------------------
// The file functions.

TEST( Files, ReadWriteSeek )
{
  const std::string strPath = StrTempPath( "rws.bin" );
  {
    FileObj fo( CreateReadWriteFile( strPath.c_str() ) );
    ASSERT_TRUE( fo.FIsOpen() );
    std::string str( 100000, ' ' );
    for ( size_t n = 0; n < str.size(); ++n )
      str[ n ] = char( n * 7 );
    uint64_t nbyWritten = 0;
    ASSERT_EQ( FileWrite( fo.HFileGet(), str.data(), str.size(), &nbyWritten ), 0 );
    EXPECT_EQ( nbyWritten, str.size() );
    EXPECT_EQ( NFileSeekAndThrow( fo.HFileGet(), 0, vkSeekCur ), int64_t( str.size() ) );
    EXPECT_EQ( NFileSeekAndThrow( fo.HFileGet(), 10, vkSeekBegin ), 10 );
    std::string strRead( 20, '\0' );
    uint64_t nbyRead = 0;
    ASSERT_EQ( FileRead( fo.HFileGet(), strRead.data(), strRead.size(), &nbyRead ), 0 );
    EXPECT_EQ( nbyRead, 20u );
    EXPECT_EQ( strRead, str.substr( 10, 20 ) );
    EXPECT_EQ( NFileSeekAndThrow( fo.HFileGet(), -5, vkSeekEnd ), int64_t( str.size() - 5 ) );
    ASSERT_EQ( FileRead( fo.HFileGet(), strRead.data(), strRead.size(), &nbyRead ), 0 );
    EXPECT_EQ( nbyRead, 5u ); // the end.
    ASSERT_EQ( FileRead( fo.HFileGet(), strRead.data(), strRead.size(), &nbyRead ), 0 );
    EXPECT_EQ( nbyRead, 0u );
    EXPECT_THROW( NFileSeekAndThrow( fo.HFileGet(), -1, vkSeekBegin ), std::exception );
    EXPECT_EQ( GetFileSizeFromHandle( fo.HFileGet() ), str.size() );
    vtyHandleAttr ha;
    ASSERT_EQ( GetHandleAttrs( fo.HFileGet(), ha ), 0 );
    EXPECT_TRUE( FIsRegularFile_HandleAttr( ha ) );
    EXPECT_EQ( GetSize_HandleAttr( ha ), str.size() );
    EXPECT_FALSE( FIsConsoleFileHandle( fo.HFileGet() ) );
    // Larger and smaller.
    ASSERT_EQ( FileSetSize( fo.HFileGet(), 200000 ), 0 );
    EXPECT_EQ( GetFileSizeFromHandle( fo.HFileGet() ), 200000u );
    ASSERT_EQ( FileSetSize( fo.HFileGet(), 50 ), 0 );
    EXPECT_EQ( GetFileSizeFromHandle( fo.HFileGet() ), 50u );
  }
  EXPECT_TRUE( FFileExists( strPath.c_str() ) );
  EXPECT_FALSE( FDirectoryExists( strPath.c_str() ) );
  EXPECT_TRUE( FDirectoryExists( testing::TempDir().c_str() ) );
  EXPECT_EQ( FileDelete( strPath.c_str() ), 0 );
  EXPECT_FALSE( FFileExists( strPath.c_str() ) );
  EXPECT_EQ( FileDelete( strPath.c_str() ), 0 ); // not there: no-op.
  EXPECT_EQ( OpenReadOnlyFile( strPath.c_str() ), vkhInvalidFileHandle );
  EXPECT_EQ( FileTouch( strPath.c_str() ), 0 );
  EXPECT_TRUE( FFileExists( strPath.c_str() ) );
  FileDelete( strPath.c_str() );
}

TEST( Files, Mapping )
{
  const std::string strPath = StrTempPath( "map.bin" );
  const size_t knbyGranularity = GetMappingGranularity(); // a page on POSIX, 64 KB on Windows.
  std::string str( 3 * knbyGranularity + 123, ' ' );
  for ( size_t n = 0; n < str.size(); ++n )
    str[ n ] = char( 'a' + n % 26 );
  WriteFile( strPath, str );
  {
    FileObj fo( OpenReadOnlyFile( strPath.c_str() ) );
    ASSERT_TRUE( fo.FIsOpen() );
    uint64_t nbySize = 0;
    FileMappingObj fmo( MapReadOnlyHandle( fo.HFileGet(), &nbySize ) );
    ASSERT_TRUE( fmo.FIsOpen() );
    EXPECT_EQ( nbySize, str.size() );
    EXPECT_EQ( std::string( (const char *)fmo.Pv(), nbySize ), str );
    // At a position: mapped from the granularity before it, the remainder given back.
    uint64_t nbyAt = knbyGranularity + 10, nbySizeAt = 0;
    FileMappingObj fmoAt( MapReadOnlyHandle( fo.HFileGet(), &nbySizeAt, &nbyAt ) );
    ASSERT_TRUE( fmoAt.FIsOpen() );
    EXPECT_EQ( nbyAt, 10u );
    EXPECT_EQ( nbySizeAt, str.size() - knbyGranularity );
    EXPECT_EQ( *fmoAt.Pby( nbyAt ), str[ knbyGranularity + 10 ] );
    // At a position which is a page but not a granularity on Windows - what failed there.
    uint64_t nbyAtPage = GetPageSize() + 3, nbySizeAtPage = 0;
    FileMappingObj fmoAtPage( MapReadOnlyHandle( fo.HFileGet(), &nbySizeAtPage, &nbyAtPage ) );
    ASSERT_TRUE( fmoAtPage.FIsOpen() );
    EXPECT_EQ( *fmoAtPage.Pby( nbyAtPage ), str[ GetPageSize() + 3 ] );
    // At or past the end: refused, whether the size is asked for or not ( on Windows it was refused only when asked ).
    uint64_t nbyBeyond = str.size();
    EXPECT_TRUE( MapReadOnlyHandle( fo.HFileGet(), nullptr, &nbyBeyond ).FFailedMapping() );
    uint64_t nbyBeyond2 = str.size() + 5, nbySizeBeyond = 0;
    EXPECT_TRUE( MapReadOnlyHandle( fo.HFileGet(), &nbySizeBeyond, &nbyBeyond2 ).FFailedMapping() );
  }
  {
    FileObj fo( CreateFileMaybeReadWrite< std::true_type >( StrTempPath( "maprw.bin" ).c_str() ) );
    ASSERT_TRUE( fo.FIsOpen() );
    ASSERT_EQ( FileSetSize( fo.HFileGet(), 4096 ), 0 );
    FileMappingObj fmo( MapReadWriteHandle( fo.HFileGet() ) );
    ASSERT_TRUE( fmo.FIsOpen() );
    memcpy( fmo.Pby( 100 ), "written through the mapping", 27 );
    uint64_t nbyRwBeyond = 4096;
    EXPECT_TRUE( MapReadWriteHandle( fo.HFileGet(), nullptr, &nbyRwBeyond ).FFailedMapping() ); // at the end: refused.
    uint64_t nbyRwBeyond2 = 5000, nbySizeRw = 0;
    EXPECT_TRUE( MapReadWriteHandle( fo.HFileGet(), &nbySizeRw, &nbyRwBeyond2 ).FFailedMapping() );
    uint64_t nbyRwAt = 10, nbySizeRwAt = 0;
    FileMappingObj fmoRwAt( MapReadWriteHandle( fo.HFileGet(), &nbySizeRwAt, &nbyRwAt ) );
    ASSERT_TRUE( fmoRwAt.FIsOpen() );
    EXPECT_EQ( nbyRwAt, 10u );
    EXPECT_EQ( nbySizeRwAt, 4096u );
  }
  EXPECT_EQ( StrReadFile( StrTempPath( "maprw.bin" ) ).substr( 100, 27 ), "written through the mapping" );
  uint64_t nbySize = 0;
  FileMappingObj fmoName( MapReadOnlyFilename( strPath.c_str(), &nbySize ) );
  EXPECT_TRUE( fmoName.FIsOpen() );
  EXPECT_EQ( nbySize, str.size() );
  fmoName.Close();
  EXPECT_FALSE( fmoName.FIsOpen() );
  FileDelete( strPath.c_str() );
  FileDelete( StrTempPath( "maprw.bin" ).c_str() );
}

TEST( Files, Copy )
{
  const std::string strSrc = StrTempPath( "copy_src.bin" ), strDst = StrTempPath( "copy_dst.bin" );
  std::string str( 300000, ' ' );
  for ( size_t n = 0; n < str.size(); ++n )
    str[ n ] = char( n );
  WriteFile( strSrc, str );
  EXPECT_EQ( FileCopy( strDst.c_str(), strSrc.c_str(), true, 4096 ), 0 );
  EXPECT_EQ( StrReadFile( strDst ), str );
  EXPECT_EQ( FileCopyNoThrow( strDst.c_str(), StrTempPath( "no_such_file" ).c_str(), true ), -1 );
#ifndef WIN32
  // A source which can't be read - a directory - is an error, not a copy of nothing.
  EXPECT_NE( FileCopyNoThrow( strDst.c_str(), testing::TempDir().c_str(), true ), 0 );
  EXPECT_FALSE( FFileExists( strDst.c_str() ) ); // deleted as asked.
#endif
  FileDelete( strSrc.c_str() );
  FileDelete( strDst.c_str() );
}

#ifndef WIN32
// Through a pipe: what is written arrives whole, however the reads and writes are divided.
TEST( Files, Pipe )
{
  int rgfd[ 2 ];
  ASSERT_EQ( pipe( rgfd ), 0 );
  std::string str( 1 << 20, ' ' );
  for ( size_t n = 0; n < str.size(); ++n )
    str[ n ] = char( n * 13 );
  std::thread thrWrite( [ & ]
                        {
                          uint64_t nbyWritten = 0;
                          EXPECT_EQ( FileWrite( rgfd[ 1 ], str.data(), str.size(), &nbyWritten ), 0 );
                          EXPECT_EQ( nbyWritten, str.size() );
                          close( rgfd[ 1 ] );
                        } );
  std::string strRead;
  for ( ;; )
  {
    char rgc[ 7919 ];
    uint64_t nbyRead = 0;
    ASSERT_EQ( FileRead( rgfd[ 0 ], rgc, sizeof rgc, &nbyRead ), 0 );
    if ( !nbyRead )
      break;
    strRead.append( rgc, nbyRead );
  }
  thrWrite.join();
  close( rgfd[ 0 ] );
  EXPECT_EQ( strRead, str );
}
#endif

// ------------------------------------------------------------------------------------------------------------------
// FileObj, FileMappingObj

static bool FIsHandleOpen( vtyFileHandle _h )
{
#ifdef WIN32
  DWORD dwFlags;
  return !!GetHandleInformation( _h, &dwFlags );
#else
  return -1 != fcntl( _h, F_GETFD );
#endif
}

// A FileObj which doesn't own its handle doesn't close it - moved, it still doesn't.
TEST( FileObj, OwnershipMoves )
{
  const std::string strPath = StrTempPath( "own.bin" );
  vtyFileHandle h = CreateWriteOnlyFile( strPath.c_str() );
  ASSERT_NE( h, vkhInvalidFileHandle );
  {
    FileObj foNotOwning( h, false );
    FileObj foMoved( std::move( foNotOwning ) );
    EXPECT_EQ( foMoved.HFileGet(), h );
    EXPECT_FALSE( foNotOwning.FIsOpen() );
    FileObj foAssigned;
    foAssigned = std::move( foMoved );
    EXPECT_EQ( foAssigned.HFileGet(), h );
  }
  EXPECT_TRUE( FIsHandleOpen( h ) ) << "closed by a FileObj which didn't own it";
  {
    FileObj foOwning( h );
    FileObj foMoved( std::move( foOwning ) );
  }
  EXPECT_FALSE( FIsHandleOpen( h ) ); // owned: closed once.
  FileObj fo;
  EXPECT_EQ( fo.Close(), 0 );
  fo.SetHFile( OpenReadOnlyFile( strPath.c_str() ) );
  EXPECT_TRUE( fo.FIsOpen() );
  EXPECT_EQ( fo.Close(), 0 );
  EXPECT_FALSE( fo.FIsOpen() );
  FileDelete( strPath.c_str() );
}

TEST( FileMappingObj, OwnershipMoves )
{
  const std::string strPath = StrTempPath( "mapown.bin" );
  WriteFile( strPath, std::string( 100, 'm' ) );
  vtyMappedMemoryHandle hmm = MapReadOnlyFilename( strPath.c_str() );
  ASSERT_FALSE( hmm.FFailedMapping() );
  {
    FileMappingObj fmoNotOwning( hmm, false );
    FileMappingObj fmoMoved( std::move( fmoNotOwning ) );
    EXPECT_EQ( fmoMoved.Pv(), hmm.Pv() );
  }
  EXPECT_EQ( *(const char *)hmm.Pv(), 'm' ) << "unmapped by a FileMappingObj which didn't own it";
  {
    FileMappingObj fmoOwning( hmm );
  }
  FileDelete( strPath.c_str() );
}

// ------------------------------------------------------------------------------------------------------------------
// FdReadRotating: a file read through a rotating buffer, as lexang reads its input.

// Read the whole of a handle: tokens of random lengths consumed or discarded as they are read, some read again.
template < bool t_fReadAhead >
static std::string StrReadRotating( vtyFileHandle _h, uint64_t _nLen, size_t _nbySegment, std::mt19937 & _rrng )
{
  FdReadRotating< char > frr( _nbySegment );
  EXPECT_TRUE( frr.FIsNull() );
  frr.Init( _h, 0, t_fReadAhead, _nLen, _nbySegment );
  std::string strOut;
  for ( ;; )
  {
    const size_t nToken = 1 + _rrng() % 40;
    std::string strToken;
    char ch;
    while ( strToken.length() < nToken && frr.FGetChar( ch ) )
      strToken += ch;
    if ( strToken.empty() )
      break;
    std::string strCurrent;
    frr.GetCurrentString( strCurrent );
    EXPECT_EQ( strCurrent, strToken );
    if ( !( _rrng() % 4 ) )
    {
      // Scanned again from the token's start - as lexang does - then the same characters.
      frr.ResetPositionToBase();
      std::string strAgain;
      for ( size_t n = 0; n < strToken.length() && frr.FGetChar( ch ); ++n )
        strAgain += ch;
      EXPECT_EQ( strAgain, strToken );
    }
    if ( _rrng() & 1 )
    {
      std::string strConsumed( strToken.length(), '#' );
      frr.ConsumeData( strConsumed.data(), strConsumed.length() );
      EXPECT_EQ( strConsumed, strToken );
    }
    else
      frr.DiscardData( frr.PosCurrent() );
    EXPECT_EQ( frr.PosBase(), frr.PosCurrent() );
    strOut += strToken;
  }
  return strOut;
}

TEST( FdReadRotating, File )
{
  FdReadRotating< char > frrDefault; // default constructed - ambiguous before, with two default constructors.
  EXPECT_TRUE( frrDefault.FIsNull() );
  const std::string strPath = StrTempPath( "rotating.txt" );
  std::string str( 100000, ' ' );
  for ( size_t n = 0; n < str.size(); ++n )
    str[ n ] = char( 'a' + n % 26 );
  WriteFile( strPath, str );
  std::mt19937 rng( 7 );
  for ( size_t nbySegment : { 16, 256, 4096 } )
  {
    {
      FileObj fo( OpenReadOnlyFile( strPath.c_str() ) );
      EXPECT_EQ( StrReadRotating< false >( fo.HFileGet(), ( std::numeric_limits< uint64_t >::max )(), nbySegment, rng ), str );
    }
    {
      FileObj fo( OpenReadOnlyFile( strPath.c_str() ) );
      EXPECT_EQ( StrReadRotating< true >( fo.HFileGet(), str.size(), nbySegment, rng ), str ) << nbySegment;
    }
    {
      // A length less than the file: no more than it is read.
      FileObj fo( OpenReadOnlyFile( strPath.c_str() ) );
      EXPECT_EQ( StrReadRotating< true >( fo.HFileGet(), 1000, nbySegment, rng ), str.substr( 0, 1000 ) );
    }
    {
      // Read ahead, of no given length: the end comes as a read shorter than asked - before, what lay past it was
      //  returned as characters.
      FileObj fo( OpenReadOnlyFile( strPath.c_str() ) );
      EXPECT_EQ( StrReadRotating< true >( fo.HFileGet(), ( std::numeric_limits< uint64_t >::max )(), nbySegment, rng ), str ) << nbySegment;
    }
  }
  FileDelete( strPath.c_str() );
}

#ifndef WIN32
// Through a pipe, written in pieces: reads are short without being the end.
TEST( FdReadRotating, Pipe )
{
  std::string str( 50000, ' ' );
  for ( size_t n = 0; n < str.size(); ++n )
    str[ n ] = char( 'A' + n % 23 );
  std::mt19937 rng( 11 );
  for ( bool fReadAhead : { false, true } )
  {
    int rgfd[ 2 ];
    ASSERT_EQ( pipe( rgfd ), 0 );
    std::thread thrWrite( [ & ]
                          {
                            for ( size_t n = 0; n < str.size(); n += 777 )
                            {
                              FileWrite( rgfd[ 1 ], str.data() + n, ( std::min )( size_t( 777 ), str.size() - n ) );
                              std::this_thread::yield();
                            }
                            close( rgfd[ 1 ] );
                          } );
    const std::string strRead = fReadAhead ? StrReadRotating< true >( rgfd[ 0 ], ( std::numeric_limits< uint64_t >::max )(), 4096, rng )
                                           : StrReadRotating< false >( rgfd[ 0 ], ( std::numeric_limits< uint64_t >::max )(), 4096, rng );
    thrWrite.join();
    close( rgfd[ 0 ] );
    EXPECT_EQ( strRead, str ) << fReadAhead;
  }
}
#endif

// Of 16 bit characters with the endian switched.
TEST( FdReadRotating, SwitchEndian )
{
  const std::string strPath = StrTempPath( "rotating16.bin" );
  std::u16string str16( u"\u00e9 sixteen bit characters, swapped" );
  std::u16string str16Swapped( str16 );
  SwitchEndian( str16Swapped.data(), str16Swapped.data() + str16Swapped.length() );
  WriteFile( strPath, std::string( (const char *)str16Swapped.data(), str16Swapped.length() * 2 ) );
  for ( bool fReadAhead : { false, true } )
  {
    FileObj fo( OpenReadOnlyFile( strPath.c_str() ) );
    FdReadRotating< char16_t, true > frr( 64 );
    frr.Init( fo.HFileGet(), 0, fReadAhead, fReadAhead ? str16.length() : ( std::numeric_limits< uint64_t >::max )(), 64 );
    std::u16string str16Read;
    char16_t ch;
    while ( frr.FGetChar( ch ) )
      str16Read += ch;
    EXPECT_EQ( str16Read, str16 ) << fReadAhead;
  }
  FileDelete( strPath.c_str() );
}

} // namespace ns_bienutiltest
