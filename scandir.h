#pragma once

// scandir.h
// Implement a simple class that can be used to produce files names in a directory, etc.
// Note that this is meant to be used with a "uniquely named lambda selector functor".
// Because scandir has no concept of a handle nor can you pass it a void pointer we must
//  ensure that each instantiation is a unique template.
// Note that this imposes several limitations on the usage of any one version of the class.
// We could also fix this by only allowing one thread into NDoScan at any one time with a member mutex.
// dbien: 09MAR2020

#include <sys/types.h>
#include <dirent.h>
#include <string>
#include <memory>

template < class t_tySelector >
class ScanDirectory
{
    typedef ScanDirectory _tyThis;
public:
    ScanDirectory( const char * _pszDir, t_tySelector const & _selector )
        :   m_strDir( _pszDir ),
            m_selector( _selector )
    {
        assert( !s_pThis );
        s_pThis = this;
    }
    ~ScanDirectory()
    {
        _FreeEntries();
        assert( this == s_pThis );
        s_pThis = nullptr;
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
        return s_pThis->FilterDirEnts( _pdeFilter );
    }

    std::string m_strDir;
    struct dirent ** m_ppdeEntries{};
    int m_nEntries{};
    struct dirent ** m_ppdeCurEntry{};
    t_tySelector m_selector;
    bool m_fIncludeDirectories{false};
    bool m_fIncludeCurParentDirs{false}; // Only included if also m_fIncludeDirectories.
    static _tyThis * s_pThis; // This is a pointer to the this pointer for the *unique class*.
};

template < class t_tySelector > ScanDirectory< t_tySelector > *
ScanDirectory< t_tySelector >::s_pThis = nullptr;