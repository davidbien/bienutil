#ifndef __FCDTOR_H
#define __FCDTOR_H

// _fcdtor.h

// Family of "call function on destruct" templates.
// These are useful for writing throw and state safe code.

__BIENUTIL_BEGIN_NAMESPACE

template < class t_TyObj >
class CMFDtor0_void
{
  typedef CMFDtor0_void< t_TyObj >    _TyThis;
  typedef void ( t_TyObj:: * _TyFunc )(); // f_CSParams - as function parameter - is expanded - no dependency.
  typedef t_TyObj *                   _TyObjectPtr;

public:
  
  CMFDtor0_void() _STLP_NOTHROW
    : m_pFunc( 0 )
  {
  }
  CMFDtor0_void( _TyObjectPtr _po,
                _TyFunc _pFunc ) _STLP_NOTHROW
    : m_po( _po ),
      m_pFunc( _pFunc )
  {
  }

  ~CMFDtor0_void()
  {
    _ReleaseInt( m_pFunc );
  }

  void  Release()
  {
    _TyFunc pFunc = m_pFunc;
    _ReleaseInt(  m_pFunc = 0, pFunc );
  }

  void  Reset() _STLP_NOTHROW
  {
    m_pFunc = 0;
  }
  void  Reset(  _TyObjectPtr _po,
                _TyFunc _pFunc ) _STLP_NOTHROW
  {
    if ( _po && _pFunc )
    {
      m_po = _po;
      m_pFunc = _pFunc;
    }
    else
    {
      m_pFunc = 0;
    }
  }

protected:

  _TyObjectPtr  m_po;
  _TyFunc       m_pFunc;

  void  _ReleaseInt( _TyFunc _pFunc )
  {
    if ( _pFunc )
    {
      (m_po->*_pFunc)();
    }
  }
};

template <  class t_TyObj, class t_TyReturn, 
            class t_TyStoreReturn = t_TyReturn >
class CMFDtor0_rtn
{
  typedef CMFDtor0_rtn< t_TyObj, t_TyReturn, t_TyStoreReturn >    _TyThis;
  typedef t_TyReturn ( t_TyObj:: * _TyFunc )(); // f_CSParams - as function parameter - is expanded - no dependency.
  typedef t_TyObj *                   _TyObjectPtr;

public:
  
  CMFDtor0_rtn() _STLP_NOTHROW
    : m_pFunc( 0 )
  {
  }
  CMFDtor0_rtn( _TyObjectPtr _po,
                _TyFunc _pFunc ) _STLP_NOTHROW
    : m_po( _po ),
      m_pFunc( _pFunc )
  {
  }
  ~CMFDtor0_rtn()
  {
    _ReleaseInt( m_pFunc );
  }

  t_TyReturn Release()
  {
    _TyFunc pFunc = m_pFunc;
    _ReleaseInt(  m_pFunc = 0, pFunc );
    return m_rtn;
  }

  void  Reset() _STLP_NOTHROW
  {
    m_pFunc = 0;
  }
  void  Reset(  _TyObjectPtr _po,
                _TyFunc _pFunc ) _STLP_NOTHROW
  {
    if ( _po && _pFunc )
    {
      m_po = _po;
      m_pFunc = _pFunc;
    }
    else
    {
      m_pFunc = 0;
    }
  }

protected:

  _TyObjectPtr    m_po;
  _TyFunc         m_pFunc;
  t_TyStoreReturn m_rtn;

  void _ReleaseInt( _TyFunc _pFunc )
  {
    if ( _pFunc )
    {
      m_rtn = (m_po->*_pFunc)();
    }
  }
};

template <  class t_TyObj,
            class t_TyP1Call,
            class t_TyP1Store = t_TyP1Call >
class CMFDtor1_void
{
  typedef CMFDtor1_void< t_TyObj, t_TyP1Call, t_TyP1Store >    _TyThis;
  typedef void ( t_TyObj:: * _TyFunc )( t_TyP1Call );
  typedef t_TyObj *                         _TyObjectPtr;

public:
  
  CMFDtor1_void() _STLP_NOTHROW
    : m_pFunc( 0 )
  {
  }
  CMFDtor1_void( _TyObjectPtr _po,
                _TyFunc _pFunc,
                t_TyP1Call _p1 ) _STLP_NOTHROW
    : m_po( _po ),
      m_pFunc( _pFunc ),
      m_p1( _p1 )
  {
  }

  ~CMFDtor1_void()
  {
    _ReleaseInt( m_pFunc );
  }

  void  Release()
  {
    _TyFunc pFunc = m_pFunc;
    _ReleaseInt(  m_pFunc = 0, pFunc );
  }

  void  Reset() _STLP_NOTHROW
  {
    m_pFunc = 0;
  }
  void  Reset(  _TyObjectPtr _po,
                _TyFunc _pFunc,
                t_TyP1Call _p1 ) _STLP_NOTHROW
  {
    if ( _po && _pFunc )
    {
      m_po = _po;
      m_pFunc = _pFunc;
      m_p1 = _p1;
    }
    else
    {
      m_pFunc = 0;
    }
  }

protected:

  _TyObjectPtr  m_po;
  _TyFunc       m_pFunc;
  t_TyP1Store   m_p1;

  void  _ReleaseInt( _TyFunc _pFunc )
  {
    if ( _pFunc )
    {
      (m_po->*_pFunc)( m_p1 );
    }
  }
};

template <  class t_TyObj,
            class t_TyP1Call,
            class t_TyP2Call,
            class t_TyP1Store = t_TyP1Call,
            class t_TyP2Store = t_TyP2Call >
class CMFDtor2_void
{
  typedef CMFDtor2_void<  t_TyObj, t_TyP1Call, t_TyP2Call, 
                          t_TyP1Store, t_TyP2Store >    _TyThis;
  typedef void ( t_TyObj:: * _TyFunc )( t_TyP1Call, t_TyP2Call );
  typedef t_TyObj *                         _TyObjectPtr;

public:
  
  CMFDtor2_void() _STLP_NOTHROW
    : m_pFunc( 0 )
  {
  }
  CMFDtor2_void( _TyObjectPtr _po,
                  _TyFunc _pFunc,
                  t_TyP1Call _p1,
                  t_TyP2Call _p2 ) _STLP_NOTHROW
    : m_po( _po ),
      m_pFunc( _pFunc ),
      m_p1( _p1 ),
      m_p2( _p2 )
  {
  }

  ~CMFDtor2_void()
  {
    _ReleaseInt( m_pFunc );
  }

  void  Release()
  {
    _TyFunc pFunc = m_pFunc;
    _ReleaseInt(  m_pFunc = 0, pFunc );
  }

  void  Reset() _STLP_NOTHROW
  {
    m_pFunc = 0;
  }
  void  Reset(  _TyObjectPtr _po,
                _TyFunc _pFunc,
                t_TyP1Call _p1,
                t_TyP2Call _p2 ) _STLP_NOTHROW
  {
    if ( _po && _pFunc )
    {
      m_po = _po;
      m_pFunc = _pFunc;
      m_p1 = _p1;
      m_p2 = _p2;
    }
    else
    {
      m_pFunc = 0;
    }
  }

protected:

  _TyObjectPtr  m_po;
  _TyFunc       m_pFunc;
  t_TyP1Store   m_p1;
  t_TyP2Store   m_p2;

  void  _ReleaseInt( _TyFunc _pFunc )
  {
    if ( _pFunc )
    {
      (m_po->*_pFunc)( m_p1, m_p2 );
    }
  }
};

template < class t_TyP1Call, class t_TyP1Store = t_TyP1Call >
class CFDtor1_void
{
  typedef CFDtor1_void< t_TyP1Call >    _TyThis;
  typedef void ( * _TyFunc )( t_TyP1Call );

public:
  
  CFDtor1_void() _STLP_NOTHROW
    : m_pFunc( 0 )
  {
  }
  CFDtor1_void( _TyFunc _pFunc,
                t_TyP1Call _p1 ) _STLP_NOTHROW
    : m_pFunc( _pFunc ),
      m_p1( _p1 )
  {
  }

  ~CFDtor1_void()
  {
    _ReleaseInt( m_pFunc );
  }

  void  Release()
  {
    _TyFunc pFunc = m_pFunc;
    _ReleaseInt(  m_pFunc = 0, pFunc );
  }

  void  Reset() _STLP_NOTHROW
  {
    m_pFunc = 0;
  }
  void  Reset(  _TyFunc _pFunc,
                t_TyP1Call _p1 ) _STLP_NOTHROW
  {
    if ( _pFunc )
    {
      m_pFunc = _pFunc;
      m_p1 = _p1;
    }
    else
    {
      m_pFunc = 0;
    }
  }

  // Parameter access.
  t_TyP1Store const & P1() const _STLP_NOTHROW
  {
    return m_p1;
  }

protected:

  _TyFunc       m_pFunc;
  t_TyP1Store   m_p1;

  void  _ReleaseInt( _TyFunc _pFunc )
  {
    if ( _pFunc )
    {
      (*_pFunc)( m_p1 );
    }
  }
};

__BIENUTIL_END_NAMESPACE

#endif __FCDTOR_H

#if 0 // !__FAMILIES    // A half-baked concept that would require a lot of work :-)

// Anyway, this is like the template template for call function on destruct paradigm.

// This module declares a family that implements 
//  "call function on destruct" functionality.

// families:
// 1) Accept ordered sets of classes.
//    This can be used to generate:
//      a) function parameters.
//      b) structures that are synonomous with the set of ordered classes.
//        i) copy/assignment.
// 2) typedefs in a family.
//    a) If the typedef is "fatally" dependent upon the type that is a classset
//      then the type is treated as an empty classset.
//      i) "Fatal" dependency is 

family <  classset[0,1] f_CSObject, // ordered set of zero or one classes.
          classset[0] f_CSParams,   // ordered set of zero or many classes.
          class f_CRetVal >         // normal template argument.
class FFDtor
{
  typedef family FFDtor _TyThis;  // "family FFDtor" translates to the name of the implemented template.
  typedef FFDtor  _TyThis;        // ditto.
  typedef f_CRetVal ( f_CObject:: * _TyFunc )( f_CSParams );  // f_CSParams - as function parameter - is expanded - no dependency.
  typedef f_CSObject *    _TyObjectPtr; // Pointer to object - in the case of single member classsets the type is the same as the original type.
  typedef f_CSParams *    _TyParams;    // Pointer to classset structure type - fatally dependent.

public:
  
  FFDtor()
    : m_pFunc( 0 )
  {
  }
  FFDtor( _TyObjectPtr _po,
          f_CSParams _rgparams,
          _TyFunc _pFunc )
    : m_po( _po ),
      m_rgparams( _rgparams ),
      m_pFunc( _pFunc )
  {
  }

  ~FFDtor()
  {
    _ReleaseInt( m_pFunc );
  }

  f_CRetVal Release()
  {
    _TyFunc pFunc = m_pFunc;
    _ReleaseInt(  m_pFunc = 0, pFunc );
  }

  void  Reset()
  {
    m_pFunc = 0;
  }
  void  Reset(  _TyObjectPtr _po,
                f_CSParams _rgparams,
                _TyFunc _pFunc )
  {
    if ( _po && _pFunc )
    {
      m_po = _po;
      m_pFunc = _pFunc;
      m_rgparams = _rgparams;
    }
    else
    {
      m_pFunc = 0;
    }
  }

protected:

  _TyObjectPtr  m_po;
  f_CSParams    m_rgparams;
  _TyFunc       m_pFunc;

  f_CRetVal _ReleaseInt( _TyFunc _pFunc )
  {
    if ( _pFunc )
    {
      return m_po->(*_pFunc)( m_rgparams );
    }
  }
};


family FFDtor implement template < [], [ n = 0...3 ], class t_CRetVal > class FFDtor_global[n];
family FFDtor implement template < class t_CObject, [ n = 0...3 ], class t_CRetVal > class FFDtor_object[n];

#endif 0 // __FAMILIES

