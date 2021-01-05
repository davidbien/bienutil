#pragma once
//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

__BIENUTIL_BEGIN_NAMESPACE

// Simple list "delete" on destruct object.

template < class t_Ty >
class _smartptr
{
  typedef _smartptr _TyThis;
  t_Ty * m_pt;
public:
  _smartptr() : m_pt( 0 )
  {
  }
  explicit _smartptr( t_Ty * _pt ) : m_pt( _pt )
  {
  }
  explicit _smartptr( _smartptr & _r ) : m_pt( _r.m_pt )
  {
    _r.Reset();
  }
  _smartptr( _smartptr && _rr )
  {
    _rr.swap( *this );
  }
  ~_smartptr() _BIEN_NOTHROW
  {
    if ( m_pt )
    {
      delete m_pt;
    }
  }
  void swap( _smartptr & _r )
  {
    std:swap( _r.m_pt, m_pt );
  }

  t_Ty *  Ptr() const _BIEN_NOTHROW
  {
    return m_pt;
  }
  t_Ty *& PtrRef() _BIEN_NOTHROW
  {
    return m_pt;
  }

  void  Release() _BIEN_NOTHROW
  {
    if ( m_pt )
    {
      t_Ty * _pt = m_pt;
      m_pt = 0;
      delete _pt;
    }
  }
  void  Reset() _BIEN_NOTHROW
  {
    m_pt = 0;
  }
  void  Reset( t_Ty * _pt ) _BIEN_NOTHROW
  {
    m_pt = _pt;
  }
	t_Ty * transfer() _BIEN_NOTHROW
	{
		t_Ty * _pt = m_pt;
    Reset();
		return _pt;
	}
  // acquire <_pt> - destruct any current object.
  _TyThis & operator = ( t_Ty * _pt ) _BIEN_NOTHROW
  {
    if ( _pt != m_pt )
    {
      Release();
      m_pt = _pt;
    }
    return *this;
  }
  // Transfers ownership from <_r>.
  _TyThis & operator = ( _TyThis && _rr )
  {
    if ( this != &_rr )
    {
      Release();
      this->swap( _rr );
    }
    return *this;
  }
  t_Ty * operator ->() const _BIEN_NOTHROW
  {
    return m_pt;
  }
  t_Ty & operator *() const _BIEN_NOTHROW
  {
    return *m_pt;
  }
#if 0 // Don't see why these are desireable.
  // Make convert to object pointer a non-const member,
  //  prevents accidental construction or assignment to
  //  a const-_TyThis - since then conversion to pointer is
  //  inaccessible.
  operator t_Ty * () _BIEN_NOTHROW
  {
    return m_pt;
  }

  operator const t_Ty * () const _BIEN_NOTHROW
  {
    return m_pt;
  }
#endif //0
};

// Implement a very simple object to call free() on a void* on destruct.
class FreeVoid
{
  typedef FreeVoid _TyThis;
public:
  FreeVoid( void* _pv )
    : m_pv(_pv)
  { }
  ~FreeVoid()
  {
    if ( m_pv )
      free( m_pv );
  }
  FreeVoid() = default;
  FreeVoid(FreeVoid const&) = delete;
  FreeVoid& operator = (FreeVoid const&) = delete;
  FreeVoid(FreeVoid && _rr)
  {
    swap(_rr);
  }
  FreeVoid& operator = (FreeVoid && _rr)
  {
    FreeVoid acquire(std::move(_rr));
    swap(acquire);
    return *this;
  }
  void swap(_TyThis& _r)
  {
    std::swap(m_pv, _r.m_pv);
  }
  void Clear()
  {
    if ( m_pv )
    {
      void * pv = m_pv;
      m_pv = 0;
      free( pv );
    }
  }
  void * PvTransfer()
  {
      void * pv = m_pv;
      m_pv = 0;
      return pv;
  }
  bool operator !() const
  {
    return !m_pv;
  }
  void* Pv() const
  {
    return m_pv;
  }
protected:
  void* m_pv{ nullptr };
};

// Implement a very simple object to call free() on a void* on destruct.
template < class t_tyT, bool t_kfManageLifetime >
class FreeT
{
  typedef FreeT _TyThis;
public:
  typedef t_tyT _tyT;
  static const bool s_kfManageLifetime = t_kfManageLifetime;
  FreeT(t_tyT* _pt)
    : m_pt(_pt)
  { }
  ~FreeT() noexcept(false)
  {
    Clear();
  }
  FreeT() = default;
  FreeT(FreeT const&) = delete;
  FreeT& operator = (FreeT const&) = delete;
  FreeT(FreeT && _rr)
  {
    swap(_rr);
  }
  FreeT& operator = (FreeT && _rr)
  {
    FreeT acquire(std::move(_rr));
    swap(acquire);
  }
  void swap(_TyThis & _r)
  {
    std::swap(m_pt, _r.m_pt);
  }
  void Clear() noexcept(false)
  {
    if (m_pt)
    {
      _tyT* pt = m_pt;
      m_pt = nullptr;
      exception_ptr eptrSave = nullptr;
      bool fInUnwinding = !!std::uncaught_exceptions();
      if (s_kfManageLifetime)
      {
        try
        {
          pt->~_tyT();
        }
        catch (...)
        {
          if (!fInUnwinding) // Not sure that we could actually get here. If ~t_tyT doesn't check to see if it is in an unwinding before throwing an exception then we might get here or we might abort() first.
            eptrSave = current_exception();
        }
      }
      free(pt);
      if (s_kfManageLifetime && !!eptrSave)
        rethrow_exception(eptrSave);
    }
  }
  t_tyT* PtTransfer()
  {
    t_tyT* pt = m_pt;
    m_pt = 0;
    return pt;
  }
  bool operator !() const
  {
    return !m_pt;
  }
  t_tyT* Pt() const
  {
    return m_pt;
  }
  t_tyT* operator ->() const
  {
    return m_pt;
  }
  t_tyT& operator *() const
  {
    return *m_pt;
  }
protected:
  t_tyT* m_pt{ nullptr };
};

__BIENUTIL_END_NAMESPACE

