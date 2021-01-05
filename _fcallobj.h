#ifndef __FCALLOBJ_H
#define __FCALLOBJ_H

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _fcallobj.h

// This object may or may not call a function at some point.

#include <optional>
#include "jsonstrm.h"

__BIENUTIL_BEGIN_NAMESPACE

// set_optional_false: Calls reset() on the optional object upon destruction.
template < typename t_TyOpt >
class set_optional_false
{
  typedef set_optional_false _TyThis;
public:
  typedef t_TyOpt _TyOpt;
  set_optional_false(_TyOpt & _ro)
    : m_ro(_ro)
  {
  }
  ~set_optional_false()
  {
    m_ro.reset();
  }
  _TyOpt m_ro;
};

template < typename t_TyF >
class _fcallobj
{
  typedef _fcallobj< t_TyF >  _TyThis;
  typedef optional< t_TyF > _TyOptF;
  _TyOptF m_optF;
public:
  _fcallobj() = default; // construct empty.
  _fcallobj(_fcallobj const &) = delete;
  _fcallobj & operator = (_fcallobj const &) = delete; // we could allow assigment but I don't need it at this point.

  template < typename t__TyF = t_TyF >
  _fcallobj( t__TyF && _rrf )
    : m_optF(std::move(_rrf))
  { 
  }
  _fcallobj& operator = (_fcallobj && _rr)
  {
    _fcallobj acquire(std::move(_rr));
    swap(_rr);
  }
  void swap(_fcallobj& _r)
  {
    m_optF.swap(_r.m_optF);
  }

  template < typename t__TyF = t_TyF >
  _fcallobj(t__TyF const & _rf)
    : m_optF(_rf)
  {
  }

  ~_fcallobj() noexcept(false)
  {
    // We shouldn't throw from the destructor and we allow throwing from release() below.
    bool fInUnwinding = !!std::uncaught_exceptions();
    try
    {
      // Technically we don't have to reset() the object explicitly here because the destructor will already do that in ~optional<>().
      if (m_optF)
        (*m_optF)();
    }
    catch ( std::exception const & )
    {
        if ( !fInUnwinding )
          throw; // let someone else deal with this.
        Assert(0); // just to see if we ever get here during testing.
    }
  }

  // set the optional object to false without signalling any function.
  void reset() // The presumption is that the destructor of the function object shouldn't throw so we expect a nothrow here.
  {
    m_optF.reset();
  }

  template < typename t__TyF = t_TyF >
  void emplace(t__TyF && _rrf)
  {
    reset(); // don't apply the function, just rid it.
    m_optF.emplace( std::forward< t__TyF >( _rrf ) );
  }

  // release the object before destruction. We ensure that even upon throw we call optional<>::reset().
  // This could throw and is allowed to. In fact if you want to capture a throw then this method should be called explicitly.
  void release()
  {
    if (m_optF)
    {
      set_optional_false< _TyOptF > soptf(m_optF); // set optional false on destruct - handles the method throwing an exception appropriately.
      (*m_optF)();
    }
  }
};

__BIENUTIL_END_NAMESPACE

#endif //__FCALLOBJ_H
