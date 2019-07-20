
#ifndef KG_SARRAY_H
#define KG_SARRAY_H

#include <kg/SArrayContainer.h>

namespace kg
{

// ======================================================================
// SArray

template <typename T, typename L = LayoutSOA>
struct SArray;

template <typename T, typename L>
struct SArrayContainerInnerTypes<SArray<T, L>>
{
  using Layout = L;
  using Storage = Storage<T>;
};

template <typename T, typename L>
struct SArray : SArrayContainer<SArray<T, L>>
{
  using Base = SArrayContainer<SArray<T, L>>;
  using Storage = typename Base::Storage;
  using real_t = typename Base::value_type;

  SArray(Int3 ib, Int3 im, int n_comps)
    : Base{ib, im, n_comps},
      storage_{(real_t*)calloc(Base::size(), sizeof(real_t))}
  {}

  void dtor() { storage_.free(); }

private:
  Storage storage_;

  Storage& storageImpl() { return storage_; }
  const Storage& storageImpl() const { return storage_; }

  friend class SArrayContainer<SArray<T, L>>;
};

} // namespace kg

#endif
