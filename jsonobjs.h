#pragma once

//          Copyright David Lawrence Bien 1997 - 2021.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          https://www.boost.org/LICENSE_1_0.txt).

// jsonobjs.h
// JSON objects for reading/writing using jsonstrm.h.
// dbien 01APR2020

#include <vector>
#include <map>
#include <compare>
#include "jsonstrm.h"
#include "strwrsv.h"

__BIENUTIL_BEGIN_NAMESPACE

// predeclare
template <class t_tyChar>
class JsoValue;
template <class t_tyChar>
class _JsoObject;
template <class t_tyChar>
class _JsoArray;
template <class t_tyChar, bool t_kfConst>
class JsoIterator;

// This exception will get thrown if the user of the read cursor does something inappropriate given the current context.
class json_objects_bad_usage_exception : public _t__Named_exception<__JSONSTRM_DEFAULT_ALLOCATOR>
{
  typedef json_objects_bad_usage_exception _TyThis;
  typedef _t__Named_exception<__JSONSTRM_DEFAULT_ALLOCATOR> _TyBase;
public:
  json_objects_bad_usage_exception( const char * _pc ) 
      : _TyBase( _pc ) 
  {
  }
  json_objects_bad_usage_exception(const string_type &__s)
      : _TyBase(__s)
  {
  }
  json_objects_bad_usage_exception(const char *_pcFmt, va_list _args)
      : _TyBase(_pcFmt, _args)
  {
  }
};
// By default we will always add the __FILE__, __LINE__ even in retail for debugging purposes.
#define THROWJSONBADUSAGE(MESG, ...) ExceptionUsage<json_objects_bad_usage_exception>::ThrowFileLineFunc(__FILE__, __LINE__, FUNCTION_PRETTY_NAME, MESG, ##__VA_ARGS__)

// JsoIterator:
// This iterator may be iterating an object or an array.
// For an object we iterate in key order.
// For an array we iterate in index order.
template <class t_tyChar, bool t_kfConst>
class JsoIterator
{
  typedef JsoIterator _tyThis;

public:
  typedef std::conditional_t<t_kfConst, typename _JsoObject<t_tyChar>::_tyConstIterator, typename _JsoObject<t_tyChar>::_tyIterator> _tyObjectIterator;
  typedef std::conditional_t<t_kfConst, typename _JsoArray<t_tyChar>::_tyConstIterator, typename _JsoArray<t_tyChar>::_tyIterator> _tyArrayIterator;
  typedef JsoValue<t_tyChar> _tyJsoValue;
  typedef std::conditional_t<t_kfConst, const _tyJsoValue, _tyJsoValue> _tyQualJsoValue;
  typedef typename _JsoObject<t_tyChar>::_tyMapValueType _tyKeyValueType; // This type is only used by objects.
  typedef std::conditional_t<t_kfConst, const _tyKeyValueType, _tyKeyValueType> _tyQualKeyValueType;

  ~JsoIterator()
  {
    if (!!m_pvIterator)
      _DestroyIterator(m_pvIterator);
  }
  JsoIterator() = default;
  JsoIterator(const JsoIterator &_r)
      : m_fObjectIterator(_r.m_fObjectIterator)
  {
    m_fObjectIterator ? _CreateIterator(_r.GetObjectIterator()) : _CreateIterator(_r.GetArrayIterator());
  }
  JsoIterator(const JsoIterator &&_rr)
      : m_fObjectIterator(_rr.m_fObjectIterator)
  {
    m_fObjectIterator ? _CreateIterator(std::move(_rr.GetObjectIterator())) : _CreateIterator(std::move(_rr.GetArrayIterator()));
  }
  explicit JsoIterator(_tyObjectIterator const &_rit)
      : m_fObjectIterator(true)
  {
    _CreateIterator(_rit);
  }
  JsoIterator(_tyObjectIterator &&_rrit)
      : m_fObjectIterator(true)
  {
    _CreateIterator(std::move(_rrit));
  }
  explicit JsoIterator(_tyArrayIterator const &_rit)
      : m_fObjectIterator(false)
  {
    _CreateIterator(_rit);
  }
  JsoIterator(_tyArrayIterator &&_rrit)
      : m_fObjectIterator(false)
  {
    _CreateIterator(std::move(_rrit));
  }
  JsoIterator &operator=(JsoIterator const &_r)
  {
    if (this != &_r)
    {
      Clear();
      m_fObjectIterator = _r.m_fObjectIterator;
      m_fObjectIterator ? _CreateIterator(_r.GetObjectIterator()) : _CreateIterator(_r.GetArrayIterator());
    }
    return *this;
  }
  JsoIterator &operator=(JsoIterator &&_rr)
  {
    if (this != &_rr)
    {
      Clear();
      swap(_rr);
    }
    return *this;
  }

  bool FIsObjectIterator() const
  {
    return m_fObjectIterator;
  }
  bool FIsArrayIterator() const
  {
    return !m_fObjectIterator;
  }
  EJsonValueType JvtGetValueType() const
  {
    return m_fObjectIterator ? ejvtObject : ejvtArray;
  }

  void swap(JsoIterator &_r)
  {
    std::swap(m_fObjectIterator, _r.m_fObjectIterator);
    std::swap(m_pvIterator, _r.m_pvIterator);
  }

  void Clear()
  {
    if (!!m_pvIterator)
    {
      void *pv = m_pvIterator;
      m_pvIterator = nullptr;
      _DestroyIterator(pv);
    }
  }

  JsoIterator &operator++()
  {
    if (m_fObjectIterator)
      ++GetObjectIterator();
    else
      ++GetArrayIterator();
    return *this;
  }
  JsoIterator operator++(int)
  {
    if (m_fObjectIterator)
      return JsoIterator(GetObjectIterator()++);
    else
      return JsoIterator(GetArrayIterator()++);
  }
  JsoIterator &operator--()
  {
    if (m_fObjectIterator)
      --GetObjectIterator();
    else
      --GetArrayIterator();
    return *this;
  }
  JsoIterator operator--(int)
  {
    if (m_fObjectIterator)
      return JsoIterator(GetObjectIterator()--);
    else
      return JsoIterator(GetArrayIterator()--);
  }
  typename _JsoArray<t_tyChar>::difference_type operator-(const JsoIterator &_r) const
  {
    if (m_fObjectIterator)
      THROWJSONBADUSAGE("Not valid for object iterator.");
    return GetArrayIterator() - _r.GetArrayIterator();
  }

  const _tyObjectIterator &GetObjectIterator() const
  {
    return const_cast<_tyThis *>(this)->GetObjectIterator();
  }
  _tyObjectIterator &GetObjectIterator()
  {
    if (!m_pvIterator)
      THROWJSONBADUSAGE("Not connected to iterator.");
    if (!m_fObjectIterator)
      THROWJSONBADUSAGE("Called on array.");
    return *(_tyObjectIterator *)m_pvIterator;
  }
  const _tyArrayIterator &GetArrayIterator() const
  {
    return const_cast<_tyThis *>(this)->GetArrayIterator();
  }
  _tyArrayIterator &GetArrayIterator()
  {
    if (!m_pvIterator)
      THROWJSONBADUSAGE("Not connected to iterator.");
    if (m_fObjectIterator)
      THROWJSONBADUSAGE("Called on object.");
    return *(_tyArrayIterator *)m_pvIterator;
  }

  // This always returns the value for both objects and arrays since it has to return the same thing.
  _tyQualJsoValue &operator*() const
  {
    return m_fObjectIterator ? GetObjectIterator()->second : *GetArrayIterator();
  }
  _tyQualJsoValue *operator->() const
  {
    return m_fObjectIterator ? &GetObjectIterator()->second : &*GetArrayIterator();
  }
  _tyQualKeyValueType &GetKeyValue() const
  {
    return *GetObjectIterator();
  }

  bool operator==(const _tyThis &_r) const
  {
    if (m_fObjectIterator != _r.m_fObjectIterator)
      return false;
    return m_fObjectIterator ? (_r.GetObjectIterator() == GetObjectIterator()) : (_r.GetArrayIterator() == GetArrayIterator());
  }
  bool operator!=(const _tyThis &_r) const
  {
    return !this->operator==(_r);
  }

protected:
  void _CreateIterator(_tyObjectIterator const &_rit)
  {
    Assert(!m_pvIterator);
    Assert(m_fObjectIterator);
    m_pvIterator = DBG_NEW _tyObjectIterator(_rit);
  }
  void _CreateIterator(_tyObjectIterator &&_rrit)
  {
    Assert(!m_pvIterator);
    Assert(m_fObjectIterator);
    m_pvIterator = DBG_NEW _tyObjectIterator(std::move(_rrit));
  }
  void _CreateIterator(_tyArrayIterator const &_rit)
  {
    Assert(!m_pvIterator);
    Assert(!m_fObjectIterator);
    m_pvIterator = DBG_NEW _tyArrayIterator(_rit);
  }
  void _CreateIterator(_tyArrayIterator &&_rrit)
  {
    Assert(!m_pvIterator);
    Assert(!m_fObjectIterator);
    m_pvIterator = DBG_NEW _tyArrayIterator(std::move(_rrit));
  }
  void _DestroyIterator(void *_pv)
  {
    Assert(!!_pv);
    if (m_fObjectIterator)
      delete (_tyObjectIterator *)_pv;
    else
      delete (_tyArrayIterator *)_pv;
  }

  void *m_pvIterator{nullptr}; // This is a pointer to a dynamically allocated iterator of the appropriate type.
  bool m_fObjectIterator{true};
};

// JsoValue:
// Every JSON object is a value. In fact every single JSON object is represented by the class JsoValue because that is the best spacewise
//  way of doing things. We embed the string/object/array within this class to implement the different JSON objects.
#if defined(__amd64__) || defined(_M_AMD64) || defined(__x86_64__) || defined(__ia64__)
#pragma pack(push, 8) // Ensure that we pack this on an 8 byte boundary for 64bit compilation.
#else
#pragma pack(push, 4) // Ensure that we pack this on an 4 byte boundary for 32bit compilation.
#endif

template <class t_tyChar>
class JsoValue
{
  typedef JsoValue _tyThis;

public:
  typedef t_tyChar _tyChar;
  typedef JsonCharTraits<_tyChar> _tyCharTraits;
  typedef const _tyChar *_tyLPCSTR;
  typedef std::basic_string<_tyChar> _tyStdStr;
  typedef StrWRsv<_tyStdStr> _tyStrWRsv; // string with reserve buffer.
  typedef _JsoObject<_tyChar> _tyJsoObject;
  typedef _JsoArray<_tyChar> _tyJsoArray;
  typedef JsonFormatSpec<_tyCharTraits> _tyJsonFormatSpec;
  typedef JsoIterator<_tyChar, false> iterator;
  typedef JsoIterator<_tyChar, true> const_iterator;

  ~JsoValue()
  {
    _ClearValue();
  }
  JsoValue(EJsonValueType _jvt = ejvtJsonValueTypeCount)
  {
    if (ejvtJsonValueTypeCount != _jvt)
      _AllocateValue(_jvt);
  }
  JsoValue(const JsoValue &_r)
  {
    *this = _r;
  }
  JsoValue(JsoValue &&_rr)
  {
    if (_rr.JvtGetValueType() != ejvtJsonValueTypeCount)
    {
      switch (_rr.JvtGetValueType())
      {
      case ejvtNumber:
      case ejvtString:
        new (m_rgbyValBuf) _tyStrWRsv(std::move(_rr.StrGet()));
        break;
      case ejvtObject:
        new (m_rgbyValBuf) _tyJsoObject(std::move(_rr._ObjectGet()));
        break;
      case ejvtArray:
        new (m_rgbyValBuf) _tyJsoArray(std::move(_rr._ArrayGet()));
        break;
      default:
        Assert(0); // random value...
      case ejvtNull:
      case ejvtTrue:
      case ejvtFalse:
      case ejvtJsonValueTypeCount:
        break;
      }
      m_jvtType = _rr.JvtGetValueType(); // throw-safety.
    }
  }
  JsoValue &operator=(const JsoValue &_r)
  {
    if (this != &_r)
    {
      SetValueType(_r.JvtGetValueType());
      switch (JvtGetValueType())
      {
      case ejvtNull:
      case ejvtTrue:
      case ejvtFalse:
        break; // nothing to do
      case ejvtNumber:
      case ejvtString:
        StrGet() = _r.StrGet();
        break;
      case ejvtObject:
        _ObjectGet() = _r._ObjectGet();
        break;
      case ejvtArray:
        _ArrayGet() = _r._ArrayGet();
        break;
      default:
        Assert(0); // random value...
        [[fallthrough]];
      case ejvtJsonValueTypeCount:
        break; // assigned to an empty object.
      }
    }
    return *this;
  }
  JsoValue &operator=(JsoValue &&_rr)
  {
    if (this != &_rr)
    {
      Clear();
      if (_rr.JvtGetValueType() != ejvtJsonValueTypeCount)
      {
        switch (_rr.JvtGetValueType())
        {
        case ejvtNumber:
        case ejvtString:
          new (m_rgbyValBuf) _tyStrWRsv(std::move(_rr.StrGet()));
          break;
        case ejvtObject:
          new (m_rgbyValBuf) _tyJsoObject(std::move(_rr._ObjectGet()));
          break;
        case ejvtArray:
          new (m_rgbyValBuf) _tyJsoArray(std::move(_rr._ArrayGet()));
          break;
        default:
          Assert(0); // random value...
        case ejvtNull:
        case ejvtTrue:
        case ejvtFalse:
        case ejvtJsonValueTypeCount:
          break;
        }
        m_jvtType = _rr.JvtGetValueType(); // throw-safety.
      }
    }
    return *this;
  }
  void swap(JsoValue &_r)
  {
    std::swap(m_jvtType, _r.m_jvtType);
    if (_r._FHasBufData() || _FHasBufData())
    {
      uint8_t rgbyValBuf[s_kstSizeValBuf];
      memcpy(rgbyValBuf, _r.m_rgbyValBuf, sizeof(m_rgbyValBuf));
      memcpy(_r.m_rgbyValBuf, m_rgbyValBuf, sizeof(m_rgbyValBuf));
      memcpy(m_rgbyValBuf, rgbyValBuf, sizeof(m_rgbyValBuf));
    }
  }
  bool _FHasBufData() const
  {
    return m_jvtType >= ejvtFirstJsonValueless && m_jvtType <= ejvtLastJsonSpecifiedValue;
  }

  EJsonValueType JvtGetValueType() const
  {
    return m_jvtType;
  }
  void SetValueType(const EJsonValueType _jvt)
  {
    if (_jvt != m_jvtType)
    {
      _ClearValue();
      _AllocateValue(_jvt);
    }
  }
  void Clear()
  {
    SetValueType(ejvtJsonValueTypeCount);
  }

  // Compare objects:
  bool operator==(const _tyThis &_r) const
  {
    return FCompare(_r);
  }
  bool operator!=(const _tyThis &_r) const
  {
    return !FCompare(_r);
  }
  bool FCompare(_tyThis const &_r) const
  {
    bool fSame = JvtGetValueType() == _r.JvtGetValueType(); // Arbitrary comparison between different types.
    if (fSame)
    {
      switch (JvtGetValueType())
      {
      case ejvtNull:
      case ejvtTrue:
      case ejvtFalse:
        break;
      case ejvtNumber:
      case ejvtString:
        // Note that I don't mean to compare numbers as numbers - only as the strings they are represented in.
        fSame = (StrGet() == _r.StrGet());
        break;
      case ejvtObject:
        fSame = (_ObjectGet() == _r._ObjectGet());
        break;
      case ejvtArray:
        fSame = (_ArrayGet() == _r._ArrayGet());
        break;
      default:
      case ejvtJsonValueTypeCount:
        THROWJSONBADUSAGE("Invalid value type [%hhu].", JvtGetValueType());
        break;
      }
    }
    return fSame;
  }

  std::strong_ordering operator<=>(_tyThis const &_r) const
  {
    return ICompare(_r);
  }
  std::strong_ordering ICompare(_tyThis const &_r) const
  {
    auto comp = (int)JvtGetValueType() <=> (int)_r.JvtGetValueType(); // Arbitrary comparison between different types.
    if (0 == comp)
    {
      switch (JvtGetValueType())
      {
      case ejvtNull:
      case ejvtTrue:
      case ejvtFalse:
        break;
      case ejvtNumber:
      case ejvtString:
        // Note that I don't mean to compare numbers as numbers - only as the strings they are represented in.
        comp = StrGet() <=> _r.StrGet();
        break;
      case ejvtObject:
        comp = _ObjectGet() <=> _r._ObjectGet();
        break;
      case ejvtArray:
        comp = _ArrayGet() <=> _r._ArrayGet();
        break;
      default:
      case ejvtJsonValueTypeCount:
        THROWJSONBADUSAGE("Invalid value type [%hhu].", JvtGetValueType());
        break;
      }
    }
    return comp;
  }

  bool FEmpty() const
  {
    return JvtGetValueType() == ejvtJsonValueTypeCount;
  }
  bool FIsNullOrEmpty() const
  {
    return FIsNull() || FEmpty();
  }
  bool FIsNull() const
  {
    return ejvtNull == m_jvtType;
  }
  bool FIsBoolean() const
  {
    return (ejvtTrue == m_jvtType) || (ejvtFalse == m_jvtType);
  }
  bool FIsTrue() const
  {
    return ejvtTrue == m_jvtType;
  }
  bool FIsFalse() const
  {
    return ejvtFalse == m_jvtType;
  }
  bool FIsString() const
  {
    return (ejvtString == m_jvtType);
  }
  bool FIsNumber() const
  {
    return (ejvtNumber == m_jvtType);
  }
  bool FIsAggregate() const
  {
    return FIsObject() || FIsArray();
  }
  bool FIsObject() const
  {
    return ejvtObject == m_jvtType;
  }
  bool FIsArray() const
  {
    return ejvtArray == m_jvtType;
  }
  size_t GetSize() const
  {
    if (!FIsAggregate())
      THROWJSONBADUSAGE("Called on non-aggregate.");
    if (FIsObject())
      return _ObjectGet().GetSize();
    else
      return _ArrayGet().GetSize();
  }
  void GetBoolValue(bool &_rf) const
  {
    if (ejvtTrue == m_jvtType)
      _rf = true;
    else if (ejvtFalse == m_jvtType)
      _rf = false;
    else
      THROWJSONBADUSAGE("Called on non-boolean.");
  }
  const _tyStrWRsv &StrGet() const
  {
    Assert( FIsString() || FIsNumber() );
    if (!FIsString() && !FIsNumber())
      THROWJSONBADUSAGE("Called on non-string/num.");
    return *static_cast<const _tyStrWRsv *>((const void *)m_rgbyValBuf);
  }
  _tyStrWRsv &StrGet()
  {
    Assert( FIsString() || FIsNumber() );
    if (!FIsString() && !FIsNumber())
      THROWJSONBADUSAGE("Called on non-string/num.");
    return *static_cast<_tyStrWRsv *>((void *)m_rgbyValBuf);
  }
  const _tyJsoObject &_ObjectGet() const
  {
    Assert( FIsObject() );
    if (!FIsObject())
      THROWJSONBADUSAGE("Called on non-Object.");
    return *static_cast<const _tyJsoObject *>((const void *)m_rgbyValBuf);
  }
  _tyJsoObject &_ObjectGet()
  {
    Assert( FIsObject() );
    if (!FIsObject())
      THROWJSONBADUSAGE("Called on non-Object.");
    return *static_cast<_tyJsoObject *>((void *)m_rgbyValBuf);
  }
  const _tyJsoArray &_ArrayGet() const
  {
    Assert( FIsArray() );
    if (!FIsArray())
      THROWJSONBADUSAGE("Called on non-Array.");
    return *static_cast<const _tyJsoArray *>((const void *)m_rgbyValBuf);
  }
  _tyJsoArray &_ArrayGet()
  {
    Assert( FIsArray() );
    if (!FIsArray())
      THROWJSONBADUSAGE("Called on non-Array.");
    return *static_cast<_tyJsoArray *>((void *)m_rgbyValBuf);
  }
  // Various number conversion methods.
  template <class t_tyNum>
  void _GetValue(_tyLPCSTR _pszFmt, t_tyNum &_rNumber) const
  {
    Assert( FIsNumber() );
    if (ejvtNumber != JvtGetValueType())
      THROWJSONBADUSAGE("Not at a numeric value type.");

    // The presumption is that sscanf won't read past any decimal point if scanning a non-floating point number.
    int iRet = sscanf(StrGet().c_str(), _pszFmt, &_rNumber);
    Assert(1 == iRet); // Due to the specification of number we expect this to always succeed.
  }
  void GetValue(uint8_t &_rby) const { _GetValue("%hhu", _rby); }
  void GetValue(int8_t &_rsby) const { _GetValue("%hhd", _rsby); }
  void GetValue(uint16_t &_rus) const { _GetValue("%hu", _rus); }
  void GetValue(int16_t &_rss) const { _GetValue("%hd", _rss); }
  void GetValue(uint32_t &_rui) const { _GetValue("%u", _rui); }
  void GetValue(int32_t &_rsi) const { _GetValue("%d", _rsi); }
  void GetValue(uint64_t &_rul) const { _GetValue("%llu", _rul); }
  void GetValue(int64_t &_rsl) const { _GetValue("%lld", _rsl); }
  void GetValue(float &_rfl) const { _GetValue("%e", _rfl); }
  void GetValue(double &_rdbl) const { _GetValue("%e", _rdbl); }
  void GetValue(long double &_rldbl) const { _GetValue("%Le", _rldbl); }

  // Setting methods: These overwrite the existing element at this location.
  void SetEmpty() // Note that this is not the same as the NullValue - see below.
  {
    SetValueType(ejvtJsonValueTypeCount);
  }
  void SetNullValue()
  {
    SetValueType(ejvtNull);
  }
  void SetBoolValue(bool _f)
  {
    SetValueType(_f ? ejvtTrue : ejvtFalse);
  }
  _tyThis &operator=(bool _f)
  {
    SetBoolValue(_f);
  }
  void SetStringValue(_tyLPCSTR _psz, size_t _stLen = (std::numeric_limits<size_t>::max)())
  {
    if (_stLen == (std::numeric_limits<size_t>::max)())
      _stLen = StrNLen(_psz);
    SetValueType(ejvtString);
    StrGet().assign(_psz, _stLen);
  }
  // String methods for the current character type.
  template <class t_tyStr>
  void SetStringValue(t_tyStr const &_rstr) 
    requires ( TAreSameSizeTypes_v< typename t_tyStr::value_type, _tyChar > )
  {
    SetStringValue( (const _tyChar *)&_rstr[0], _rstr.length()) ;
  }
  // String methods requiring conversion. No reason for the move method since we must convert anyway.
  template <class t_tyStr>
  void SetStringValue(t_tyStr const& _rstr)
    requires ( !TAreSameSizeTypes_v< typename t_tyStr::value_type, _tyChar > )
  {
    _tyStrWRsv strConverted;
    ConvertString(strConverted, _rstr);
    SetStringValue(std::move(strConverted));
  }
  template <class t_tyStr>
  void SetStringValue(t_tyStr &&_rrstr) 
    requires ( is_same_v< typename t_tyStr::value_type, _tyChar > )
  {
    SetValueType(ejvtString);
    StrGet() = std::move( _rrstr );
  }
  template < class t_tyStr >
  _tyThis &operator=( const t_tyStr &_rstr ) 
    requires( TIsCharType_v< typename t_tyStr::value_type > )
  {
    SetStringValue( _rstr );
    return *this;
  }
  template < class t_tyStr >
  _tyThis &operator=(_tyStdStr &&_rrstr)
    requires( TIsCharType_v< typename t_tyStr::value_type > )
  {
    SetStringValue( std::move( _rrstr ) );
    return *this;
  }

  void SetValue(uint8_t _by)
  {
    _SetValue("%hhu", _by);
  }
  void SetValue(int8_t _sby)
  {
    _SetValue("%hhd", _sby);
  }
  void SetValue(uint16_t _us)
  {
    _SetValue("%hu", _us);
  }
  _tyThis &operator=(uint16_t _us)
  {
    SetValue(_us);
    return *this;
  }
  void SetValue(int16_t _ss)
  {
    _SetValue("%hd", _ss);
  }
  _tyThis &operator=(int16_t _ss)
  {
    SetValue(_ss);
    return *this;
  }
  void SetValue(uint32_t _ui)
  {
    _SetValue("%u", _ui);
  }
  _tyThis &operator=(uint32_t _ui)
  {
    SetValue(_ui);
    return *this;
  }
  void SetValue(int32_t _si)
  {
    _SetValue("%d", _si);
  }
  _tyThis &operator=(int32_t _si)
  {
    SetValue(_si);
    return *this;
  }
  void SetValue(uint64_t _ul)
  {
    _SetValue("%llu", _ul);
  }
  _tyThis &operator=(uint64_t _ul)
  {
    SetValue(_ul);
    return *this;
  }
  void SetValue(int64_t _sl)
  {
    _SetValue("%lld", _sl);
  }
  _tyThis &operator=(int64_t _sl)
  {
    SetValue(_sl);
    return *this;
  }
  void SetValue(double _dbl)
  {
    _SetValue("%f", _dbl);
  }
  _tyThis &operator=(double _dbl)
  {
    SetValue(_dbl);
    return *this;
  }
  void SetValue(long double _ldbl)
  {
    _SetValue("%Lf", _ldbl);
  }
  _tyThis &operator=(long double _ldbl)
  {
    SetValue(_ldbl);
    return *this;
  }
#if defined( WIN32 ) || defined ( __APPLE__ ) // Linux seems to handle this correctly without - and there's an endless loop with...
  // Translate long and unsigned long appropriately by platform size:
  void SetValue(long _l)
     requires ( sizeof( int32_t ) == sizeof( long ) )
  {
    return SetValue((int32_t)_l);
  }
  _tyThis &operator =(long _l)
     requires ( sizeof( int32_t ) == sizeof( long ) )
  {
    SetValue((int32_t)_l);
    return *this;
  }
  void SetValue(long _l)
    requires (sizeof(int64_t) == sizeof(long))
  {
    return SetValue((int64_t)_l);
  }
  _tyThis &operator =(long _l)
    requires (sizeof(int64_t) == sizeof(long))
  {
    SetValue((int64_t)_l);
    return *this;
  }
  void SetValue(unsigned long _l)
    requires (sizeof(uint32_t) == sizeof(unsigned long))
  {
    return SetValue((uint32_t)_l);
  }
  _tyThis &operator =(unsigned long _l)
    requires (sizeof(uint32_t) == sizeof(unsigned long))
  {
    SetValue((uint32_t)_l);
    return *this;
  }
  void SetValue(unsigned long _l)
    requires (sizeof(uint64_t) == sizeof(unsigned long))
  {
    return SetValue((uint64_t)_l);
  }
  _tyThis &operator =(unsigned long _l)
    requires (sizeof(uint64_t) == sizeof(unsigned long))
  {
    SetValue((uint64_t)_l);
    return *this;
  }
#endif //WIN32 || APPLE

  // Read JSON from a string - throws upon finding bad JSON, etc.
  template <class t_tyStr>
  void FromString(t_tyStr const &_rstr)
  {
    FromString(&_rstr[0], _rstr.length()); // works for string views.
  }
  void FromString(const _tyChar *_psz, size_t _stLen)
  {
    typedef JsonFixedMemInputStream<_tyCharTraits> _tyJsonInputStream;
    typedef JsonReadCursor<_tyJsonInputStream> _tyJsonReadCursor;
    _tyJsonInputStream jisFixed(_psz, _stLen);
    _tyJsonReadCursor jrc;
    jisFixed.AttachReadCursor(jrc);
    FromJSONStream(jrc);
  }
  template <class t_tyJsonInputStream>
  void FromJSONStream(JsonReadCursor<t_tyJsonInputStream> &_jrc)
  {
    SetValueType(_jrc.JvtGetValueType());
    switch (JvtGetValueType())
    {
    case ejvtNull:
    case ejvtTrue:
    case ejvtFalse:
      break;
    case ejvtNumber:
    case ejvtString:
      _jrc.GetValue(StrGet());
      break;
    case ejvtObject:
      _ObjectGet().FromJSONStream(_jrc);
      break;
    case ejvtArray:
      _ArrayGet().FromJSONStream(_jrc);
      break;
    default:
    case ejvtJsonValueTypeCount:
      THROWJSONBADUSAGE("Invalid value type [%hhu].", JvtGetValueType());
      break;
    }
  }
  template <class t_tyStr, class t_tyFilter>
  void FromString( t_tyStr const &_rstr, t_tyFilter &_rfFilter )
  {
    FromString( &_rstr[0], _rstr.length(), _rfFilter );
  }
  template <class t_tyFilter>
  void FromString( const _tyChar *_psz, size_t _stLen, t_tyFilter &_rfFilter )
  {
    typedef JsonFixedMemInputStream<_tyCharTraits> _tyJsonInputStream;
    typedef JsonReadCursor<_tyCharTraits> _tyJsonReadCursor;
    _tyJsonInputStream jisFixed(_psz, _stLen);
    _tyJsonReadCursor jrc;
    jisFixed.AttachReadCursor(jrc);
    FromJSONStream(jrc, _rfFilter);
  }
  // FromJSONStream with a filter method.
  // The filtering takes place in the objects and arrays because these create sub-values.
  // The filter is not applied to the root value itself.
  template <class t_tyJsonInputStream, class t_tyFilter>
  void FromJSONStream(JsonReadCursor<t_tyJsonInputStream> &_jrc, t_tyFilter &_rfFilter)
  {
    SetValueType(_jrc.JvtGetValueType());
    switch (JvtGetValueType())
    {
    case ejvtNull:
    case ejvtTrue:
    case ejvtFalse:
      break;
    case ejvtNumber:
    case ejvtString:
      _jrc.GetValue(StrGet());
      break;
    case ejvtObject:
      _ObjectGet().FromJSONStream(_jrc, *this, _rfFilter);
      break;
    case ejvtArray:
      _ArrayGet().FromJSONStream(_jrc, *this, _rfFilter);
      break;
    default:
    case ejvtJsonValueTypeCount:
      THROWJSONBADUSAGE("Invalid value type [%hhu].", JvtGetValueType());
      break;
    }
  }

  template <class t_tyStr>
  void ToString(t_tyStr &_rstr, const _tyJsonFormatSpec *_pjfs = 0) const
  {
    // Stream to memory stream (segmented array) and then copy that to the string.
    typedef JsonOutputMemStream<_tyCharTraits> _tyJsonOutputStream;
    typedef JsonValueLife<_tyJsonOutputStream> _tyJsonValueLife;
    _tyJsonOutputStream jos;
    { //B
      _tyJsonValueLife jvlRoot(jos, JvtGetValueType(), _pjfs);
      ToJSONStream(jvlRoot);
    } //EB
    size_t stLen = jos.GetLengthChars();
    _rstr.resize(stLen); // fills to stLen+1 with 0 and sets length to stLen.
    typename _tyJsonOutputStream::_tyMemStream &rms = jos.GetMemStream();
    rms.Seek(0, SEEK_SET);
    rms.Read(&_rstr[0], stLen * sizeof(_tyChar));
  }
  template <class t_tyJsonOutputStream>
  void ToJSONStream(JsonValueLife<t_tyJsonOutputStream> &_jvl) const
  {
    Assert(JvtGetValueType() == _jvl.JvtGetValueType());
    switch (JvtGetValueType())
    {
    case ejvtNull:
    case ejvtTrue:
    case ejvtFalse:
      break; // nothing to do - _jvl has already been created with the correct value type.
    case ejvtNumber:
    case ejvtString:
      _jvl.RJvGet().PCreateStringValue()->assign(StrGet());
      break;
    case ejvtObject:
      _ObjectGet().ToJSONStream(_jvl);
      break;
    case ejvtArray:
      _ArrayGet().ToJSONStream(_jvl);
      break;
    default:
    case ejvtJsonValueTypeCount:
      THROWJSONBADUSAGE("Invalid value type [%hhu].", JvtGetValueType());
      break;
    }
  }
  template <class t_tyStr, class t_tyFilter>
  void ToString(t_tyStr &_rstr, t_tyFilter &_rfFilter, const _tyJsonFormatSpec *_pjfs = 0) const
  {
    // Stream to memory stream (segmented array) and then copy that to the string.
    typedef JsonOutputMemStream<_tyCharTraits> _tyJsonOutputStream;
    typedef JsonValueLife<_tyJsonOutputStream> _tyJsonValueLife;
    _tyJsonOutputStream jos;
    { //B
      _tyJsonValueLife jvlRoot(jos, JvtGetValueType(), _pjfs);
      ToJSONStream(jvlRoot, _rfFilter);
    } //EB
    size_t stLen = jos.GetLengthChars();
    _rstr.resize(stLen); // fills to stLen+1 with 0 and sets length to stLen.
    typename _tyJsonOutputStream::_tyMemStream &rms = jos.GetMemStream();
    rms.Seek(0, SEEK_SET);
    rms.Read(&_rstr[0], stLen * sizeof(_tyChar));
  }
  template <class t_tyJsonOutputStream, class t_tyFilter>
  void ToJSONStream(JsonValueLife<t_tyJsonOutputStream> &_jvl, t_tyFilter &_rfFilter) const
  {
    Assert(JvtGetValueType() == _jvl.JvtGetValueType());
    switch (JvtGetValueType())
    {
    case ejvtNull:
    case ejvtTrue:
    case ejvtFalse:
      break; // nothing to do - _jvl has already been created with the correct value type.
    case ejvtNumber:
    case ejvtString:
      _jvl.RJvGet().PCreateStringValue()->assign(StrGet());
      break;
    case ejvtObject:
      _ObjectGet().ToJSONStream(_jvl, *this, _rfFilter);
      break;
    case ejvtArray:
      _ArrayGet().ToJSONStream(_jvl, *this, _rfFilter);
      break;
    default:
    case ejvtJsonValueTypeCount:
      THROWJSONBADUSAGE("Invalid value type [%hhu].", JvtGetValueType());
      break;
    }
  }
  // Output to ostream:
  template < class t_tyOstream >
	friend t_tyOstream & operator << ( t_tyOstream & _ros, const _tyThis & _r )
	{
    typedef t_tyOstream _tyOstream;
    std::ostream::sentry s(_ros);
    if ( s ) 
		{
			typedef JsonOutputOStream< _tyCharTraits, _tyOstream > _tyJsonOutputStream;
			typedef JsonFormatSpec< _tyCharTraits > _tyJsonFormatSpec;
			_tyJsonOutputStream jos( _ros );
			_tyJsonFormatSpec jfs;
			JsonValueLife< _tyJsonOutputStream > jvl( jos, _r.JvtGetValueType(), &jfs );
      _r.ToJSONStream( jvl );
    }
		return _ros;
	}

  // This will change the type of this object to an array (if necessary)
  //  and then set the capacity.
  void SetArrayCapacity( size_t _n )
  {
    SetValueType( ejvtArray );
    _ArrayGet().SetCapacity( _n );
  }
  // Reading arrays and objects:
  // Array index can be used on either array or object.
  // Will throw semanticerror due to bad index access if accessed beyond the end.
  JsoValue &operator[](size_t _st)
  {
    return GetEl(_st);
  }
  const JsoValue &operator[](size_t _st) const
  {
    return GetEl(_st);
  }
  JsoValue &GetEl(size_t _st)
  {
    return _ArrayGet().GetEl(_st);
  }
  const JsoValue &GetEl(size_t _st) const
  {
    return _ArrayGet().GetEl(_st);
  }

  JsoValue &operator[](_tyLPCSTR _psz)
  {
    return GetEl(_psz);
  }
  const JsoValue &operator[](_tyLPCSTR _psz) const
  {
    return GetEl(_psz);
  }
  JsoValue &GetEl(_tyLPCSTR _psz)
  {
    return _ObjectGet().GetEl(_psz).second;
  }
  const JsoValue &GetEl(_tyLPCSTR _psz) const
  {
    return _ObjectGet().GetEl(_psz).second;
  }
  // Writing arrays and objects:
  // This will create a (null) value at position _st. And (null) values along the way in between the last existing value and _st.
  JsoValue &operator()(size_t _st)
  {
    return CreateOrGetEl(_st);
  }
  JsoValue &CreateOrGetEl(size_t _st)
  {
    // Gracefully change from a ejvtNull to an ejvtArray here for ease of usage.
    if ( ejvtNull == m_jvtType )
      SetValueType( ejvtArray );
    return _ArrayGet().CreateOrGetEl(_st);
  }
  JsoValue &AppendEl()
  {
    return _ArrayGet().AppendEl();
  }
  // This is create an object element with key named _psz or return the existing element named _psz.
  // A null value is added initially if the object key doesn't exist.
  JsoValue &operator()(_tyLPCSTR _psz)
  {
    return CreateOrGetEl(_psz);
  }
  JsoValue &CreateOrGetEl(_tyLPCSTR _psz)
  {
    return _ObjectGet().CreateOrGetEl(_psz).second;
  }

// We either return or throw and clang can't recognize that.
#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#else //_MSC_VER
#pragma warning(push)
#pragma warning(disable : 4715)
#endif //_MSC_VER
  iterator begin()
  {
    Assert( FIsAggregate() );
    if (ejvtObject == JvtGetValueType())
      return iterator(_ObjectGet().begin());
    else if (ejvtArray == JvtGetValueType())
      return iterator(_ArrayGet().begin());
    else
      THROWJSONBADUSAGE("Called on non-aggregate.");
  }
  const_iterator begin() const
  {
    Assert( FIsAggregate() );
    if (ejvtObject == JvtGetValueType())
      return const_iterator(_ObjectGet().begin());
    else if (ejvtArray == JvtGetValueType())
      return const_iterator(_ArrayGet().begin());
    else
      THROWJSONBADUSAGE("Called on non-aggregate.");
  }
  iterator end()
  {
    Assert( FIsAggregate() );
    if (ejvtObject == JvtGetValueType())
      return iterator(_ObjectGet().end());
    else if (ejvtArray == JvtGetValueType())
      return iterator(_ArrayGet().end());
    else
      THROWJSONBADUSAGE("Called on non-aggregate.");
  }
  const_iterator end() const
  {
    Assert( FIsAggregate() );
    if (ejvtObject == JvtGetValueType())
      return const_iterator(_ObjectGet().end());
    else if (ejvtArray == JvtGetValueType())
      return const_iterator(_ArrayGet().end());
    else
      THROWJSONBADUSAGE("Called on non-aggregate.");
  }
#ifndef _MSC_VER
#pragma GCC diagnostic pop
#else //_MSC_VER
#pragma warning(pop)
#endif //_MSC_VER

protected:
  template <class t_tyNum>
  void _SetValue(_tyLPCSTR _pszFmt, t_tyNum _num)
  {
    const int knNum = 512;
    char rgcNum[knNum];
    int nPrinted = snprintf(rgcNum, knNum, _pszFmt, _num);
    Assert(nPrinted < knNum);
    SetValueType(ejvtNumber);
    StrGet().assign(rgcNum, (std::min)(nPrinted, knNum - 1));
  }
  void _ClearValue()
  {
    EJsonValueType jvt = m_jvtType;
    m_jvtType = ejvtJsonValueTypeCount;
    switch (jvt)
    {
    case ejvtNumber:
    case ejvtString:
      static_cast<_tyStrWRsv *>((void *)m_rgbyValBuf)->~_tyStrWRsv();
      break;
    case ejvtObject:
      static_cast<_tyJsoObject *>((void *)m_rgbyValBuf)->~_tyJsoObject();
      break;
    case ejvtArray:
      static_cast<_tyJsoArray *>((void *)m_rgbyValBuf)->~_tyJsoArray();
      break;
    default:
      Assert(0); // random value...
      [[fallthrough]];
    case ejvtNull:
    case ejvtTrue:
    case ejvtFalse:
    case ejvtJsonValueTypeCount:
      break;
    }
  }
  void _AllocateValue(const EJsonValueType _jvt)
  {
    Assert(ejvtJsonValueTypeCount == JvtGetValueType());
    switch (_jvt)
    {
    case ejvtNumber:
    case ejvtString:
      new (m_rgbyValBuf) _tyStrWRsv();
      break;
    case ejvtObject:
      new (m_rgbyValBuf) _tyJsoObject();
      break;
    case ejvtArray:
      new (m_rgbyValBuf) _tyJsoArray();
      break;
    default:
      Assert(0); // random value...
      [[fallthrough]];
    case ejvtNull:
    case ejvtTrue:
    case ejvtFalse:
    case ejvtJsonValueTypeCount:
      break;
    }
    m_jvtType = _jvt; // throw-safety.
  }

  // Put the object buffer as the first member because then it will have alignment of the object - which is 8(64bit) or 4(32bit).
  static constexpr size_t s_kstSizeValBuf = (std::max)(sizeof(_tyStrWRsv), (std::max)(sizeof(_tyJsoObject), sizeof(_tyJsoArray)));
  uint8_t m_rgbyValBuf[s_kstSizeValBuf]; // We aren't initializing this on purpose.
  EJsonValueType m_jvtType{ejvtJsonValueTypeCount};
};

#pragma pack(pop)

// _JsoObject:
// This is internal impl only - never exposed to the user of JsoValue.
template <class t_tyChar>
class _JsoObject
{
  typedef _JsoObject _tyThis;

public:
  typedef t_tyChar _tyChar;
  typedef JsonCharTraits<_tyChar> _tyCharTraits;
  typedef const t_tyChar *_tyLPCSTR;
  typedef std::basic_string<t_tyChar> _tyStdStr;
  typedef StrWRsv<_tyStdStr> _tyStrWRsv; // string with reserve buffer.
  typedef JsoValue<t_tyChar> _tyJsoValue;
  typedef std::map<_tyStrWRsv, _tyJsoValue> _tyMapValues;
  typedef typename _tyMapValues::iterator _tyIterator;
  typedef typename _tyMapValues::const_iterator _tyConstIterator;
  typedef typename _tyMapValues::value_type _tyMapValueType;

  _JsoObject() = default;
  ~_JsoObject() = default;
  _JsoObject(const _JsoObject &) = default;
  _JsoObject &operator=(const _JsoObject &) = default;
  _JsoObject(_JsoObject &&) = default;
  _JsoObject &operator=(_JsoObject &&) = default;

  void AssertValid(bool _fRecursive) const
  {
#if ASSERTSENABLED
    _tyConstIterator itCur = m_mapValues.begin();
    const _tyConstIterator itEnd = m_mapValues.end();
    for (; itCur != itEnd; ++itCur)
    {
      _tyChar tc = itCur->first[0];
      itCur->first[0] = 'a';
      itCur->first[0] = tc;
      if (_fRecursive)
        itCur->second.AssertValid(true);
      else
        Assert(ejvtJsonValueTypeCount != itCur->second.JvtGetValueType());
    }
#endif //ASSERTSENABLED
  }

  void Clear()
  {
    m_mapValues.clear();
  }
  size_t GetSize() const
  {
    return m_mapValues.size();
  }
  _tyIterator begin()
  {
    return m_mapValues.begin();
  }
  _tyConstIterator begin() const
  {
    return m_mapValues.begin();
  }
  _tyIterator end()
  {
    return m_mapValues.end();
  }
  _tyConstIterator end() const
  {
    return m_mapValues.end();
  }

  // Throws if there is no such el.
  _tyMapValueType &GetEl(_tyLPCSTR _psz)
  {
    _tyIterator it = m_mapValues.find(_psz);
    if (m_mapValues.end() == it)
      THROWJSONBADUSAGE("No such key [%s]", _psz);
    return *it;
  }
  const _tyMapValueType &GetEl(_tyLPCSTR _psz) const
  {
    return const_cast<_tyThis *>(this)->GetEl(_psz);
  }
  _tyMapValueType &CreateOrGetEl(_tyLPCSTR _psz)
  {
    std::pair<_tyIterator, bool> pib = m_mapValues.try_emplace(_psz, ejvtNull);
    return *pib.first;
  }

  bool operator==(const _tyThis &_r) const
  {
    return FCompare(_r);
  }
  bool FCompare(_tyThis const &_r) const
  {
    return m_mapValues == _r.m_mapValues;
  }
  std::strong_ordering operator<=>(_tyThis const &_r) const
  {
    return m_mapValues <=> _r.m_mapValues; // This doesn't compile.
  }

  template <class t_tyJsonInputStream>
  void FromJSONStream(JsonReadCursor<t_tyJsonInputStream> &_jrc)
  {
    Assert(m_mapValues.empty()); // Note that this isn't required just that it is expected. Remove assertion if needed.
    JsonRestoreContext<t_tyJsonInputStream> rxc(_jrc);
    if (!_jrc.FMoveDown())
      THROWJSONBADUSAGE("FMoveDown() returned false unexpectedly.");
    for (; !_jrc.FAtEndOfAggregate(); (void)_jrc.FNextElement())
    {
      _tyStrWRsv strKey;
      EJsonValueType jvt;
      bool f = _jrc.FGetKeyCurrent(strKey, jvt);
      if (!f)
        THROWJSONBADUSAGE("FGetKeyCurrent() returned false unexpectedly.");
      _tyJsoValue jvValue(jvt);
      jvValue.FromJSONStream(_jrc);
      std::pair<_tyIterator, bool> pib = m_mapValues.try_emplace(std::move(strKey), std::move(jvValue));
      if (!pib.second) // key already exists.
        THROWBADJSONSTREAM("Duplicate key found[%s].", strKey.c_str());
    }
  }
  template <class t_tyJsonInputStream, class t_tyFilter>
  void FromJSONStream(JsonReadCursor<t_tyJsonInputStream> &_jrc, _tyJsoValue const &_rjvContainer, t_tyFilter &_rfFilter)
  {
    Assert(m_mapValues.empty()); // Note that this isn't required just that it is expected. Remove assertion if needed.
#if 0                            // REVIEW:<dbien>: This was an idea but it would be hard for the caller to tell what was up - i.e. in which context we were - without flags or something. \
									 //    Also the same can be accomplished by merely refusing all elements below.
        if ( !_rfFilter( _jrc, _rjvContainer ) )
            return; // Leave this object empty.
#endif                           //0
    JsonRestoreContext<t_tyJsonInputStream> rxc(_jrc);
    if (!_jrc.FMoveDown())
      THROWJSONBADUSAGE("FMoveDown() returned false unexpectedly.");
    for (; !_jrc.FAtEndOfAggregate(); (void)_jrc.FNextElement())
    {
      if (!_rfFilter(_jrc, _rjvContainer))
        continue; // Skip this element.
      _tyStrWRsv strKey;
      EJsonValueType jvt;
      bool f = _jrc.FGetKeyCurrent(strKey, jvt);
      if (!f)
        THROWJSONBADUSAGE("FGetKeyCurrent() returned false unexpectedly.");
      _tyJsoValue jvValue(jvt);
      jvValue.FromJSONStream(_jrc, _rfFilter);
      std::pair<_tyIterator, bool> pib = m_mapValues.try_emplace(std::move(strKey), std::move(jvValue));
      if (!pib.second) // key already exists.
        THROWBADJSONSTREAM("Duplicate key found[%s].", strKey.c_str());
    }
  }

  template <class t_tyJsonOutputStream>
  void ToJSONStream(JsonValueLife<t_tyJsonOutputStream> &_jvl) const
  {
    _tyConstIterator itCur = m_mapValues.begin();
    const _tyConstIterator itEnd = m_mapValues.end();
    for (; itCur != itEnd; ++itCur)
    {
      JsonValueLife<t_tyJsonOutputStream> jvlObjectElement(_jvl, itCur->first.c_str(), itCur->second.JvtGetValueType());
      itCur->second.ToJSONStream(jvlObjectElement);
    }
  }
  template <class t_tyJsonOutputStream, class t_tyFilter>
  void ToJSONStream(JsonValueLife<t_tyJsonOutputStream> &_jvl, _tyJsoValue const &_rjvContainer, t_tyFilter &_rfFilter) const
  {
    typedef JsoIterator<t_tyChar, true> _tyJsoIterator;
    _tyJsoIterator itCur(m_mapValues.begin());
    _tyJsoIterator const itEnd(m_mapValues.end());
    for (; itCur != itEnd; ++itCur)
    {
      if (!_rfFilter(_rjvContainer, itCur))
        continue; // Skip this element.
      JsonValueLife<t_tyJsonOutputStream> jvlObjectElement(_jvl, itCur.GetObjectIterator()->first.c_str(),
                                                           itCur.GetObjectIterator()->second.JvtGetValueType());
      itCur.GetObjectIterator()->second.ToJSONStream(jvlObjectElement, _rfFilter);
    }
  }
protected:
  _tyMapValues m_mapValues;
};

// _JsoArray:
// This is internal impl only - never exposed to the user of JsoValue.
template <class t_tyChar>
class _JsoArray
{
  typedef _JsoArray _tyThis;

public:
  typedef t_tyChar _tyChar;
  typedef const t_tyChar *_tyLPCSTR;
  typedef std::basic_string<t_tyChar> _tyStdStr;
  typedef StrWRsv<_tyStdStr> _tyStrWRsv; // string with reserve buffer.
  typedef JsoValue<t_tyChar> _tyJsoValue;
  typedef std::vector<_tyJsoValue> _tyVectorValues;
  typedef typename _tyVectorValues::iterator _tyIterator;
  typedef typename _tyVectorValues::const_iterator _tyConstIterator;
  typedef typename _tyVectorValues::value_type _tyVectorValueType;
  __ASSERT_SAME_TYPE(_tyJsoValue,_tyVectorValueType);
  typedef typename _tyVectorValues::difference_type difference_type;

  _JsoArray() = default;
  ~_JsoArray() = default;
  _JsoArray(const _JsoArray &) = default;
  _JsoArray &operator=(const _JsoArray &) = default;
  _JsoArray(_JsoArray &&) = default;
  _JsoArray &operator=(_JsoArray &&) = default;

  void Clear()
  {
    m_vecValues.clear();
  }
  size_t GetSize() const
  {
    return m_vecValues.size();
  }
  void SetCapacity( size_t _nCap )
  {
    m_vecValues.reserve( _nCap );
  }
  _tyIterator begin()
  {
    return m_vecValues.begin();
  }
  _tyConstIterator begin() const
  {
    return m_vecValues.begin();
  }
  _tyIterator end()
  {
    return m_vecValues.end();
  }
  _tyConstIterator end() const
  {
    return m_vecValues.end();
  }

  // Throws if there is no such el.
  _tyVectorValueType &GetEl(size_t _st)
  {
    if (_st > m_vecValues.size())
      THROWJSONBADUSAGE("_st[%zu] exceeds array size[%zu].", _st, m_vecValues.size());
    return m_vecValues[_st];
  }
  const _tyVectorValueType &GetEl(size_t _st) const
  {
    return const_cast<_tyThis *>(this)->GetEl(_st);
  }
  _tyVectorValueType &CreateOrGetEl(size_t _st)
  {
    // Create null items up until item _st if necessary.
    if (_st >= m_vecValues.size())
    {
      size_t stCreate = _st - m_vecValues.size() + 1;
      while (stCreate--)
        m_vecValues.emplace_back(ejvtNull);
    }
    return m_vecValues[_st];
  }
  _tyVectorValueType &AppendEl()
  {
    m_vecValues.emplace_back(ejvtNull);
    return m_vecValues.back();
  }

  bool operator==(const _tyThis &_r) const
  {
    return FCompare(_r);
  }
  bool FCompare(_tyThis const &_r) const
  {
    return m_vecValues == _r.m_vecValues;
  }
  std::strong_ordering operator<=>(_tyThis const &_r) const
  {
    return m_vecValues <=> _r.m_vecValues;
  }

  template <class t_tyJsonInputStream>
  void FromJSONStream(JsonReadCursor<t_tyJsonInputStream> &_jrc)
  {
    Assert(m_vecValues.empty()); // Note that this isn't required just that it is expected. Remove assertion if needed.
    JsonRestoreContext<t_tyJsonInputStream> rxc(_jrc);
    if (!_jrc.FMoveDown())
      THROWJSONBADUSAGE("FMoveDown() returned false unexpectedly.");
    for (; !_jrc.FAtEndOfAggregate(); (void)_jrc.FNextElement())
    {
      _tyJsoValue jvValue(_jrc.JvtGetValueType());
      jvValue.FromJSONStream(_jrc);
      m_vecValues.emplace_back(std::move(jvValue));
    }
  }
  template <class t_tyJsonInputStream, class t_tyFilter>
  void FromJSONStream(JsonReadCursor<t_tyJsonInputStream> &_jrc, _tyJsoValue const &_rjvContainer, t_tyFilter &_rfFilter)
  {
    Assert(m_vecValues.empty()); // Note that this isn't required just that it is expected. Remove assertion if needed.
#if 0                            // REVIEW:<dbien>: This was an idea but it would be hard for the caller to tell what was up - i.e. in which context we were - without flags or something. \
									 //    Also the same can be accomplished by merely refusing all elements below.
        if ( !_rfFilter( _jrc, _rjvContainer ) )
            return; // Leave this array empty.
#endif                           //0
    JsonRestoreContext<t_tyJsonInputStream> rxc(_jrc);
    if (!_jrc.FMoveDown())
      THROWJSONBADUSAGE("FMoveDown() returned false unexpectedly.");
    for (; !_jrc.FAtEndOfAggregate(); (void)_jrc.FNextElement())
    {
      if (!_rfFilter(_jrc, _rjvContainer))
        continue; // Skip this array element.
      _tyJsoValue jvValue(_jrc.JvtGetValueType());
      jvValue.FromJSONStream(_jrc, _rfFilter);
      m_vecValues.emplace_back(std::move(jvValue));
    }
  }

  template <class t_tyJsonOutputStream>
  void ToJSONStream(JsonValueLife<t_tyJsonOutputStream> &_jvl) const
  {
    _tyConstIterator itCur = m_vecValues.begin();
    _tyConstIterator itEnd = m_vecValues.end();
    for (; itCur != itEnd; ++itCur)
    {
      JsonValueLife<t_tyJsonOutputStream> jvlArrayElement(_jvl, itCur->JvtGetValueType());
      itCur->ToJSONStream(jvlArrayElement);
    }
  }
  template <class t_tyJsonOutputStream, class t_tyFilter>
  void ToJSONStream(JsonValueLife<t_tyJsonOutputStream> &_jvl, _tyJsoValue const &_rjvContainer, t_tyFilter &_rfFilter) const
  {
    typedef JsoIterator<t_tyChar, true> _tyJsoIterator;
    _tyJsoIterator itCur(m_vecValues.begin());
    _tyJsoIterator const itEnd(m_vecValues.end());
    for (; itCur != itEnd; ++itCur)
    {
      if (!_rfFilter(_rjvContainer, itCur))
        continue; // Skip this element.
      JsonValueLife<t_tyJsonOutputStream> jvlArrayElement(_jvl, itCur.GetArrayIterator()->JvtGetValueType());
      itCur.GetArrayIterator()->ToJSONStream(jvlArrayElement, _rfFilter);
    }
  }

protected:
  _tyVectorValues m_vecValues;
};

namespace n_JSONObjects
{

  // Read data from a ReadCursor into a JSON object.
  template <class t_tyJsonInputStream>
  void StreamReadJsoValue(JsonReadCursor<t_tyJsonInputStream> &_jrc, JsoValue<typename t_tyJsonInputStream::_tyChar> &_jv)
  {
    _jv.FromJSONStream(_jrc);
  }

  template <class t_tyJsonInputStream>
  auto JsoValueStreamRead(JsonReadCursor<t_tyJsonInputStream> &_jrc)
      -> JsoValue<typename t_tyJsonInputStream::_tyChar>
  {
    JsoValue<typename t_tyJsonInputStream::_tyChar> jv(_jrc.JvtGetValueType());
    jv.FromJSONStream(_jrc);
    return jv; // we expect clang/gcc to employ NRVO.
  }

  template <class t_tyJsonInputStream, class t_tyJsonOutputStream>
  struct StreamJSONObjects
  {
    typedef t_tyJsonInputStream _tyJsonInputStream;
    typedef t_tyJsonOutputStream _tyJsonOutputStream;
    typedef typename _tyJsonInputStream::_tyCharTraits _tyCharTraits;
    static_assert(std::is_same_v<_tyCharTraits, typename _tyJsonOutputStream::_tyCharTraits>);
    typedef JsonFormatSpec<_tyCharTraits> _tyJsonFormatSpec;
    typedef JsonReadCursor<_tyJsonInputStream> _tyJsonReadCursor;
    typedef JsonValueLife<_tyJsonOutputStream> _tyJsonValueLife;
    typedef std::pair<const char *, vtyFileHandle> _tyPrFilenameHandle;

    static void Stream(const char *_pszInputFile, _tyPrFilenameHandle _prfnhOutput, bool _fReadOnly, bool _fCheckSkippedKey, const _tyJsonFormatSpec *_pjfs)
    {
      typedef JsoValue<typename t_tyJsonInputStream::_tyChar> _tyJsoValue;
      _tyJsoValue jvRead;
      { //B
        _tyJsonInputStream jis;
        jis.Open(_pszInputFile);
        _tyJsonReadCursor jrc;
        jis.AttachReadCursor(jrc);
        jvRead.SetValueType(jrc.JvtGetValueType());
        jvRead.FromJSONStream(jrc);
      } //EB

      if (!_fReadOnly)
      {
        // Open the write file to which we will be streaming JSON.
        _tyJsonOutputStream jos;
        if (!!_prfnhOutput.first)
          jos.Open(_prfnhOutput.first); // Open by default will truncate the file.
        else
          jos.AttachFd(_prfnhOutput.second);
        _tyJsonValueLife jvl(jos, jvRead.JvtGetValueType(), _pjfs);
        jvRead.ToJSONStream(jvl);
      }
    }
    static void Stream(vtyFileHandle _fileInput, _tyPrFilenameHandle _prfnhOutput, bool _fReadOnly, bool _fCheckSkippedKey, const _tyJsonFormatSpec *_pjfs)
    {
      typedef JsoValue<typename t_tyJsonInputStream::_tyChar> _tyJsoValue;
      _tyJsoValue jvRead;
      { //B
        _tyJsonInputStream jis;
        jis.AttachFd(_fileInput);
        _tyJsonReadCursor jrc;
        jis.AttachReadCursor(jrc);
        jvRead.SetValueType(jrc.JvtGetValueType());
        jvRead.FromJSONStream(jrc);
      } //EB

      if (!_fReadOnly)
      {
        // Open the write file to which we will be streaming JSON.
        _tyJsonOutputStream jos;
        if (!!_prfnhOutput.first)
          jos.Open(_prfnhOutput.first); // Open by default will truncate the file.
        else
          jos.AttachFd(_prfnhOutput.second);
        _tyJsonValueLife jvl(jos, jvRead.JvtGetValueType(), _pjfs);
        jvRead.ToJSONStream(jvl);
      }
    }
    static void Stream(const char *_pszInputFile, const char *_pszOutputFile, bool _fReadOnly, bool _fCheckSkippedKey, const _tyJsonFormatSpec *_pjfs)
    {
      typedef JsoValue<typename t_tyJsonInputStream::_tyChar> _tyJsoValue;
      _tyJsoValue jvRead;
      { //B
        _tyJsonInputStream jis;
        jis.Open(_pszInputFile);
        _tyJsonReadCursor jrc;
        jis.AttachReadCursor(jrc);
        jvRead.SetValueType(jrc.JvtGetValueType());
        jvRead.FromJSONStream(jrc);
      } //EB

      if (!_fReadOnly)
      {
        // Open the write file to which we will be streaming JSON.
        _tyJsonOutputStream jos;
        jos.Open(_pszOutputFile); // Open by default will truncate the file.
        _tyJsonValueLife jvl(jos, jvRead.JvtGetValueType(), _pjfs);
        jvRead.ToJSONStream(jvl);
      }
    }
    static void Stream(vtyFileHandle _fileInput, const char *_pszOutputFile, bool _fReadOnly, bool _fCheckSkippedKey, const _tyJsonFormatSpec *_pjfs)
    {
      typedef JsoValue<typename t_tyJsonInputStream::_tyChar> _tyJsoValue;
      _tyJsoValue jvRead;
      { //B
        _tyJsonInputStream jis;
        jis.AttachFd(_fileInput);
        _tyJsonReadCursor jrc;
        jis.AttachReadCursor(jrc);
        jvRead.SetValueType(jrc.JvtGetValueType());
        jvRead.FromJSONStream(jrc);
      } //EB

      if (!_fReadOnly)
      {
        // Open the write file to which we will be streaming JSON.
        _tyJsonOutputStream jos;
        jos.Open(_pszOutputFile); // Open by default will truncate the file.
        _tyJsonValueLife jvl(jos, jvRead.JvtGetValueType(), _pjfs);
        jvRead.ToJSONStream(jvl);
      }
    }
  };

} // namespace n_JSONObjects

__BIENUTIL_END_NAMESPACE
