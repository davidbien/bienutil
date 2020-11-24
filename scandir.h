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

#include <sys/types.h>
#include <dirent.h>
#include <string>
#include <memory>

struct ScanDirectory_SelectAll
{
    bool operator()( const struct dirent & _rdeFilter )
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
        struct dirent ** ppdeEntries;
        // We are pretty sure that scandir cannot throw a C++ exception. It could crash, perhaps, but not much we can do about that and C++ object won't be unwound on a crash.
        // Therefore set the TLS pointer for this here since this thread clearly cannot be in this place twice.
        Assert( !s_tls_pThis );
        if ( !!s_tls_pThis )
            return -1; // This code is not reentrant.
        s_tls_pThis = this;
        int nScandir = scandir( m_strDir.c_str(), &ppdeEntries, ScanDirectory::StaticFilterDirEnts, alphasort );
        Assert( this == s_tls_pThis );
        s_tls_pThis = nullptr;
        if ( -1 == nScandir )
            return -1; // Allow caller to deal with repercussions.
        m_nEntries = nScandir;
        m_ppdeCurEntry = m_ppdeEntries = ppdeEntries;
        return m_nEntries;
    }

    bool FGetNextEntry( dirent *& _rpdeEntry, std::string & _rstrFilePath )
    {
        if ( m_ppdeCurEntry == m_ppdeEntries + m_nEntries )
            return false;
        _rpdeEntry = *m_ppdeCurEntry++;
        _rstrFilePath = m_strDir;
        _rstrFilePath += "/";
        _rstrFilePath += _rpdeEntry->d_name;
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
            struct dirent ** ppdeEntries = m_ppdeEntries;
            m_ppdeEntries = 0;
            int nEntries = m_nEntries;
            m_nEntries = 0;
            // Free each individual pointer:
            struct dirent ** const ppdeEntriesEnd = ppdeEntries + nEntries;
            for ( struct dirent ** ppdeEntriesCur = ppdeEntries; ppdeEntriesEnd != ppdeEntriesCur; ++ppdeEntriesCur )
            {
                Assert( !!*ppdeEntriesCur );
                free( *ppdeEntriesCur );
            }
            free( ppdeEntries ); // Free the array of pointers.
        }
    }

    int FilterDirEnts( const struct dirent * _pdeFilter )
    {
        if ( !!( DT_DIR & _pdeFilter->d_type ) )
        {
            if ( !m_fIncludeDirectories )
                return false;
            if ( !m_fIncludeCurParentDirs && ( !!strcmp( _pdeFilter->d_name, "." ) || !!strcmp( _pdeFilter->d_name, ".." ) ) )
                return false;
        }
        // Now run it against the selector:
        return m_selector( *_pdeFilter );
    }
    static int StaticFilterDirEnts( const struct dirent * _pdeFilter )
    {
        return s_tls_pThis->FilterDirEnts( _pdeFilter );
    }

    std::string m_strDir;
    struct dirent ** m_ppdeEntries{};
    int m_nEntries{};
    struct dirent ** m_ppdeCurEntry{};
    t_tySelector m_selector;
    bool m_fIncludeDirectories{false};
    bool m_fIncludeCurParentDirs{false}; // Only included if also m_fIncludeDirectories.
    static __thread _tyThis * s_tls_pThis; // This is a pointer to the this pointer for the *unique class*.
};

template < class t_tySelector > __thread ScanDirectory< t_tySelector > *
ScanDirectory< t_tySelector >::s_tls_pThis = nullptr;