#pragma once

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// memfile.h
// In-memory file and stream objects.
// dbien: 21MAR2020
// Would like to templatize by allocator but I have to propagate it and it's annoying right now. Later.

#include <stdlib.h>
#include <mutex>
#include "segarray.h"

template < class t_tyFilePos = size_t, bool t_kfMultithreaded = false >
class MemFile;
template < class t_tyFilePos = size_t, bool t_kfMultithreaded = false >
class MemFileContainer;
template< class t_tyFilePos = size_t, bool t_kfMultithreaded = false >
class MemStream;

// _MemFileBase:
// This will contains synchronization or not depending on whether we care about multithreading.
template < bool t_kfMultithreaded >
class _MemFileBase
{
    typedef _MemFileBase _tyThis;
protected:
    struct FakeLock { };
    typedef FakeLock _tyLock;
    void LockMutex( _tyLock &) const { }
};

// The multithreaded base.
template <>
class _MemFileBase< true >
{
    typedef _MemFileBase _tyThis;
protected:
    typedef std::unique_lock< std::mutex > _tyLock;
    void LockMutex( _tyLock & _rlock ) const
    {
        _tyLock lock( m_mtx );
        _rlock.swap( lock );
    }
    mutable std::mutex m_mtx; // the mutex.
};

// MemFile:
// This represents the physical file.
// Currently we are going to implement this on top of fixed sized blocks. This is because all the writing into a file
//  usually takes place at the end of the file, not in the middle. There will be significant performance issues with writing into
//  the middle of a file, but it will be implemented.
// Currently as well we are not going to have "floating file positions" - i.e. positions that would move appropriately when you wrote
//  data before them, for instance. We could implement that relatively easily but I don't see the need for it at this point.
//  Instead file positions will be fixed and located purely in the MemStream objects themselves.
template < class t_tyFilePos, bool t_kfMultithreaded >
class MemFile : public _MemFileBase< t_kfMultithreaded >
{
    typedef _MemFileBase< t_kfMultithreaded > _tyBase;
    typedef MemFile _tyThis;
    friend MemStream< t_tyFilePos, t_kfMultithreaded >; 
public:
    typedef t_tyFilePos _tyFilePos;
    typedef typename std::make_signed<_tyFilePos>::type _tySignedFilePos;
    using typename _tyBase::_tyLock;
    typedef MemStream< t_tyFilePos, t_kfMultithreaded > _tyMemStream;

    MemFile( _tyFilePos _sizeBlock = 65536 )
        : m_rgsImpl( _sizeBlock )
    {
    }
    MemFile( MemFile const & _r )
        : m_rgsImpl( _r.m_rgsImpl )
    {
    }

    _tyFilePos GetEndPos() const
    {
        _tyLock lock;
        LockMutex( lock );
        return m_rgsImpl.NElements();
    }

    // As with a file-system file this will not insert data, it will overwrite data. (note that Insert is provide below)
    _tySignedFilePos Write( _tyFilePos _posWrite, const void * _pbyWrite, _tyFilePos _nBytes )
    {
        // If we run out of memory we should throw. There shouldn't be any other reason we should fail - barring A/V.
        _tyLock lock;
        LockMutex( lock );
        m_rgsImpl.Overwrite( _posWrite, (const uint8_t*)_pbyWrite, _nBytes );
        return _nBytes;
    }
    _tySignedFilePos Read( _tyFilePos _posRead, void * _pbyRead, _tyFilePos _nBytes )
    {
        _tyLock lock;
        LockMutex( lock );
        _tySignedFilePos nbyRead = m_rgsImpl.Read( _posRead, (uint8_t*)_pbyRead, _nBytes );
        return nbyRead;
    }

    // Insert data into the data stream. Note that since the underlying impl is a segmented array this can be expensive.
    _tySignedFilePos Insert( _tyFilePos _posInsert, const void * _pbyInsert, _tyFilePos _nBytes )
    {
        // If we run out of memory we should throw. There shouldn't be any other reason we should fail - barring A/V.
        _tyLock lock;
        LockMutex( lock );
        m_rgsImpl.Insert( _posInsert, _pbyInsert, _nBytes );
        return _nBytes;
    }
    void WriteToFd( int _fd, _tyFilePos _nPos = 0, _tyFilePos _nElsWrite = std::numeric_limits< _tyFilePos >::max() ) const
    {
        _tyLock lock;
        LockMutex( lock );
        m_rgsImpl.WriteToFd( _fd, _nPos, _nElsWrite );
    }
protected:
    using _tyBase::LockMutex;
    typedef SegArray< uint8_t, std::false_type, _tyFilePos > _tySegArrayImpl;
    _tySegArrayImpl m_rgsImpl; // segmented array of bytes is impl.
};

// MemFileContainer:
// This contains a memory file. This allows streams to be opened on this MemFile.
// MemFile can be used directly but streams may only be opened via a MemFileContainer.
template < class t_tyFilePos, bool t_kfMultithreaded >
class MemFileContainer
{
    typedef MemFileContainer _tyThis;
public:
    typedef t_tyFilePos _tyFilePos;
    typedef typename std::make_signed<_tyFilePos>::type _tySignedFilePos;
    typedef MemFile< t_tyFilePos, t_kfMultithreaded > _tyMemFile;
    typedef typename _tyMemFile::_tyLock _tyLock;
    typedef MemStream< t_tyFilePos, t_kfMultithreaded > _tyMemStream;

    MemFileContainer( _tyFilePos _sizeBlock = 65536 )
    {
        m_spmfMemFile = std::make_shared<_tyMemFile>( _sizeBlock );
    }

    void OpenStream( _tyMemStream & _rStream )
    {
        _rStream._OpenStream( m_spmfMemFile );
    }
protected:
    std::shared_ptr< _tyMemFile > m_spmfMemFile; // This file that this MemFileContainer contains.
};

// MemStream:
// This represents a single stream which may be opened on a MemFile for reading or writing.
template< class t_tyFilePos, bool t_kfMultithreaded >
class MemStream
{
    typedef MemStream _tyThis;
    friend MemFileContainer< t_tyFilePos, t_kfMultithreaded >;
public:
    typedef t_tyFilePos _tyFilePos;
    typedef typename std::make_signed<_tyFilePos>::type _tySignedFilePos;
    typedef MemFile< t_tyFilePos, t_kfMultithreaded > _tyMemFile;

    MemStream() = default;

    void swap( MemStream & _r )
    {
        _r.m_spmfMemFile.swap( m_spmfMemFile ); // This file to which this MemStream is connected.
        std::swap( _r.m_posCur, m_posCur );
    }

    std::shared_ptr< _tyMemFile > const & GetMemFileSharedPtr() const
    {
        return m_spmfMemFile;
    }

    _tyFilePos GetEndPos() const
    {
        return m_spmfMemFile->GetEndPos();
    }

    // Reuse existing constant SEEK_SET, SEEK_CUR and SEEK_END.
    // Return the resultant position. We do allow the caller to seek beyond the end of the file.
    // We don't allow the position to be set to a negative position and we will throw when that happens.
    _tySignedFilePos Seek( _tySignedFilePos _off, int _iWhence )
    {
        _tyFilePos posNew;
        switch( _iWhence )
        {
            case SEEK_SET:
                if ( _off < 0 )
                    THROWNAMEDEXCEPTION( "SegArray::Seek(): Attempt to SEEK_SET to a negative position." );
                posNew = _off;
                break;
            case SEEK_CUR:
            {
                _tySignedFilePos sPos = m_posCur;
                sPos += _off;
                if ( sPos < 0 )
                    THROWNAMEDEXCEPTION( "SegArray::Seek(): Attempt to SEEK_CUR to a negative position." );
                posNew = sPos;
                break;
            }
            case SEEK_END:
            {
                _tySignedFilePos sPos = m_spmfMemFile->GetEndPos();
                sPos += _off;
                if ( sPos < 0 )
                    THROWNAMEDEXCEPTION( "SegArray::Seek(): Attempt to SEEK_END to a negative position." );
                posNew = sPos;
                break;
            }
            default:
                THROWNAMEDEXCEPTION( "SegArray::Seek(): Bogus _iWhence value [%d].", _iWhence );
        }
        return m_posCur = posNew;
    }
    _tySignedFilePos Write( const void * _pbyWrite, _tyFilePos _nBytes )
    {
        if ( !m_spmfMemFile )
        {
            errno = EBADF;
            return -1;
        }
        _tySignedFilePos posRet = m_spmfMemFile->Write( m_posCur, _pbyWrite, _nBytes );
        if ( -1 == posRet )
            return -1;
        Assert( posRet == _nBytes ); // otherwise we should have throw due to allocation issues.
        m_posCur += posRet;
        return posRet;
    }
    _tySignedFilePos Read( void * _pbyRead, _tyFilePos _nBytes )
    {
        if ( !m_spmfMemFile )
        {
            errno = EBADF;
            return -1;
        }
        _tySignedFilePos posRet = m_spmfMemFile->Read( m_posCur, _pbyRead, _nBytes );
        if ( -1 == posRet )
            return -1;
        m_posCur += posRet;
        return posRet;
    }
    _tySignedFilePos Insert( const void * _pbyInsert, _tyFilePos _nBytes )
    {
        if ( !m_spmfMemFile )
        {
            errno = EBADF;
            return -1;
        }
        _tySignedFilePos posRet = m_spmfMemFile->Insert( m_posCur, _pbyInsert, _nBytes );
        if ( -1 == posRet )
            return -1;
        Assert( posRet == _nBytes ); // otherwise we should have throw due to allocation issues.
        m_posCur += posRet;
        return posRet;
    }
    // This will write from the current position of this memstream until the end of the stream to the FD.
    // That is unless you pass in overriding arguments.
    void WriteToFd( int _fd, _tyFilePos _nPos = std::numeric_limits< _tyFilePos >::max(), _tyFilePos _nElsWrite = std::numeric_limits< _tyFilePos >::max() ) const
    {
        if ( !m_spmfMemFile )
            THROWNAMEDEXCEPTIONERRNO( EBADF, "SegArray::WriteToFd(): Not connected to a file." );
        if ( std::numeric_limits< _tyFilePos >::max() == _nPos )
            _nPos = m_posCur;
        m_spmfMemFile->WriteToFd( _fd, _nPos, _nElsWrite );
    }
    
protected:
    void _OpenStream( std::shared_ptr< _tyMemFile > const & _spmfMemFile )
    {
        m_spmfMemFile = _spmfMemFile;
        m_posCur = 0;
    }
    std::shared_ptr< _tyMemFile > m_spmfMemFile; // This file to which this MemStream is connected.
    _tyFilePos m_posCur{};
};
