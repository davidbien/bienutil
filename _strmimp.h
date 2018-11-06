#ifndef __STRMIMP_H__
#define __STRMIMP_H__

// _strmimp.h

// IStream implementation template.

namespace ATL
{

template <class T>
struct ATL_NO_VTABLE IStreamImpl : public IStream
{
  // Implementation defines IStreamImpl_HrAccessStream( false ) to perhaps open a stream
  //   to which it is wrapping access.
  enum EStreamAccess { indeterminate, forwrite, forread };
  HRESULT IStreamImpl_HrAccessStream( EStreamAccess _eaccess )
  {
    return !pT->m_pStream ? E_UNEXPECTED : S_OK;
  }

  virtual /* [local] */ HRESULT STDMETHODCALLTYPE Read( 
      /* [length_is][size_is][out] */ void __RPC_FAR *pv,
      /* [in] */ ULONG cb,
      /* [out] */ ULONG __RPC_FAR *pcbRead)
  {
		ATLTRACE2(atlTraceCOM, 0, _T("IStreamImpl::Read\n"));
		T* pT = static_cast<T*>(this);
    return pT->IStreamImpl_Read( pv, cb, pcbRead );
  }
  HRESULT IStreamImpl_Read( void * pv, ULONG cb, ULONG * pcbRead )
  {
		T* pT = static_cast<T*>(this);
    HRESULT hr;
    if ( FAILED( hr = pT->IStreamImpl_HrAccessStream( forread ) ) )
      return hr;
    return pT->m_pStream->Read( pv, cb, pcbRead );
  }
    
  virtual /* [local] */ HRESULT STDMETHODCALLTYPE Write( 
      /* [size_is][in] */ const void __RPC_FAR *pv,
      /* [in] */ ULONG cb,
      /* [out] */ ULONG __RPC_FAR *pcbWritten)
  {
		ATLTRACE2(atlTraceCOM, 0, _T("IStreamImpl::Write\n"));
		T* pT = static_cast<T*>(this);
    return pT->IStreamImpl_Write( pv, cb, pcbWritten );
  }
  HRESULT IStreamImpl_Write( const void * pv, ULONG cb, ULONG * pcbWritten )
  {
		T* pT = static_cast<T*>(this);
    HRESULT hr;
    if ( FAILED( hr = pT->IStreamImpl_HrAccessStream( forwrite ) ) )
      return hr;
    return pT->m_pStream->Write( pv, cb, pcbWritten );
  }

  virtual HRESULT STDMETHODCALLTYPE Seek( 
      /* [in] */ LARGE_INTEGER dlibMove,
      /* [in] */ DWORD dwOrigin,
      /* [out] */ ULARGE_INTEGER __RPC_FAR *plibNewPosition)
  {
		ATLTRACE2(atlTraceCOM, 0, _T("IStreamImpl::Seek\n"));
		T* pT = static_cast<T*>(this);
    return pT->IStreamImpl_Seek( dlibMove, dwOrigin, plibNewPosition );
  }
  HRESULT IStreamImpl_Seek( LARGE_INTEGER dlibMove, DWORD dwOrigin,
                ULARGE_INTEGER * plibNewPosition )
  {
		T* pT = static_cast<T*>(this);
    HRESULT hr;
    if ( FAILED( hr = pT->IStreamImpl_HrAccessStream( indeterminate ) ) )
      return hr;
    return pT->m_pStream->Seek( dlibMove, dwOrigin, plibNewPosition );
  }
  
  virtual HRESULT STDMETHODCALLTYPE SetSize( 
      /* [in] */ ULARGE_INTEGER libNewSize)
  {
		ATLTRACE2(atlTraceCOM, 0, _T("IStreamImpl::SetSize\n"));
		T* pT = static_cast<T*>(this);
    return pT->IStreamImpl_SetSize( libNewSize );
  }
  HRESULT IStreamImpl_SetSize( ULARGE_INTEGER libNewSize )
  {
		T* pT = static_cast<T*>(this);
    HRESULT hr;
    if ( FAILED( hr = pT->IStreamImpl_HrAccessStream( indeterminate ) ) )
      return hr;
    return pT->m_pStream->SetSize( libNewSize );
  }
  
  virtual HRESULT STDMETHODCALLTYPE CopyTo( 
      /* [unique][in] */ IStream __RPC_FAR *pstm,
      /* [in] */ ULARGE_INTEGER cb,
      /* [out] */ ULARGE_INTEGER __RPC_FAR *pcbRead,
      /* [out] */ ULARGE_INTEGER __RPC_FAR *pcbWritten)
  {
		ATLTRACE2(atlTraceCOM, 0, _T("IStreamImpl::CopyTo\n"));
		T* pT = static_cast<T*>(this);
    return pT->IStreamImpl_CopyTo( pstm, cb, pcbRead, pcbWritten );
  }
  HRESULT IStreamImpl_CopyTo( IStream * pstm, ULARGE_INTEGER cb,
                              ULARGE_INTEGER * pcbRead,
                              ULARGE_INTEGER * pcbWritten )
  {
		T* pT = static_cast<T*>(this);
    HRESULT hr;
    if ( FAILED( hr = pT->IStreamImpl_HrAccessStream( forread ) ) )
      return hr;
    return pT->m_pStream->CopyTo( pstm, cb, pcbRead, pcbWritten );
  }
  
  virtual HRESULT STDMETHODCALLTYPE Commit( 
      /* [in] */ DWORD grfCommitFlags)
  {
		ATLTRACE2(atlTraceCOM, 0, _T("IStreamImpl::Commit\n"));
		T* pT = static_cast<T*>(this);
    return pT->IStreamImpl_Commit( grfCommitFlags );
  }
  HRESULT IStreamImpl_Commit( DWORD grfCommitFlags )
  {
		T* pT = static_cast<T*>(this);
    HRESULT hr;
    if ( FAILED( hr = pT->IStreamImpl_HrAccessStream( indeterminate ) ) )
      return hr;
    return pT->m_pStream->Commit( grfCommitFlags );
  }
  
  virtual HRESULT STDMETHODCALLTYPE Revert( void )
  {
		ATLTRACE2(atlTraceCOM, 0, _T("IStreamImpl::Revert\n"));
		T* pT = static_cast<T*>(this);
    return pT->IStreamImpl_Revert( );
  }
  HRESULT IStreamImpl_Revert( void )
  {
		T* pT = static_cast<T*>(this);
    HRESULT hr;
    if ( FAILED( hr = pT->IStreamImpl_HrAccessStream( indeterminate ) ) )
      return hr;
    return pT->m_pStream->Revert( );
  }
  
  virtual HRESULT STDMETHODCALLTYPE LockRegion( 
      /* [in] */ ULARGE_INTEGER libOffset,
      /* [in] */ ULARGE_INTEGER cb,
      /* [in] */ DWORD dwLockType)
  {
		ATLTRACE2(atlTraceCOM, 0, _T("IStreamImpl::LockRegion\n"));
		T* pT = static_cast<T*>(this);
    return pT->IStreamImpl_LockRegion( libOffset, cb, dwLockType );
  }
  HRESULT IStreamImpl_LockRegion( 
      /* [in] */ ULARGE_INTEGER libOffset,
      /* [in] */ ULARGE_INTEGER cb,
      /* [in] */ DWORD dwLockType)
  {
		T* pT = static_cast<T*>(this);
    HRESULT hr;
    if ( FAILED( hr = pT->IStreamImpl_HrAccessStream( indeterminate ) ) )
      return hr;
    return pT->m_pStream->LockRegion( libOffset, cb, dwLockType );
  }
  
  virtual HRESULT STDMETHODCALLTYPE UnlockRegion( 
      /* [in] */ ULARGE_INTEGER libOffset,
      /* [in] */ ULARGE_INTEGER cb,
      /* [in] */ DWORD dwLockType)
  {
		ATLTRACE2(atlTraceCOM, 0, _T("IStreamImpl::UnlockRegion\n"));
		T* pT = static_cast<T*>(this);
    return pT->IStreamImpl_UnlockRegion( libOffset, cb, dwLockType );
  }
  HRESULT IStreamImpl_UnlockRegion( 
      /* [in] */ ULARGE_INTEGER libOffset,
      /* [in] */ ULARGE_INTEGER cb,
      /* [in] */ DWORD dwLockType)
  {
		T* pT = static_cast<T*>(this);
    HRESULT hr;
    if ( FAILED( hr = pT->IStreamImpl_HrAccessStream( indeterminate ) ) )
      return hr;
    return pT->m_pStream->UnlockRegion( libOffset, cb, dwLockType );
  }
  
  virtual HRESULT STDMETHODCALLTYPE Stat( 
      /* [out] */ STATSTG __RPC_FAR *pstatstg,
      /* [in] */ DWORD grfStatFlag)
  {
		ATLTRACE2(atlTraceCOM, 0, _T("IStreamImpl::Stat\n"));
		T* pT = static_cast<T*>(this);
    return pT->IStreamImpl_Stat( pstatstg, grfStatFlag );
  }
  HRESULT IStreamImpl_Stat( STATSTG __RPC_FAR *pstatstg, DWORD grfStatFlag )
  {
		T* pT = static_cast<T*>(this);
    HRESULT hr;
    if ( FAILED( hr = pT->IStreamImpl_HrAccessStream( indeterminate ) ) )
      return hr;
    return pT->m_pStream->Stat( pstatstg, grfStatFlag );
  }
  
  virtual HRESULT STDMETHODCALLTYPE Clone( 
      /* [out] */ IStream __RPC_FAR *__RPC_FAR *ppstm)
  {
		ATLTRACE2(atlTraceCOM, 0, _T("IStreamImpl::Clone\n"));
		T* pT = static_cast<T*>(this);
    return pT->IStreamImpl_Clone( ppstm );
  }
  HRESULT IStreamImpl_Clone( IStream __RPC_FAR *__RPC_FAR *ppstm )
  {
		T* pT = static_cast<T*>(this);
    HRESULT hr;
    if ( FAILED( hr = pT->IStreamImpl_HrAccessStream( indeterminate ) ) )
      return hr;
    return pT->m_pStream->Clone( ppstm );
  }
};

} // namespace ATL

#endif __STRMIMP_H__
