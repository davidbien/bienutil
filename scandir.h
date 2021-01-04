#pragma once

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// scandir.h
// Implement a simple class that can be used to produce files names in a directory, etc.
// Due to limitations of scandir we must use a static thread-local-storage pointer to find our
// class during the filter callback inside of scandir().
// dbien: 09MAR2020

#ifndef WIN32
#include <sys/types.h>
#include <dirent.h>
#endif //!WIN32
#include <string>
#include <memory>

__BIENUTIL_BEGIN_NAMESPACE

struct ScanDirectory_SelectAll
{
    bool operator()( const vtyDirectoryEntry & _rdeFilter )
    {
        return true;
    }
};

template < class t_tySelector = ScanDirectory_SelectAll >
class ScanDirectory
{
    typedef ScanDirectory _tyThis;
public:
    ScanDirectory( const char * _pszDir, t_tySelector const & _selector = t_tySelector() )
        :   m_strDir( _pszDir ),
            m_selector( _selector )
    {
      if (m_strDir.back() == TChGetFileSeparator<char>() && m_strDir.length() > 1)
      {
        m_strDir.resize(m_strDir.length() - 1);
      }
    }
    ~ScanDirectory()
    {
        _FreeEntries();
    }

    // Perform or reperform the scan using the current parameters.
    // Return the return value of scandir.
    int NDoScan()
    {
        _FreeEntries();
        vtyDirectoryEntry ** ppdeEntries;
        // We are pretty sure that scandir cannot throw a C++ exception. It could crash, perhaps, but not much we can do about that and C++ object won't be unwound on a crash.
        // Therefore set the TLS pointer for this here since this thread clearly cannot be in this place twice.
        Assert( !s_tls_pThis );
        if ( !!s_tls_pThis )
            return -1; // This code is not reentrant.
        s_tls_pThis = this;
#ifndef WIN32
        int nScandir = scandir( m_strDir.c_str(), &ppdeEntries, ScanDirectory::StaticFilterDirEnts, alphasort );
#else //!WIN32
        int nScandir = _EmulateScanDirWin32(&ppdeEntries);
#endif //!WIN32
        Assert( this == s_tls_pThis );
        s_tls_pThis = nullptr;
        if ( -1 == nScandir )
            return -1; // Allow caller to deal with repercussions.
        m_nEntries = nScandir;
        m_ppdeCurEntry = m_ppdeEntries = ppdeEntries;
        return m_nEntries;
    }

    bool FGetNextEntry( vtyDirectoryEntry *& _rpdeEntry, std::string & _rstrFilePath )
    {
        if ( m_ppdeCurEntry == m_ppdeEntries + m_nEntries )
            return false;
        _rpdeEntry = *m_ppdeCurEntry++;
        _rstrFilePath = m_strDir;
        _rstrFilePath += TChGetFileSeparator<char>();
        _rstrFilePath += PszGetName_DirectoryEntry(*_rpdeEntry);
        return true;
    }
    void ResetEntryIteration()
    {
        m_ppdeCurEntry = m_ppdeEntries;
    }

    std::string const & GetRStrDir() const
    {
        return m_strDir;
    }
    bool FIncludeDirectories()
    {
        return m_fIncludeDirectories;
    }
    void SetFIncludeDirectories( bool _fIncludeDirectories )
    {
        m_fIncludeDirectories = _fIncludeDirectories;
    }

protected:
    void _FreeEntries()
    {
        if ( m_ppdeEntries )
        {
            Assert( !!m_nEntries );
            m_ppdeCurEntry = 0;
            vtyDirectoryEntry ** ppdeEntries = m_ppdeEntries;
            m_ppdeEntries = 0;
            int nEntries = m_nEntries;
            m_nEntries = 0;
            // Free each individual pointer:
            vtyDirectoryEntry ** const ppdeEntriesEnd = ppdeEntries + nEntries;
            for ( vtyDirectoryEntry ** ppdeEntriesCur = ppdeEntries; ppdeEntriesEnd != ppdeEntriesCur; ++ppdeEntriesCur )
            {
                Assert( !!*ppdeEntriesCur );
                free( *ppdeEntriesCur );
            }
            free( ppdeEntries ); // Free the array of pointers.
        }
    }
    int FilterDirEnts( const vtyDirectoryEntry * _pdeFilter )
    {
        if (FIsDir_DirectoryEntry(*_pdeFilter))
        {
            if ( !m_fIncludeDirectories )
                return false;
            if ( !m_fIncludeCurParentDirs && ( !!strcmp(PszGetName_DirectoryEntry(*_pdeFilter), "." ) || !!strcmp(PszGetName_DirectoryEntry(*_pdeFilter), ".." ) ) )
                return false;
        }
        // Now run it against the selector:
        return m_selector( *_pdeFilter );
    }
    static int StaticFilterDirEnts( const vtyDirectoryEntry * _pdeFilter )
    {
        return s_tls_pThis->FilterDirEnts( _pdeFilter );
    }
#ifdef WIN32
    typedef FreeT< vtyDirectoryEntry, false > _tyFreeDirEntObj;
    int _EmulateScanDirWin32( vtyDirectoryEntry*** _pppdeEntries )
    {
      // So, we will use FindFirstFile, ..NextFile, to find the files.
      // We immediately pass it through the filter. If it passes then we added it to the segmented array.
      typedef SegArray< _tyFreeDirEntObj, true_type > _tySegArray;
      _tySegArray saDirEnts;
      vtyDirectoryEntry dirent;
      string strWildcard(m_strDir);
      strWildcard += TChGetFileSeparator<char>();
      strWildcard += "*";
      HANDLE hFindFile = ::FindFirstFileA(strWildcard.c_str(), &dirent);
      if (INVALID_HANDLE_VALUE == hFindFile)
        return -1;
      try
      {
        do
        {
          if (!!FilterDirEnts(&dirent))
          {
            _tyFreeDirEntObj pdeNew((vtyDirectoryEntry*)malloc(sizeof(vtyDirectoryEntry)));
            if (!pdeNew)
            {
              ::FindClose(hFindFile);
              ::SetLastError(ERROR_OUTOFMEMORY);
              return -1;
            }
            memcpy(&*pdeNew, &dirent, sizeof dirent);
            saDirEnts.emplaceAtEnd(std::move(pdeNew));
          }
        } 
        while (FindNextFileA(hFindFile, &dirent));
      }
      catch (...)
      {
        ::FindClose(hFindFile);
        throw;
      }
      ::FindClose(hFindFile);
      // Take what we have found and transfer the pointers to a fixed size array:
      size_t nDirEnts = saDirEnts.NElements();
      *_pppdeEntries = (vtyDirectoryEntry**)malloc(nDirEnts * sizeof(vtyDirectoryEntry*));
      if (!*_pppdeEntries)
      {
        ::SetLastError(ERROR_OUTOFMEMORY);
        return -1;
      }
      vtyDirectoryEntry** ppdeCur = *_pppdeEntries;
      vtyDirectoryEntry** const ppdeEnd = *_pppdeEntries + nDirEnts;
      saDirEnts.ApplyContiguous(0, nDirEnts,
        [&ppdeCur, ppdeEnd](_tyFreeDirEntObj* _pfdeoBegin, _tyFreeDirEntObj* _pfdeoEnd)
        {
          Assert(ppdeCur + (_pfdeoEnd - _pfdeoBegin) <= ppdeEnd);
          _tyFreeDirEntObj* pfdeoCur = _pfdeoBegin;
          for (; _pfdeoEnd != pfdeoCur; ++pfdeoCur)
            *ppdeCur++ = pfdeoCur->PtTransfer();
        }
      );
      return (int)nDirEnts;
    }
#endif //WIN32
    std::string m_strDir;
    vtyDirectoryEntry ** m_ppdeEntries{};
    int m_nEntries{};
    vtyDirectoryEntry ** m_ppdeCurEntry{};
    t_tySelector m_selector;
    bool m_fIncludeDirectories{false};
    bool m_fIncludeCurParentDirs{false}; // Only included if also m_fIncludeDirectories.
    static THREAD_DECL _tyThis * s_tls_pThis; // This is a pointer to the this pointer for the *unique class*.
};

template < class t_tySelector > THREAD_DECL ScanDirectory< t_tySelector > *
ScanDirectory< t_tySelector >::s_tls_pThis = nullptr;

__BIENUTIL_END_NAMESPACE
