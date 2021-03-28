#pragma once

// _logarray.h
// Logarithmic array.
// dbien
// 28MAR2021

// The idea here is to create a segmented array (similar to segarray) that instead has a variable block size
//  which starts at some power of two and ends at another power of two. The final power of two block size is then repeated
//  as the array grows. This allows constant time calculation of block size and reasonably easy loops to write.
// My ideas for uses of this are for arrays of object that are generally very small, but you want to allow to be very
//  large in more rare situations. Ideally then you don't want to (as well) change this thing after it is created.
// When initially created the object does not have an array of pointers, just a single pointer to a block of size 2^t_knPow2Min.
// My current plan for usage for this is as the underlying data structure for the lexang _l_data.h, _l_data<> template.

__BIENUTIL_BEGIN_NAMESPACE

// Range of block size are [2^t_knPow2Min, 2^t_knPow2Max].
// If t_tyFOwnLifetime is false_type then the contained object's lifetimes are not managed - i.e. they are assumed to have
//  no side effects upon destruction and are not destructed.
template < class t_TyT, class t_tyFOwnLifetime, size_t t_knPow2Min, size_t t_knPow2Max >
class LogArray
{
  typedef LogArray _TyThis;
public:
  static_assert( t_knPow2Max >= t_knPow2Min );
  typedef t_TyT _TyT;
  static constexpr size_t s_knPow2Min = t_knPow2Min;
  static constexpr size_t s_knPow2Max = t_knPow2Max;
  static constexpr size_t s_knPow2Max = t_knPow2Max;


  LogArray() = default;
  LogArray( size_t _nElements )
  {
    SetSize( _nElements );
  }
  LogArray( LogArray const & _r )
  {
    // todo.
  }
  LogArray & operator =( LogArray const & _r )
  {
    _TyThis copy( _r )
    swap( copy );
    return *this;
  }
  void swap( _TyThis & _r )
  {
    std::swap( m_nElements, _r.m_nElements );
    std::swap( m_ptSingleBlock, _r.m_ptSingleBlock );
    std::swap( m_pptBlocksEnd, _r.m_pptBlocksEnd );
  }

  size_t _NBlockFromEl( size_t _nEl ) const
  {
    // if t_knPow2Min > 0 then we need to multiply _nEl by this before obtaining the index:
    size_t nElShifted = (_nEl+1) << t_knPow2Min;
    size_t nBlock = Log2( nElShifted );
    if ( nBlock >  )
  }

protected:
  size_t m_nElements[0];
  union
  {
    _TyT * m_ptSingleBlock{nullptr}; // When m_nElements <= 2^t_knPow2Min then we point at only this single block. Note that this requires that we compress when the elements fall below this threshold during removal.
    _TyT ** m_pptBlocksBegin; 
  };
    _TyT ** m_pptBlocksEnd{nullptr};
};

__BIENUTIL_END_NAMESPACE
