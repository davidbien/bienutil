#ifndef __FCALLOBJ_H
#define __FCALLOBJ_H

// _fcallobj.h

// This object may or may not call a function at some point.

#include <optional>

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
};

template < typename t_TyF >
class _fcallobj
{
  typedef _fcallobj< t_TyF >  _TyThis;
  typedef optional< t_TyF > _TyOptF;
  _TyOptF m_optF;
#ifdef _FCALLOBJ_TRACK_IGNORED_THROWS
  static size_t vs_nIgnoredThrows;
#endif //_FCALLOBJ_TRACK_IGNORED_THROWS
public:

  _fcallobj() _BIEN_NOTHROW = default; // construct empty.
  _fcallobj(_fcallobj const &) _BIEN_NOTHROW = default; // allow copy of another _fcallobj - but both *will* be signalled or not.
  _fcallobj & operator = (_fcallobj const &) = delete; // we could allow assigment but I don't need it at this point.

  template < typename t__TyF = t_TyF >
  _fcallobj( t__TyF && _rrf ) _BIEN_NOTHROW
    : m_optF(_rrf)
  { 
  }

  template < typename t__TyF = t_TyF >
  _fcallobj(t__TyF const & _rf) _BIEN_NOTHROW
    : m_optF(std::inplace, _rf)
  {
  }

  ~_fcallobj() _BIEN_NOTHROW
  {
    // We shouldn't throw from the destructor and we allow throwing from release() below.
    try
    {
      // Technically we don't have to reset() the object explicitly here because the destructor will already do that in ~optional<>().
      if (m_optF)
        (*m_optF)();
    }
    catch (...)
    {
      // we shouldn't throw out of a destructor because it could result in termination of the application.
      // The presumption is that we are not leaking memory by doing this but I would have to verify on all compilers, etc.
#ifdef _FCALLOBJ_TRACK_IGNORED_THROWS
      ++vs_nIgnoredThrows;
#endif //_FCALLOBJ_TRACK_IGNORED_THROWS
    }
  }

  // set the optional object to false without signalling any function.
  void reset() _BIEN_NOTHROW // The presumption is that the destructor of the function object shouldn't throw so we expect a nothrow here.
  {
    m_optF.reset();
  }

  template < typename t__TyF = t_TyF >
  void emplace(t__TyF const & _rf)
  {
    reset(); // don't apply the function, just rid it.
    m_optF.emplace(_rf);
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
