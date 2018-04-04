
#pragma once

#include "psc_balance_private.h"
#include "particles.hxx"

// ======================================================================
// BalanceBase

struct BalanceBase
{
  virtual std::vector<uint> initial(psc* psc, const std::vector<uint>& n_prts_by_patch) = 0;
  virtual void operator()(psc* psc, MparticlesBase& mp) = 0;
};

// ======================================================================
// PscBalance

template<typename S>
struct PscBalance
{
  using sub_t = S;
  
  static_assert(std::is_convertible<sub_t*, BalanceBase*>::value,
  		"sub classes used in PscBalance must derive from BalanceBase");
  
  explicit PscBalance(psc_balance *balance)
    : balance_(balance)
  {}

  void operator()(struct psc* psc, PscMparticlesBase mprts)
  {
    if (balance_->every > 0 && psc->timestep % balance_->every == 0) {
      (*sub())(psc, *mprts.sub());
    }
  }

  std::vector<uint> initial(struct psc* psc, const std::vector<uint>& n_prts_by_patch)
  {
    return sub()->initial(psc, n_prts_by_patch);
  }

  sub_t* sub() { return mrc_to_subobj(balance_, sub_t); }
  sub_t* operator->() { return sub(); }

private:
  psc_balance *balance_;
};

using PscBalanceBase = PscBalance<BalanceBase>;

// ======================================================================
// BalanceWrapper

template<typename Balance>
class BalanceWrapper
{
public:
  const static size_t size = sizeof(Balance);

  static void setup(struct psc_balance* _balance)
  {
    PscBalance<Balance> balance(_balance);
    new(balance.sub()) Balance{_balance->every, _balance->factor_fields,
	_balance->print_loads, _balance->write_loads};
  }

  static void destroy(struct psc_balance* _balance)
  {
    PscBalance<Balance> balance(_balance);
    balance->~Balance();
  }
};

