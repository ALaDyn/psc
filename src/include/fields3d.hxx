
#ifndef FIELDS3D_HXX
#define FIELDS3D_HXX

#include "psc.h"

#include "grid.hxx"
#include <mrc_io.hxx>
#include <kg/Array3d.h>

#include <mrc_profile.h>

#include <type_traits>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <algorithm>
#include <unordered_map>
#include <typeindex>
#include <list>
#include <string>

// ======================================================================
// fields3d

template<typename R>
class Storage
{
public:
  Storage(R* data)
    : data_{data} {}
  
  R* data() { return data_; }
  const R* data() const { return data_; }

  void free()
  {
    ::free(data_);
    data_ = nullptr;
  }
  
private:
  R* data_;
};

// FIXME, do noexcept?

template<typename D, typename R, typename L=kg::LayoutSOA>
class fields3d_container
{
public:
  using Derived = D;
  using Storage = Storage<R>;

  fields3d_container(Int3 ib, Int3 im, int n_comps)
    : ib_{ib}, im_{im},
      n_comps_{n_comps}
  {
  }

  Int3 ib()     const { return ib_; }
  Int3 im()     const { return im_; }
  int n_cells() const { return im_[0] * im_[1] * im_[2]; }
  int n_comps() const { return n_comps_; }
  int size()    const { return n_comps() * n_cells(); }

  R* data() { return storage().data(); }
  const R* data() const { return storage().data(); }

  const R& operator()(int m, int i, int j, int k) const { return storage().data()[index(m, i, j, k)];  }
  R& operator()(int m, int i, int j, int k)             { return storage().data()[index(m, i, j, k)];  }

  void zero(int m)
  {
    // FIXME, only correct for SOA!!!
    memset(&(*this)(m, ib_[0], ib_[1], ib_[2]), 0, n_cells() * sizeof(R));
  }

  void zero(int mb, int me)
  {
    for (int m = mb; m < me; m++) {
      zero(m);
    }
  }

  void zero()
  {
    memset(storage().data(), 0, sizeof(R) * size());
  }

  int index(int m, int i, int j, int k) const
  {
#ifdef BOUNDS_CHECK
    assert(m >= 0 && m < n_comps_);
    assert(i >= ib_[0] && i < ib_[0] + im_[0]);
    assert(j >= ib_[1] && j < ib_[1] + im_[1]);
    assert(k >= ib_[2] && k < ib_[2] + im_[2]);
#endif

    if (L::isAOS::value) {
      return (((((k - ib_[2])) * im_[1] +
		(j - ib_[1])) * im_[0] +
	       (i - ib_[0])) * n_comps_ + m);
    } else {
      return (((((m) * im_[2] +
		 (k - ib_[2])) * im_[1] +
		(j - ib_[1])) * im_[0] +
	       (i - ib_[0])));
    }
  }
      
  void set(int m, R val)
  {
    for (int k = ib_[2]; k < ib_[2] + im_[2]; k++) {
      for (int j = ib_[1]; j < ib_[1] + im_[1]; j++) {
	for (int i = ib_[0]; i < ib_[0] + im_[0]; i++) {
	  (*this)(m, i,j,k) = val;
	}
      }
    }
  }

  void scale(int m, R val)
  {
    for (int k = ib_[2]; k < ib_[2] + im_[2]; k++) {
      for (int j = ib_[1]; j < ib_[1] + im_[1]; j++) {
	for (int i = ib_[0]; i < ib_[0] + im_[0]; i++) {
	  (*this)(m, i,j,k) *= val;
	}
      }
    }
  }

  void copy_comp(int mto, const Derived& from, int mfrom)
  {
    for (int k = ib_[2]; k < ib_[2] + im_[2]; k++) {
      for (int j = ib_[1]; j < ib_[1] + im_[1]; j++) {
	for (int i = ib_[0]; i < ib_[0] + im_[0]; i++) {
	  (*this)(mto, i,j,k) = from(mfrom, i,j,k);
	}
      }
    }
  }

  void axpy_comp(int m_y, R alpha, const Derived& x, int m_x)
  {
    for (int k = ib_[2]; k < ib_[2] + im_[2]; k++) {
      for (int j = ib_[1]; j < ib_[1] + im_[1]; j++) {
	for (int i = ib_[0]; i < ib_[0] + im_[0]; i++) {
	  (*this)(m_y, i,j,k) += alpha * x(m_x, i,j,k);
	}
      }
    }
  }

  R max_comp(int m)
  {
    R rv = -std::numeric_limits<R>::max();
    for (int k = ib_[2]; k < ib_[2] + im_[2]; k++) {
      for (int j = ib_[1]; j < ib_[1] + im_[1]; j++) {
	for (int i = ib_[0]; i < ib_[0] + im_[0]; i++) {
	  rv = std::max(rv, (*this)(m, i,j,k));
	}
      }
    }
    return rv;
  }

  void dump()
  {
    for (int k = ib_[2]; k < ib_[2] + im_[2]; k++) {
      for (int j = ib_[1]; j < ib_[1] + im_[1]; j++) {
	for (int i = ib_[0]; i < ib_[0] + im_[0]; i++) {
	  for (int m = 0; m < n_comps_; m++) {
	    mprintf("dump: ijk %d:%d:%d m %d: %g\n", i, j, k, m, (*this)(m, i,j,k));
	  }
	}
      }
    }
  }

protected:
  Storage& storage()
  {
    return derived().storageImpl();
  }

  const Storage& storage() const
  {
    return derived().storageImpl();
  }
  
  Derived& derived()
  {
    return *static_cast<Derived*>(this);
  }

  const Derived& derived() const
  {
    return *static_cast<const Derived*>(this);
  }

protected: // FIXME, for now
  const Int3 ib_, im_; //> lower bounds and length per direction
  const int n_comps_; // # of components
};

template<typename R, typename L=kg::LayoutSOA>
struct fields3d : fields3d_container<fields3d<R, L>, R, L>
{
  using Base = fields3d_container<fields3d<R, L>, R, L>;
  using Storage = Storage<R>;
  using real_t = R;
  using layout = L;

  using Base::ib_;
  using Base::im_;
  using Base::n_comps_;

  fields3d(const Grid_t& grid, Int3 ib, Int3 im, int n_comps, real_t* data=nullptr)
    : Base{ib, im, n_comps},
      grid_{grid},
      storage_{data ? data : (real_t *) calloc(Base::size(), sizeof(real_t))}
  {
  }

  void dtor()
  {
    storage_.free();
  }

  const Grid_t& grid() const { return grid_; }

private:
  Storage& storageImpl() { return storage_; }
  const Storage& storageImpl() const { return storage_; }

  friend class fields3d_container<fields3d<R, L>, R, L>;
  
  Storage storage_;
  const Grid_t& grid_;
};

// ======================================================================
// MfieldsBase

struct MfieldsBase
{
  using convert_func_t = void (*)(MfieldsBase&, MfieldsBase&, int, int);
  using Convert = std::unordered_map<std::type_index, convert_func_t>;
  
  struct fields_t { struct real_t {}; };
  
  MfieldsBase(const Grid_t& grid, int n_fields, Int3 ibn)
    : grid_(&grid),
      n_fields_(n_fields),
      ibn_(ibn)
  {
    instances.push_back(this);
  }

  virtual ~MfieldsBase()
  {
    instances.remove(this);
  }

  virtual void reset(const Grid_t& grid) { grid_ = &grid; }
  
  int n_patches() const { return grid_->n_patches(); }
  int n_comps() const { return n_fields_; }
  Int3 ibn() const { return ibn_; }

  virtual void zero_comp(int m) = 0;
  virtual void set_comp(int m, double val) = 0;
  virtual void scale_comp(int m, double val) = 0;
  virtual void axpy_comp(int m_y, double alpha, MfieldsBase& x, int m_x) = 0;
  virtual void copy_comp(int mto, MfieldsBase& from, int mfrom) = 0;
  virtual double max_comp(int m) = 0;
  virtual void write_as_mrc_fld(mrc_io *io, const std::string& name, const std::vector<std::string>& comp_names)
  {
    assert(0);
  }

  void zero()            { for (int m = 0; m < n_fields_; m++) zero_comp(m); }
  void scale(double val) { for (int m = 0; m < n_fields_; m++) scale_comp(m, val); }
  void axpy(double alpha, MfieldsBase& x)
  {
    for (int m = 0; m < n_fields_; m++) {
      axpy_comp(m, alpha, x, m);
    }
  }

  const Grid_t& grid() const { return *grid_; }
  
  template<typename MF>
  MF& get_as(int mb, int me)
  {
    // If we're already the subtype, nothing to be done
    if (typeid(*this) == typeid(MF)) {
      return *dynamic_cast<MF*>(this);
    }
    
    static int pr;
    if (!pr) {
      pr = prof_register("Mfields_get_as", 1., 0, 0);
    }
    prof_start(pr);

    // mprintf("get_as %s (%s) %d %d\n", type, psc_mfields_type(mflds_base), mb, me);
    
    auto& mflds = *new MF{grid(), n_comps(), ibn()};
    
    MfieldsBase::convert(*this, mflds, mb, me);

    prof_stop(pr);
    return mflds;
  }

  template<typename MF>
  void put_as(MF& mflds, int mb, int me)
  {
    // If we're already the subtype, nothing to be done
    if (typeid(*this) == typeid(mflds)) {
      return;
    }
    
    static int pr;
    if (!pr) {
      pr = prof_register("Mfields_put_as", 1., 0, 0);
    }
    prof_start(pr);
    
    MfieldsBase::convert(mflds, *this, mb, me);
    delete &mflds;
    
    prof_stop(pr);
  }

  virtual const Convert& convert_to() { static const Convert convert_to_; return convert_to_; }
  virtual const Convert& convert_from() { static const Convert convert_from_; return convert_from_; }
  static void convert(MfieldsBase& mf_from, MfieldsBase& mf_to, int mb, int me);

  static std::list<MfieldsBase*> instances;
  
protected:
  int n_fields_;
  const Grid_t* grid_;
  Int3 ibn_;
};

#if 0

using MfieldsStateBase = MfieldsBase;

#else

// ======================================================================
// MfieldsStateBase

struct MfieldsStateBase
{
  using convert_func_t = void (*)(MfieldsStateBase&, MfieldsStateBase&, int, int);
  using Convert = std::unordered_map<std::type_index, convert_func_t>;
  
  MfieldsStateBase(const Grid_t& grid, int n_fields, Int3 ibn)
    : grid_(&grid),
      n_fields_(n_fields),
      ibn_(ibn)
  {
    instances.push_back(this);
  }

  virtual ~MfieldsStateBase()
  {
    instances.remove(this);
  }

  virtual void reset(const Grid_t& grid) { grid_ = &grid; }

  int n_patches() const { return grid_->n_patches(); }
  int n_comps() const { return n_fields_; }
  Int3 ibn() const { return ibn_; }

  virtual void write_as_mrc_fld(mrc_io *io, const std::string& name, const std::vector<std::string>& comp_names)
  {
    assert(0);
  }

  const Grid_t& grid() { return *grid_; }

  virtual const Convert& convert_to() { static const Convert convert_to_; return convert_to_; }
  virtual const Convert& convert_from() { static const Convert convert_from_; return convert_from_; }
  static void convert(MfieldsStateBase& mf_from, MfieldsStateBase& mf_to, int mb, int me);

  static std::list<MfieldsStateBase*> instances;

  template<typename MF>
  MF& get_as(int mb, int me)
  {
    // If we're already the subtype, nothing to be done
    if (typeid(*this) == typeid(MF)) {
      return *dynamic_cast<MF*>(this);
    }
    
    static int pr;
    if (!pr) {
      pr = prof_register("Mfields_get_as", 1., 0, 0);
    }
    prof_start(pr);

    // mprintf("get_as %s (%s) %d %d\n", type, psc_mfields_type(mflds_base), mb, me);
    
    auto& mflds = *new MF{grid()};
    
    MfieldsStateBase::convert(*this, mflds, mb, me);

    prof_stop(pr);
    return mflds;
  }

  template<typename MF>
  void put_as(MF& mflds, int mb, int me)
  {
    // If we're already the subtype, nothing to be done
    if (typeid(*this) == typeid(mflds)) {
      return;
    }
    
    static int pr;
    if (!pr) {
      pr = prof_register("Mfields_put_as", 1., 0, 0);
    }
    prof_start(pr);
    
    MfieldsStateBase::convert(mflds, *this, mb, me);
    delete &mflds;
    
    prof_stop(pr);
  }

protected:
  int n_fields_;
  const Grid_t* grid_;
  Int3 ibn_;
};

#endif

// ======================================================================
// Mfields

template<typename F>
struct Mfields : MfieldsBase
{
  using fields_t = F;
  using real_t = typename fields_t::real_t;

  Mfields(const Grid_t& grid, int n_fields, Int3 ibn)
    : MfieldsBase(grid, n_fields, ibn)
  {
    unsigned int size = 1;
    for (int d = 0; d < 3; d++) {
      ib[d] = -ibn[d];
      im[d] = grid_->ldims[d] + 2 * ibn[d];
      size *= im[d];
    }

    data.reserve(n_patches());
    for (int p = 0; p < n_patches(); p++) {
      data.emplace_back(new real_t[n_fields * size]{});
    }
  }

  virtual void reset(const Grid_t& grid) override
  {
    MfieldsBase::reset(grid);
    data.clear();

    unsigned int size = 1;
    for (int d = 0; d < 3; d++) {
      size *= im[d];
    }

    data.reserve(n_patches());
    for (int p = 0; p < n_patches(); p++) {
      data.emplace_back(new real_t[n_comps() * size]);
    }
  }
  
  fields_t operator[](int p)
  {
    return fields_t(grid(), Int3::fromPointer(ib), Int3::fromPointer(im), n_fields_, data[p].get());
  }

  void zero_comp(int m) override
  {
    for (int p = 0; p < n_patches(); p++) {
      (*this)[p].zero(m);
    }
  }

  void set_comp(int m, double val) override
  {
    for (int p = 0; p < n_patches(); p++) {
      (*this)[p].set(m, val);
    }
  }
  
  void scale_comp(int m, double val) override
  {
    for (int p = 0; p < n_patches(); p++) {
      (*this)[p].scale(m, val);
    }
  }

  void copy_comp(int mto, MfieldsBase& from_base, int mfrom) override
  {
    // FIXME? dynamic_cast would actually be more appropriate
    Mfields& from = static_cast<Mfields&>(from_base);
    for (int p = 0; p < n_patches(); p++) {
      (*this)[p].copy_comp(mto, from[p], mfrom);
    }
  }
  
  void axpy_comp(int m_y, double alpha, MfieldsBase& x_base, int m_x) override
  {
    // FIXME? dynamic_cast would actually be more appropriate
    Mfields& x = static_cast<Mfields&>(x_base);
    for (int p = 0; p < n_patches(); p++) {
      (*this)[p].axpy_comp(m_y, alpha, x[p], m_x);
    }
  }

  double max_comp(int m) override
  {
    double rv = -std::numeric_limits<double>::max();
    for (int p = 0; p < n_patches(); p++) {
      rv = std::max(rv, double((*this)[p].max_comp(m)));
    }
    return rv;
  }

  void write_as_mrc_fld(mrc_io *io, const std::string& name, const std::vector<std::string>& comp_names) override
  {
    MrcIo::write_mflds(io, *this, name, comp_names);
  }

  static const Convert convert_to_, convert_from_;
  const Convert& convert_to() override { return convert_to_; }
  const Convert& convert_from() override { return convert_from_; }

  std::vector<std::unique_ptr<real_t[]>> data;
  int ib[3]; //> lower left corner for each patch (incl. ghostpoints)
  int im[3]; //> extent for each patch (incl. ghostpoints)
};

// ======================================================================
// MfieldsStateFromMfields

template<typename Mfields>
struct MfieldsStateFromMfields : MfieldsStateBase
{
  using fields_t = typename Mfields::fields_t;
  using real_t = typename Mfields::real_t;

  MfieldsStateFromMfields(const Grid_t& grid)
    : MfieldsStateBase{grid, NR_FIELDS, grid.ibn}, // FIXME, still hacky ibn handling...
      mflds_{grid, NR_FIELDS, grid.ibn}
  {}

  fields_t operator[](int p) { return mflds_[p]; }

  static const Convert convert_to_, convert_from_;
  const Convert& convert_to() override { return convert_to_; }
  const Convert& convert_from() override { return convert_from_; }

  Mfields& mflds() { return mflds_; }

private:
  Mfields mflds_;
};

#endif
