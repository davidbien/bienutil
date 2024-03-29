#ifndef __SWAPOBJ_H
#define __SWAPOBJ_H

//          Copyright David Lawrence Bien 1997 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _swapobj.h

// generic swapping wrapper - allows containers to contain elements that
//  are slow to construct.

__BIENUTIL_BEGIN_NAMESPACE

template < class t_Ty >
class _swap_object
{
private:
  typedef   _swap_object< t_Ty >  _TyThis;
protected:
  t_Ty  m_t;
public:

  typedef t_Ty  _Ty;

  // We swap with a const object:
  _swap_object( const t_Ty & _r )
    : m_t( _r, std::false_type() ) // Only copy the allocator.
  {
    m_t.swap( const_cast< _Ty & >( _r ) );
  }

  // We swap with even a const self.
  _swap_object( const _TyThis & _r )
    : m_t( _r.m_t, std::false_type() )
  {
    m_t.swap( const_cast< _Ty & >( _r.RObject() ) );
  }

  _Ty & RObject() _BIEN_NOTHROW
  {
    return m_t;
  }
  const _Ty & RObject() const _BIEN_NOTHROW
  {
    return m_t;
  }

  operator _Ty & () _BIEN_NOTHROW
  {
    return RObject();
  }
  operator const _Ty & () const _BIEN_NOTHROW
  {
    return RObject();
  }

  _TyThis & operator = ( _TyThis & _r )
  {
    m_t.swap( _r.RObject() );
    return *this;
  }
  _TyThis & operator = ( _Ty & _r )
  {
    m_t.swap( _r );
    return *this;
  }

  bool  operator == ( _TyThis const & _r ) const
  {
    // delegate:
    return m_t == _r.m_t;
  }

  bool  operator < ( _TyThis const & _r ) const
  {
    // delegate:
    return m_t < _r.m_t;
  }
};

__BIENUTIL_END_NAMESPACE

namespace std {
__BIENUTIL_USING_NAMESPACE

// The hash for a swap object is the same as the hash for the base type:
template < class t_Ty >
struct hash< _swap_object< t_Ty > >
  : public hash< t_Ty >
{
  typedef hash< t_Ty >          _TyDelegate;
  typedef _swap_object< t_Ty >  _TySwapObj;
  size_t operator()(const _TySwapObj & _r) const
  {
    return _TyDelegate::operator ()(_r.RObject());
  }
};

template < class t_Ty >
struct less < _swap_object< t_Ty > > :
  public less< t_Ty >
{
  typedef less< t_Ty >          _TyDelegate;
  typedef _swap_object< t_Ty >  _TySwapObj;
  bool operator()(const _TySwapObj & _rl, const _TySwapObj & _rr) const
  {
    return _TyDelegate::operator ()(_rl.RObject(), _rr.RObject());
  }
};

} // end namespace std


#endif //__SWAPOBJ_H
