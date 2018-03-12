
#pragma once

#include "psc_balance_private.h"

// ======================================================================
// BalanceBase

struct BalanceBase
{
  //virtual void run(PscMparticlesBase mprts_base) = 0;
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

  void operator()(struct psc* psc)
  {
    psc_balance_run(balance_, psc);
  }

  void initial(struct psc* psc, uint*& n_prts_by_patch)
  {
    psc_balance_initial(balance_, psc, &n_prts_by_patch);
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
    
    new(balance.sub()) Balance{};
  }

  static void destroy(struct psc_balance* _balance)
  {
    PscBalance<Balance> balance(_balance);
    
    balance->~Balance();
  }
};

