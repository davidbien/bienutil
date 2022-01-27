#pragma once

// _jniutil.h
// JNI utility stuff.
// dbien 25JAN2022

#include <jni.h>

__BIENUTIL_BEGIN_NAMESPACE

enum class EJNIUtilIsModifiable : uint8_t
{
  ejniuIsCopyFalse,
    // Isn't a modifiable copy and just be copied if it is to be modified.
  ejniuIsCopyTrue,
    // Is a copy returned from the JNIEnv which then requires the appropriate ...Release() method to be called.
  ejniuIsLocalCopy,
    // Is a local copy allocated via new[] and thus needs to be delete[]d.
  ejniuEJNIUtilIsModifiableCount
    //This always last in this enumeration.
};

// JNIEnvBase:
// Base class for JNI utility objects that require a release to be performed, since they will need a pointer to the JNIEnv object.
class JNIEnvBase
{
  typedef JNIEnvBase _TyThis;
public:
  JNIEnvBase() = default;
  JNIEnvBase( JNIEnvBase const & ) = default;
  JNIEnvBase & operator =( JNIEnvBase const & ) = default;
  JNIEnvBase( JNIEnv * _pjeEnv )
    : m_pjeEnv( _pjeEnv )
  {
  }
  void swap( _TyThis & _r )
  {
    Assert( !m_pjeEnv || !_r.m_pjeEnv || ( m_pjeEnv != _r.m_pjeEnv ) );
    std::swap( m_pjeEnv, _r.m_pjeEnv );
  }
  void SetEnv( JNIEnv * _pjeEnv )
  {
    m_pjeEnv = _pjeEnv;
  }
  JNIEnv * PGetEnv() const
  {
    return m_pjeEnv;
  }
protected:
  JNIEnv* m_pjeEnv{nullptr};
};

// JNIStringUTF8:
// Represents a null terminated UTF8 string of char's obtained from (JNIEnv,jstring) pair.
class JNIStringUTF8 : public JNIEnvBase
{
  typedef JNIEnvBase _TyBase;
  typedef JNIStringUTF8 _TyThis;
public:
  void AssertValid() const noexcept
  {
    Assert( !!m_pjeEnv || ( !m_jstr && !m_pcUTF8 && ( EJNIUtilIsModifiable::ejniuIsCopyFalse == m_jniuIsModifiable ) ) ); // null state.
    Assert( ( EJNIUtilIsModifiable::ejniuIsCopyFalse == m_jniuIsModifiable ) || !!m_pcUTF8 ); // local copy and modifiable copy state.
    Assert( ( EJNIUtilIsModifiable::ejniuIsLocalCopy == m_jniuIsModifiable ) || ( !m_jstr == !m_pcUTF8 ) ); // modifiable copy state.
    Assert( ( EJNIUtilIsModifiable::ejniuIsLocalCopy != m_jniuIsModifiable ) || ( !m_jstr && !!m_pcUTF8 ) ); // local copy state.
  }
  JNIStringUTF8() noexcept = default;
  // Provide a copy constructor which produces a local modifiable copy.
  JNIStringUTF8( const JNIStringUTF8 & _r ) noexcept(false)
    : _TyBase( _r )
  {
    _r.AssertValid();
    if ( !!_r.m_pcUTF8 )
    {
      size_t stLen = strlen( (const char*)_r.m_pcUTF8 );
      m_pcUTF8 = DBG_NEW char8_t[ stLen + 1 ]; // throws.
      memcpy( m_pcUTF8, _r.m_pcUTF8, ( stLen + 1 ) * sizeof( char8_t ) );
      m_jniuIsModifiable = EJNIUtilIsModifiable::ejniuIsLocalCopy;
      // Leave m_jstr at nullptr since we have a local copy that isn't maintained by the Java VM.
    }
    AssertValid();
  }
  JNIStringUTF8( JNIStringUTF8 && _rr ) noexcept
    : _TyBase( _rr ) // We want _rr to be left with a valid JNIEnv* if it already has one.
  {
    swap( _rr );
  }
  JNIStringUTF8( JNIEnv * _pjeEnv ) noexcept
    : _TyBase( _pjeEnv )
  {
  }
  // Will throw if the call to GetStringUTFChars() fails by returning NULL.
  JNIStringUTF8( JNIEnv * _pjeEnv, jstring _jstr ) noexcept( false )
    : _TyBase( _pjeEnv ),
      m_jstr( _jstr )
  {
    jboolean jbIsCopy;
    VerifyThrowSz( !!( m_pcUTF8 = (char8_t*)m_pjeEnv->GetStringUTFChars( m_jstr, &jbIsCopy ) ), "GetStringUTFChars() failed." );
    if ( JNI_TRUE == jbIsCopy )
      m_jniuIsModifiable = EJNIUtilIsModifiable::ejniuIsCopyTrue;
    AssertValid();
  }
  ~JNIStringUTF8() noexcept
  {
    Release();
  }
  void swap( _TyThis & _r ) noexcept
  {
    _TyBase::swap( _r );
    std::swap( m_pcUTF8, _r.m_pcUTF8 );
    std::swap( m_jstr, _r.m_jstr );
    std::swap( m_jniuIsModifiable, _r.m_jniuIsModifiable );
  }
  void AttachString( jstring _jstr, JNIEnv * _pjeEnv = nullptr ) noexcept(false)
  {
    Release();
    // We don't allow this method to be a no-op.
    VerifyThrowSz( ( !!m_pjeEnv || !!_pjeEnv ) && !!_jstr, "No JNIEnv or empty jstr." );
    if ( !!_pjeEnv )
      m_pjeEnv = _pjeEnv; // One imagines this will always be the same but who knows (at this point in my learning).
    jboolean jbIsCopy;
    VerifyThrowSz( !!( m_pcUTF8 = (char8_t*)m_pjeEnv->GetStringUTFChars( _jstr, &jbIsCopy ) ), "GetStringUTFChars() failed." );
    m_jstr = _jstr; // TS
    if ( JNI_TRUE == jbIsCopy )
      m_jniuIsModifiable = EJNIUtilIsModifiable::ejniuIsCopyTrue;
    AssertValid();
  }
  void Release() noexcept
  {
    AssertValid();
    if ( m_pcUTF8 )
    {
      char8_t * pcUTF8 = m_pcUTF8;
      m_pcUTF8 = nullptr;
      EJNIUtilIsModifiable jniuIsModifiable = m_jniuIsModifiable;
      m_jniuIsModifiable = EJNIUtilIsModifiable::ejniuIsCopyFalse;
      if ( EJNIUtilIsModifiable::ejniuIsLocalCopy == jniuIsModifiable )
      {
        // Then we have already called ReleaseStringUTFChars().
        delete[] pcUTF8;
      }
      else
      {
        jstring jstr = m_jstr;
        m_jstr = nullptr;
        m_pjeEnv->ReleaseStringUTFChars( jstr, (const char *)pcUTF8 );
      }
    }
    Assert( FIsNull() );
  }
  // Explcit call to make a local copy of the string.
  void MakeLocalCopy() noexcept(false)
  {
    AssertValid();
    if ( !!m_pcUTF8 && ( EJNIUtilIsModifiable::ejniuIsLocalCopy != m_jniuIsModifiable )  )
    {
      // Then we need to make a copy and then release our existing string:
      size_t stLen = strlen( (const char *)m_pcUTF8 );
      char8_t * pcUTF8Copy = DBG_NEW char8_t[ stLen + 1 ]; // throws.
      memcpy( pcUTF8Copy, m_pcUTF8, ( stLen + 1 ) * sizeof( char8_t ) );
      jstring jstr = m_jstr;
      m_jstr = nullptr;
      char8_t * pcUTF8 = m_pcUTF8;
      m_pcUTF8 = pcUTF8Copy;
      m_jniuIsModifiable = EJNIUtilIsModifiable::ejniuIsLocalCopy;
      m_pjeEnv->ReleaseStringUTFChars( jstr, (const char *)pcUTF8 );
    }
  }
  bool FIsNull() const noexcept
  {
    AssertValid();
    return !m_pcUTF8;
  }
  // We will allow a null to be returned here.
  const char8_t * PCGet() const noexcept
  {
    AssertValid();
    return m_pcUTF8;
  }
  // Provide this for compatibility with string.
  const char * c_str() const noexcept
  {
    return (const char *)PCGet();
  }
  operator const char8_t * () const noexcept
  {
    return PCGet();
  }
  // Get a non-const pointer to the string - a modifiable pointer - one must of course be careful to not write beyond the last non-'\0' char.
  // Use a special name to 
  char8_t * PCGetNonConst() noexcept(false)
  {
    if ( !!m_pcUTF8 )
    {
      if ( EJNIUtilIsModifiable::ejniuIsCopyFalse == m_jniuIsModifiable )
        MakeLocalCopy();
      return m_pcUTF8;
    }
    return nullptr;
  }
  // Note that this returns the "code unit" length of the string - not the code length of the potentially multibyte UTF8 string.
  // In this case it is the same as the byte length.
  size_t UnitLength() const noexcept
  {
    AssertValid();
    return !m_pcUTF8 ? 0 : strlen( (const char*)m_pcUTF8 );
  }
protected:
  char8_t * m_pcUTF8{ nullptr }; // Use char8_t to require a cast to indicate that we are potentially a multibyte character string.
  jstring m_jstr{ nullptr };
  EJNIUtilIsModifiable m_jniuIsModifiable{ EJNIUtilIsModifiable::ejniuIsCopyFalse }; // This mirrors the isCopy result from acquiring the string. Before returning a non-const access to this string a copy is made if it currently is not a modifiable string.
};

// JNIStringUTF16:
// Represents a non-null-terminated UTF16 string of char's obtained from (JNIEnv,jstring) pair.
class JNIStringUTF16 : public JNIEnvBase
{
  typedef JNIEnvBase _TyBase;
  typedef JNIStringUTF16 _TyThis;
public:
  static_assert( sizeof( jchar ) == sizeof( char16_t ) );

  void AssertValid() const noexcept
  {
    Assert( !!m_pjeEnv || ( !m_jstr && !m_stUnitLength && !m_pcUTF16 && ( EJNIUtilIsModifiable::ejniuIsCopyFalse == m_jniuIsModifiable ) ) ); // null state.
    Assert( ( EJNIUtilIsModifiable::ejniuIsCopyFalse == m_jniuIsModifiable ) || !!m_pcUTF16 ); // local copy and modifiable copy state.
    Assert( ( EJNIUtilIsModifiable::ejniuIsLocalCopy == m_jniuIsModifiable ) || ( !m_jstr == !m_pcUTF16 ) ); // modifiable copy state.
    Assert( ( EJNIUtilIsModifiable::ejniuIsLocalCopy != m_jniuIsModifiable ) || ( !m_jstr && !!m_pcUTF16 ) ); // local copy state.
  }
  JNIStringUTF16() noexcept = default;
  // Provide a copy constructor which produces a local modifiable copy.
  JNIStringUTF16( const JNIStringUTF16 & _r ) noexcept(false)
    : _TyBase( _r )
  {
    _r.AssertValid();
    if ( !!_r.m_pcUTF16 )
    {
      m_pcUTF16 = DBG_NEW char16_t[ _r.m_stUnitLength ]; // throws.
      m_stUnitLength = _r.m_stUnitLength;
      memcpy( m_pcUTF16, _r.m_pcUTF16, m_stUnitLength * sizeof( char16_t ) );
      m_jniuIsModifiable = EJNIUtilIsModifiable::ejniuIsLocalCopy;
      // Leave m_jstr at nullptr since we have a local copy that isn't maintained by the Java VM.
    }
    AssertValid();
  }
  JNIStringUTF16( JNIStringUTF16 && _rr ) noexcept
    : _TyBase( _rr ) // We want _rr to be left with a valid JNIEnv* if it already has one.
  {
    swap( _rr );
  }
  JNIStringUTF16( JNIEnv * _pjeEnv ) noexcept
    : _TyBase( _pjeEnv )
  {
  }
  // Will throw if the call to GetStringUTFChars() fails by returning NULL.
  JNIStringUTF16( JNIEnv * _pjeEnv, jstring _jstr ) noexcept( false )
    : _TyBase( _pjeEnv ),
      m_jstr( _jstr )
  {
    jboolean jbIsCopy;
    VerifyThrowSz( !!( m_pcUTF16 = (char16_t*)m_pjeEnv->GetStringChars( m_jstr, &jbIsCopy ) ), "GetStringChars() failed." );
    if ( JNI_TRUE == jbIsCopy )
      m_jniuIsModifiable = EJNIUtilIsModifiable::ejniuIsCopyTrue;
    m_stUnitLength = m_pjeEnv->GetStringLength( m_jstr );
    AssertValid();
  }
  ~JNIStringUTF16() noexcept
  {
    Release();
  }
  void swap( _TyThis & _r ) noexcept
  {
    _TyBase::swap( _r );
    std::swap( m_pcUTF16, _r.m_pcUTF16 );
    std::swap( m_stUnitLength, _r.m_stUnitLength );
    std::swap( m_jstr, _r.m_jstr );
    std::swap( m_jniuIsModifiable, _r.m_jniuIsModifiable );
  }
  void AttachString( jstring _jstr, JNIEnv * _pjeEnv = nullptr ) noexcept(false)
  {
    Release();
    // We don't allow this method to be a no-op.
    VerifyThrowSz( ( !!m_pjeEnv || !!_pjeEnv ) && !!_jstr, "No JNIEnv or empty jstr." );
    if ( !!_pjeEnv )
      m_pjeEnv = _pjeEnv; // One imagines this will always be the same but who knows (at this point in my learning).
    jboolean jbIsCopy;
    VerifyThrowSz( !!( m_pcUTF16 = (char16_t*)m_pjeEnv->GetStringChars( _jstr, &jbIsCopy ) ), "GetStringChars() failed." );
    m_jstr = _jstr; // TS
    if ( JNI_TRUE == jbIsCopy )
      m_jniuIsModifiable = EJNIUtilIsModifiable::ejniuIsCopyTrue;
    m_stUnitLength = m_pjeEnv->GetStringLength( m_jstr );
    AssertValid();
  }
  void Release() noexcept
  {
    AssertValid();
    if ( m_pcUTF16 )
    {
      char16_t * pcUTF16 = m_pcUTF16;
      m_pcUTF16 = nullptr;
      m_stUnitLength = 0;
      EJNIUtilIsModifiable jniuIsModifiable = m_jniuIsModifiable;
      m_jniuIsModifiable = EJNIUtilIsModifiable::ejniuIsCopyFalse;
      if ( EJNIUtilIsModifiable::ejniuIsLocalCopy == jniuIsModifiable )
      {
        // Then we have already called ReleaseStringChars().
        delete[] pcUTF16;
      }
      else
      {
        jstring jstr = m_jstr;
        m_jstr = nullptr;
        m_pjeEnv->ReleaseStringChars( jstr, (const jchar *)pcUTF16 );
      }
    }
    Assert( FIsNull() );
  }
  // Explcit call to make a local copy of the string.
  void MakeLocalCopy() noexcept(false)
  {
    AssertValid();
    if ( !!m_pcUTF16 && ( EJNIUtilIsModifiable::ejniuIsLocalCopy != m_jniuIsModifiable )  )
    {
      // Then we need to make a copy and then release our existing string:
      char16_t * pcUTF16Copy = DBG_NEW char16_t[ m_stUnitLength ]; // throws.
      memcpy( pcUTF16Copy, m_pcUTF16, m_stUnitLength * sizeof( char16_t ) );
      jstring jstr = m_jstr;
      m_jstr = nullptr;
      char16_t * pcUTF16 = m_pcUTF16;
      m_pcUTF16 = pcUTF16Copy;
      m_jniuIsModifiable = EJNIUtilIsModifiable::ejniuIsLocalCopy;
      m_pjeEnv->ReleaseStringChars( jstr, (const jchar *)pcUTF16 );
    }
  }
  bool FIsNull() const noexcept
  {
    AssertValid();
    return !m_pcUTF16;
  }
  // We will allow a null to be returned here.
  std::pair< const char16_t *, size_t > GetPairStringLen() const
  {
    return std::pair< const char16_t *, size_t >( m_pcUTF16, m_stUnitLength );
  }
  std::pair< char16_t *, size_t > GetPairStringLenNonConst() noexcept(false)
  {
    if ( !!m_pcUTF16 )
    {
      if ( EJNIUtilIsModifiable::ejniuIsCopyFalse == m_jniuIsModifiable )
        MakeLocalCopy();
    }
    return std::pair< char16_t *, size_t >( m_pcUTF16, m_stUnitLength );
  }
  // Note that this returns the "code unit" length of the string - not the code length of the potentially multi-code-unit UTF16 string.
  size_t UnitLength() const noexcept
  {
    AssertValid();
    return m_stUnitLength;
  }
protected:
  char16_t * m_pcUTF16{ nullptr }; // Use char16_t to require a cast to indicate that we are potentially a multibyte character string.
  size_t m_stUnitLength{0}; // the code-unit length of the UTF16 string.
  jstring m_jstr{ nullptr };
  EJNIUtilIsModifiable m_jniuIsModifiable{ EJNIUtilIsModifiable::ejniuIsCopyFalse }; // This mirrors the isCopy result from acquiring the string. Before returning a non-const access to this string a copy is made if it currently is not a modifiable string.
};

__BIENUTIL_END_NAMESPACE
