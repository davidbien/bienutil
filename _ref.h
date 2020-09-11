#ifndef __REF_H__
#define __REF_H__

//          Copyright David Lawrence Bien 1997 - 2020.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// _ref.h

// reference container

__BIENUTIL_BEGIN_NAMESPACE

template < class t_Ty >
class _ref
{
private:
  typedef _ref< t_Ty >	_TyThis;
protected:
	
  t_Ty &	m_r;

public:
	
  _ref( t_Ty & _r )	// implicit
    : m_r( _r )
  {
  }

  _ref( _TyThis const & _r ) // implicit
    : m_r( _r.m_r )
  {
  }

  t_Ty & Ref() const
  {
    return m_r;
  }

  operator t_Ty & () const
  {
    return m_r;
  }

  bool	operator == ( _TyThis const & _r ) const
  {
    return m_r == _r.m_r;
  }

  bool	operator < ( _TyThis const & _r ) const
  {
    return m_r < _r.m_r;
  }

  t_Ty *	operator -> ()  const
  {
    return &m_r;
  }
};

__BIENUTIL_END_NAMESPACE

#endif //__REF_H__
