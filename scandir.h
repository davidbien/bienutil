#pragma once

// scandir.h
// Implement a simple class that can be used to produce files names in a directory, etc.
// dbien: 09MAR2020

#include <sys/types.h>
#include <dirent.h>
#include <string>
#include <memory>

class ScanDirectory
{
    typedef ScanDirectory _tyThis;
public:
    ScanDirectory( const char * _pszDir )
        : m_strDir( _pszDir )
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
        int nScandir = scandir( m_strDir.c_str(), &ppdeEntries, ScanDirectory::StaticFilterDirEnts, alphasort );
        if ( -1 == nScandir )
            return -1; // Allow caller to deal with repercussions.
        m_nEntries = nScandir;
        m_ppdeCurEntry = m_ppdeEntries = ppdeEntries;
    }

    bool FGetNextEntry( dirent *& _rpdeEntry )
    {
        if ( m_ppdeCurEntry == m_ppdeEntries + m_nEntries )
            return false;
        _rpdeEntry = *m_ppdeCurEntry++;
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
            assert( !!m_nEntries );
            m_ppdeCurEntry = 0;
            struct dirent ** ppdeEntries = m_ppdeEntries;
            m_ppdeEntries = 0;
            int nEntries = m_nEntries;
            m_nEntries = 0;
            // Free each individual pointer:
            struct dirent ** const ppdeEntriesEnd = ppdeEntries + nEntries;
            for ( struct dirent ** ppdeEntriesCur = ppdeEntries; ppdeEntriesEnd != ppdeEntriesCur; ++ppdeEntriesCur )
            {
                assert( !!*ppdeEntriesCur );
                free( *ppdeEntriesCur );
            }
            free( ppdeEntries ); // Free the array of pointers.
        }
    }

    static int StaticFilterDirEnts( const struct dirent * _pdeFilter )
    {
        if ( !!( DT_DIR & _pdeFilter->d_type ) )
        {
            if ( !m_fIncludeDirectories )
                return false;
            if ( !m_fIncludeCurParentDirs && ( !!strcmp( _pdeFilter->d_name, "." ) || !!strcmp( _pdeFilter->d_name, ".." ) ) )
                return false;
        }
        return true;
    }

    std::string m_strDir;
    struct dirent ** m_ppdeEntries{};
    int m_nEntries{};
    struct dirent ** m_ppdeCurEntry{};
    bool m_fIncludeDirectories{false};
    bool m_fIncludeCurParentDirs{false}; // Only included if also m_fIncludeDirectories.
};