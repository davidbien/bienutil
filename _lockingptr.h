#pragma once

// _lockingptr.h
// dbien
// 24MAR2024
// LockingPtr paradigm from "volatile: The Multithreaded Programmer's Best Friend" By Andrei Alexandrescu, February 01, 2001
// Adapted to allow for multiple mutex classes and multiple locks, etc.

#include "bienutil.h"

__BIENUTIL_BEGIN_NAMESPACE

template < typename t_Ty, typename t_TyMutex, typename t_TyLockClass > class LockingPtr
{
  typedef LockingPtr _TyThis;
public:
  typedef t_Ty _Ty;

  LockingPtr() = delete;
  LockingPtr( const LockingPtr & ) = delete;
  LockingPtr & operator=( const LockingPtr & ) = delete;

  // We take a volatile reference to a mutex for convenience's sake - it's likely that the mutex will be
  //  stored in the same parent object as _obj, so it will be volatile as well and there is no comparable
  //  keyword to allow volatility to vary as "mutable" allows const to vary.
  LockingPtr( volatile t_Ty & _obj, volatile t_TyMutex & _mtx )
    : m_p( const_cast< t_Ty * >( &_obj ) )
    , m_pMtx( const_cast< t_TyMutex * >( &_mtx ) )
    , m_lock( *m_pMtx )
  {
  }
  ~LockingPtr() 
  { 
  }
  LockingPtr( LockingPtr && ) = default;
  LockingPtr & operator=( LockingPtr && ) = default;

  // Pointer behavior
  t_Ty & operator*() { return *m_p; }
  t_Ty * operator->() { return m_p; }

private:
  t_Ty * m_p;
  t_TyMutex * m_pMtx;
  t_TyLockClass m_lock;
};

__BIENUTIL_END_NAMESPACE
