#pragma once

// _lockingptr.h
// dbien
// 24MAR2024
// LockingPtr paradigm from "volatile: The Multithreaded Programmer's Best Friend" By Andrei Alexandrescu, February 01, 2001
// Adapted to allow for multiple mutex classes and multiple locks, etc.

#include "bienutil.h"
#include <boost/thread/locks.hpp>

__BIENUTIL_BEGIN_NAMESPACE

template < typename t_Ty, typename t_TyMutex, typename t_TyLockClass > class LockingPtr
{
  typedef LockingPtr _TyThis;
public:
  typedef t_Ty _Ty;
  typedef t_TyMutex _TyMutex;
  typedef t_TyLockClass _TyLockClass;

  LockingPtr() = delete;
  LockingPtr( const LockingPtr & ) = delete;
  LockingPtr & operator=( const LockingPtr & ) = delete;

  // We take a volatile reference to a mutex for convenience's sake - it's likely that the mutex will be
  //  stored in the same parent object as _obj, so it will be volatile as well and there is no comparable
  //  keyword to allow volatility to vary as "mutable" allows const to vary.
  LockingPtr( volatile t_Ty & _obj, volatile t_TyMutex & _mtx )
    : m_p( const_cast< t_Ty * >( &_obj ) )
    , m_lock( const_cast< t_TyMutex & >( _mtx ) )
  {
  }
  ~LockingPtr() 
  { 
  }
  LockingPtr( LockingPtr && ) = default;
  LockingPtr & operator=( LockingPtr && ) = default;

  // Pointer behavior
  t_Ty & operator*() const { return *m_p; }
  t_Ty * operator->() const { return m_p; }

  _TyLockClass & GetLock() { return m_lock; }
  _TyLockClass const & GetLock() const { return m_lock; }

private:
  t_Ty * m_p;
  t_TyLockClass m_lock;
};

// Partially specialize for boost::upgrade_to_unique_lock<> to implement correctly.
template < typename t_Ty, typename t_TyMutex > class LockingPtr< t_Ty, t_TyMutex, boost::upgrade_to_unique_lock< t_TyMutex > >
{
  typedef LockingPtr _TyThis;
public:
  typedef t_Ty _Ty;
  typedef t_TyMutex _TyMutex;
  typedef boost::upgrade_to_unique_lock< t_TyMutex > _TyLockClass;
  typedef LockingPtr< const t_Ty, t_TyMutex, boost::upgrade_lock< t_TyMutex > > _TyUpgradeLockingPtr;

  LockingPtr() = delete;
  LockingPtr( const LockingPtr & ) = delete;
  LockingPtr & operator=( const LockingPtr & ) = delete;

  // We take a locking ptr to upgrade since we need to get the object pointer from it.
  LockingPtr( _TyUpgradeLockingPtr & _krlkUpgrade )
    : m_p( const_cast< t_Ty * >( &*_krlkUpgrade ) )
    , m_lock( _krlkUpgrade.GetLock() )
  {
  }
  ~LockingPtr() 
  { 
  }
  LockingPtr( LockingPtr && ) = default;
  LockingPtr & operator=( LockingPtr && ) = default;

  // Pointer behavior
  t_Ty & operator*() const { return *m_p; }
  t_Ty * operator->() const { return m_p; }

  _TyLockClass & GetLock() { return m_lock; }
  _TyLockClass const & GetLock() const { return m_lock; }

private:
  t_Ty * m_p;
  _TyLockClass m_lock;
};


__BIENUTIL_END_NAMESPACE
