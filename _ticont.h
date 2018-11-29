#ifndef __TICONT_H__
#define __TICONT_H__

// _ticont.h

// STL container compatible type_info wrapper.

__BIENUTIL_BEGIN_NAMESPACE

struct _type_info_wrap
{
private:
	typedef _type_info_wrap	_TyThis;
public:

	type_info const & m_rti;
	
	_type_info_wrap( type_info const & _rti )
		: m_rti( _rti )
	{
	}

	_type_info_wrap( _type_info_wrap const & _r )
		: m_rti( _r.m_rti )
	{
	}

	bool	operator < ( _TyThis const & _r ) const
	{
		return m_rti.before( _r.m_rti );
	}

	bool	operator == ( _TyThis const & _r ) const
	{
		return m_rti == _r.m_rti;
	}

	bool	operator != ( _TyThis const & _r ) const
	{
		return m_rti != _r.m_rti;
	}

  size_t  hash() const
  {
    return reinterpret_cast< size_t >( &m_rti );
  }
};

__BIENUTIL_END_NAMESPACE

__STL_BEGIN_NAMESPACE
__BIENUTIL_USING_NAMESPACE

_STLP_TEMPLATE_NULL struct hash<_type_info_wrap>
{
  size_t operator()(_type_info_wrap const & _rtiw ) const 
  { 
    return _rtiw.hash(); 
  }
};

__STL_END_NAMESPACE

#endif __TICONT_H__
