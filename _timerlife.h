#pragma once

// _timerlife.h
// dbien
// 24MAR2024
// This is an attempt to make a timer object whose lifetime is owned by the handler.
// Then there is no need to have a seperate container for the timed object - it is managed by async_wait().

template <typename t_TyTimer>
class CTimerLife : public std::enable_shared_from_this<CTimerLife<t_TyTimer>>
{
public:
  using _TyBase = std::enable_shared_from_this<CTimerLife<t_TyTimer>>;
  using _TyThis = CTimerLife<t_TyTimer>;
  using _TyTimer = t_TyTimer;

  // Delete the copy constructor and copy assignment operator
  CTimerLife(const _TyThis&) = delete;
  CTimerLife& operator=(const _TyThis&) = delete;

  // Default constructor
  CTimerLife() = default;

  // Move constructor
  CTimerLife(_TyThis&&) = default;

  // Move assignment operator
  _TyThis& operator=(_TyThis&&) = default;

  // Destructor
  ~CTimerLife() = default;
};